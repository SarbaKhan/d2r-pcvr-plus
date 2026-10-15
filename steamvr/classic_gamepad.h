#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace pcvrplus::gamepad {
struct Delta { float yaw=0, pitch=0; };
constexpr bool CameraView(bool flat, bool external, int view) {
    return (flat && (view==2 || view==3)) || (external && view==2);
}
// Rotate only the native locomotion vector, preserving analog magnitude.
// Positive angle means the viewer turned left in world space relative to the
// original game camera. Buttons and right stick never enter this operation.
inline void RotateWalk(float& x,float& y,float angle) {
    if(!std::isfinite(angle))return;
    const float c=std::cos(angle),s=std::sin(angle),oldX=x;
    x=oldX*c-y*s;y=oldX*s+y*c;
}
inline float Axis(float x,float dead) {
    if(!std::isfinite(x)) return 0;
    dead=std::clamp(dead,0.05f,0.8f);
    const float a=std::clamp((std::fabs(x)-dead)/(1-dead),0.f,1.f);
    return std::copysign(a*a,x);
}
// One real XInput slot owns camera input. Idle/disconnected secondary slots
// cannot erase the active slot, and stale input never resumes across UI epochs.
struct Camera {
    int owner=-1, context=-1;
    uint32_t epoch=0;
    uint64_t at=0;
    float x=0,y=0;
    void Reset(){ *this=Camera{}; }
    void Poll(int slot,bool connected,float rx,float ry,bool active,int mode,uint32_t ui,uint64_t now) {
        if(!connected) { if(owner==slot)Reset(); return; }
        if(owner!=slot && owner!=-1 && !active) return;
        if(owner!=slot && !active) return;
        owner=slot;context=mode;epoch=ui;at=now;x=rx;y=ry;
    }
    Delta Step(bool allowed,int mode,uint32_t ui,uint64_t now,float dt,float dead,float speedX,float speedY,bool invert) {
        if(!allowed || context!=mode || epoch!=ui || owner<0 || now<at || now-at>150) {Reset();return {};}
        dt=std::clamp(dt,0.f,.05f);
        return {Axis(x,dead)*speedX*dt, Axis(y,dead)*speedY*dt*(invert?1.f:-1.f)};
    }
};
}
