#include "native_hud.h"
#include "callback_ticket.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
using namespace framevr::nativehud;
int main(){
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
