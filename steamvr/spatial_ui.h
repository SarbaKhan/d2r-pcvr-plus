#pragma once
#include "perspective_rig.h"
#include <string_view>
namespace framevr::spatialui {
inline bool Dialogue(std::string_view name) {
    return name.find("Npc")!=name.npos||name.find("NPC")!=name.npos||
        name.find("ScrollSpeech")!=name.npos;
}
constexpr bool Spatial(int mode,bool dialogueOnly) {
    return mode==3||mode==4||((mode==1||mode==2)&&dialogueOnly);
}
// Full native dialogue canvas: keep its angular width stable even in F1's
// distant orbit. The previous 5 m cap made native text tiny at that distance.
inline float DialogueWidth(float distance) {return 1.4f*std::max(distance,.5f);}
inline perspective::Pose DialoguePanel(math::Vec anchor,math::Vec viewer) {
    anchor.y+=.35f;
    const auto d=viewer-anchor;
    const float yaw=std::atan2(d.x,d.z);
    const float pitch=-std::atan2(d.y,std::hypot(d.x,d.z));
    return {anchor,math::Multiply(math::Yaw(yaw),perspective::Pitch(pitch))};
}
inline constexpr float HudScale=2.f/3.f;
// +Z is the readable face of an XR quad. Pitch up without following gaze.
inline math::Quat Outward(float angle,float tilt) {
    return math::Multiply(math::Yaw(angle),perspective::Pitch(-tilt));
}
inline float FollowAngle(float previous,math::Vec head,math::Vec center,float dt) {
    const auto d=head-center;
    if(d.x*d.x+d.z*d.z<.01f)return previous;
    const float goal=std::atan2(d.x,d.z);
    const float delta=std::atan2(std::sin(goal-previous),std::cos(goal-previous));
    return previous+delta*(1-std::exp(-std::clamp(dt,0.f,.1f)/.18f));
}
inline perspective::Pose HudSegment(math::Vec center,float radius,float heading,
    float arc,float height,float tilt,int index,int count) {
    const float angle=heading+arc*((index+.5f)/count-.5f);
    auto p=center+math::Vec{std::sin(angle)*radius,.015f+height*.5f*std::cos(tilt),std::cos(angle)*radius};
    return {p,Outward(angle,tilt)};
}
}
