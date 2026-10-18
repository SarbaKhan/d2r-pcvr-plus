#pragma once
#include "native_panels.h"
#include <string_view>
namespace framevr::frontend {
// Pilot: the frontend backdrop only. Loading/creation/gameplay stay unchanged.
inline bool Scene(std::string_view biome) { return biome.find("frontend")!=biome.npos; }
constexpr float Separation=.12f;
inline float BackgroundScale(float distance) { return (std::max(distance,.1f)+Separation)/std::max(distance,.1f); }
inline bool Eligible(bool scene,bool world,bool capture,bool compositor,bool layer,bool disabled) {
    return scene&&!world&&capture&&compositor&&layer&&!disabled;
}
}
