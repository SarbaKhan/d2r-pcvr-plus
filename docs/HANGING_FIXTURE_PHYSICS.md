# Hanging-fixture motion in stereo

Status: candidate fix; headset result pending. Keep this separate from roof
retention. The owner confirmed the roof fix in Kurast Docks on 2026-10-16 and
reported erratic hanging-fixture motion across the game. The supplied clip
shows the metal lantern model changing position/orientation, not merely its
emitted light changing brightness.

## Evidence

Read-only inspection of the local Steam D2R 3.3.93787 assets identifies the
town's `lantern_metal01_01` as a skinned model with
`PhysicsPartialRagdollDefinitionComponent`. It has two dynamic joints, wind
forces and damping. This instance has no roof `FadeTargetComponent`.

The mod renders two full native draws per game tick. The first receives native
elapsed time; the second uses `right_dt_ms` (default 0.01 ms) so particle effects
still render without advancing a full frame. That tiny positive time also
reaches partial-ragdoll physics. Its motion preparation contains a reciprocal
time calculation; alternating a normal step and this tiny step is a plausible
cause of the observed instability. This is a code-backed hypothesis, not a
headset-confirmed root cause.

Local native call-path evidence (RVAs):

- Physics early jobs `0x7E7010`, regular tick `0x7EAD30`, and wait `0x7EB950`
  are called by the native draw path around `0x650300` / `0x650598`.
- Partial-ragdoll worker calls `D2PhysicsPartialRagdollSkeleton::Step(float)`
  (`0x828FB0`) at `0x7E3EB2`. Profiler metadata identifies the method.
- Step calls preparation at `0x8290A9` through thunk `0x998910 -> 0x994C00`.
  It computes inverse dt at `0x994CF0`, explicitly handling dt <= 0 separately.
- Step calls simulation `0x998B60` at `0x8290B5`, then reads the physical pose
  at `0x8290C9` via `0x998A00` and applies the resulting bone matrices.
- The native draw waits for its physics jobs before returning. Thus the existing
  paired-draw/eye state remains valid for those worker calls.

## Candidate change

Hook preparation and simulation, restricted to the two verified callers in
partial-ragdoll Step. During the second eye of an actual stereo pair, retain
the existing physics state by skipping these two stages. Keep the outer native
Step and its pose readback/application running so both eyes receive the pose.
Skipping the entire Step could leave the right eye with a reset animation pose.

The first eye and non-paired drawing keep their native calls and arguments.
Other callers of these functions are unchanged. Particle timing, light
brightness, asset parameters, roofs and game command/damage logic are unchanged.
This affects the shared partial-ragdoll path, not a list of lamp assets; it may
also affect other cosmetic props using that path. Cloth uses a different path.

Both entry signatures, the preparation thunk and the pair of call sites must
match. Neither stage is skipped until both hooks install successfully. Unknown
signatures leave native simulation running. Logs report readiness and counts of
native versus held preparation/simulation calls every ten seconds when used.
No per-frame asset reads or additional dynamic allocations are introduced.

## Verification

`stereo_prop_physics_test.cpp` checks every fingerprint byte and relative call
target. A synthetic outer Step verifies one physics advance per pair with pose
application in both eyes over 90 frames, plus partial-installation, non-paired
and unrelated-caller fallbacks. It does not execute the game's physics solver.

Headset check: use the isolated developer shortcut and offline character. Look
at the same lantern in Asheara's hut while still and while moving. Its motion
should be stable and agree between eyes, with normal slow motion retained.
Check another hanging fixture elsewhere. Confirm the roof remains present and
ordinary particle effects still render. Inspect `stereo prop physics` runtime
lines for nonzero held counts in both stages. Other physics-driven models,
other game builds/editions and frame-time impact remain unverified.

Game assets and executable captures used for investigation remain local and
must not be committed or attached to releases.
