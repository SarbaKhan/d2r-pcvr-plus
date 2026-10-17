#pragma once
#include <cstdint>
namespace framevr::skills {
constexpr int Count=10;
constexpr uint16_t Buttons(int slot) {
    constexpr uint16_t buttons[]={0x2000,0x4000,0x8000,0x0200}; // B X Y RB
    return slot>=8&&slot<Count?0x1000:slot>=0&&slot<8?buttons[slot%4]:0;
}
constexpr bool Alternate(int slot){return (slot>=4&&slot<8)||slot==9;}
constexpr const char* NativeLabel(int slot) {
    constexpr const char* labels[]={"B","X","Y","RB","LT + B","LT + X","LT + Y","LT + RB","A","LT + A"};
    return slot>=0&&slot<Count?labels[slot]:"Unknown";
}
constexpr int HandSlot(int hand,int button) {
    constexpr int slots[2][5]={{0,8,1,2,3},{4,5,6,7,9}};
    return hand>=0&&hand<2&&button>=0&&button<5?slots[hand][button]:-1;
}
constexpr int Hand(int slot){return slot==4||slot==5||slot==6||slot==7||slot==9?1:0;}
constexpr int TriggerSlot(int hand){return HandSlot(hand,0);}
}
