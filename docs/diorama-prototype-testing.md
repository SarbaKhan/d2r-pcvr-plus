# Horizontal Diorama prototype (F4)

Experimental, gamepad-only gameplay with optional OpenXR alpha passthrough.
No VR manipulation or physical surface detection. F3 remains the default at launch. F1/F2 and Flat are retained.

## Install for testing

Close the game. Keep copies of the two installed files so you can restore them.
From this artifact, replace only:

- `d2rl-vrcam.dll` -> `<game>/d2rloader/plugins/d2rl-vrcam.dll`
- `D2R_DepthFog.fx` -> `<game>/reshade-shaders/Shaders/D2R_DepthFog.fx`

Use BOTH files from this build. Existing launchers, profiles and saves stay in
place. The DLL runs on Windows and Linux/Proton using the existing VR launchers.
No installer update is needed. F4 requires the updated shader and enabled ReShade
effects; missing support refuses F4 and logs the reason. Shader compilation and
GPU rendering still need validation inside the actual game.

## First headset test

1. Load an offline character. Sit/stand comfortably and look straight ahead.
2. Press F4. Look down slightly: the horizontal circular miniature is initially
   121 cm across, 65 cm forward and 45 cm below your eye level.
3. Play with the native gamepad. VR controllers must not attack or move the hero.
4. Lean sideways/forward and turn your head. Check genuine depth and parallax;
   the miniature must stay in place, not follow your gaze.
5. Walk with the gamepad: the world scrolls smoothly inside the stationary area.
6. Press F11 to place the table in front of your current position again.
7. Open inventory, dialogue and stash. Check the native gamepad cursor and UI.
8. Switch F4 -> F1 -> F2 -> F3 and back, including while holding gameplay buttons.
   Check native pad controls, F2 camera, F3 physical controls and no stuck inputs.
9. Check Flat separately. Report runtime, GPU, frame rate and any shader errors.

F4 uses the validated F1 toolbar size/height. Menus keep the existing full UI
panel. No custom gamepad cursor or additional XInput emulation is introduced.
Table placement survives perspective changes during a session; F11 resets it.

## Optional VR profile settings

Defaults work without editing the installed `d2r_vr.ini`. To adjust, add this
section to `<game>/d2rloader/plugins/d2r_vr.ini` (do not overwrite other settings):

```ini
[diorama]
width_m=1.2124
radius_units=24
distance_m=0.65
below_eye_m=0.45
follow_seconds=0.15
```

Width is physical diameter. Radius is the sampled horizontal game-world radius,
not monster activation distance. Smaller radius shows less world at larger scale.
Distance/drop take effect on F11 placement. Follow time controls content smoothing;
lag is capped at 20% of radius and large jumps reset the sampled centre.

## Known prototype limits / pending hardware validation

- The visible radius is provisional; no monster generation distance is proven.
  No room loading, generation, AI or save behavior is changed.
- The circular border fades over its outer 12%; clipping uses the depth buffer.
  Transparent particles, labels and surfaces absent from depth can behave
  differently. Check roofs/walls, outdoor terrain and dungeon transitions.
- The border is a horizontal footprint, not a closed solid with generated sides
  or a bottom. Looking from underneath is outside this prototype's target.
- Teleports reset content without a transition fade. Tracking-origin changes may
  require F11. Placement is not persisted across game sessions.
- Native menu/panel legibility, cursor alignment, effects on UI, real stereo,
  performance and all runtime combinations require headset testing.
- Passthrough is experimental and conditional on the runtime; complete Diorama
  feature support is not claimed.

## Feedback revision: menus, viewer-relative movement and larger footprint

The native game cursor capture/panel path used in F1 is now enabled for F4
menus too; no replacement cursor artwork is introduced. F4 also uses F1 label
appearance rather than legacy floor-label fading. Confirm the native circular
gamepad cursor in inventory, stash, trade and dialogue.

F4 rotates only the left-stick gameplay vector by the viewer's horizontal head
orientation relative to the original game camera. Walking to the opposite side
and looking back should reverse the mapping. Menu navigation, right stick and
buttons remain native. Near straight-down/up gaze, the last usable yaw is held
to avoid a singular direction jump. Head turns do not move the table.

