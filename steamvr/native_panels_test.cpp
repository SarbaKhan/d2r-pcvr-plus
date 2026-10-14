#include "native_panels.h"
#include "panel_input_mode.h"
#include "menu_controls.h"
#include "quick_cast.h"
#include "journal.h"
#include <cstdio>
#include <cstdlib>
using namespace framevr;
static void Check(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
int main(){
    for(auto name:{"UISwitcher","VendorPanelLayout","VendorPanelOriginalLayout","TradeLayout","GamblePanel",
        "BankExpansionLayout","BankOriginalLayout","StashPanel","PlayerInventoryExpansionLayout",
        "PlayerInventoryOriginalLayout","HoradricCubeLayout","HirelingInventoryPanel","MercenaryInventoryLayout"})
        Check(panels::Modal(name)&&panels::OpaqueBackground(name),"management surfaces use the same opaque background in both eyes");
    for(auto name:{"NpcDialogPanel","ScrollSpeechPanel","QuestLogPanelExpansion","GoldTransferPanel","QuantityPanel","HUDPanel","HUDTradeHint","MiniMenuPanel","TooltipsPanel"})
        Check(!panels::OpaqueBackground(name),"dialogue, journal and auxiliary panels do not independently black out the world");
    panels::InputModeLease mode;
    Check(!mode.PreserveController(100,true),"desktop input untouched without VR ownership");
    mode.Refresh(100);
    Check(mode.PreserveController(100,true),"guard armed before queued cursor movement");
    Check(!mode.PreserveController(101,false),"do not silently force a mouse layout into controller mode");
    Check(mode.PreserveController(399,true),"queued click and release retain controller mode");
    Check(!mode.PreserveController(400,true),"tracking/focus loss expires guard without another frame");
    mode.Refresh(500);mode.Refresh(750);
    Check(mode.PreserveController(1049,true)&&!mode.PreserveController(1050,true),"active pointer renews bounded ownership");
    Check(panels::FocusUsesPointer(mode.Active(800),true),"laser hover keeps exact widget focus");
    Check(!panels::FocusUsesPointer(mode.Active(800),false),"native navigation restores cursor snapping despite queued mouse-release lease");
    Check(mode.Active(800),"switching focus does not cancel protection for queued pointer events");
    // Reproduce a laser-started Amazon session: physical bow/javelin releases
    // and walking send no native A, so KB/M remains active and target lookup
    // never gives the semantic interact guard a usable item/entrance target.
    panels::WorldInputMode world;
    panels::InputModeLease pointerLease;
    Check(!world.NeedsController(100,true,false,false),"desktop session never requests a world layout change");
    for(uint64_t now=100;now<1000;now+=20) {
        world.Update(now,true);
        Check(world.NeedsController(now,true,false,false),"tracked physical combat can recover controller targeting without pressing attack");
        Check(!world.NeedsController(now,true,false,true),"committed controller layout needs no further transition");
        Check(!pointerLease.Active(now),"world ownership never blocks walking or claims UI pointer dispatch");
    }
    Check(!world.NeedsController(1000,false,false,false),"character loading and teardown cannot request a native world transition");
    Check(!world.NeedsController(1000,true,true,false),"open native menus retain their input ownership");
    Check(world.NeedsController(1279,true,false,false)&&!world.NeedsController(1280,true,false,false),"missing XR samples expire world ownership without another input frame");
    world.Update(1300,true);world.Update(1301,false);
    Check(!world.NeedsController(1301,true,false,false),"tracking loss, settings and inactive gameplay release ownership immediately");
    world.Update(1400,true);
    Check(world.NeedsController(1400,true,false,false),"returning to tracked world gameplay recovers targeting after menus or transitions");
    Check(panels::MessageHash("InputMessage")==0x4ae067c6248b6042ull&&
          panels::MessageHash("LeftMouseButtonDown")==0xbdf8daff2e44666full,"native pointer message hashes");
    for(auto command:{"MouseMove","MouseScroll","LeftMouseButtonDown","LeftMouseButtonUp","RightMouseButtonDown","RightMouseButtonUp"})
        Check(panels::PointerMessage(panels::MessageHash("InputMessage"),panels::MessageHash(command)),"all laser event kinds use pointer widget behavior");
    Check(!panels::PointerMessage(panels::MessageHash("PanelManager"),panels::MessageHash("OpenPanel")),"panel changes use actual controller layout");
    bool pointer=false;int calls=0;
    Check(panels::RoutePointer(true,pointer,[&]{
        ++calls;Check(pointer,"native UI receives pointer behavior");
        {panels::PointerScope nested(pointer,false);Check(!pointer,"nested panel open retains controller mode");}
        Check(pointer,"nested dispatch restores outer pointer behavior");
        return false; // A blank panel pixel: no widget handled this click.
    }),"UI miss is consumed before world input");
    Check(calls==1&&!pointer,"native pointer callback runs once and restores mode");
    Check(panels::RoutePointer(true,pointer,[&]{return true;}),"handled UI event remains consumed");
    Check(!panels::RoutePointer(false,pointer,[&]{Check(!pointer,"desktop event retains native mode");return false;}),"desktop miss is not swallowed");
    Check(panels::RoutePointer(false,pointer,[&]{return true;}),"desktop handled result preserved");
    Check(!panels::RoutePointer(mode.Active(1050),pointer,[&]{Check(!pointer,"expired ownership restores normal dispatch");return false;}),"ownership expiry stops consuming events");
    // Recorded vendor failure: MouseMove finds a Yes/No row, then the separate
    // controller-cursor pass replaces it with the grid beneath the popup.
    enum Hover {None,Yes,No,VendorGrid};
    auto nativeFocus=[&](Hover hit,bool nativeSnap,bool binding=false){
        Hover hover=hit;
        if(nativeSnap&&panels::CursorSnapAllowed(pointer,binding))hover=VendorGrid;
        return hover;
    };
    Check(nativeFocus(Yes,true)==VendorGrid,"fixture reproduces native cursor snap stealing popup focus");
    for(Hover row:{Yes,No}) {
        panels::RoutePointer(true,pointer,[&]{
            Check(nativeFocus(row,true)==row,"laser keeps exact Yes/No row through native focus update");
            Check(nativeFocus(None,true)==None,"laser miss cannot snap to a nearby purchase or grid");
            Check(nativeFocus(row,true,true)==VendorGrid,"explicit controller assignment keeps native snapping");
            {panels::PointerScope nested(pointer,false);
                Check(nativeFocus(row,true)==VendorGrid,"panel layout message keeps native cursor behavior");}
            Check(nativeFocus(row,true)==row,"nested message restores laser focus policy");
            return true;
        });
        Check(nativeFocus(row,true)==VendorGrid,"controller snapping restored after pointer callback");
        Check(nativeFocus(row,false)==row,"native disabled snapping is never enabled");
    }
    for(auto name:{"UISwitcher","ControllerOverlay","PlayerInventoryExpansionLayout","VendorPanelLayout","BankExpansionLayout","NpcDialogPanel","ScrollSpeechPanel","QuestLogSpeechPanel","HoradricCubeLayout","GoldTransferPanel"})
        Check(panels::Modal(name),"native management/dialog root routed as modal");
    for(auto name:{"HUDPanel","HUDMonsterHealth","MiniMenuPanel","TooltipsPanel","ShowItemsPanel","PanelBordersPanel"})
        Check(!panels::Modal(name),"persistent HUD must not open VR workspace");
    panels::MouseGate mouse;
    mouse.Step(false,false,true,false);
    Check(!mouse.Step(true,true,true,false).press[0],"held attack cannot pick a dialog option");
    mouse.Step(true,true,false,false);
    Check(mouse.Step(true,true,true,false).press[0],"fresh trigger begins item drag");
    Check(!mouse.Step(true,false,true,false).release[0]&&mouse.down[0],"ray miss keeps item held");
    Check(mouse.Step(true,false,false,false).release[0],"physical release ends drag at last valid pixel");
    Check(mouse.Step(true,true,false,true).press[1],"fresh secondary pointer press");
    Check(mouse.Step(false,true,false,true).release[1],"lost focus releases context click");
    Check(!mouse.Step(true,true,false,true).press[1],"focus return requires new context press");
    mouse.Suspend();Check(!mouse.Step(true,true,true,false).press[0],"mode transition consumes held trigger");
    menus::LaserClick laser;
    laser.Step(true,false);Check(laser.Left()&&!laser.Right(),"laser trigger selects and drags");
    laser.Step(true,true);Check(laser.Left()&&!laser.Right(),"grip pressed during drag cannot turn it into a context click");
    laser.Step(false,true);Check(!laser.Left()&&!laser.Right(),"physical release ends either laser button");
    laser.Step(true,true);Check(laser.Right()&&!laser.Left(),"left grip plus fresh trigger supplies pointer context action without native A");
    laser.Step(true,false);Check(laser.Right()&&!laser.Left(),"releasing modifier first cannot start an unintended second click");
    panels::ScrollGate scroll;
    Check(scroll.Step(true,1,0)==1&&scroll.Step(true,1,.2)==0&&scroll.Step(true,1,.4)==1,"scroll delay and repeat");
    Check(scroll.Step(true,-1,.41)==-1&&scroll.Step(false,-1,.6)==0,"scroll direction and focus guard");
    // World -> stash -> world, with the same A/trigger/stick held across both
    // boundaries. New controls remain usable without releasing every grip.
    menus::ReleaseGate input;
    menus::Pad held;held.buttons=menus::A|menus::RightBumper;held.rt=255;held.lx=20000;
    input.Step(menus::Mode::World,0,{});
    Check(input.Step(menus::Mode::World,0,held).rt==255,"fresh world attack available");
    auto guarded=input.Step(menus::Mode::Native,1,held);
    Check(!menus::Busy(guarded),"held attack and movement cannot activate newly opened stash");
    held.buttons|=menus::B;
    guarded=input.Step(menus::Mode::Native,1,held);
    Check(guarded.buttons==menus::B,"fresh menu B works while old A/grip remain held");
    Check(!menus::Busy(input.Step(menus::Mode::World,2,held)),"menu close cannot attack, swap, or walk in world");
    input.Step(menus::Mode::World,2,{});
    Check(input.Step(menus::Mode::World,2,held).buttons==held.buttons,"release rearms world controls");
    Check(!menus::Busy(input.Step(menus::Mode::Blocked,2,held)),"opening VR settings consumes held native controls");
    input.Suspend();Check(!menus::Busy(input.Step(menus::Mode::Native,2,held)),"focus/tracking recovery requires release");
    skills::QuickCast quick;
    menus::Pad shortcut;shortcut.lt=255;shortcut.buttons=menus::Y|menus::LeftBumper;
    quick.Capture(true,shortcut.lt/255.f,8);
    Check(!menus::SettingsChord(shortcut,quick.Suppress(3)),"simultaneous LT + grip + Y remains quick-cast slot 8, not VR settings");
    quick.Capture(false,0,0);quick.Capture(false,0,8);
    Check(menus::SettingsChord(shortcut,quick.Suppress(3)),"grip + Y opens VR settings when quick cast does not own it");
    for(auto mode:{menus::Mode::World,menus::Mode::Native,menus::Mode::Blocked}) {
        const auto tag=uint32_t(mode);
        Check(menus::Tagged(tag),"tagged report opts into context validation");
        Check(!menus::Allowed(tag,7,8,true,false)&&!menus::Allowed(tag,7,8,false,false),"open-close-open rejects an older report even when menu state matches");
        Check(!menus::Allowed(tag,7,7,true,true),"mod-owned settings never receive native menu pad");
    }
    Check(menus::Allowed(uint32_t(menus::Mode::Native),7,7,true,false),"current native report reaches menu");
    Check(!menus::Allowed(uint32_t(menus::Mode::Native),7,7,false,false),"native A cannot leak into world before next XR frame");
    Check(!menus::Allowed(uint32_t(menus::Mode::World),7,7,true,false),"stale world attack cannot leak into menu");
    Check(!menus::Tagged(0),"legacy BodyWalk reports retain their format");

    menus::Navigation navigation;
    menus::Pad ui;
    Check(!menus::Busy(navigation.Step(true,{},true,.5f,.5f,false))&&!navigation.Native(),"laser starts without native pad events");
    ui.rt=255;
    Check(!menus::Busy(navigation.Step(true,ui,true,.5f,.5f,false)),"laser trigger does not also send native RT");
    ui.buttons=menus::A|menus::RightBumper;ui.lt=255;
    Check(!menus::Busy(navigation.Step(true,ui,false,0,0,true))&&!navigation.Native(),"drag retains ownership while ray leaves canvas or a face button is touched");
    auto native=navigation.Step(true,ui,true,.5f,.5f,false);
    Check(navigation.Native()&&native.buttons==menus::A&&!native.lt&&!native.rt,"native A has one action and existing laser modifiers are consumed");
    navigation.Step(true,{},true,.5f,.5f,false);
    Check(navigation.Native(),"idle laser does not steal native focus");
    ui={};ui.buttons=menus::LeftBumper|menus::RightBumper;ui.lt=128;ui.rt=255;ui.ly=18000;ui.rx=-18000;
    native=navigation.Step(true,ui,true,.8f,.5f,false);
    Check(navigation.Native()&&native.buttons==ui.buttons&&native.lt==128&&native.rt==255&&native.ly==18000&&native.rx==-18000,"native bumpers, triggers and both sticks pass while navigating");
    navigation.Step(true,{},true,.81f,.51f,false);
    Check(navigation.Native(),"small tracked aim jitter cannot overwrite native focus");
    Check(!menus::Busy(navigation.Step(true,{},true,.9f,.5f,false))&&!navigation.Native(),"deliberate reaim returns to laser without synthetic native action");
    navigation.Step(true,menus::Pad{menus::X},true,.9f,.5f,false);
    Check(navigation.Native(),"native X is available rather than toggling menu closed");
    navigation.Step(false,{},false,0,0,false);
    Check(!navigation.Native(),"focus loss resets navigation ownership");
    menus::ReleaseGate sortGate;menus::Navigation inventory;
    sortGate.Step(menus::Mode::Native,9,{});
    menus::Pad sort;sort.buttons=menus::RightStick;
    for(int frame=0;frame<180;++frame) {
        const auto report=inventory.Step(true,sortGate.Step(menus::Mode::Native,9,sort),true,.5f,.5f,false);
        Check(report.buttons==menus::RightStick,"right-stick hold reaches native sort on every frame, not just a click edge");
        Check(!menus::WorldButtons(sort),"physical right-stick click cannot request a gameplay weapon swap");
    }
    Check(!inventory.Step(true,{},true,.5f,.5f,false).buttons,"releasing right-stick ends native hold-to-sort");
    menus::Pad drop;drop.buttons=menus::X;
    for(int frame=0;frame<90;++frame)
        Check(inventory.Step(true,drop,true,.5f,.5f,false).buttons==menus::X,"X/drop is forwarded for the entire native hold duration");
    Check(!menus::Busy(sortGate.Step(menus::Mode::World,10,sort)),"closing inventory during a sort hold cannot swap in gameplay");
    // Source pixels, physical panel and mouse mapping agree at multiple aspects.
    for(auto size:{journal::Rect{0,0,3840,2160},journal::Rect{0,0,2560,1440},journal::Rect{0,0,1920,1200}}) {
        for(float u:{.01f,.25f,.5f,.75f,.99f})for(float v:{.01f,.5f,.99f}) {
            const auto rot=math::Yaw(.65f);const math::Vec origin{.3f,1.4f,-1.25f};
            auto point=origin+math::Rotate(rot,{(u-.5f)*panels::Width,(.5f-v)*panels::Height(size.w,size.h),0});
            const auto front=math::Rotate(rot,{0,0,1});math::PanelHit hit;math::Vec pixel{};
            Check(math::IntersectPanel(point+front,front*-1,origin,rot,panels::Width,panels::Height(size.w,size.h),hit),"rotated panel ray");
            Check(journal::Pixel(size,hit.u,hit.v,size.w,size.h,pixel)&&std::fabs(pixel.x-u)<.00001f&&std::fabs(pixel.y-v)<.00001f,"native pixel mapping across both panes and tooltips");
        }
    }
    std::puts("Native panel routing, drag ownership, focus recovery, scrolling and pixel mapping passed");
}
