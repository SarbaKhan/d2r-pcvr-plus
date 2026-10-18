#include "native_hud.h"
#include "spatial_ui.h"
#include "native_panels.h"
#include "frontend_panel.h"
#include "callback_ticket.h"
#include <cassert>
#include <vector>
#include <limits>
#include <cstdio>
#include <initializer_list>
using namespace framevr::nativehud;
int main(){
    assert(framevr::frontend::Scene("act2_frontend_biome"));
    assert(!framevr::frontend::Scene("act1_wilderness"));
    assert(!framevr::frontend::Scene("ui_characterselectscreen"));
    for(int preset=0;preset<3;++preset) {
        const float d=framevr::panels::MenuDistance(preset,true);
        assert(std::fabs(framevr::frontend::BackgroundScale(d)/(d+.12f)-1/d)<.0001f);
    }
    assert(framevr::frontend::Eligible(true,false,true,true,true,false));
    assert(!framevr::frontend::Eligible(true,true,true,true,true,false));
    assert(!framevr::frontend::Eligible(true,false,true,false,true,false));
    assert(!framevr::frontend::Eligible(true,false,true,true,true,true));

    const auto defaults=Layout(VitalsY,XpY,VitalsY);
    assert(defaults.vitalsY==-.50f&&defaults.xpY==.55f&&defaults.beltY==-.50f);
    assert(defaults.vitalsX==VitalsX&&defaults.vitalsScale==1&&defaults.xpScale==1&&defaults.beltScale==1);
    assert(defaults.vitalsX==-.40f&&defaults.skillsScale==1.2f&&defaults.skillsY==-.50f);
    const auto invalid=Layout(NAN,NAN,NAN,NAN,1,1,1,NAN,NAN,NAN);
    assert(invalid.vitalsY==-.50f&&invalid.xpY==.55f&&invalid.beltY==-.50f);
    assert(invalid.vitalsX==-.40f&&invalid.skillsY==-.50f&&invalid.skillsScale==1.2f);
    // Upstream separates UI capture from background policy. Dialogue stays
    // stereoscopic; management panels share an opaque canvas in every mode.
    for(auto name:{"NpcDialogPanel","ScrollSpeechPanel"}) {
        assert(framevr::panels::Modal(name));
        assert(!framevr::panels::OpaqueBackground(name));
        assert(!framevr::panels::BlockWorldEffects(true,true,false));
    }
    for(auto name:{"UISwitcher","PlayerInventoryPanel","VendorPanelLayout","WaypointPanel"}) {
        assert(framevr::panels::OpaqueBackground(name));
        assert(framevr::panels::BlockWorldEffects(true,true,true));
    }
    assert(framevr::panels::MenuDistance(0)<framevr::panels::MenuDistance(1));
    assert(framevr::panels::MenuDistance(1)<framevr::panels::MenuDistance(2));
    // Shared NPC dialogue placement faces the viewer, retaining angular size
    // at tabletop as well as distant orbit-camera distances.
    for(float distance:{.6f,1.2f,5.f,20.f}) {
        const auto pose=framevr::spatialui::DialoguePanel({0,0,0},{distance,1,distance});
        const auto toward=framevr::math::Normalized(framevr::math::Vec{distance,1,distance}-pose.p);
        const auto normal=framevr::math::Rotate(pose.q,{0,0,1});
        assert(framevr::math::Dot(normal,toward)>.999f);
        assert(std::fabs(framevr::spatialui::DialogueWidth(distance)/distance-1.4f)<.001f);
    }
    const auto custom=Layout(-.5f,.4f,-.3f,-.4f,1.5f,.5f,2.f);
    assert(custom.vitalsX==-.4f&&custom.vitalsY==-.5f&&custom.xpY==.4f&&custom.beltY==-.3f);
    assert(custom.vitalsScale==1.5f&&custom.xpScale==.5f&&custom.beltScale==2.f);
    const auto bounded=Layout(0,0,0,9,0,9,std::numeric_limits<float>::infinity());
    assert(bounded.vitalsX==.8f&&bounded.vitalsScale==.25f&&bounded.xpScale==3&&bounded.beltScale==1);
    assert(Layout(0,0,0,std::numeric_limits<float>::quiet_NaN()).vitalsX==VitalsX);
    const auto limits=Layout(-9,9,std::numeric_limits<float>::quiet_NaN());
    assert(limits.vitalsY==-.8f&&limits.xpY==.8f&&limits.beltY==VitalsY);
    assert(Layout(std::numeric_limits<float>::infinity(),.1f,-.2f).vitalsY==VitalsY);
    assert(defaults.beltX-BeltWidth/2>0); // belt stays on the right
    const auto newLayout=Layout(0,0,0,0,1,1,1,9,-9,99);
    assert(newLayout.beltX==.8f&&newLayout.skillsY==-.8f&&newLayout.skillsScale==3);
    assert(XpWidth>0&&XpHeight>0&&BeltHeight>0);
    struct Upload {uint64_t fence;};
    std::vector<Upload> uploads{{10},{20},{30}};
    RetireUploads(uploads,9);assert(uploads.size()==3);
    RetireUploads(uploads,20);assert(uploads.size()==1&&uploads[0].fence==30);
    // External views must sustain repeated refreshes, not stop after three.
    for(uint64_t completed=30;completed<300;++completed){
        RetireUploads(uploads,completed);
        assert(uploads.size()<3);
        uploads.push_back({completed+2});
    }
    RetireUploads(uploads,1000);assert(uploads.empty());

    for(int mode:{1,2,3,4}) {
        assert(HeadVitalsVisible(mode,true,true,false)==(mode==1||mode==2||mode==3));
        assert(!HeadVitalsVisible(mode,true,true,true));
        assert(!HeadVitalsVisible(mode,false,true,false));
        assert(!HeadVitalsVisible(mode,true,false,false));
        // All VR views use the same existing hide policy, even classic views.
        const auto policy=Resolve(true,false,mode!=4,true,true,0);
        assert(policy.hide==1&&!policy.copyToolbar&&!policy.redrawToolbar);
    }
    assert(VitalsCurrent(7,7,100,750));
    assert(!VitalsCurrent(7,7,100,751));
    assert(!VitalsCurrent(7,8,100,110));
    assert(!VitalsCurrent(0,0,100,110));
    assert(!VitalsCurrent(7,7,100,99));
    // Entire panel stays left/below centre and within the central +/- 30 degrees.
    assert(VitalsX+VitalsWidth/2<0&&VitalsY+VitalsHeight/2<0);
    assert((-VitalsX+VitalsWidth/2)/VitalsDistance<.5774f);
    assert((-VitalsY+VitalsHeight/2)/VitalsDistance<.5774f);

    // Crash regression: the hidden HUD update dereferenced a null first
    // notification widget at +0x145. Cover either missing child and a layout
    // rebuild; normal desktop updates and fully bound layouts pass through.
    for(bool hidden:{false,true}) {
        assert(DeferUpdate(hidden,true,0,0x10000)==hidden);
        assert(DeferUpdate(hidden,true,0x10000,0)==hidden);
        assert(DeferUpdate(hidden,false,0x10000,0x20000)==hidden);
        assert(!DeferUpdate(hidden,true,0x10000,0x20000));
    }
    VisibilityOverride visibility;bool shown=true;int calls=0;
    auto set=[&](bool visible){shown=visible;++calls;return true;};
    visibility.Update(false,true,set);assert(shown&&calls==0); // desktop untouched
    visibility.Update(true,true,set);assert(!shown&&visibility.hidden);
    shown=true;visibility.Update(true,true,set);assert(!shown); // game rebuilt controller HUD
    visibility.Update(false,true,[](bool){return false;});assert(visibility.hidden); // retry restoration
    visibility.Update(false,true,set);assert(shown&&!visibility.hidden); // VR stop only
    visibility.Update(true,false,set);assert(!visibility.hidden); // unloaded panel
    visibility.Update(true,true,set);assert(!shown&&visibility.hidden); // new game
    visibility.Update(false,false,set);assert(!visibility.hidden); // no stale handle on exit
    // A native panel handler can re-show the HUD after the initial hide. A
    // queued UI refresh must reapply the session override, including when
    // opening/closing menus repeatedly and after recreating the HUD panel.
    framevr::CallbackTicket refresh;
    for(int transition=0;transition<8;++transition) {
        visibility.Update(true,true,set);assert(!shown);
        const auto token=refresh.Start(100+transition);
        assert(token&&!refresh.Start(100+transition)); // coalesce message burst
        shown=true; // native handler follows our message listener
        assert(refresh.Claim(token));visibility.Update(true,true,set);
        assert(!shown&&visibility.hidden);
    }
    visibility.Update(true,false,set); // old HUD unloaded
    shown=true;visibility.Update(true,true,set);assert(!shown); // replacement hidden
    const auto pending=refresh.Start(200);
    visibility.Update(false,true,set);assert(shown); // session ends before queued refresh
    assert(refresh.Claim(pending));visibility.Update(false,true,set);assert(shown); // cannot re-hide desktop
    // Reproduce both reported routes: old hide=0 INIs and a classic camera
    // with its toolbar zoom/position effect enabled. Neither can leak a bar.
    for(int configured:{0,1,2})for(bool classic:{false,true})for(bool changed:{false,true})for(bool staleMenu:{false,true}){
        const auto p=Resolve(true,staleMenu,classic,true,changed,configured);
        assert(p.hide==1&&!p.copyToolbar&&!p.redrawToolbar&&p.ClearToolbar(false));
    }
    assert(Gameplay(true,true,false,false));
    assert(!Gameplay(true,true,true,false)); // inventory/waypoint fallback
    assert(!Gameplay(true,true,true,true));  // spatial menu
    assert(!Gameplay(true,false,false,false)); // title/character selection
    assert(!Gameplay(false,true,false,false)); // stopped/lost XR or desktop
    // Releasing the per-eye override restores the exact legacy behavior.
    auto p=Resolve(false,false,false,false,false,0);
    assert(p.hide==0&&!p.copyToolbar&&!p.ClearToolbar(true));
    p=Resolve(false,false,false,false,false,1);
    assert(p.hide==1&&p.copyToolbar&&!p.redrawToolbar&&p.ClearToolbar(false));
    p=Resolve(false,false,false,false,false,2);assert(p.hide==2&&p.copyToolbar);
    p=Resolve(false,true,false,true,true,2);assert(p.hide==0&&!p.copyToolbar);
    p=Resolve(false,false,true,false,true,1);assert(p.hide==0);
    p=Resolve(false,false,true,true,false,1);assert(p.hide==0);
    p=Resolve(false,false,true,true,true,0);
    assert(p.hide==1&&p.copyToolbar&&p.redrawToolbar&&!p.ClearToolbar(false)&&p.ClearToolbar(true));
    std::puts("Native VR toolbar suppression, menu refresh recovery, desktop restoration and legacy redraw policy passed");
}
