#include "../vr/native_belt.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <array>

namespace {
bool hidden=true,menu=false,verified=true,local=true,rootReady=true,beltReady=true,bindWorks=true,handled=true;
uint64_t session=10;uint32_t player[4]={0,1,7,1};
std::array<uint8_t,0x5d0> widget{};
int binds=0,commands=0;uint32_t lastCommand=0;
// Mercenary query: native original, gate, hook capture and a non-belt caller.
bool gate=false,queryFromShortcut=false;int nativeQueries=0;
bool (*mercDetour)()=nullptr;
bool NativeMercQuery(){++nativeQueries;return false;} // a player without Shift held
bool Gate(){return gate;}
uintptr_t Root(int panel){assert(panel==0);return rootReady?123:0;}
uintptr_t Find(uintptr_t root,const char* name){assert(root==123&&!std::strcmp(name,"Belt"));return beltReady?uintptr_t(widget.data()):0;}
void Bind(uintptr_t belt,uintptr_t unit){
    assert(belt==uintptr_t(widget.data())&&unit==uintptr_t(player));++binds;
    if(bindWorks){std::memcpy(widget.data()+0x5c4,player+2,4);std::memcpy(widget.data()+0x5c8,player,4);}
}
void Unbound(){uint32_t owner[]={UINT32_MAX,6};std::memcpy(widget.data()+0x5c4,owner,sizeof owner);}
}
namespace hud {bool NativeToolbarHidden(){return hidden;}}
namespace gamestate {uint64_t LocalPlayer(){return session;}bool MenuOpen(){return menu;}}
namespace combat {
bool LocalPlayerUnit(uintptr_t& p){p=uintptr_t(player);return local;}
bool LocalPlayerId(uint32_t& id){id=player[2];return local;}
}
namespace d2rsig {
bool Check(uint64_t,const void*,uint32_t){return verified;}
uintptr_t Addr(uint64_t rva){
    switch(rva){case 0x846190:return uintptr_t(&Root);case 0x855c90:return uintptr_t(&Find);case 0x2aab40:return uintptr_t(&Bind);
        case 0x8d3b0:return uintptr_t(&Gate);case 0x228d7e:return 0x1000;} // belt call site: never this test's caller
    assert(false);return 0;
}
bool Hook(uint64_t rva,const void*,uint32_t,void* detour,void** original){
    assert(rva==0x160ac0);mercDetour=reinterpret_cast<bool(*)()>(detour);*original=reinterpret_cast<void*>(&NativeMercQuery);return true;
}
}
namespace framevr::logging {void Write(const char*){}}
namespace gamecmd {
bool ExecuteUiCommand(uint32_t command){
    if(hidden){uint32_t owner[2];std::memcpy(owner,widget.data()+0x5c4,sizeof owner);assert(owner[0]==player[2]&&owner[1]==0);}
    // The native belt shortcut consults the mercenary query. This caller is
    // not its call site, so the hook must defer to the native answer.
    if(mercDetour&&queryFromShortcut)assert(!mercDetour());
    ++commands;lastCommand=command;return handled;
}
}
int main(){
    // Regression: hidden HUD never updated its Belt owner (-1/type 6).
    // Exercise the actual adapter: it must bind before dispatch, for all slots.
    for(int column=0;column<4;++column){
        Unbound();assert(nativebelt::Drink(column,session));assert(lastCommand==uint32_t(0x17+column));
    }
    assert(commands==4&&binds==4&&hidden);
    // Layout rebuild, zone change and valid native player ID zero.
    ++session;player[2]=0;Unbound();assert(nativebelt::Drink(0,session));
    assert(commands==5&&binds==5);
    assert(!nativebelt::Drink(0,session-1)&&!nativebelt::Drink(0,0));
    assert(!nativebelt::Drink(-1,session)&&!nativebelt::Drink(4,session));
    menu=true;assert(!nativebelt::Drink(0,session));menu=false;
    verified=false;assert(!nativebelt::Drink(0,session));verified=true;
    local=false;assert(!nativebelt::Drink(0,session));local=true;
    rootReady=false;assert(!nativebelt::Drink(0,session));rootReady=true;
    beltReady=false;assert(!nativebelt::Drink(0,session));beltReady=true;
    Unbound();bindWorks=false;assert(!nativebelt::Drink(0,session));bindWorks=true;
    assert(commands==5); // unavailable/invalid ownership never consumes
    handled=false;assert(!nativebelt::Drink(1,session));assert(commands==6); // no retry/fallback
    handled=true;hidden=false;verified=false;local=false;
    const auto before=binds;assert(nativebelt::Drink(2,session));assert(commands==7&&binds==before); // desktop untouched
    // Thrown potions never become the player's drink when the native
    // mercenary query cannot be answered: no hook, or the native gate is set.
    hidden=true;verified=local=true;
    Unbound();assert(!nativebelt::FeedMerc(0,session)&&commands==7); // not installed: nothing pressed
    verified=false;nativebelt::Install();assert(!mercDetour);verified=true; // mismatched code: no hook
    nativebelt::Install();assert(mercDetour);
    gate=true;Unbound();assert(!nativebelt::FeedMerc(0,session)&&commands==7);gate=false;
    menu=true;assert(!nativebelt::FeedMerc(0,session));menu=false;
    assert(!nativebelt::FeedMerc(0,session-1)&&!nativebelt::FeedMerc(4,session));
    Unbound();bindWorks=false;assert(!nativebelt::FeedMerc(0,session));bindWorks=true;
    assert(commands==7); // refused gates never press anything
    // The answer is scoped to the belt shortcut's call site: another caller
    // during our press gets the native answer, and the feed is not confirmed.
    queryFromShortcut=true;Unbound();
    assert(!nativebelt::FeedMerc(2,session)&&commands==8&&lastCommand==0x19&&nativeQueries==1);
    // Ordinary drinks never receive a forced answer, wherever they ask.
    Unbound();assert(nativebelt::Drink(1,session)&&nativeQueries==2);queryFromShortcut=false;
    std::puts("Hidden native belt owner refresh, exact slot dispatch, cancellation and desktop routing passed");
}
