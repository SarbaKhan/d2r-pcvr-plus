#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace framevr::resolution {
struct Size {
    uint32_t w=0,h=0;
    bool operator==(const Size&)const=default;
    explicit operator bool()const{return w&&h;}
};
inline constexpr const char* Choices[]={"Desktop","Headset","1536 x 1536","2048 x 2048","2560 x 2560","3072 x 3072","4096 x 4096"};
inline constexpr int ChoiceCount=7;
// Fixed presets are square; Headset preserves the runtime's recommended aspect.
// Bound allocations and respect BOTH eyes' runtime limits. Never upscale only
// the XR copy: this size is sent to the game's own WM_SIZE resize path.
inline Size Select(int choice,Size recommended,Size maximum) {
    if(choice<=0||choice>=ChoiceCount||!maximum)return {};
    constexpr uint32_t presets[]={0,0,1536,2048,2560,3072,4096};
    Size wanted=choice==1?recommended:Size{presets[choice],presets[choice]};
    if(!wanted)return {};
    maximum.w=std::min(maximum.w,4096u);maximum.h=std::min(maximum.h,4096u);
    const double scale=std::min({1.,double(maximum.w)/wanted.w,double(maximum.h)/wanted.h});
    wanted={uint32_t(std::floor(wanted.w*scale)),uint32_t(std::floor(wanted.h*scale))};
    return wanted.w>=640&&wanted.h>=480?wanted:Size{};
}
inline uint64_t Pack(Size s){return uint64_t(s.w)<<32|s.h;}
// Native HD menus and font sizing expect a widescreen content band. The world
// can remain square/portrait; do not enlarge fonts to that taller eye height.
inline Size UiCanvas(Size render) {
    return {render.w,std::min(render.h,render.w*9u/16u)};
}
inline Size Unpack(uint64_t v){return {uint32_t(v>>32),uint32_t(v)};}
inline int Coordinate(int point,uint32_t from,uint32_t to) {
    if(from<2||to<2)return point;
    return int(std::clamp(std::round(double(point)*(to-1)/(from-1)),-32768.,32767.));
}
}
