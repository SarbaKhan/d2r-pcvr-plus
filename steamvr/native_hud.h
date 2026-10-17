#pragma once
#include <cstdint>
namespace framevr::nativehud {
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
