#pragma once
#include "frame_math.h"
#include <cstdint>

namespace framevr::perspective {
// Persisted IDs: 3 is Diorama (F4), 4 is immersive (F3).
constexpr bool Supported(int mode) { return mode==1||mode==2||mode==3||mode==4; }
constexpr bool External(int mode) { return mode==1||mode==2||mode==3; }
constexpr int KeyMode(int key) { return key==1?1:key==2?2:key==3?4:key==4?3:0; }
constexpr int Next(int mode) { return mode==4?1:mode==1?2:mode==2?3:4; }
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
// Mouse aiming uses the orbit camera's neutral pose, never a live HMD pose.
inline Pose MouseCamera(const Rig& r,float referenceYaw) {
    return {r.anchor,math::Multiply(r.rotation,math::Yaw(referenceYaw))};
}
inline math::Vec ToTracking(const Rig& r,math::Vec world) {
    return r.reference+math::Rotate(math::Conjugate(r.rotation),world-r.anchor)*(1/r.units);
}
constexpr bool MouseVisible(int mode,bool panel) {return mode==1||((mode==2||mode==3)&&panel);}
inline math::Vec MouseDirection(Pose camera,float sx,float sy,float u,float v) {
    return math::Normalized(math::Rotate(camera.q,{(2*u-1)/sx,(1-2*v)/sy,-1}));
}
// A world-space marker at the player's ground height. The game still performs
// its own terrain/object picking along this ray (no extra scene traversal).
inline math::Vec MouseMarker(Pose camera,math::Vec direction,float groundY,float fallback) {
    const float t=direction.y<-.0001f?(groundY-camera.p.y)/direction.y:-1.f;
    return camera.p+direction*(std::isfinite(t)&&t>0&&t<fallback*10?t:fallback);
}
inline math::Quat Pitch(float angle) {return {std::sin(angle*.5f),0,0,std::cos(angle*.5f)};}
// Head-relative horizontal placement, at an absolute height above the floor.
inline math::Vec TablePosition(math::Vec head,float yaw,float distance,float floor,float height) {
    auto p=head+math::Rotate(math::Yaw(yaw),{0,0,-distance});
    p.y=floor+height;return p;
}
// Horizontal miniature: tracking-space table maps to the sampled world centre.
inline Rig Table(math::Vec world,math::Vec table,float heading,float units) {
    return {world,table,math::Yaw(heading),units};
}
inline math::Vec Follow(math::Vec current,math::Vec target,float dt,float tau,float maxLag) {
    const auto delta=target-current;
    if(math::Dot(delta,delta)>maxLag*maxLag*16)return target; // teleport/level change
    auto next=current+delta*(1-std::exp(-std::clamp(dt,0.f,.1f)/std::max(tau,.001f)));
    auto lag=target-next;const float length=std::sqrt(math::Dot(lag,lag));
    if(length>maxLag)next=target-lag*(maxLag/length);
    return next;
}
inline Rig Orbit(math::Vec hero,float height,float distance,float heading,float down,
                 float units,math::Vec reference,float referenceYaw) {
    const auto orientation=math::Multiply(math::Yaw(heading),Pitch(-down));
    const auto forward=math::Rotate(orientation,{0,0,-1});
    return {hero+math::Vec{0,height,0}-forward*distance, reference,
            math::Multiply(orientation,math::Yaw(-referenceYaw)),units};
}
}