Default diameter is now approximately 1.2124 m (sqrt(3) times the original
diameter, three times the footprint area), with the same 24-unit game-world radius. This enlarges the
miniature without exposing more unloaded world. If you already added an explicit
`width_m=0.7`, change it to `width_m=1.2124`; existing explicit settings are respected.
Check movement from all four sides, diagonals and partial stick deflection; open
a menu while holding the stick and verify normal cursor navigation. Recheck F1,
F2 camera, F3 physical controls and Flat. Hardware validation remains required.

## Mixed reality alpha-composition test

Replace BOTH the DLL and shader from this artifact, even if an earlier Diorama
prototype already worked. Keep your tested width/radius/distance/height settings.
Add this key to the existing `[diorama]` section of `d2r_vr.ini`:

```ini
passthrough=1
```

This is enabled by default if the key is absent. `passthrough=0` keeps the opaque
VR background. Restart for a controlled A/B comparison. F1/F2/F3 remain opaque.

Start with the existing Linux WiVRn launcher and connected headset. Press F4 in
an offline game. The intended result is the actual room around the miniature,
with opaque game terrain and readable HUD. Check dark terrain, border fading,
menus and native cursor, turning/leaning, repeated F3/F4 transitions and frame
rate. Then compare with `passthrough=0` and an unsupported runtime's fallback.
The user validated native mixed reality with WiVRn and Quest 2.
SteamVR and Virtual Desktop use the ordinary dark-background Diorama.

Read `<game>/d2rloader/logs/d2r-vr-frame.log` for `Diorama MR:` lines:

- `runtime advertises ALPHA_BLEND`: capability detected through the application's
  actual OpenXR/Wine path, not inferred from the headset model.
- `ACTIVE`: the application submits alpha-composed frames. This is not proof
  that the runtime is showing camera imagery; confirm visually in the headset.
- `opaque fallback`: request/runtime/texture/shader/blocked flags explain why.
- `alpha frame rejected`: MR is disabled for the rest of the session; next frame
  uses opaque VR. Restart to retry after fixing the runtime configuration.

Missing capability, query failure, old shader or a texture format without alpha
keeps the ordinary Diorama. No optional passthrough extension is required to
start the game. Menus remain opaque panels; their surroundings can show the room.
The mask uses depth/geometry, not a black color key, so black terrain stays solid.
The final shader pass preserves alpha and OpenXR is told to use straight alpha.
Other user-added postprocessing effects that overwrite alpha may interfere.

### Runtime scope

This first MR implementation uses core `XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND`
only; `XR_FB_passthrough` object/layer creation is not implemented yet.

- WiVRn's public driver advertises alpha blend when the client reports passthrough;
  its configuration documents encoding the alpha stream. Validated by the user
  with Quest 2 on Linux.
- Virtual Desktop / VDXR uses the dark-background Diorama. Its experimental
  client-side color-key path has been removed after headset testing.
- SteamVR on either OS must actually advertise ALPHA_BLEND for this path to run.
  A headset's camera or a separate compositor room-view feature is insufficient.
  No API layer is installed or enabled by this mod.
- Runtime versions/permissions and the Wine OpenXR bridge may affect capability.
  The application's log and an actual headset test are authoritative.

Primary references reviewed for the implementation:

- https://registry.khronos.org/OpenXR/specs/1.1/man/html/xrEnumerateEnvironmentBlendModes.html
- https://registry.khronos.org/OpenXR/specs/1.1-khr/html/xrspec.html
- https://github.com/WiVRn/WiVRn/blob/master/server/driver/wivrn_hmd.cpp
- https://github.com/WiVRn/WiVRn/blob/master/docs/configuration.md
- https://github.com/mbucchia/VirtualDesktop-OpenXR/blob/main/virtualdesktop-openxr/system.cpp


### Virtual Desktop color-key experiment retired

