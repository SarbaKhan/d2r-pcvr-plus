# Head-space vitals test build

This follows the tested Diorama skybox preview. Install only the supplied DLL
in an existing Plus installation; retain its shader, panorama and configuration.

All native OpenXR perspectives now use upstream F3 HUDPanel visibility control,
including its hidden-widget update guard. Flat Screen does not enter this path.
Menus, NPC dialogue and nameplates retain their existing rendering paths.
The old Diorama curved toolbar capture/swapchain/submission is removed.
Its legacy arc/tilt settings are accepted but no longer affect an active HUD.

F1/F2 reuse the F3 wrist atlas artwork and game statistics. A single transparent
quad is submitted in OpenXR VIEW space, visible to both eyes. Its centre is
0.30 m left and 0.25 m below the viewing centre at 1.10 m distance; width is
0.40 m. These are physical dimensions, independent of desktop/render resolution,
aspect ratio and Diorama scale. The wrist XP strip is excluded from this first
life/mana panel. F4 has no replacement HUD yet. F3's hand display is retained.

Rendering reuses the existing background painter at at most 10 requests/second.
Unchanged statistics reuse the atlas. No gameplay actions are injected by this
path. Session identity and freshness checks suppress obsolete character data.
Menus suppress the head-space panel. Perspective changes invalidate pending art.

## Headset validation required

- Test Desktop 2560x1440, Headset, 2048x2048 and 4096x4096 where performance allows.
  Change resolution live and restart with the selected resolution.
- Verify both eyes, lower-left readability, head motion and no original toolbar.
- Take damage, recover health/mana, change maximum values, load another area,
  return to character selection and enter with another character.
- Cycle F3 -> F1 -> F2 -> F4 -> F3; check menus, inventory cursor, NPC names,
  choices and dialogues in all views. F4 floating menus must retain the world.
- Confirm F3 hand controls/panels and Flat keyboard/mouse/gamepad HUD are unchanged.
- Repeat on Windows SteamVR/VDXR and Linux SteamVR/WiVRn.

Automated CPU fixtures cover mode gating, stale/foreign-session rejection,
central-field geometry and the shared native visibility policy. They cannot
verify the game's HUDPanel hierarchy or actual headset comfort.
