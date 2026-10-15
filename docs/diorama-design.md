# Native gamepad cameras and OpenXR diorama: staged design

Base: user-validated main `61ad69d87a3444070411ca9be37e01d5f4ebd187`.
Branch: `feature/diorama-gamepad`. The direct-launcher variant is not included.
This document records the code inspection before the stage-1 changes. F4 is
NOT enabled by the first delivery. Hardware validation is a gate between stages.
GitHub Issues are disabled in this repository; this document and the draft PR
track the work without changing repository settings.

## Existing ownership to reuse

`perspective_rig.h::External` identifies persisted modes 1 and 2. F3's persisted
ID remains 4; ID 3 is reserved for the deferred tabletop mode. Do not renumber
existing profiles. `CommitNativeMode` invokes `SetExternalPerspective` at the
existing safe transition point, cancels weapon/grip state, clears pending input,
invalidates picking, and marks a camera cut, without recreating OpenXR.

`transport.cpp::Input` synchronizes actions before its external-perspective
branch. This branch publishes the head pose, marks hands invalid for gameplay,
clears action-held bits, and returns before VR gameplay translation.
`PadMirrorRead` and `ActionsHeld` refuse synthetic VR input in external modes.
`XDetour` therefore returns the actual physical/Steam Input XInput report. F1's
`TurnStick` branch returns without changing it; native inventory navigation and
the game's controller cursor remain Diablo II's responsibility.

F2 already zeroed BOTH right-stick axes but only stored horizontal input in
`g_rightX`. The subsequent VR turn path depended on `turn.right_stick` and snap
settings, and never integrated pitch. Mouse rotation updates `g_mouseYaw` and
`g_mousePitch`; stereo pair latching already snapshots these for both eyes.
Flat deliberately hid all XInput devices through `FlatNoPad` and the distributed
`flat_no_pad=1` profile. Merely adding a right-stick camera would not fix that.

## Stage 1: native gamepad support (this delivery)

Enable the real pad by default in Flat, without virtual devices or replacement
button mappings. A `[gamepad]` section supersedes the old Flat-only hide flag.
F1 remains native; VR F2 and Flat F2/F3 consume only right-stick axes for camera
rotation and reuse existing camera-relative left-stick conversion. Buttons,
triggers, D-pad and stick-clicks retain their native assignments. VR immersive
F3 continues through the original physical-controller path.

Camera input uses a rescaled squared dead zone and degrees/second timing,
independent of poll rate and VR snap-turn. A slot owner prevents an idle second
pad from cancelling the active one. Disconnect, stale reports, UI epochs,
view changes, focus loss and menus invalidate samples. No inertia continues
after stick release. Menu reports pass through before gameplay conversion.

A read-only observer of the existing validated native controller-mode getter
caches the game's own UI mode on its calling thread. Flat installs only this
observer; it does not acquire the VR pointer lease or force an input mode.
Mouse recentering/message rewriting pauses while the native controller UI is
active. Real mouse events still reach the game, allowing normal handover.

## Stage 2: stereoscopic miniature

Extend the native rig, not the legacy FlatVR `TableCamera` matrices. Keep a
tracking-space diorama anchor and a separate game-space centre near the hero.
Map each predicted eye pose through the inverse table transform into game
coordinates. Scale eye separation and head translation consistently, preserving
true stereo and 6DoF parallax. Use the existing per-pair latch for all transforms.

Start with a 0.7 m visible width, approximately 0.8 m ahead; allow manual height
and tilt. A world-space volume must bound the scene. A smaller camera scale alone
is NOT a tabletop: distant terrain/sky would fill the room. Investigate a depth-
reconstructed world-space box mask in the existing D3D12 capture/composition
path, with a controlled edge/base and correctly scaled near plane. Reject a
screen-space rectangle masquerading as a 3D cutout. Verify depth/MSAA/DLSS and
particles before claiming a complete cutout; do not key all black pixels away.
Use an opaque virtual background first. Limit work to the existing two eye
renders; avoid another full scene pass or CPU depth readback.

## Stage 3: manipulation and native gameplay

F4 becomes an external mode for gameplay ownership. The external branch must
still return before synthetic pad/mouse/combat publication, but read valid hand
poses and grip values into a dedicated manipulation state first. Do NOT re-enable
`handsValid` for legacy weapon consumers just to manipulate the table.

