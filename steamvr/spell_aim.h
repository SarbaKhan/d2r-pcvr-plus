#pragma once
#include "frame_math.h"
#include <cstdint>

namespace framevr::spellaim {
struct Cast {bool directed=false;float x=0,y=0;bool contactPoint=false;};
struct Scope {
    Cast& current;Cast previous;
    Scope(Cast& current,Cast next):current(current),previous(current){current=next;}
    ~Scope(){current=previous;}
    Scope(const Scope&)=delete;
};
// Compiled Skills flags and server behavior, validated against the owner's
// local skills.bin/text tables. Class and equipped weapon do not decide which
// spells are directional. Target-only/corpse skills keep native selection.
inline bool Directed(uint32_t flags,int missile,int serverFunction) {
    if(flags&((1u<<4)|(1u<<20)|(1u<<24)))return false; // passive, targetable-only, corpse
    if(missile>=0)return true;
    switch(serverFunction) {
        case 8:  // Teeth / multi-shot / Shock Wave: preserve native fan
        case 10: // initially directed seeking missile
        case 17: // Charged Bolt
        case 19: // Inferno / Arctic Blast
        case 26: // Chain Lightning
        case 48: // Blade Fury
        case 118: // Twister / Tornado
            return true;
        default:return false; // ground placements, summons, self/area effects
    }
}
inline bool Horizontal(math::Vec direction,float& x,float& y) {
    const float n=std::hypot(direction.x,direction.z);
    if(!std::isfinite(direction.x+direction.y+direction.z)||n<.001f)return false;
    x=direction.x/n;y=direction.z/n;return true;
}
// Use a sufficiently distant native point to avoid coarse integer-cell aim,
// without changing missile count, velocity, lifetime or homing/spread behavior.
inline bool Point(float x,float y,math::Vec direction,float& targetX,float& targetY) {
    float dx=0,dy=0;
    if(!std::isfinite(x+y)||!Horizontal(direction,dx,dy))return false;
    targetX=x+dx*60;targetY=y+dy*60;return true;
}
}
