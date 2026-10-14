#pragma once
#include <string_view>
#include "frame_math.h"

namespace framevr::panels {
inline constexpr char TabbedMenu[]="UISwitcher";
// These full management canvases share one opaque image against black in XR.
// VendorPanelLayout also hosts NPC gambling; keep speech/quest panels spatial.
inline bool OpaqueBackground(std::string_view name) {
    return name==TabbedMenu||name.starts_with("Vendor")||
        name.starts_with("Trade")||name.starts_with("Gamble")||
        name.starts_with("Bank")||name.starts_with("Stash")||
        name.starts_with("PlayerInventory")||name.starts_with("HoradricCube")||
        name.starts_with("HirelingInventory")||name.starts_with("MercenaryInventory");
}
// Persistent HUD roots are not modal. Speech can outlive NpcDialogPanel.
inline bool Modal(std::string_view name) {
    if(name==TabbedMenu||name=="ControllerOverlay")return true;
    if(name.starts_with("HUD")||name.starts_with("MiniMenu"))return false;
    for(auto key:{"Vendor","PlayerInventory","Bank","Stash","HoradricCube","CharacterStats",
        "SkillsTree","SkillSelect","QuestLog","Pause","Waypoint","Npc","NPC","Trade","Gamble",
        "HirelingInventory","MercenaryInventory","Options","ScrollSpeech","ScrollOfInifuss",
        "GoldTransfer","Quantity","Confirmation"})
        if(name.find(key)!=name.npos)return true;
    return false;
}

// One stable canvas preserves native relationships between inventory, shop,
// comparison tooltips, dragged icons and confirmation dialogs. Empty pixels
// use the canvas's background policy. Do not recrop when hovering: that would
// move the target.
constexpr float Width=1.9f,Distance=1.25f;
inline float Height(unsigned width,unsigned height) {
    return width&&height?Width*float(height)/width:1.06875f;
}

// Require a fresh press on the surface; never drop a drag just because a ray
// briefly leaves it. Focus loss releases injected buttons and requires rearm.
struct MouseGate {
    bool held[2]{},down[2]{};
    struct Edges {bool press[2]{},release[2]{};};
    Edges Step(bool active,bool hit,bool left,bool right) {
        const bool pressed[]={left,right};Edges e;
        for(int i=0;i<2;++i) {
            e.release[i]=down[i]&&(!active||!pressed[i]);
            e.press[i]=active&&hit&&pressed[i]&&!held[i];
            if(e.release[i])down[i]=false;
            if(e.press[i])down[i]=true;
            held[i]=pressed[i];
        }
        return e;
    }
    void Suspend(){held[0]=held[1]=true;down[0]=down[1]=false;}
};
struct ScrollGate {
    double next=0;int direction=0;
    int Step(bool active,float y,double now) {
        const int d=active&&std::isfinite(y)?(y>.6f?1:y<-.6f?-1:0):0;
        if(!d){direction=0;next=0;return 0;}
        if(d!=direction){direction=d;next=now+.4;return d;}
        if(now<next)return 0;
        next=now+.12;return d;
    }
};
}
