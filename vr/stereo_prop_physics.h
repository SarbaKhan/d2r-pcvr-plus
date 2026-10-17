#pragma once
#include <cstddef>
#include <cstdint>

namespace stereoprops {
// Steam D2R 3.3.93787. Partial-ragdoll Step calls Prepare through a tail-jump
// thunk, then Simulate, then reads the pose back and applies the bone matrices.
constexpr uintptr_t PrepareRva=0x994C00, SimulateRva=0x998B60;
constexpr uintptr_t ThunkRva=0x998910, CallsRva=0x8290A9;
constexpr uintptr_t PrepareReturnRva=0x8290AE, SimulateReturnRva=0x8290BA;
constexpr uint8_t PrepareSig[]={0x48,0x8B,0xC4,0x55,0x56,0x41,0x55,0x48,0x8D,0xA8,0xE8,0xFC,0xFF,0xFF};
constexpr uint8_t SimulateSig[]={0x40,0x53,0x48,0x83,0xEC,0x40,0x0F,0x29,0x74,0x24,0x30,0x0F,0x57,0xC0,0x0F,0x28,0xF1};
constexpr uint8_t ThunkSig[]={0xE9,0xEB,0xC2,0xFF,0xFF};
constexpr uint8_t CallsSig[]={0xE8,0x62,0xF8,0x16,0x00,0x48,0x8B,0x4E,0x48,0x0F,0x28,0xCE,
                             0xE8,0xA6,0xFA,0x16,0x00,0x48,0x8B,0x4E,0x48};
template<class Check>
bool Verified(Check check) {
    return check(PrepareRva,PrepareSig,sizeof PrepareSig) &&
           check(SimulateRva,SimulateSig,sizeof SimulateSig) &&
           check(ThunkRva,ThunkSig,sizeof ThunkSig) &&
           check(CallsRva,CallsSig,sizeof CallsSig);
}

// Both stages must be installed before either can skip work. Rendering still
// calls the outer Step so it reapplies the cached physical pose in both eyes.
inline bool Hold(bool ready, bool paired, int eye, uintptr_t caller, uintptr_t expected) {
    return ready && paired && eye==1 && expected && caller==expected;
}
template<class Native>
bool Stage(bool hold, Native native) {
    if (hold) return true;
    native();
    return false;
}
} // namespace stereoprops
