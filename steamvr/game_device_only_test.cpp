#include "../vr/game_device_only.h"
#include <cassert>
#include <cstdio>
using namespace reshade::api;
struct Device {device_api api;device_api get_api()const{return api;}};
struct Object {Device* owner;Device* get_device()const{return owner;}};
static int calls=0;
static bool Draw(Object*,int& value){++calls;++value;return true;}
static void Created(Device*,int value){calls+=value;}
int main(){
    Device game{device_api::d3d12},runtime{device_api::d3d11},other{device_api::vulkan};
    Object gameList{&game},runtimeList{&runtime},otherList{&other},orphan{nullptr};
    int value=3;
    using Filter=d2rvr::D3D12Only<&Draw>;
    for(auto* object:{&runtimeList,&otherList,&orphan,static_cast<Object*>(nullptr)})
        assert(!Filter::Call(object,value));
    assert(calls==0&&value==3); // ignored bool event must NOT suppress the runtime's draw
    assert(Filter::Call(&gameList,value)&&calls==1&&value==4);
    d2rvr::D3D12Only<&Created>::Call(&runtime,7);
    d2rvr::D3D12Only<&Created>::Call(nullptr,7);
    assert(calls==1);
    d2rvr::D3D12Only<&Created>::Call(&game,7);assert(calls==8);
    std::puts("ReShade device guard: game events forwarded; runtime devices/null rejected without side effects");
}
