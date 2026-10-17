#pragma once
#include "../steamvr/bow_flight.h"
namespace spellvr {
void Install();
bool Ready();
void Report();
// Consume feedback on the UI thread with a freshly resolved local player.
void Feedback(uint64_t session,uintptr_t player,uint32_t id,bool active);
bool Prepare(framevr::bowflight::Shot& shot,uintptr_t game,uintptr_t player);
bool Cast(const framevr::bowflight::Shot& shot,uintptr_t game,uintptr_t player);
void Fired(const framevr::bowflight::Shot& shot,uintptr_t game,uintptr_t player);
bool Position(uintptr_t game,uintptr_t missile,uintptr_t room,int x,int y);
bool FinishVisual(uintptr_t client,const framevr::bowflight::Flight& flight);
}
