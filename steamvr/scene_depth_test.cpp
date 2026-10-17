#include "scene_depth.h"
#include <cassert>
#include <cstdio>
using namespace framevr::scenedepth;
int main(){
    Candidates scene;
    auto bind=[&](uintptr_t device,uint64_t resource,uint32_t w,uint32_t h,int n=1){
        for(int i=0;i<n;++i)scene.Bind(device,resource,w,h,true);
        scene.Barrier(device,resource,uint32_t(reshade::api::resource_usage::shader_resource));
    };
    // A heavily bound square shadow target must not beat the actual scene.
    for(int i=0;i<100;++i)scene.Bind(1,10,4096,4096,false);
    scene.Barrier(1,10,uint32_t(reshade::api::resource_usage::shader_resource));
    assert(!scene.Select(1,4096,4096).resource); // exact-size shadow still rejected
    bind(1,20,2200,2200,3);
    assert(scene.Select(1,2200,2200).resource==20);
    scene.EndEye(1);assert(!scene.Select(1,2200,2200).resource);
    // Left-eye records are not a right-eye fallback. Unknown states are unsafe.
    scene.Bind(1,30,2200,2200,true);assert(!scene.Select(1,2200,2200).resource);
    scene.Barrier(1,30,uint32_t(reshade::api::resource_usage::depth_stencil_read));
    assert(scene.Select(1,2200,2200).resource==30);
    scene.Destroy(1,30);assert(!scene.Select(1,2200,2200).resource);
    scene.Bind(1,30,2200,2200,true);assert(!scene.Select(1,2200,2200).resource); // recycled address has no state
    scene.Destroy(1);
    // Preserve supersampling and non-square windows without accepting shadows.
    bind(1,40,3840,2160,4);scene.Bind(1,41,2048,2048,false);
    assert(scene.Select(1,1920,1080).resource==40);
    bind(1,42,960,540,5);assert(scene.Select(1,1920,1080).resource==42);
    bind(1,43,1920,1080);assert(scene.Select(1,1920,1080).resource==43); // exact preferred
    bind(2,43,1920,1080,100);assert(scene.Select(1,1920,1080).binds==1); // runtime device isolation
    assert(scene.Select(2,1920,1080).binds==100);
    scene.Destroy(1);assert(!scene.Select(1,1920,1080).resource&&scene.Select(2,1920,1080).resource);
    assert(!scene.Select(2,0,1080).resource);
    scene.EndEye(2);scene.EndEye(2);assert(!scene.Select(2,1920,1080).resource);
    bind(2,43,4096,4096);assert(scene.Select(2,4096,4096).resource==43); // exact square scene is allowed
    scene.Destroy(2);bind(2,44,2048,2048);
    assert(scene.Select(2,4096,4096).resource==44); // square DLSS scene is valid when color-paired
    std::puts("Scene depth: shadow rejection, scaling, per-eye freshness, known states and resource/device lifetimes passed");
}
