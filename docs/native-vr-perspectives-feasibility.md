# Native OpenXR alternative perspectives: feasibility study

Status: pre-implementation study, retained as the design record.
The subsequent opt-in prototype and its limits are documented in
[native-vr-perspectives-testing.md](native-vr-perspectives-testing.md).
Baseline: main at `96e8e2bed91e1d41e5dfc68916226f56554789cd`.
Working branch: `feature/native-vr-perspectives`.

## Requested behavior

| Key | Perspective | Input | Initial scope |
| --- | --- | --- | --- |
| F1 | Overhead/isometric-style stereoscopic VR | Keyboard/mouse or physical gamepad | Required |
| F2 | Third-person stereoscopic VR | Keyboard/mouse or physical gamepad | Required |
| F3 | Existing immersive first-person VR | Existing tracked-controller controls | Default at startup |
| F4 | Room-anchored diorama | To be designed separately | Deferred |

Head rotation and translation remain active in every implemented VR perspective.
Switching must not recreate the OpenXR session or require a game restart.
SteamVR and VDXR remain supported; Linux SteamVR and WiVRn are regression targets.
An 8BitDo is supported through its Xbox/XInput presentation or Steam Input;
this is not a promise of support for every device firmware/input mode.

## Findings in the current source

- `vr/vrcam.cpp`, settings loading: native OpenXR forces `vr_mode=4`.
  `SetView` rejects other VR modes and `Toggle` rejects cycling in native VR.
  All three guards are intentional and must stay until replacement paths work.
- `VrViewInner`: only `nativeFirst` calls `framevr::View` for the center and
  individual eyes. Other paths use legacy camera construction and AFR eye shifts.
- `VrProjInner`: only first person uses `framevr::Projection`. Other paths can
  use the FlatVR screen frustum and manually add convergence shifts.
- `TopStereoView` rotates eye cameras around the hero; `TopCamera` derives scale
  and distance from a virtual screen. Neither is a drop-in native OpenXR camera.
  `ApplyView` can disable camera overrides entirely in mode 1.
- `steamvr/transport.cpp`, `EndPair`: the projection layer submits `views[i].pose`
  and `views[i].fov` in the runtime reference space. Legacy matrices would not
  describe the scene consistently with these submitted views.
- `framevr::View` includes first-person scale, origin/recenter handling and bow
  release/preview resolution. It is not a side-effect-free matrix utility.
- `steamvr/frame_math.h` already supplies quaternion composition, view matrices
  and the game's reversed-Z projection. Its symmetric enclosing FOV policy is
  an existing workaround for D2R lighting; preserve it in rendering AND submission.
- `XDetour` can replace controller slot zero through `PadFromBodyWalk`, including
  native transport reports. A physical pad must not be overwritten in F1/F2.
- `VrKeyWalk` currently covers only mode 2; `VrKeyInput` hardcodes WASD. The Flat
  layout helper exists, but cannot be enabled by pretending this is a Flat session.
- `PushArms`, bow/melee/shield ticks have partial perspective gates. Transport
  motion actions, spell hooks, held commands and UI ownership also need auditing.
- Native menus use dedicated UI paths/layers. `ClassicNow` is a legacy FlatVR
  policy and must not become the new native menu policy by accident.
- `cleanroom/camera/d2rcam.cpp` has `ScreenToRay`/`PickRay` using camera snapshots.
  Mouse picking must use a defined center-eye view rather than whichever eye was
  rendered last. Shadow-fitting rays intentionally retain the game camera.
- Current persisted mode IDs are 1 overhead, 2 third person, 3 table, 4 immersive.
  The requested F3 mapping must not reinterpret existing mode 3 saves as immersive.

## Feasibility and rendering design

Feasible as an extension to the current native renderer, not by enabling the
legacy branches. Retain its two-eye D3D12 rendering, OpenXR lifecycle, frame timing,
swapchains and runtime selection. No BodyWalk or FlatVR dependency is needed.

