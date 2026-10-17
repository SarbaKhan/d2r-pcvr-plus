#pragma once
#include <cstddef>
#include <cstdint>

// Steam D2R 3.3.93787: FadeTargetComponent's active-box branch asks the
// generic opacity animator for alpha 0. Other callers also use that animator
// (including creature fades); only this verified call may be overridden.
namespace environmentcutaway {
constexpr uintptr_t SetterRva = 0xEEA920;
constexpr uintptr_t HideSetupRva = 0xF5220D;
constexpr uintptr_t HideReturnRva = 0xF5221B;
constexpr uint8_t SetterSig[] = {
    0xF3,0x0F,0x10,0x19,0x0F,0x2E,0xCB,0x7A,0x02,0x74,0x2E,
    0xF3,0x0F,0x10,0x41,0x10
};
constexpr uint8_t HideSig[] = {
    0x0F,0x28,0xD6,             // duration in xmm2
    0x0F,0x57,0xC9,             // alpha 0 in xmm1
    0x48,0x8B,0xCD,             // opacity component in rcx
    0xE8,0x05,0x87,0xF9,0xFF,  // call SetterRva
    0x45,0x32,0xFF
};

template<class Check>
bool Verified(Check check) {
    return check(SetterRva, SetterSig, sizeof SetterSig) &&
           check(HideSetupRva, HideSig, sizeof HideSig);
}

inline float TargetAlpha(bool active, uintptr_t caller, uintptr_t hideReturn,
                         float nativeAlpha) {
    return active && hideReturn && caller == hideReturn && nativeAlpha == 0.0f
        ? 1.0f : nativeAlpha;
}
} // namespace environmentcutaway
