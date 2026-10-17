#include "../vr/environment_cutaway.h"
#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

int main() {
    using namespace environmentcutaway;
    std::vector<uint8_t> setter(std::begin(SetterSig), std::end(SetterSig));
    std::vector<uint8_t> hide(std::begin(HideSig), std::end(HideSig));
    const auto check = [&](uintptr_t rva, const uint8_t* expected, size_t size) {
        const auto& bytes = rva == SetterRva ? setter : hide;
        return bytes.size() == size && std::memcmp(bytes.data(), expected, size) == 0;
    };
    assert(Verified(check));
    // A changed call destination or setup must refuse the hook, even when
    // the generic animator has the same prologue. Every fingerprint byte counts.
    for (auto* bytes : {&setter, &hide}) {
        for (auto& byte : *bytes) {
            byte ^= 1;
            assert(!Verified(check));
            byte ^= 1;
        }
    }
    assert(!Verified([](uintptr_t, const uint8_t*, size_t) { return false; }));
    int32_t relative{};
    std::memcpy(&relative, HideSig + 10, sizeof relative);
    assert(HideSetupRva + 14 == HideReturnRva);
    assert(static_cast<intptr_t>(HideReturnRva) + relative == SetterRva);

    constexpr uintptr_t base = 0x7ff600000000ULL;
    constexpr uintptr_t hideReturn = base + HideReturnRva;
    assert(TargetAlpha(true, hideReturn, hideReturn, 0) == 1);
    assert(TargetAlpha(false, hideReturn, hideReturn, 0) == 0);
    assert(TargetAlpha(true, hideReturn, 0, 0) == 0);
    // All other observed callers, including creatures and the environment's
    // show branch, must retain every ordinary opacity value verbatim.
    for (uintptr_t rva : {0x69CD38, 0x69CD6F, 0x7850C0, 0xF5223F}) {
        for (float alpha : {0.0f, 0.25f, 1.0f})
            assert(TargetAlpha(true, base+rva, hideReturn, alpha) == alpha);
    }
    for (float alpha : {-1.0f, 0.25f, 1.0f, 2.0f})
        assert(TargetAlpha(true, hideReturn, hideReturn, alpha) == alpha);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float result = TargetAlpha(true, hideReturn, hideReturn, nan);
    assert(std::memcmp(&result, &nan, sizeof nan) == 0);
    puts("Environment cutaway: signature, caller isolation and opacity checks passed");
}
