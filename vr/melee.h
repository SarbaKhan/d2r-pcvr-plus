#pragma once
#include "../steamvr/melee_contact.h"
namespace D2RL {struct PluginContext;}
namespace melee {
void Init(const D2RL::PluginContext* context);
void Install();
void Submit(const framevr::melee::Sample& sample);
void Cancel();
void CancelShield();
void CancelWeapon();
bool ShieldReady();
void Report();
// Attack speed: one landed weapon hit per native attack cycle, one creature
// per stroke. Off restores unlimited free-swing contacts. Cycle 0 = unknown,
// which never gates. SetAttackCycle comes from the UI thread's stat read.
void ConfigureAttackSpeed(bool enabled,bool traceHits); // trace: [debug] melee_trace
void SetAttackCycle(uint64_t session,uint32_t milliseconds);
// Physical swings and trigger/button melee use the native to-hit/block/level
// rolls. Off restores the contact_hits policy for them. Missiles unaffected.
void ConfigureAttackRating(bool native); // [gameplay] melee_rules=1 (Classic)
// Owning game thread only; shared with physical arrows. Re-resolves the unit
// from its session/id rather than passing a native pointer across threads.
bool LocalServer(uintptr_t& game,uintptr_t& player);
uintptr_t ServerUnit(uintptr_t game,int type,uint32_t id); // owning game thread only
// Owning game thread only. The live, non-hostile hireling nearest a thrown
// bottle's landing point (world metres, with the rendered player and scale of
// that frame); true when it landed at the hireling's feet. distance<0: none.
bool NearHireling(framevr::math::Vec world,framevr::math::Vec player,float unitsPerMetre,float& distance,uint32_t& classId);
// Owning game thread only. The local hireling's life, max life, class and
// unit id; hint is the last known id (direct lookup). False when none is alive.
bool HirelingVitals(uint32_t hint,float& health,float& maxHealth,uint32_t& classId,uint32_t& unitId);
// Owning game thread only, on that exact hireling. Native cosmetic effects
// synced to the client like the game's own: a state (colour tint) and a
// one-shot overlay. Setting needs a live hireling; clearing a state also
// reaches a dead one. A state set here outranks the hireling's other colour
// tints (cold, poison) until it is cleared. False when unavailable or gone.
bool HirelingState(uint32_t unitId,int state,bool on);
bool HirelingOverlay(uint32_t unitId,int overlay);
}
