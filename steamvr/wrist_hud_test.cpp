#include "wrist_hud.h"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace framevr::wrist;
int main() {
    Experience xp{1,99,0,0,500,true};
    assert(xp.Valid()&&xp.Progress()==0);
    xp.current=499;assert(std::abs(xp.Progress()-.998)<1e-12);
    // Mixed level/stat reads during level-up must not show a false full bar.
    xp.current=500;assert(!xp.Valid());
    xp={2,99,500,500,1500,true};assert(xp.Valid()&&xp.Progress()==0);
    xp.current=499;assert(!xp.Valid()); // death penalty/transition stale bounds
    xp={98,99,3400000000u,3229426756u,3520485254u,true};
    assert(xp.Valid()&&xp.Progress()>.58&&xp.Progress()<.59);
    xp={99,99,3520485254u,3520485254u,3837739017u,true};
    assert(xp.Maximum()&&xp.Progress()==1);
    xp.level=100;assert(!xp.Valid());
    xp={};assert(!xp.Valid()&&xp.Progress()==0);
    xp={1,99,10,0,0,true};assert(!xp.Valid());
    xp={1,99,10,0,500,false};assert(!xp.Valid());

    MapLease map;
    assert(map.Step(1,1000,true,false,false)==1); // temporarily open
    assert(map.Active(1000,false));
    assert(map.Step(1,1200,true,false,true)==-1); // no repeat toggles
    assert(map.Step(1,1700,false,false,true)==-1); // brief pose gap
    assert(map.Step(1,1950,false,false,true)==0); // restore closed
    assert(!map.Active(1950,false));
    assert(map.Step(1,2200,true,false,true)==-1); // user's already-open map
    assert(map.Step(1,3000,false,false,true)==-1); // must remain open
    assert(map.Step(1,3200,true,false,false)==1);
    assert(map.Step(1,3300,false,true,true)==0); // menus release ownership
    assert(!map.Active(3300,true));
    assert(map.Step(1,3400,false,false,false)==-1); // no stale menu reopen
    assert(map.Step(1,3500,true,false,false)==1);
    assert(map.Step(2,3600,false,false,true)==0); // new character clears lease
    assert(!map.Active(3600,false));
    assert(map.Step(2,3700,true,false,false)==1);
    assert(map.Step(0,3800,false,false,true)==0); // exit session
    assert(!map.Active(3800,false));
    std::puts("Wrist HUD: unsigned XP, level-up/max/unknown states, map restoration, menus and session changes passed");
}
