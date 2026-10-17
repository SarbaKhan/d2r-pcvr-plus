# Diorama editor and floating menus

Based on the upstream refresh validated by the owner on Linux and Windows.

In F4 gameplay, hold **Start for 0.5 seconds** to enter editing. The table stays
where it was. Release Start, then use:

| Input | Action |
| --- | --- |
| Hold RT | Move with headset horizontal translation and yaw, preserving the initial grab offset |
| RB | Recenter in front of the headset at the current floor-relative height |
| D-pad up/down | Height +10/-10 cm |
| LT + up/down | Height +1/-1 cm |
| D-pad left/right | Physical width -10/+10 cm; visible game radius unchanged |
| Tap Start | Finish editing |

Height is bounded to 0.20–1.50 m; width to 0.50–3.00 m. Defaults are 0.74 m
and 1.50 m. Height and width persist in `[diorama]` when editing ends.
Select retains its native map behavior. Outside editing, short Start is delivered
on release. Editing consumes gameplay controls on the owner pad. Menus, focus
loss, tracking loss, disconnection or perspective changes cancel editing;
consumed buttons/triggers require release before returning to gameplay.

F3/F4 spatial menus now keep the stereo world and native UI alpha. Native window
art remains visible. The upstream world-only compositor is retained for both
eyes, including sky/fog during dialogue. F1/F2 menu behavior is unchanged.
The F4 HUD keeps its physical arc length/height while following the table edge;
menu and dialogue dimensions remain independent of table scale.

Installation repair adds the current mod-owned ReShade search roots without
removing custom paths, hotkeys or comments. Windows setup uses the AppData default
instead of inheriting obsolete game-local helper paths; explicit /DIR still works.

## Headset acceptance (not covered by CPU tests)

- Both eyes retain moving scenery with F3/F4 inventory, stash, shop and waypoint UI.
- Native cursor/VR laser, UI distance, dialogue, sky and fog remain correct.
- F1/F2 and Flat menus remain unchanged.
- Start short/long, Select native map, RT grab without jump, turn/move/release,
  RB recenter including while RT is held, and all height/size bounds.
- Minimum/maximum scale preserves UI readability and the same visible map radius.
- Focus/perspective/menu changes, unplug/reconnect and held buttons never leak attacks.
- Height/width survive restart. Test Windows SteamVR/VDXR and Linux SteamVR/WiVRn.
