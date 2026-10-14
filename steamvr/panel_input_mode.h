#pragma once
#include <atomic>
#include <cstdint>
#include <initializer_list>
#include <string_view>

namespace framevr::panels {
// SendInput is queued: protection must already exist before moving/clicking
// the pointer, and must survive the last button-up when a panel closes. The
// short lease expires even if XR stops producing frames.
class InputModeLease {
    std::atomic<uint64_t> until{0};
public:
    void Refresh(uint64_t now) { until.store(now+300,std::memory_order_release); }
    void Clear() { until.store(0,std::memory_order_release); }
    bool Active(uint64_t now) const {
        const auto end=until.load(std::memory_order_acquire);
        return end&&now<end;
    }
    bool PreserveController(uint64_t now,bool controller) const {
        return controller&&Active(now);
    }
};

// Physical bows/javelins and direct walking do not send native pad buttons.
// Starting through the laser can therefore leave the game in KB/M mode, where
// controller interaction targets are not updated. Request the native layout
// on the UI thread while tracked VR gameplay owns input. This lease is separate
// from pointer ownership: it must never block walking or route world input to UI.
class WorldInputMode {
    InputModeLease lease;
public:
    void Update(uint64_t now,bool active) { if(active)lease.Refresh(now);else lease.Clear(); }
    bool NeedsController(uint64_t now,bool player,bool menu,bool controller) const {
        return player&&!menu&&!controller&&lease.Active(now);
    }
};

constexpr uint64_t MessageHash(std::string_view text) {
    uint64_t hash=14695981039346656037ull;
    for(unsigned char c:text)hash=(hash^c)*1099511628211ull;
    return hash;
}
constexpr bool PointerMessage(uint64_t target,uint64_t command) {
    if(target!=MessageHash("InputMessage"))return false;
    for(auto name:{"MouseMove","MouseScroll","LeftMouseButtonDown","LeftMouseButtonUp",
                   "RightMouseButtonDown","RightMouseButtonUp"})
        if(command==MessageHash(name))return true;
    return false;
}
// FocusManager has a separate cursor-snapping flag, independent of the input
// layout. Its nearest-control search can overwrite an exact mouse hit with a
// vendor grid beneath a modal dialog. Only pointer callbacks opt out; native
// controller navigation, rendering and explicit binding actions keep it.
constexpr bool CursorSnapAllowed(bool pointer,bool binding) {
    return !pointer||binding;
}
constexpr bool FocusUsesPointer(bool leaseActive,bool laserSelected) {
    return leaseActive&&laserSelected;
}
// Thread-local in the adapter. Rendering, gameplay, and nested panel-open
// messages still see the real input mode; only pointer dispatch uses mouse
// widget behavior. No native mode byte is modified.
struct PointerScope {
    bool& context;bool previous;
    PointerScope(bool& slot,bool pointer):context(slot),previous(slot){context=pointer;}
    ~PointerScope(){context=previous;}
    PointerScope(const PointerScope&)=delete;
};
template<class Callback> bool RoutePointer(bool owned,bool& context,Callback native) {
    PointerScope scope(context,owned);
    const bool handled=native();
    // An unhandled UI event must not continue to the gameplay input handler.
    return owned||handled;
}
}
