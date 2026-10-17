#pragma once
#include "bow_flight.h"

namespace framevr::aimassist {
using namespace bowflight;
inline constexpr float Range=12.f,Degrees=.01745329252f;
inline float Limit(int mode){return mode==1?4*Degrees:mode==2?12*Degrees:0;}
struct Candidate {
    Vec direction{};
    float angle=0,distance=0;
    uint32_t id=0;
    bool direct=false,valid=false;
};
// Find the closest section of a vertical body capsule to the hand ray, then
// rotate into its interior. The result holds no native pointers.
// Ground volleys use the footprint; planar balls require a body at
// their actual muzzle height rather than silently introducing pitch.
inline Candidate Evaluate(const Shot& shot,uint32_t id,Vec bottom,Vec top,float radius) {
    Candidate out;out.id=id;
    const float limit=Limit(shot.aimAssist);
    if(!limit||!Finite(bottom)||!Finite(top)||!std::isfinite(radius)||radius<=0||
       !Finite(shot.origin)||!Finite(shot.direction)||std::fabs(Length(shot.direction)-1)>.001f||
       top.y<bottom.y||std::fabs(top.x-bottom.x)>.001f||std::fabs(top.z-bottom.z)>.001f)return out;
    const bool ground=spellcast::NativePath(shot.skill),planar=shot.planar||ground;
    const Vec origin=shot.origin,direction=shot.direction;
    if(planar&&std::fabs(direction.y)>.0001f)return out;
    Vec center{};float section=radius;
    if(planar) {
        const float dy=ground?0:origin.y-std::clamp(origin.y,bottom.y,top.y);
        if(std::fabs(dy)>=radius)return out;
        section=std::sqrt(radius*radius-dy*dy);
        center={bottom.x,origin.y,bottom.z};
        bottom=top=center;
    } else {
        const Vec axis=top-bottom,q=bottom-origin;
        const float h2=math::Dot(axis,axis),ad=math::Dot(axis,direction);
        const float denom=h2-ad*ad;
        const float t=denom>1e-8f?std::clamp((math::Dot(q,direction)*ad-math::Dot(q,axis))/denom,0.f,1.f):.5f;
        center=bottom+axis*t;
    }
    float entry=0;
    if(CapsuleEntry(origin,origin+direction*Range,bottom,top,section,entry)) {
        out.direction=direction;out.distance=entry*Range;out.direct=out.valid=true;return out;
    }
    const Vec to=center-origin;const float distance=Length(to);
    if(distance<=section||distance-section>Range||math::Dot(to,direction)<=0)return out;
    const Vec toward=to*(1/distance);
    const float cosine=std::clamp(math::Dot(direction,toward),-1.f,1.f);
    const float theta=std::acos(cosine);
    // Acquisition needs a real intersection inside the angular budget.
    // Grazing 95% of the radius made tiny target movements enough to miss.
    // Light aims halfway inside; Strong aims at the axis, capped at the same
    // correction limit even for targets on the acquisition boundary.
    const float entryTurn=std::max(0.f,theta-std::asin(std::clamp(section*.95f/distance,0.f,1.f)));
    if(entryTurn>limit)return out;
    const float inset=shot.aimAssist==1?.5f:0.f;
    const float turn=std::min(limit,std::max(0.f,theta-std::asin(std::clamp(section*inset/distance,0.f,1.f))));
    const Vec side=math::Normalized(toward-direction*cosine);
    out.direction=math::Normalized(direction*std::cos(turn)+side*std::sin(turn));
    if(planar)out.direction.y=0;
    // Confirm the corrected ray intersects the actual body within range.
    if(!CapsuleEntry(origin,origin+out.direction*Range,bottom,top,section,entry))return {};
    out.angle=turn;out.distance=entry*Range;out.valid=true;return out;
}
class Selection {
    uint32_t previous=0;
    bool preferred=false;
    Candidate best{};
public:
    Selection(uint32_t id,bool valid):previous(id),preferred(valid){}
    bool Better(const Candidate& c) const {
        if(!c.valid)return false;
        if(!best.valid)return true;
        if(c.direct!=best.direct)return c.direct;
        if(c.direct)return c.distance<best.distance||(c.distance==best.distance&&c.id<best.id);
        const auto score=[&](const Candidate& a){return a.angle-((preferred&&a.id==previous)?.5f*Degrees:0);};
        const float difference=score(c)-score(best);
        if(std::fabs(difference)>1e-6f)return difference<0;
        return c.distance<best.distance||(c.distance==best.distance&&c.id<best.id);
    }
    void Accept(const Candidate& c){if(Better(c))best=c;}
    const Candidate& Best()const{return best;}
};
// One instance per hand, game thread only. A slight preference is allowed
// only for fresh candidates that pass the same angle/range/visibility gates.
class History {
    Shot last{};
    uint32_t target=0;
    bool valid=false;
public:
    Selection Begin(const Shot& shot) {
        const bool same=valid&&shot.session==last.session&&shot.epoch==last.epoch&&shot.hand==last.hand&&
            shot.activation==last.activation&&shot.binding==last.binding&&shot.equipment==last.equipment&&
            shot.skill==last.skill&&shot.owner==last.owner&&shot.aimAssist==last.aimAssist&&shot.planar==last.planar&&
            shot.at>=last.at&&shot.at-last.at<=750;
        last=shot;valid=false;return {target,same};
    }
    void Finish(const Candidate& c){target=c.id;valid=c.valid;}
};
// Per-projectile identity, never a retained native pointer. Strong acquires
// once at launch; losing that target permanently ends steering for this shot.
struct Homing {
    static constexpr float Rate=60*Degrees,Budget=45*Degrees,Lifetime=1.5f;
    uint32_t target=0,classId=0;
    bool active=false;
    float age=0,turned=0;
    Vec previousBody{};
    void Arm(const Shot& shot,const Candidate& candidate,uint32_t type,Vec body) {
        *this={};
        if(shot.kind!=Kind::Spell||shot.aimAssist!=2||spellcast::NativePath(shot.skill)||!candidate.valid||!Finite(body))return;
        target=candidate.id;classId=type;previousBody=body;active=true;
    }
    bool Step(const Flight& flight,Vec bottom,Vec top,float radius,bool visible,float dt,Vec& direction) {
        if(!active)return false;
        const auto stop=[&](){active=false;return false;};
        if(!visible||!Finite(bottom)||!Finite(top)||!std::isfinite(radius)||radius<=0||
           top.y<bottom.y||!std::isfinite(dt)||dt<=0||dt>.1f)return stop();
        age+=dt;
        if(age>Lifetime||turned>=Budget)return stop();
        const Vec body=(bottom+top)*.5f;
        // A teleport/path discontinuity must not drag a projectile elsewhere.
        if(Length(body-previousBody)>std::max(.75f,dt*12.f))return stop();
        previousBody=body;
        const Vec origin=flight.Point(),forward=flight.Direction();
        Vec aim=body;
        if(flight.shot.planar) {
            if(origin.y<=bottom.y-radius||origin.y>=top.y+radius)return stop();
            aim.y=origin.y;
        }
        const Vec delta=aim-origin;
        const float distance=Length(delta);
        if(!Finite(origin)||!Finite(forward)||!std::isfinite(distance)||distance>Range+2)return stop();
        if(distance<.001f)return false;
        const Vec toward=delta*(1/distance);
        const float cosine=std::clamp(math::Dot(forward,toward),-1.f,1.f);
        if(cosine<=0)return stop(); // never turn back after passing the enemy
        const float angle=std::acos(cosine);
        if(angle<.0001f)return false;
        const float turn=std::min({angle,Rate*dt,Budget-turned});
        const Vec side=math::Normalized(toward-forward*cosine);
        direction=math::Normalized(forward*std::cos(turn)+side*std::sin(turn));
        if(flight.shot.planar)direction=math::Normalized({direction.x,0,direction.z});
        turned+=turn;return true;
    }
};
}