On a single-grip edge, capture table-to-hand offset. On a second-grip edge,
rebase from the current transform, recording hand midpoint, separation and
horizontal direction. Compute uniform scale ratio and yaw around that midpoint,
clamped to a usable range (initial proposal: 0.3 to 1.5 m width). Rebase again
when dropping from two hands to one. Small/invalid separation and tracking loss
freeze placement and require fresh grip edges; never divide by zero or jump.
Freeze the anchor on release. Preserve it on F4 exit; require release-to-arm on
entry so a held F3 combat grip cannot unexpectedly grab the table. F11 recentres.

## Stage 4: optional mixed reality (research, not implemented)

Do capability discovery through the ACTUAL application runtime, including Wine's
OpenXR translation on Linux; headset Android extensions alone are not evidence.
Enumerate environment blend modes for the selected system/view configuration and
instance extensions before instance creation. Log runtime identity and results.

Preliminary source inspection on 2026-10-15:

| Route | Evidence | Delivery policy |
|---|---|---|
| WiVRn / Linux | `server/driver/wivrn_hmd.cpp` adds `XRT_BLEND_MODE_ALPHA_BLEND` when `info.passthrough` is true; configuration docs describe a separate alpha stream | Strong candidate; verify alpha reaches the application through Proton and the deployed client/server |
| VDXR / Windows | Public `virtualdesktop-openxr/system.cpp` currently enumerates only `XR_ENVIRONMENT_BLEND_MODE_OPAQUE`; inspected instance source has no passthrough support advertised | No native MR promise; opaque fallback unless the installed runtime actually exposes a usable mechanism |
| SteamVR / Windows | Rectus provides an additional OpenXR passthrough API layer; this is not proof of built-in SteamVR support or Quest camera availability through a streaming driver | Probe actual capabilities; do not install a layer automatically or rely on chroma key |
| SteamVR / Linux | Valve issue #909 documents a compositor-specific Room View limitation; Room View is not an OpenXR application passthrough contract | No guaranteed application passthrough; probe and retain opaque fallback |

References:
- https://github.com/WiVRn/WiVRn/blob/master/server/driver/wivrn_hmd.cpp
- https://github.com/WiVRn/WiVRn/blob/master/docs/configuration.md
- https://github.com/mbucchia/VirtualDesktop-OpenXR/blob/main/virtualdesktop-openxr/system.cpp
- https://github.com/mbucchia/VirtualDesktop-OpenXR/blob/main/virtualdesktop-openxr/instance.cpp
- https://github.com/Rectus/openxr-steamvr-passthrough
- https://github.com/ValveSoftware/SteamVR-for-Linux/issues/909
- https://registry.khronos.org/OpenXR/specs/1.1/html/xrspec.html#xrEnumerateEnvironmentBlendModes
- https://developers.meta.com/vr/documentation/native/android/mobile-passthrough/

Implement alpha-blend composition when advertised and preserve a correct alpha
channel through the capture/shader pipeline. Separately evaluate optional
`XR_FB_passthrough` object/layer lifecycle if advertised and supported through
the installed runtime. FB passthrough can use OPAQUE environment blend plus a
passthrough underlay; those are distinct mechanisms. Negotiate optional extensions
at launch so F4 activation needs no session restart. Creation/permission failures
must disable only MR and leave F4 VR and F1/F2/F3 usable. Avoid mandatory new XR
extensions. Do not advertise cross-runtime MR support before headset tests.

## Stage 5: UI, persistence and acceptance

Reuse the existing independent native UI panel at readable metre dimensions,
never scaled with the miniature. Ensure native controller cursor and selection
are on the same surface. First prototype may show full panels during inventory/
dialogue, as F1/F2 already do; later refine the HUD without duplicating input.

Persist width, comfortable distance/height, yaw/tilt, passthrough preference and
recenter configuration in `[diorama]`. Session anchors are valid only in their
reference space: save relative placement preferences, not an unqualified absolute
pose guaranteed across launches. Handle reference-space changes/recentering by
rebasing. Do not mutate characters or their save paths.

Headset gates: real stereo and lateral parallax; stationary anchor when looking
away/leaning; bounded world at every viewing angle; one/two-hand transitions and
tracking loss; native gamepad gameplay/cursor; readable inventory/dialogue; repeated
F1/F2/F3/F4 switches with held controls; optional MR failure fallback; Windows
SteamVR/VDXR and Linux SteamVR/WiVRn; CPU/GPU frame time against validated F3.
