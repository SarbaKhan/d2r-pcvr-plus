#pragma once
#include "frame_math.h"
namespace framevr::dioramasky {
// This preview always uses an opaque virtual environment, even on WiVRn.
constexpr bool Preview=true;
struct Rays { math::Vec corner,horizontal,vertical; };
inline Rays MakeRays(math::Quat orientation,float left,float right,float down,float up) {
    const float l=std::tan(left),r=std::tan(right),d=std::tan(down),u=std::tan(up);
    return {math::Rotate(orientation,{l,u,-1}),math::Rotate(orientation,{r-l,0,0}),
        math::Rotate(orientation,{0,d-u,0})};
}
inline math::Vec Direction(const Rays& rays,float u,float v) {
    return math::Normalized(rays.corner+rays.horizontal*u+rays.vertical*v);
}
}
