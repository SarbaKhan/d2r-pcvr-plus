#pragma once
// Adapted from natpoh/diablo2-vr-port 4b6284c, MIT, Copyright (c) 2026 BodyWalkVR.
// Filter ReShade events by graphics API. Resource trackers must still keep
// individual D3D12 devices separate; this does not identify one specific device.
//
// ReShade wraps every device made in the process, not only the game's: native OpenXR's
// runtime (Virtual Desktop's VDXR) makes a D3D11 device of its own beside the game's
// D3D12 one, and its immediate context's Flush comes as an execute_command_list event.
// Our handlers take what they get for D3D12 objects (the replay called a D3D12 queue's
// GetDesc on that D3D11 context: the game crashed in d3d11.dll, 2026-10-10), so each is
// registered through D3D12Only<&Handler>::Call, which drops the others' events.

#include <type_traits>
#include <reshade.hpp>

namespace d2rvr {

template <class T> bool OnGameDevice(T* object) {
    if (!object) return false;
    if constexpr (requires { object->get_api(); })
        return object->get_api() == reshade::api::device_api::d3d12;
    else return OnGameDevice(object->get_device());
}

template <auto F> struct D3D12Only;
template <typename R, typename First, typename... A, R (*F)(First, A...)>
struct D3D12Only<F> {
    static R Call(First first, A... rest) {
        if (!OnGameDevice(first)) {
            if constexpr (std::is_void_v<R>) return;
            else return R{};   // (a bool event: false, not skipped)
        }
        return F(first, rest...);
    }
};

}  // namespace d2rvr