For alternative perspectives define one immutable camera rig per stereo pair:
anchor in game space, stage-to-game rotation, world units per meter, reference
head position/orientation and mode generation. Derive each eye from the actual
located OpenXR pose using the SAME transform and predicted frame time.

Conceptually (quaternion/vector notation, not implementation matrix order):

```
eye_world = anchor + units_per_meter * rotate(rig_rotation, eye_stage - reference_stage)
eye_orientation_world = rig_rotation * eye_orientation_stage
view = inverse_rigid_pose(eye_world, eye_orientation_world)
```

Recenter orientation is included in `rig_rotation`. Preserve the game's actual
row-vector convention, axis conversion and reversed-Z depth in implementation.
The rig transform maps the virtual world to the runtime's tracking space; the
layer can continue submitting the actual tracking-space eye poses. Do not submit
game-world camera coordinates as if they were tracking-space meters.

Use the same selected eye FOV for render and submission, including the current
symmetric-projection option. Do not add FlatVR convergence, toe-in or a second
manual IPD shift. Scale must affect the whole tracking-space-to-world mapping,
not just eye separation. Keep an explicit center-eye matrix for picking and HUD.

F1 should reproduce the original overhead framing with perspective stereo and
6DoF, not demand the original orthographic matrix unchanged. A strict orthographic
image is not a direct replacement for a headset projection layer. Derive initial
framing from the game camera; choose bounded distance and world scale independently
of FlatVR screen dimensions. Evaluate overhead rig pitch and physical translation
comfort in the headset; never tilt or reorient the rig from head movement itself.

F2 uses a hero-following pivot, distance/height and orbit yaw. Mouse/right-stick
orbit adjusts the rig; head motion adjusts the viewpoint within that rig, so
looking sideways does not orbit the camera around the hero. Movement direction
should follow the horizontal rig heading rather than inadvertent head turns.
Near walls, avoid camera penetration if a safe existing obstruction query can be
reused; otherwise document and test the limitation instead of inventing unverified
collision hooks. Distance changes must not rescale the world implicitly.

F3 keeps the existing first-person matrix and physical-combat path unchanged.
Add a separate pure alternative-view builder rather than generalizing the current
side-effectful `framevr::View` in the first implementation.

## Input and interface policy

Separate perspective, runtime transport and input ownership. In F1/F2, keep XR
pose/action synchronization but suppress tracked gameplay output and mirrored pad
replacement. Pass through physical XInput state. Preserve game-native buttons,
optionally transforming the movement stick to the rig heading in F2. Audit right
stick consumption and action translation before claiming native pad compatibility.

For keyboard/mouse, F1 starts with a visible cursor and normal click-to-move/attack.
F2 can reuse validated Flat movement/layout and mouse-orbit primitives, with a
clear cursor-release mode. Respect chat, focus loss, inventory and dialogue input.
Do not allow a controller resting on a desk to steal pointer ownership or switch
D2R back to controller UI. Keep physical devices available during native menus.

A mouse cursor in a headset needs a consistent visual surface and picking ray.
Define a center-view virtual cursor plane and map the same pointer coordinate
through the center-eye inverse view/projection to world selection. Preserve the
native separate-panel path for inventory/dialogues; validate its coordinate mapping
and event ownership independently. Do not reuse a desktop pixel position without
checking viewport, eye texture and panel crop transformations.

Keep current panel distance, scale and dialogue visibility in F3. For F1/F2,
retain readable runtime-space UI rather than scaling panels with the world. Audit
labels, tooltips, automap, belt, potion and interaction overlays, not only inventory.
Physical avatar posing, held weapons and physical contact attacks must be disabled
in external views while the normal animated player model remains visible.

## Safe live switching

