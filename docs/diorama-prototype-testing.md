# Horizontal Diorama prototype (F4)

Experimental, gamepad-only gameplay. No passthrough, VR manipulation or physical
surface detection. F3 remains the default at launch. F1/F2 and Flat are retained.

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
- No claim of passthrough or complete Diorama feature support in this artifact.

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
