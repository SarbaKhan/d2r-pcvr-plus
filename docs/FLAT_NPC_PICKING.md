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
The diagnostic is automatic in focused Flat F1/F2 gameplay. Mouse and native
controller UI are identified separately; menus suspend camera/label sampling.
No INI change is needed. Controller input and selection are never overridden.

1. Start a fresh process with the gamepad connected and an offline character
   near a stationary NPC (Akara is convenient). Use the gamepad to activate
   native controller UI; keep the mouse still during this pass.
2. In F1, aim at the face, torso, then feet for two seconds each. Press A once
   at each position, closing the dialogue before continuing. If needed, first
   turn away from the NPC, then aim back to distinguish selection retention
   from acquisition. Keep your position approximately fixed.
3. Repeat in F2. Note whether the native interaction prompt stays on the NPC.
4. If practical, restart the game and repeat using mouse clicks. The earlier
   mouse-only capture is already useful; a new pass is optional.
5. Close the game and supply the new session portion of
   `mods/D2R-PCVR/d2rloader/logs/d2r-vr-frame.log` (or the complete text log).

`Flat pick controller:` records the exact original InteractTarget return,
BEFORE the existing helper's optional secondary query; zero targets matter too.
`Flat pick pad A:` records raw physical A edges before input translation.
`Flat pick centre:` records the visual centre ray independently of whether
native controller selection calls the mouse ScreenToRay routine.
`Flat pick ray:`, `Flat pick NPC:` and `Flat pick click:` remain available.
`Flat pick context:` identifies input source and F1/F2 transitions.

Each camera channel has its own 240-line budget per input type (mouse/pad),
sampled at most four times per second. Controller targets have 240 lines,
A edges 80, mouse clicks 40. Start near an NPC and restart between long passes.
Only class/runtime IDs and coordinates are recorded; no memory dumps, unit
pointers or character-save contents. Raw A traces are not proof of interaction;
correlate them with native targets and the observed dialogue opening.

## First mouse capture (2026-10-19)

Six clicks: in each view the first two had no target, the third selected
class 155, runtime ID 4. The sampled rays' intersection with the NPC ground
height reproduces the click grid coordinates to integer precision. In F2,
the first two rays pass close to the NPC axis at body height. NPC label anchors
only appear in parts of the capture near ground selection. This supports the
ground-picking hypothesis but does not establish a usable unselected-NPC list
or the controller selector's internals. The final F2 click occurs after the
old shared trace budget was exhausted; its last ray is 3.6 seconds old and
must not be treated as a simultaneous sample. Raw logs are not committed.

Automated compilation does not establish the live native picking behavior. The
trace is temporary and must be removed or made opt-in in the eventual fix.
