#pragma once
#include <cstdint>

namespace framevr::skills {
// The controller processes face-button edges and the alternate-bank trigger
// separately. Deliver the modifier by itself before delivering its button.
// Run at the game's pad consumer, not at the faster XR producer: otherwise
// the game can miss every intermediate report during a render hitch.
class SkillPad {
    bool alternate=false,primed=false;
    uint64_t changedAt=0,lastAt=0;
    uint32_t changedPacket=0;
public:
    void Reset(){*this={};}
    uint16_t Poll(uint16_t buttons,bool nextAlternate,uint32_t packet,uint64_t now) {
        if(!primed||now<lastAt||now-lastAt>150) {
            Reset();primed=true;
        }
        lastAt=now;
        if(nextAlternate!=alternate) {
            alternate=nextAlternate;changedAt=now;changedPacket=packet;
            return 0; // includes a button release when changing hands
        }
        // One native simulation interval plus a margin; repeated polls of the
        // same published report cannot turn a modifier-only edge into a cast.
        if(changedAt&&(now-changedAt<50||packet==changedPacket))return 0;
        return buttons;
    }
};
}
