#pragma once
namespace framevr::lightbounds {
// Independently written depth reduction for the verified native light-bin ABI.
// A 32x20 lighting grid has rectangular tiles at square VR resolutions. The
// native shader uses index / tileHeight for its row, missing lower rows (and
// spilling into other tiles in wide views). Both coordinates need tileWidth.
// No game shader bytecode is distributed here.
inline constexpr char Shader[]=R"hlsl(
cbuffer DepthRect : register(b3) { float4 rect[4]; };
cbuffer LightGrid : register(b12) { uint4 grid[2]; };
Texture2D<float> depthTexture : register(t10);
SamplerState depthSampler : register(s1);
RWStructuredBuffer<float2> bounds : register(u0);
groupshared uint nearestBits;
groupshared uint farthestBits;
[numthreads(32,1,1)]
void main(uint3 tile:SV_GroupID,uint lane:SV_GroupIndex) {
    if(lane==0) { nearestBits=0; farthestBits=asuint(1.0); }
    GroupMemoryBarrierWithGroupSync();
    uint2 begin=uint2(float2(tile.xy)*rect[1].xy/float2(grid[0].xy));
    uint2 end=uint2(float2(tile.xy+1)*rect[1].xy/float2(grid[0].xy));
    uint2 extent=(end-begin+1)/2; // each gather covers 2x2 depth samples
    float low=1.0,high=0.0;
    for(uint i=lane;i<extent.x*extent.y;i+=32) {
        uint2 local=uint2(i%extent.x,i/extent.x);
        float2 uv=(float2(begin+2*local)+0.5)/rect[1].xy;
        uv=uv*rect[3].xy+rect[2].zw;
        float4 d=depthTexture.GatherRed(depthSampler,uv);
        high=max(high,max(max(d.x,d.y),max(d.z,d.w)));
        // Zero is background in reverse Z and must not enlarge a tile's range.
        d=float4(d.x>0?d.x:1.0,d.y>0?d.y:1.0,d.z>0?d.z:1.0,d.w>0?d.w:1.0);
        low=min(low,min(min(d.x,d.y),min(d.z,d.w)));
    }
    InterlockedMin(farthestBits,asuint(low));
    InterlockedMax(nearestBits,asuint(high));
    GroupMemoryBarrierWithGroupSync();
    if(lane==0) bounds[tile.y*grid[1].y+tile.x]=nearestBits==0?float2(0,0):float2(asfloat(farthestBits),asfloat(nearestBits));
}
)hlsl";
}
