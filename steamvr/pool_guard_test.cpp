// Synthetic allocator fixtures: no game code/assets. Checks the production
// preflight and lock/ownership boundary. Native TLSF behavior is covered by the
// separate, optional pools-snapshot-test using an owner's local game snapshot.
#include "../cleanroom/pools/pools.h"
#include "../cleanroom/camera/game_layout.h"
#include <windows.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {
using namespace d2rcam::layout;
constexpr uint64_t MB = 1024 * 1024;
std::array<uint64_t, 6> object{};
std::array<uintptr_t, 5> vtable{};
std::array<uint8_t, 64> freeBody{};
uintptr_t buffer;
int calls;
bool available = true, missing = false;

void Check(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}

void __fastcall NativeFree(void* heap, void* payload) {
    ++calls;
    Check(reinterpret_cast<uintptr_t>(heap) == buffer, "wrong TLSF heap");
    auto* lock = reinterpret_cast<SRWLOCK*>(&object[tlsfalloc::Lock / 8]);
    Check(!TryAcquireSRWLockExclusive(lock), "native Free must run under allocator lock");
    const auto p = reinterpret_cast<uintptr_t>(payload);
    Check(p < buffer || p >= buffer + MB, "added memory must not overlap original pool");
    Check((p & 15) == 0, "payload alignment");
    const uint64_t len = *reinterpret_cast<uint64_t*>(p - tlsf::SizeWord);
    Check(len == MB - tlsf::Payload - tlsf::SizeWord, "new block capacity/flags");
    Check(*reinterpret_cast<uint64_t*>(p + len) == 0, "end marker must be in use with size zero");
    Check(object[tlsfalloc::Size / 8] == MB, "original buffer size must stay intact");
}

void Make() {
    buffer = reinterpret_cast<uintptr_t>(VirtualAlloc(nullptr, MB, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Check(buffer != 0, "fixture allocation");
    const uint64_t len = (MB - tlsf::Control - tlsf::PoolOverhead) & ~7ull;
    *reinterpret_cast<uint64_t*>(buffer + tlsf::Control) = len; // one allocated block
    *reinterpret_cast<uint64_t*>(buffer + tlsf::Control + len + tlsf::SizeWord) = 0;
    object[tlsfalloc::Vtable / 8] = reinterpret_cast<uintptr_t>(vtable.data());
    object[tlsfalloc::Buffer / 8] = object[tlsfalloc::Tlsf / 8] = buffer;
    object[tlsfalloc::Size / 8] = MB;
    vtable[tlsfalloc::FreeSlot] = reinterpret_cast<uintptr_t>(freeBody.data());
    freeBody[tlsfalloc::FreeCallAt] = 0xE8;
    const intptr_t delta = reinterpret_cast<intptr_t>(&NativeFree) -
        reinterpret_cast<intptr_t>(freeBody.data() + tlsfalloc::FreeCallAt + 5);
    Check(delta == static_cast<int32_t>(delta), "fixture relative call range");
    const int32_t rel = static_cast<int32_t>(delta);
    std::memcpy(freeBody.data() + tlsfalloc::FreeCallAt + 1, &rel, sizeof(rel));
}
}

namespace d2rsig {
uintptr_t Addr(uint64_t key) {
    if (!available) return 0;
    if (key == d2rcam::layout::RendererPool) return reinterpret_cast<uintptr_t>(object.data());
    if (key == d2rcam::layout::TlsfVtable) return reinterpret_cast<uintptr_t>(vtable.data());
    if (key == d2rcam::layout::TlsfAllocFree) return reinterpret_cast<uintptr_t>(freeBody.data());
    if (key == d2rcam::layout::TlsfFree) return reinterpret_cast<uintptr_t>(&NativeFree);
    return 0;
}
bool NotInBuild(uint64_t) { return missing; }
}

int main(int argc, char** argv) {
    Check(argc == 2, "specify fixture case");
    const std::string mode = argv[1];
    Make();
    unsigned wanted = 2;
    bool rejected = false;
    if (mode == "off") { available = false; wanted = 0; }
    else if (mode == "enough") wanted = 1;
    else if (mode == "foreign") { object[0] = 1; rejected = true; }
    else if (mode == "free-slot") { vtable[tlsfalloc::FreeSlot] = 0; rejected = true; }
    else if (mode == "free-call") { freeBody[tlsfalloc::FreeCallAt] = 0x90; rejected = true; }
    else if (mode == "layout") { object[tlsfalloc::Tlsf / 8] += 8; rejected = true; }
    else if (mode == "broken") { *reinterpret_cast<uint64_t*>(buffer + tlsf::Control) = 0x22; rejected = true; }
    else if (mode == "prev-flag") {
        *reinterpret_cast<uint64_t*>(buffer + MB - tlsf::SizeWord) = tlsf::PrevFreeBit;
        rejected = true;
    }
    else if (mode == "missing") { available = false; missing = true; rejected = true; }
    else if (mode == "waiting") available = false;
    else Check(mode == "grow", "unknown fixture case");
    char line[400]{};
    bool done = d2rcam::pools::Grow(wanted, 0, line, sizeof(line));
    if (mode == "waiting") {
        Check(!done && calls == 0, "unresolved signature must wait without mutation");
        available = true;
        done = d2rcam::pools::Grow(wanted, 0, line, sizeof(line));
    }
    Check(done, "fixture must settle");
    const bool grown = mode == "grow" || mode == "waiting";
    Check(calls == (grown ? 1 : 0), "unexpected native allocator call");
    Check(!rejected || std::strstr(line, "renderer NOT grown"), "unsafe pool must report refusal");
    Check(!grown || std::strstr(line, "renderer 1 -> 2 MB"), "growth log");
    Check(d2rcam::pools::Grow(2, 0, line, sizeof(line)), "repeated startup must settle");
    Check(calls == (grown ? 1 : 0), "must not grow again or change a settled decision");
    std::puts(line);
    return 0;
}
