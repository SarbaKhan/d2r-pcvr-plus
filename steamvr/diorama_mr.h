#pragma once
#include <string_view>
namespace framevr::dioramamr {
// Optional alpha composition, never a prerequisite for VR session creation.
constexpr bool Enabled(int mode,bool requested,bool supported,bool alphaTexture,bool shader,bool blocked) {
    return mode==3&&requested&&supported&&alphaTexture&&shader&&!blocked;
}
// Explicit client-side color key fallback. Native alpha always takes priority.
constexpr bool ChromaRuntime(std::string_view name) {
    return name=="VirtualDesktopXR";
}
constexpr bool ChromaEnabled(bool nativeDiorama,bool requested,bool nativeAlpha,bool vdxr) {
    return nativeDiorama&&requested&&!nativeAlpha&&vdxr;
}
}
