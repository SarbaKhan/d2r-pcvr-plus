#pragma once
#include "../steamvr/bow_flight.h"
namespace D2RL {struct PluginContext;}
namespace bowvr {
void Init(const D2RL::PluginContext* context);
void Configure(bool enabled);
void Install();
void Update(const framevr::bowflight::Gate& gate,uintptr_t clientPlayer);
void Submit(const framevr::bowflight::Shot& shot);
bool PreviewReady(); // lock-free; native mesh replaced the tracked draw guide
void Created(uintptr_t game,uintptr_t params,uintptr_t unit);
bool Visual(uint32_t entity,uintptr_t unit,bool hasReleasePoint);
void Report();
}
