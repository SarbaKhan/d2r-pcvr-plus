#pragma once
#include <cstdint>
namespace framevr::skills {
constexpr int Count=8;
constexpr uint16_t Buttons(int slot) {
    constexpr uint16_t buttons[]={0x2000,0x4000,0x8000,0x0200}; // B X Y RB
    return slot>=0&&slot<Count?buttons[slot%4]:0;
}
constexpr bool Alternate(int slot){return slot>=4&&slot<Count;}
constexpr const char* NativeLabel(int slot) {
    constexpr const char* labels[]={"B","X","Y","RB","LT + B","LT + X","LT + Y","LT + RB"};
    return slot>=0&&slot<Count?labels[slot]:"Unknown";
}
constexpr const char* VrLabel(int slot) {
    constexpr const char* labels[]={"LT + Right A","LT + Right B","LT + Left A/X","LT + Left B/Y",
        "LT + Left Grip\nRight A","LT + Left Grip\nRight B","LT + Left Grip\nLeft A/X","LT + Left Grip\nLeft B/Y"};
    return slot>=0&&slot<Count?labels[slot]:"";
}
}