The user confirmed passthrough worked, but a hard border and persistent magenta
fringe remained. Similarity 25% and Smoothness 20% were the highest usable
settings before other game colors were affected. This fallback is not retained.

Replace BOTH the DLL and `D2R_DepthFog.fx` from this artifact to return to the
native-alpha implementation validated with WiVRn. SteamVR and Virtual Desktop
retain the ordinary dark background and its smooth border. Turn off **VR
Passthrough / Environment** in Virtual Desktop so its client does not remove
colors from the game. This external setting is not controlled by the DLL.

Old `[diorama] chroma_key` and `chroma_color` entries are ignored and may be
deleted. Keep `passthrough=1` for automatic native alpha when supported, including
WiVRn. Detection remains capability-based; WiVRn is the only headset-validated
MR route in this project, not an operating-system allowlist. No additional
API layer, camera bridge or color-key dependency is installed.

Recheck F4 with SteamVR and VDXR for the dark background and smooth border, then
WiVRn for the room and smooth alpha edge. Check menus, native cursor and F3/F4
transitions. No placement, gameplay, save, installer or F1/F2/F3/Flat changes
are included in this rollback.


## Gamepad placement (F4 only)

No virtual room or VR-controller manipulation is included. Use the same native
XInput controller as gameplay (Xbox / compatible 8BitDo mode, including Proton).

- Short Select/Back: native map button, delivered on release as a brief press.
- Hold Select/Back for 0.5 seconds: attach the horizontal miniature in front of
  the headset. Its X/Z position and yaw follow the headset; looking up/down,
  crouching and standing do not change its floor-relative height.
- While holding: D-pad Up/Down adds/subtracts 10 cm; Right/Left adds/subtracts 1 cm.
  One step per press; holding a direction does not auto-repeat.
- Release Select/Back: leave the miniature in place. D-pad buttons still held
  at release stay suppressed until released, preventing gameplay actions.
- F11 remains available and now preserves the selected floor-relative height.

Add to the existing `[diorama]` section if desired (defaults apply if absent):

```ini
height_m=0.74
floor_offset_m=0.0
```

The initial height is 74 cm; adjustments are limited to 20–200 cm and saved as
`height_m` in the active `d2r_vr.ini` after placement ends. Only height persists
across sessions; the tracking-space position is placed again on a new session.
Existing width, radius, follow time and distance are retained. A correctly
calibrated STAGE reference space supplies floor zero. Without STAGE, the first
placement estimates a fixed floor from the head and legacy `below_eye_m`; this
is NOT a measured floor. The log reports this fallback. `floor_offset_m` is a
manual correction in metres, positive upward, clamped to -3..3 m.

Placement is unavailable in inventory, stash, dialogues, chat, mod settings or
while unfocused. Leaving F4, losing focus/tracking, stale input (250 ms), or
controller disconnection cancels attachment without replaying a map press.
Release Select before starting again. Other buttons and native gameplay remain
available; only Select and D-pad are consumed during the gesture. F1/F2/F3 and
Flat retain native Select/D-pad behavior outside cleanup of a canceled gesture.

### Required headset and gamepad validation

Automated policy/math tests do not validate the game's handling of the deferred
map press or Steam Input mappings. Test on Windows and Linux:

1. Tap Select repeatedly: one map toggle per tap, no movement of the miniature.
2. Hold for >0.5 s: no map toggle. Walk/turn, crouch/stand, then release. The
   miniature follows only while held and remains horizontal at fixed height.
3. Check each D-pad direction, held directions, and repeated presses. No potion
   or native D-pad action should fire during placement or at Select release.
4. Open a panel or switch F4/F3 while holding Select. Release all controls, then
   confirm native controls and a fresh placement gesture work normally.
5. Disconnect/reconnect the pad, including with Select held; alt-tab; lose and
   recover tracking. No resumed attachment or delayed map toggles are expected.
6. Check two controller slots: an idle/disconnected second pad must not cancel
   the active gesture, and two pads must not place the table simultaneously.
7. Restart the game: selected height persists. Recheck F1/F2 camera, immersive
   F3 physical controls, Flat, menus/cursor, and WiVRn native MR.
