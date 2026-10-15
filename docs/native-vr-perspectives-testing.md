# Native VR perspectives — experimental DLL, prototype 1

For an existing, working D2R PCVR Plus installation. This is NOT a stable release.
The installer and launchers are unchanged. Windows and Linux/Proton use the same DLL.

1. Close D2R completely. Keep your known-working DLL outside the plugins directory.
2. Replace only `d2rloader/plugins/d2rl-vrcam.dll` with this artifact's DLL.
3. Use your existing SteamVR, Virtual Desktop/VDXR or WiVRn shortcut.
4. Enter an offline game. The session always starts in the existing first-person VR.
5. With the game focused, press F1 (overhead), F2 (third person), F3 (immersive).
   F12 cycles those three. F4/tabletop is intentionally unavailable.
   Switches wait for gameplay, valid tracking and no open menu/settings panel.
6. F11 recenters. In F2, F9 releases/captures the mouse. Keyboard movement uses
   the WASD/ZQSD/custom bindings from `d2r_pcvr_plus_flat.ini` beside the VR profile.
   F1 uses normal cursor/click navigation. F2 uses keyboard movement and mouse look.
7. A physical Xbox/XInput gamepad (including an 8BitDo in XInput mode or through
   Steam Input) supplies the normal game buttons. In F2 the right stick orbits
   horizontally; the left stick moves relative to the rig, not head orientation.
   F1 passes the gamepad through without camera remapping.
8. In F1/F2, tracked-controller gameplay and pointer actions are suspended. Native
   game menus use a standard headset panel for mouse/gamepad input. Return to F3
   to use the existing physical VR controls and spatial menus.

Optional in the active VR INI (not required):

```ini
[native_views]
top_distance=40
third_distance=12
```

Distances are game-world units, not meters. Defaults are provisional. F1 starts
at 55 degrees downward; F2 starts at 15 degrees and mouse look adjusts its pitch.
Do not move either installer folder or change the OpenXR launch route for this test.
Linux VR still uses the validated non-native-Wayland route. Flat launch behavior
and its existing Wayland setting remain unchanged.

## First acceptance pass

- First test F3 BEFORE switching: original A/B/X/Y, sticks, physical melee, bow,
  skills, menu distance, NPC dialogue and frame time must match your known DLL.
- Test F2 while stationary: turn/tilt the head, lean sideways, crouch. Check both
  eyes, depth, world stability, head/orbit independence and F11 recenter.
- Try mouse movement/attacks/loot/NPC selection and then a physical pad. Open
  inventory and dialogue. Check cursor visibility and that selections match it.
- Repeat in F1. Confirm the player model is complete and the overhead view has
  depth rather than a flat screen or doubled image.
- Repeat F1/F2/F3 at least 20 times. Check held buttons, mouse capture, attacks,
  portals, death/loading, tracking loss and controller disconnect/reconnect.
- Repeat on Windows SteamVR/VDXR and Linux SteamVR/WiVRn as available.
- Finally use the Flat shortcut and confirm its existing three views still work.

Record runtime, Proton version (Linux), render resolution, refresh rate, mode,
input device, reproduction steps and the plugin/runtime logs with each report.
F8 can capture a stereo pair; exclude capture frames from performance comparisons.
Compare CPU/GPU frame times and frame-time spikes against the old DLL in the same
scene. Automated checks cannot certify headset comfort or unchanged performance.

## Prototype limits and rollback

No camera-wall collision avoidance or diorama is implemented. Camera distances,
overhead framing, cursor/label depth and mouse picking need real-game acceptance.
F1/F2 use a symmetric enclosing XR FOV for consistent lighting and center picking.
Only F2 has configurable keyboard walking in this prototype; F1 keeps click-to-move.
Menus in F1/F2 are the ordinary flat panel, not tracked spatial panels.
DLSS gets a two-eye reset and motion-vector history invalidation on transitions;
the game's other temporal effects still need visual validation (report trails or
flashes, including whether DLSS is enabled). The return-to-F3 code keeps its original
camera/bow math, but real hardware regression testing is still mandatory.

If anything regresses, close the game and restore your original DLL. The prototype
does not persist the selected VR perspective or change save locations. Startup
always returns to F3. Do not put a second plugin DLL beside the active one.

Build from source: `steamvr/build_windows.ps1 -ExperimentalPerspectives` after the
normal pinned Windows tool setup. The CMake option is OFF by default, and the
experimental workflow distributes only this DLL and these test instructions.
The original release source-baseline manifest is retained unchanged; the dedicated
experimental source test allows the explicit adapters and checks untouched sources
and the first-person eye/projection/bow math against the validated main revision.
