# Flat NPC picking investigation

Branch: `fix/flat-npc-targeting`, based on main after the Flat dialogue-effects fix.
This is a diagnostic build, not a targeting correction. It intentionally preserves
native selection, click dispatch, camera rays and menus.

## Evidence from the current code

- Mouse messages during mouse look are centred in the client viewport.
- Camera::ScreenToRay uses the replacement camera matrices, except shadow fitting.
- MapClick asks the native HoverUnit function for its target. An empty target
  adds Stand Still when flat_click_shoot is enabled.
- Controller interaction uses a separate InteractTarget path.
- UnitToScreen exposes native world anchors used for labels. It has not been
  established whether unselected NPCs reach this callback in mouse mode.

The ground-only selection hypothesis is plausible but not proven. Do not replace
native targets with guessed pointers, change all rays downward, or assume the
label visibility flag performs occlusion testing. Before implementing body picking,
identify the relevant native picker or establish a reliable candidate source with
native eligibility and collision checks.

## Offline test (Windows or Proton)

Keep an original DLL for rollback, outside the plugins directory. Replace only
`mods/D2R-PCVR/d2rloader/plugins/d2rl-pcvr.dll`; keep all settings and assets.
The diagnostic is automatic, only while focused in Flat mouse-look F1/F2 with no
menu/chat or controller UI. No INI change is needed.

1. Start a fresh process and use an offline character near Akara or another NPC.
2. In F1, aim steadily at the face for two seconds and click once.
3. Aim at the torso for two seconds and click once.
4. Aim at the feet for two seconds and click once; close any opened dialogue.
5. Repeat in F2, preferably at the same distance. Note which clicks interacted.
6. Close the game and supply the new session portion of
   `mods/D2R-PCVR/d2rloader/logs/d2r-vr-frame.log` (or the complete text log).

Lines start with `Flat pick ray:`, `Flat pick NPC:` and `Flat pick click:`.
Rays and NPC anchors share a 240-line process budget, sampled at most four times
per second per channel. Clicks have a separate 40-line budget. Restart if the
budget was spent before testing. Only class/runtime IDs and coordinates are
recorded; no memory dumps, unit pointers or character-save contents.

Automated compilation does not establish the live native picking behavior. The
trace is temporary and must be removed or made opt-in in the eventual fix.
