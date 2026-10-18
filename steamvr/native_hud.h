#pragma once
#include <cstdint>
#include <algorithm>
#include <cmath>
namespace framevr::nativehud {
// Retain GPU resources until their fence completes, in every VR perspective.
template<class Uploads>void RetireUploads(Uploads& uploads,uint64_t completed){
    uploads.erase(std::remove_if(uploads.begin(),uploads.end(),
        [completed](const auto& upload){return upload.fence<=completed;}),uploads.end());
}
// Persisted mode IDs: F1=1, F2=2, F4=3, immersive F3=4.
constexpr bool HeadVitalsMode(int mode){return mode==1||mode==2;}
constexpr bool HeadVitalsVisible(int mode,bool focused,bool inWorld,bool menu){
    return HeadVitalsMode(mode)&&focused&&inWorld&&!menu;
}
constexpr bool VitalsCurrent(uint64_t paintedSession,uint64_t session,uint64_t at,uint64_t now){
    return session&&paintedSession==session&&now>=at&&now-at<=650;
}
// Metres in XR VIEW space: left/below the centre, at a comfortable fixed depth.
// Independent of game resolution, aspect ratio and Diorama scale.
constexpr float VitalsX=-.30f,VitalsY=-.42f,VitalsDistance=1.1f;
constexpr float VitalsWidth=.40f,VitalsHeight=VitalsWidth*214.f/512.f;
struct HeadLayout {
    float vitalsY=VitalsY,xpY=.30f,beltY=VitalsY;
    float vitalsX=VitalsX,vitalsScale=1.f,xpScale=1.f,beltScale=1.f;
    float beltX=.40f,skillsY=VitalsY,skillsScale=1.f;
};
inline HeadLayout Layout(float vitals,float xp,float belt,float x=VitalsX,float vs=1.f,float xs=1.f,float bs=1.f,float bx=.40f,float sy=VitalsY,float ss=1.f){
    const auto safe=[](float v,float fallback){return std::isfinite(v)?std::clamp(v,-.8f,.8f):fallback;};
    const auto scale=[](float v){return std::isfinite(v)?std::clamp(v,.25f,3.f):1.f;};
    return {safe(vitals,VitalsY),safe(xp,.30f),safe(belt,VitalsY),safe(x,VitalsX),scale(vs),scale(xs),scale(bs),safe(bx,.40f),safe(sy,VitalsY),scale(ss)};
}
constexpr float XpWidth=.75f,XpHeight=XpWidth*132.f/1024.f;
constexpr float BeltWidth=.32f,BeltHeight=BeltWidth*180.f/815.f;
constexpr float SkillsWidth=.20f,SkillsHeight=SkillsWidth*180.f/814.f;

// The native HUD update assumes ExpFlashLeft/ExpFlashRight exist. A hidden
// controller HUD may lack them (observed null +0x170 during combat).
// Defer that presentation update until the layout is ready.
inline bool DeferUpdate(bool hidden,bool readable,uintptr_t first,uintptr_t second) {
    return hidden&&(!readable||!first||!second);
}
// Hide the native panel itself, so nonstandard UI layouts cannot escape a
// bottom-strip crop. Restore only a panel this override actually hid. Calls
// run on the UI thread; repeated hides cover native mode/layout refreshes.
struct VisibilityOverride {
    bool hidden=false;
    template<class Set>void Update(bool hide,bool found,Set set){
        if(!found){hidden=false;return;}
        if(hide){if(set(false))hidden=true;}
        else if(hidden&&set(true))hidden=false;
    }
};
struct Policy {
    int hide=0;
    bool copyToolbar=false,redrawToolbar=false;
    // Classic views only remove a bar when its replacement copy succeeded.
    bool ClearToolbar(bool copied)const{return hide==1&&(!redrawToolbar||copied);}
};
inline bool Gameplay(bool stereo,bool inWorld,bool nativeMenu,bool spatialMenu) {
    return stereo&&inWorld&&!nativeMenu&&!spatialMenu;
}
inline Policy Resolve(bool vrGameplay,bool menu,bool classic,bool pictureReady,bool barChanged,int configuredHide) {
    // The tracked vitals/belt replace the desktop toolbar in native OpenXR.
    // Ignore legacy INI hide=0 and classic-camera redraw preferences here.
    if(vrGameplay)return {1,false,false};
    if(menu)return {};
    if(classic)return pictureReady&&barChanged?Policy{1,true,true}:Policy{};
    return {configuredHide,configuredHide>=1,false};
}
}
