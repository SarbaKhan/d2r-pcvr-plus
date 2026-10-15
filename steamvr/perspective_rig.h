#pragma once
#include "frame_math.h"
#include <cstdint>

namespace framevr::perspective {
// Persisted IDs deliberately retain the historical meaning (3 is the deferred table).
constexpr bool Supported(int mode) { return mode==1||mode==2||mode==4; }
constexpr bool External(int mode) { return mode==1||mode==2; }
constexpr int KeyMode(int key) { return key==1?1:key==2?2:key==3?4:0; }
constexpr int Next(int mode) { return mode==4?1:mode==1?2:4; }
struct Pose { math::Vec p; math::Quat q; };
struct Rig {
    math::Vec anchor{}, reference{};
    math::Quat rotation{0,0,0,1};
    float units=1;
};
inline bool Finite(math::Vec v) { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }
inline bool Valid(const Rig& r) {
    const auto q=r.rotation;
    const float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
    return Finite(r.anchor)&&Finite(r.reference)&&std::isfinite(r.units)&&r.units>.001f&&r.units<10000&&
        std::isfinite(norm)&&std::fabs(norm-1)<.01f;
}
inline Pose Transform(const Rig& r, Pose eye) {
    return {r.anchor+math::Rotate(r.rotation,eye.p-r.reference)*r.units,
            math::Multiply(r.rotation,eye.q)};
}
inline math::Quat Pitch(float angle) {return {std::sin(angle*.5f),0,0,std::cos(angle*.5f)};}
inline Rig Orbit(math::Vec hero,float height,float distance,float heading,float down,
                 float units,math::Vec reference,float referenceYaw) {
    const auto orientation=math::Multiply(math::Yaw(heading),Pitch(-down));
    const auto forward=math::Rotate(orientation,{0,0,-1});
    return {hero+math::Vec{0,height,0}-forward*distance, reference,
            math::Multiply(orientation,math::Yaw(-referenceYaw)),units};
}
}