Use semantic modes and an explicit key mapping: F1 -> overhead, F2 -> third,
F3 -> existing ID 4. Leave ID 3/table unavailable. Keep old INI meanings; separate
requested key numbers from stored IDs. Default to immersive at every startup for
this first version; restoring the previous external view is a separate opt-in.

Queue requests on input, commit at a stereo-pair boundary, and use an immutable
snapshot for both eyes. No frame may combine one perspective per eye. On switch:
release held/injected inputs, cancel pending physical attacks, advance relevant
pose/interaction generations, reset pointer capture and invalidate temporal camera
history. Audit DLSS/TAA/motion-vector history; use existing reset facilities where
available and add an explicit camera-cut reset if absent. Do not assume `g_gen`
alone resets all histories. Do not recenter or rebuild the OpenXR session globally.

Defer switching during menu interactions and tracking loss initially. Invalid rig
inputs should preserve the last valid mode or fall back cleanly to F3, not publish
half-built matrices. Configuration reload must not race with a stereo pair.

## Implementation stages and acceptance gates

1. Freeze F3 behavioral/performance baseline and add pure camera-rig math tests.
   Add semantic mode/input policy helpers without enabling alternatives.
2. Implement experimental native F2 stereo view/projection and pair-boundary
   switching. Preserve F3 path; test head motion before integrating gameplay input.
3. Enable physical-pad and keyboard/mouse ownership, center-eye picking and UI.
   Prove return to F3 restores existing controls with no stuck attack or key.
4. Implement F1 perspective overhead framing using the same validated rig builder.
   Add default/startup and legacy-configuration migration tests.
5. Enable F1/F2 publicly only after runtime/hardware acceptance. Keep F4 deferred.

Automated tests: identity/reference poses, yaw/pitch/roll and room translation,
recenter invariance, scaled IPD, finite/invertible matrices, projection/submitted
FOV agreement, center-ray reprojection, paired-eye mode generation, held-input
release, physical-pad pass-through, menu/focus transitions, legacy ID mapping,
and unchanged F3 matrix results. Run existing camera/math, projection, menu,
combat, input and DLL build suites. Installer behavior need not change.

Hardware matrix: Windows SteamVR and VDXR; Linux SteamVR and WiVRn through Proton.
Use Quest 2 plus keyboard/mouse and a physical Xbox/8BitDo pad. Validate yaw/pitch/
roll, leaning/crouching, recenter, prolonged alternation F1/F2/F3, both eyes,
NPC dialogue, item targeting, inventory, skills, map, portals, teleport/death,
focus loss, sleeping/disconnected controllers and headset tracking loss.

Compare baseline DLL vs candidate F3 at identical scene, settings, resolution and
refresh rate. Record CPU/GPU frame time, tail latency, dropped/reprojected frames
and memory over repeated switches. Existing `FrameTiming` and profiling support
can help; exclude stereo screenshot readback frames. No new render passes, texture
copies or per-frame allocations are intended. More visible geometry in overhead
views can cost more; numerical math cost alone does not establish performance.
No regression claim is valid before those hardware measurements.

## Delivery and open risks

First deliverable after this study: experimental DLL and tests, not rebuilt setup.
The existing launchers and runtime configuration can stay intact. Optional new INI
keys must have safe defaults so replacing only the DLL suffices. Document them in
English; expose them in setup only if a later UI requirement calls for it.

Highest uncertainty: native mouse picking/UI consistency, multi-device input
ownership and safe render-history reset. Moderate uncertainty: world-scale comfort,
overhead culling/lighting and third-person camera obstruction. Diorama is deferred
because room anchoring, world scale, recentering and UI require separate design.

This is source-level feasibility, not a completed prototype or headset validation.
No code, installer, save path or existing VR configuration was changed for it.

## Reference

Khronos OpenXR: `XrCompositionLayerProjectionView`, including consistency of pose,
FOV and reference space with the rendered image:
https://registry.khronos.org/OpenXR/specs/1.0/man/html/XrCompositionLayerProjectionView.html
