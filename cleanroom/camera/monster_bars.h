#pragma once
#include <cstdint>

namespace d2rcam::monsterbars {
// D2R 3.3.93787: UnitAny::pMonsterData at +0x10; MonsterData's
// MonStats pointer at +0, special-monster flags at +0x1a. The latter's
// low bits identify superuniques, champions, uniques and their minions.
// See docs/monster-bars.md for layout evidence and validation limits.
// Read is injected so failed/stale reads can be covered without a game.
template<class Read> bool Hide(bool stereoUi,bool showOrdinary,bool above,
                                uintptr_t unit,Read read) {
    if(!stereoUi||showOrdinary||!above||unit<0x10000)return false;
    uint32_t type=0,classId=0;uintptr_t data=0,row=0;
    uint8_t special=0;uint16_t rowId=0;uint32_t flags=0;
    if(!read(unit,type)||type!=1||!read(unit+4,classId)||classId>0xffff||
       !read(unit+0x10,data)||data<0x10000||!read(data+0x1a,special)||special||
       !read(data,row)||row<0x10000||!read(row,rowId)||rowId!=classId||
       !read(row+0x3c,flags))return false;
    // Require a recognized, enabled, killable monster row. Preserve bosses
    // even when they lack unique flags, and preserve NPC/interaction labels.
    constexpr uint32_t required=(1u<<25)|(1u<<15);
    constexpr uint32_t keep=(1u<<6)|(1u<<7)|(1u<<8)|(1u<<9)|(1u<<10);
    return (flags&required)==required&&(flags&keep)==0;
}
}
