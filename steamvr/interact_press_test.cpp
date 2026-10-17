#include "interact_press.h"
#include <cassert>
#include <cstdio>
using framevr::interaction::InteractPress;
int main(){
    InteractPress press;
    auto poll=[&](bool held,uintptr_t target,bool active=true,uint32_t epoch=1,uint64_t session=10){
        return press.Poll(held,target,true,active,session,epoch);
    };
    assert(poll(true,100).down); // chest
    assert(poll(true,100).down); // unchanged door/NPC keeps native hold
    assert(poll(true,0).lost); // opening chest drops its target
    assert(!poll(true,101).down); // dropped item must not become another press
    assert(!poll(true,100).down); // returning to old target cannot rearm
    assert(!poll(false,101).down);
    assert(poll(true,101).down); // fresh press takes loot
    assert(poll(true,102).lost&&!poll(true,102).down); // no sweep of a pile
    poll(false,0);assert(poll(true,0).miss);
    assert(!poll(true,103).down); // empty press does not wait for range
    poll(false,103);assert(poll(true,103).down);
    // Regression: gameplay translation is bypassed during a menu. A release
    // there must clear the old press, so the first new press works on exit.
    assert(!poll(true,103,false,2).down);
    assert(!poll(false,0,false,2).down);
    // No neutral gameplay poll between closing the menu and this fresh press.
    assert(poll(true,104,true,3).down);
    // If A stayed down through the entire menu, it cannot interact on exit.
    assert(!poll(true,104,false,4).down);
    assert(!poll(true,104,true,5).down);
    assert(!poll(true,105,true,5).down);
    poll(false,105,true,5);assert(poll(true,105,true,5).down);
    // Even if no poll occurred inside a brief menu, its epoch cancels the hold.
    assert(!poll(true,105,true,7).down);
    poll(false,105,true,7);assert(poll(true,105,true,7).down);
    // A different local game can reuse an address; the old press stays spent.
    assert(!poll(true,105,true,7,11).down);
    poll(false,105,true,7,11);assert(poll(true,105,true,7,11).down);
    // Focus/tracking loss with a released semantic action rearms a new press.
    poll(false,0,false,7,11);assert(poll(true,106,true,7,11).down);
    // Unsupported interact hooks retain the PR's legacy plain-A fallback.
    InteractPress fallback;
    assert(fallback.Poll(true,0,false,true,1,1).down);
    assert(fallback.Poll(true,0,false,true,1,1).down);
    assert(!fallback.Poll(false,0,false,true,1,1).down);
    assert(fallback.Poll(true,0,false,true,1,1).down);
    assert(!fallback.Poll(true,0,false,true,2,2).down);
    assert(!fallback.Poll(true,0,false,true,2,2).down);
    fallback.Poll(false,0,false,true,2,2);
    assert(fallback.Poll(true,0,false,true,2,2).down);
    std::puts("One-target interaction, menu release, context cancellation and fallback tests passed");
}
