# Custom controller HUD test build

This experimental F1/F2/F4 native OpenXR presentation retains the existing life/mana
panel. It does not alter Flat Screen or immersive F3 hand panels. F4 temporarily shares this head-attached HUD, independent of table scale.

Place the owner's four PNGs under `HUD` beside `d2rl-pcvr.dll` in
`mods/D2R-PCVR/d2rloader/plugins`. Preserve exact filename case on Linux:

- `HUD/ExpbarBackground.png`
- `HUD/ExpBar.png`
- `HUD/SkillBackground.png`
- `HUD/BeltStaminaBackground.png`

Restart after changing textures. Missing/invalid backgrounds leave that block
empty; they cannot prevent launch. Image loading is asynchronous. Extracted game
textures are local inputs, not committed or distributed with this DLL update.

The existing `[head_hud]` position/scale options remain supported. New options:

```ini
belt_x_m=0.40
skills_y_m=-0.50
skills_scale=1.2
```

Belt/stamina moves as one block on the right. Skills and XP are horizontally
centred. Positions use OpenXR VIEW-space metres, independently of desktop or
headset render resolution. Size adjustments preserve the block's proportions.

## Implemented and limitations

- Existing life/mana artwork and readings are retained.
- Adapted XP texture fills by actual progress, clipped around decorative dividers.
- Belt uses existing native item icon cache; no item names, counts, or bottle
  placeholder. Unavailable icons leave empty frames.
- Stamina is read through the already signature-checked stat accessor (10/11).
- Five digital skill-button assignments per group use the existing native
  button-to-slot resolver and current weapon-set bank, without VR A-button fallback.
- LT changes the displayed group by observing the game's existing XInput poll.
  This does not inject inputs or change the selected native skill.
- RT remains blank: the analog-action resolver has not yet been audited. Do not
  guess a binding-table index or treat a trigger as a digital XInput mask.
- Xbox-style button legends and D-pad graphics are provisional drawn symbols,
  not the native glyph textures. Other controller glyph families are not detected.
- A provisional `+` appears for unspent stat/skill points (stats 4/5).
  The native LevelUpButton texture and quest-notification state are pending.
- The persistent walk/run toggle state and native figure icon are pending;
  movement animation alone must not be mistaken for the toggle while idle.

No native memory, character data, assignments or save files are written.

## Required headset checks

Check F1/F2/F4 at Desktop 16:9 and Headset square resolution, then restart. Check
continuous refresh during damage, potion consumption, XP gain, stamina drain and
recovery. Compare A/X/B/Y/RB skills with the original HUD, holding/releasing LT,
rebinding and swapping weapon sets. Verify no belt text and correct D-pad ordering.
Test menus, NPCs, loading, death, character switches, controller disconnect and
F1/F2/F3/F4 transitions. F3 gameplay HUD and Flat Screen must retain their previous appearance.

The Windows CPU preview fixture uses synthetic PNGs to check loading, XP clipping
and byte-identical preservation of the existing vitals crop. It cannot validate
native stat semantics, skill bindings, readability or headset performance.

## Menu policy

Management menus use upstream's native UI capture and opaque-background policy
in all VR perspectives, including the existing menu-distance presets. Dialogue
classification controls placement only; it must not disable stereo UI capture.
NPC conversations alone retain the stereo world. F4 uses the same observed NPC
interaction target, viewer-facing dialogue panel and angular size as F1/F2.
Verify both eyes, shops, inventories, quest/options/pause menus, and transitions
from speech to choices to trade. Existing menu-distance preferences are retained.

Validated HUD defaults: vitals (-0.40, -0.50), XP Y 0.55, belt (0.40, -0.50),
skills Y -0.50. Vitals/XP/belt scale 1.0, skills scale 1.2. Explicit INI settings
remain authoritative; matching DLL fallback values apply when entries are absent.
