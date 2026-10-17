#include "light_bounds_shader.h"
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <vector>
using Microsoft::WRL::ComPtr;
struct Pair {float low,high;};
static void Check(HRESULT hr,const char* what){if(FAILED(hr)){std::fprintf(stderr,"%s: %08lx\n",what,(unsigned long)hr);throw std::runtime_error(what);}}
static void Run(ID3D11Device* device,ID3D11DeviceContext* context,ID3D11ComputeShader* shader,unsigned w,unsigned h,bool atlas=false) {
    constexpr unsigned cols=32,rows=20,stride=36;
    const unsigned actualW=atlas?2*w:w,actualH=atlas?2*h:h,ox=atlas?w/2:0,oy=atlas?h/2:0;
    std::vector<float> pixels(size_t(actualW)*actualH,.99f);
    std::array<Pair,stride*rows> expected{};
    for(unsigned ty=0;ty<rows;++ty)for(unsigned tx=0;tx<cols;++tx) {
      const unsigned x0=tx*w/cols,x1=(tx+1)*w/cols,y0=ty*h/rows,y1=(ty+1)*h/rows;
      for(unsigned y=y0;y<y1;++y)for(unsigned x=x0;x<x1;++x) {
        const auto tile=tx+ty*cols;
        float d=tile%7==0?0.f:.1f+float(tile%5)*.01f;
        if(d>0&&x==x1-3&&y==y1-3)d=.02f;
        if(d>0&&x==x1-4&&y==y1-4)d=.7f+float(tile%5)*.01f;
        if(x==x0+1&&y==y0+1)d=0; // background must not become the minimum
        pixels[size_t(y+oy)*actualW+x+ox]=d;
      }
    }
    // Independent oracle: visit every pixel in each tile, including extrema
    // near its bottom edge. The native height divisor misses those in squares.
    for(unsigned ty=0;ty<rows;++ty)for(unsigned tx=0;tx<cols;++tx) {
        Pair p{1,0};
        const unsigned x0=tx*w/cols,y0=ty*h/rows;
        // Retain the native 2x2 gather's padded edge on odd-sized tiles and
        // clamp at the actual resource edge, including viewport atlas offsets.
        const unsigned x1=x0+(((tx+1)*w/cols-x0+1)/2)*2,y1=y0+(((ty+1)*h/rows-y0+1)/2)*2;
        for(unsigned y=y0;y<y1;++y)for(unsigned x=x0;x<x1;++x){
            float d=pixels[size_t(std::min(y+oy,actualH-1))*actualW+std::min(x+ox,actualW-1)];if(d>0){p.low=std::min(p.low,d);p.high=std::max(p.high,d);}
        }
        expected[ty*stride+tx]=p.high>0?p:Pair{};
    }
    D3D11_TEXTURE2D_DESC desc{};desc.Width=actualW;desc.Height=actualH;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;
    desc.Format=DXGI_FORMAT_R32_FLOAT;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA initial{pixels.data(),actualW*sizeof(float),0};ComPtr<ID3D11Texture2D> texture;
    Check(device->CreateTexture2D(&desc,&initial,&texture),"depth texture");
    ComPtr<ID3D11ShaderResourceView> srv;Check(device->CreateShaderResourceView(texture.Get(),nullptr,&srv),"depth view");
    const float rect[16]={0,0,0,0,float(w),float(h),0,0,0,0,atlas?.25f:0,atlas?.25f:0,atlas?.5f:1,atlas?.5f:1,0,0};
    const unsigned grid[8]={cols,rows,0,0,0,stride,0,0};
    D3D11_BUFFER_DESC cb{};cb.ByteWidth=sizeof rect;cb.Usage=D3D11_USAGE_IMMUTABLE;cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    initial={rect,0,0};ComPtr<ID3D11Buffer> rectangle,counts,output,readback;
    Check(device->CreateBuffer(&cb,&initial,&rectangle),"rectangle");
    cb.ByteWidth=sizeof grid;initial={grid,0,0};Check(device->CreateBuffer(&cb,&initial,&counts),"grid");
    D3D11_BUFFER_DESC out{};out.ByteWidth=sizeof expected;out.Usage=D3D11_USAGE_DEFAULT;out.BindFlags=D3D11_BIND_UNORDERED_ACCESS;
    out.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;out.StructureByteStride=sizeof(Pair);
    std::array<Pair,stride*rows> zeros{};initial={zeros.data(),0,0};Check(device->CreateBuffer(&out,&initial,&output),"output");
    D3D11_UNORDERED_ACCESS_VIEW_DESC ud{};ud.ViewDimension=D3D11_UAV_DIMENSION_BUFFER;ud.Buffer.NumElements=stride*rows;
    ComPtr<ID3D11UnorderedAccessView> uav;Check(device->CreateUnorderedAccessView(output.Get(),&ud,&uav),"bounds view");
    out.Usage=D3D11_USAGE_STAGING;out.BindFlags=out.MiscFlags=out.StructureByteStride=0;out.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    Check(device->CreateBuffer(&out,nullptr,&readback),"readback");
    D3D11_SAMPLER_DESC sd{};sd.Filter=D3D11_FILTER_MIN_MAG_MIP_POINT;sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sd.MaxLOD=D3D11_FLOAT32_MAX;
    ComPtr<ID3D11SamplerState> sampler;Check(device->CreateSamplerState(&sd,&sampler),"sampler");
    auto* rawRect=rectangle.Get();auto* rawCounts=counts.Get();auto* rawSrv=srv.Get();auto* rawUav=uav.Get();auto* rawSampler=sampler.Get();
    context->CSSetShader(shader,nullptr,0);context->CSSetConstantBuffers(3,1,&rawRect);context->CSSetConstantBuffers(12,1,&rawCounts);
    context->CSSetShaderResources(10,1,&rawSrv);context->CSSetSamplers(1,1,&rawSampler);context->CSSetUnorderedAccessViews(0,1,&rawUav,nullptr);
    context->Dispatch(cols,rows,1);context->CopyResource(readback.Get(),output.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};Check(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped),"map");
    auto* got=static_cast<const Pair*>(mapped.pData);bool correct=true;
    for(unsigned i=0;i<expected.size();++i)if(!std::isfinite(got[i].low)||!std::isfinite(got[i].high)||std::abs(got[i].low-expected[i].low)>1e-6||std::abs(got[i].high-expected[i].high)>1e-6){
        std::fprintf(stderr,"%ux%u atlas=%d tile=%u: got %g,%g wanted %g,%g\n",w,h,atlas,i,got[i].low,got[i].high,expected[i].low,expected[i].high);correct=false;break;
    }
    context->Unmap(readback.Get(),0);context->ClearState();if(!correct)throw std::runtime_error("light depth bounds differ from full tile scan");
}
int main(){try {
    ComPtr<ID3DBlob> blob,errors;
    const auto hr=D3DCompile(framevr::lightbounds::Shader,sizeof framevr::lightbounds::Shader-1,"light-bounds",nullptr,nullptr,"main","cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&blob,&errors);
    if(FAILED(hr)&&errors)std::fprintf(stderr,"%s\n",static_cast<const char*>(errors->GetBufferPointer()));Check(hr,"compile");
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL feature;
    Check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&feature,&context),"WARP device");
    ComPtr<ID3D11ComputeShader> shader;Check(device->CreateComputeShader(blob->GetBufferPointer(),blob->GetBufferSize(),nullptr,&shader),"shader");
    Run(device.Get(),context.Get(),shader.Get(),320,320);Run(device.Get(),context.Get(),shader.Get(),640,200);
    Run(device.Get(),context.Get(),shader.Get(),320,200);Run(device.Get(),context.Get(),shader.Get(),320,320,true);
    // A full square source divisible by the native 32x20 grid; non-square tiles.
    Run(device.Get(),context.Get(),shader.Get(),3840,3840);
    Run(device.Get(),context.Get(),shader.Get(),4096,4096);
    std::puts("Light bounds GPU: rectangular/square tiles, lower-edge extrema, background, atlas coordinates and output stride passed");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
