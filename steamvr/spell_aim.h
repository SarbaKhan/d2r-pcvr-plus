#pragma once
#include "frame_math.h"
#include <cstdint>
#include <cstddef>

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
// Native skill-command validation rejects abs(target-player)>50 on EITHER
// grid axis (3.3.93787: 0x4fe076 -> 0x4fe1f0). A 60-cell ray only passes near
// diagonals, leaving stale client aim between accepted casts. Stay inside that
// square with margin for movement between client resolution and server use.
// This point encodes heading; it does not change native missile lifetime/range.
inline constexpr float AimDistance=40.f;
inline bool Point(float x,float y,math::Vec direction,float& targetX,float& targetY) {
    float dx=0,dy=0;
    if(!std::isfinite(x+y)||!Horizontal(direction,dx,dy))return false;
    targetX=x+dx*AimDistance;targetY=y+dy*AimDistance;return true;
}
// The controller quick-cast path takes integer grid cells. Refuse malformed
// positions before conversion, rather than returning an unrelated old target.
inline bool ControllerPoint(float x,float y,math::Vec direction,int& targetX,int& targetY) {
    float tx=0,ty=0;
    if(!Point(x,y,direction,tx,ty)||tx<1||ty<1||tx>65534||ty>65534)return false;
    targetX=int(tx);targetY=int(ty);return true;
}
// Prefix of the private stack command assembled by native MapClick. The
// resolver's point is not authoritative: MapClick selects a unit again and
// may replace that point before checking/dispatching the skill command.
struct NativeCommand {
    uint32_t flags,padding;
    uintptr_t actor,target;
    int x,y;
};
static_assert(offsetof(NativeCommand,actor)==0x08&&offsetof(NativeCommand,target)==0x10&&
              offsetof(NativeCommand,x)==0x18&&sizeof(NativeCommand)==0x20);
struct CommandAim {uintptr_t actor=0;int x=0,y=0;};
inline bool ControllerCast(int type) {return type>=7&&type<=10;}
inline bool ApplyCommand(const CommandAim& aim,NativeCommand& command) {
    if(!aim.actor||command.actor!=aim.actor||aim.x<1||aim.y<1||aim.x>65534||aim.y>65534)return false;
    command.target=0;command.x=aim.x;command.y=aim.y;
    return true;
}
// Apply after native resolution, including paths that use movement/facing
// without calling AttackPoint. Non-directional skills retain native targets.
inline bool Resolve(const Cast& cast,float playerX,float playerY,float& targetX,float& targetY) {
    if(!cast.directed)return false;
    if(cast.contactPoint) {
        if(!std::isfinite(cast.x)||!std::isfinite(cast.y))return false;
        targetX=cast.x;targetY=cast.y;return true;
    }
    return Point(playerX,playerY,{cast.x,0,cast.y},targetX,targetY);
}
}
