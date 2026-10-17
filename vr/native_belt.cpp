#include "native_belt.h"
#include <intrin.h>
#include "combat.h"
#include "sigscan.h"
#include "memory.h"
#include "logging.h"
#include <cstdio>

namespace hud {bool NativeToolbarHidden();}
namespace gamestate {uint64_t LocalPlayer();bool MenuOpen();}
namespace gamecmd {bool ExecuteUiCommand(uint32_t);}
namespace nativebelt {
namespace {
// 3.3.93787: the belt shortcut (0x228d10) finds HUDPanel's Belt child.
// Its use handler resolves the owner cached at +0x5c4/+0x5c8. Hiding HUDPanel
// prevents Belt::Update (0x228db0) from binding that owner; a handled shortcut
// then does nothing. Perform only that native owner-binding step, not a full
// widget update, visibility toggle, item edit or replacement potion effect.
constexpr uint64_t HudRootRva=0x846190,FindChildRva=0x855c90,SetOwnerRva=0x2aab40;
constexpr uint8_t HudRootSig[]={0x48,0x83,0xec,0x28,0x48,0x83,0x3d,0xd4,0x9f,0xbf,0x02,0x00,
    0x74,0x18,0xe8,0xfd,0x18,0x88,0xff};
constexpr uint8_t FindChildSig[]={0x40,0x53,0x55,0x56,0x57,0x41,0x54,0x48,0x83,0xec,0x20,
    0x4c,0x89,0x7c,0x24,0x68,0x48,0x8b,0xf2,0x48,0x8b,0xf9};
constexpr uint8_t SetOwnerSig[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,
    0x48,0x8b,0xfa,0x48,0x8b,0xd9,0x48,0x85,0xd2,0x74,0x2e};
// Validate the native cache layout too, not merely the setter's entry.
constexpr uint8_t OwnerFieldsSig[]={0x89,0x83,0xc4,0x05,0x00,0x00,0x8b,0x07,
    0x89,0x83,0xc8,0x05,0x00,0x00};
bool Prepare(uint64_t session) {
    if(!d2rsig::Check(HudRootRva,HudRootSig,sizeof HudRootSig)||
       !d2rsig::Check(FindChildRva,FindChildSig,sizeof FindChildSig)||
       !d2rsig::Check(SetOwnerRva,SetOwnerSig,sizeof SetOwnerSig)||
       !d2rsig::Check(0x2aab6a,OwnerFieldsSig,sizeof OwnerFieldsSig))return false;
    uintptr_t player=0;uint32_t id=0;
    if(!combat::LocalPlayerUnit(player)||!combat::LocalPlayerId(id)||session!=gamestate::LocalPlayer())return false;
    const auto root=reinterpret_cast<uintptr_t(*)(int)>(d2rsig::Addr(HudRootRva))(0);
    if(!root)return false;
    const auto belt=reinterpret_cast<uintptr_t(*)(uintptr_t,const char*)>(d2rsig::Addr(FindChildRva))(root,"Belt");
    if(!belt)return false;
    uint32_t before[2]{},after[2]{};
    if(!framevr::ReadMemory(before,reinterpret_cast<void*>(belt+0x5c4),sizeof before))return false;
    reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(d2rsig::Addr(SetOwnerRva))(belt,player);
    if(!framevr::ReadMemory(after,reinterpret_cast<void*>(belt+0x5c4),sizeof after)||after[0]!=id||after[1]!=0)return false;
    char line[176];std::snprintf(line,sizeof line,
        "Frame potion sip: hidden native belt owner refreshed (%u/%u -> %u/%u)",before[0],before[1],after[0],after[1]);
    framevr::logging::Write(line);
    return true;
}
// 3.3.93787: the belt shortcut asks 0x160ac0 "give this potion to the
// mercenary?" and passes the answer to Belt::Use. The query reads the physical
// Shift key (KB/M) or controller button 0x400 (pad), never a game command, and
// returns false first while the UI gate 0x8d3b0 is set. Answer true only for
// our own thrown-potion press, only at the belt shortcut's call site.
constexpr uint64_t MercQueryRva=0x160ac0,MercGateRva=0x8d3b0,BeltCallRva=0x228d7e;
constexpr uint8_t MercQuerySig[]={0x48,0x83,0xec,0x28,0xe8,0xe7,0xc8,0xf2,0xff,0x84,0xc0,0x75,0x26,0xe8,0x3e,0x73,0xf1,0xff,
    0x84,0xc0,0x74,0x09,0x48,0x83,0xc4,0x28,0xe9,0x61,0xc4,0xfe,0xff,0xb9,0x10,0x00,0x00,0x00,0xe8,0x17,0x96,0x0a,0x01,
    0x85,0xc0,0x0f,0x95,0xc0,0x48,0x83,0xc4,0x28,0xc3};
constexpr uint8_t MercGateSig[]={0x48,0x8b,0x05,0x29,0x63,0x99,0x02,0x48,0x85,0xc0,0x74,0x08,0x80,0x78,0x5c,0x01,0x0f,0x94,0xc0,0xc3};
constexpr uint8_t BeltCallSig[]={0xe8,0x3d,0x7d,0xf3,0xff,0x44,0x0f,0xb6,0xc0,0x8b,0xd6,0x48,0x8b,0xcf};
using MercQuery=bool(*)();
MercQuery originalMercQuery=nullptr;
bool mercHookIn=false;
uintptr_t beltCallReturn=0;
thread_local bool feeding=false;
thread_local int answered=0;
bool OnMercQuery() {
    if(feeding&&reinterpret_cast<uintptr_t>(_ReturnAddress())==beltCallReturn){++answered;return true;}
    return originalMercQuery();
}
bool MercGate() {return reinterpret_cast<bool(*)()>(d2rsig::Addr(MercGateRva))();}
}
void Install() {
    if(mercHookIn)return;
    if(!d2rsig::Check(MercQueryRva,MercQuerySig,sizeof MercQuerySig)||!d2rsig::Check(MercGateRva,MercGateSig,sizeof MercGateSig)||
       !d2rsig::Check(BeltCallRva,BeltCallSig,sizeof BeltCallSig))return;
    beltCallReturn=d2rsig::Addr(BeltCallRva)+5;
    mercHookIn=d2rsig::Hook(MercQueryRva,MercQuerySig,sizeof MercQuerySig,reinterpret_cast<void*>(&OnMercQuery),reinterpret_cast<void**>(&originalMercQuery));
    if(mercHookIn)framevr::logging::Write("Frame potion toss: native belt mercenary query hooked at the belt shortcut only");
}
namespace {
bool Use(int column,uint64_t session,bool mercenary) {
    if(column<0||column>=4||!session||session!=gamestate::LocalPlayer()||gamestate::MenuOpen())return false;
    if(hud::NativeToolbarHidden()&&!Prepare(session)) {
        framevr::logging::Write("Frame potion sip: native belt owner unavailable; request discarded");
        return false;
    }
    // Exactly one command, still subject to native item/potion rules and the
    // caller's consumption confirmation. Never retry an unconfirmed drink.
    if(!mercenary)return gamecmd::ExecuteUiCommand(0x17+column);
    // A thrown potion must never become the player's drink: without the hook,
    // or while the native gate would refuse the mercenary, press nothing.
    if(!mercHookIn||MercGate()) {
        framevr::logging::Write("Frame potion toss: native mercenary belt use unavailable; nothing pressed");
        return false;
    }
    feeding=true;answered=0;
    const bool handled=gamecmd::ExecuteUiCommand(0x17+column);
    feeding=false;
    if(handled&&answered!=1)framevr::logging::Write("Frame potion toss: belt shortcut did not consult the mercenary query");
    return handled&&answered==1;
}
}
bool Drink(int column,uint64_t session) {return Use(column,session,false);}
bool FeedMerc(int column,uint64_t session) {return Use(column,session,true);}
}
