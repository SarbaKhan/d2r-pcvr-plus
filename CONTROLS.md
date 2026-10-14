# D2R PCVR controls

Windows preview for **SteamVR and Virtual Desktop (VDXR)**. These controls
use two tracked VR controllers; an Xbox controller is not required.

## Button names

| Hand / button | Quest 2 / 3 label | Steam Frame label |
| --- | --- | --- |
| Right primary / secondary | A / B | A / B |
| Left primary / secondary | X / Y | A / B |
| Trigger | Index trigger | Index trigger |
| Grip | Grip button | Grip input |
| Stick click | Press the thumbstick | Press the thumbstick |

**Left primary** below means **Quest Left X / Frame Left A**.
**Left secondary** means **Quest Left Y / Frame Left B**.

## Gameplay

| Action | VR control |
| --- | --- |
| Walk | Left stick |
| Snap turn | Right stick left / right |
| Interact / pick up / talk | Right A while aiming at the target |
| Open the tabbed game menu | Left primary; select Inventory, Character, Skills or Journal in its tabs |
| Spell wheel | Hold left secondary and turn your left palm toward you; select with right laser + trigger |
| Assign wheel skills | Wheel → **EDIT SKILLS** → slot → learned skill |
| Select basic weapon mode | Choose **WEAPON** in the wheel center, then release the trigger |
| Free melee swing / thrust | Move your equipped melee weapon through an enemy; also works with a wheel skill selected |
| Manual attack / selected skill | Right trigger; hold for the skill's native repeat behavior |
| Bow | Bring the right hand to the bowstring, hold right trigger, draw back, then release |
| Javelin throw | Javelin equipped in **WEAPON** mode: hold right trigger, make a throwing motion, then release; direction follows the hand's motion |
| Cancel a grab / bow draw / javelin throw | Right B, then release the inputs |
| Swap weapon sets I / II | Reach the swap hand over either shoulder and press its grip; right hand by default, adjustable in VR settings |
| Seated weapon selection / stow | Wheel → **EQUIPMENT / BELT** → **Weapon I**, **Weapon II** or **Stow/Draw** |
| Grab a belt potion | Reach to a bottle and hold that hand's grip |
| Drink | Keep grip held, bring the bottle opening to your mouth and tip it; release an unused bottle to return it |
| Quick potion | Wheel → **EQUIPMENT / BELT** → select a potion with right trigger |
| Wrist health / mana / XP | Look at the back of your left wrist, like checking a watch |
| Minimap tab | Spell wheel ? **MAP** at the bottom; right laser + trigger |
| Map toggle | Left stick click |
| Reset height | Left Grip + left stick click, during gameplay |
| VR settings | Left Grip + left secondary; also **VR SETTINGS** in the wheel |
| Calibrate height / arm span | VR settings → **Calibrate body**; stand straight in a T-pose, release both triggers, then hold both together until **Saved** |
| Recenter | PC F11 |
| Save stereo images | PC F8 |

The wrist shows health, mana, level and XP progress. The minimap is in the
spell wheel's bottom **MAP** tab. Keep holding left secondary while viewing it;
choose **SPELLS** to return to your skills or release the button to close the
menu. Switching tabs preserves the selected skill. Use the game's left or right
corner minimap layout. The tab uses the same explored areas and markers and
restores the automap's previous visibility after closing. Left stick click
still toggles the normal map.

Free swings pause during casting, menus and potion handling. Selecting a wheel
skill changes the right-trigger action; its quick-cast binding stays available.
The belt shows potions in the game's belt slots even without an equipped belt.

Javelins use the game's normal basic throw, ammunition and damage. A stationary
trigger release does not throw. Jabs work with the trigger released. If a wheel
skill is selected, right trigger casts that skill; choose **WEAPON** to return
to physical throwing, or use the quick-cast shortcuts for skills. Physical
throwing currently covers basic javelins; skill projectiles retain their normal
casting controls.

## Quick-cast shortcuts

Hold **Left Trigger**, aim the right hand, then press the assigned face button.
Add **Left Grip** for slots 5–8. Assign the eight slots through **EDIT SKILLS**;
the wheel shows each slot's VR shortcut. Release the face button before changing
slots or the grip modifier.

| Wheel slot | Quest shortcut | Steam Frame shortcut |
| --- | --- | --- |
| 1 | Left Trigger + Right A | Left Trigger + Right A |
| 2 | Left Trigger + Right B | Left Trigger + Right B |
| 3 | Left Trigger + Left X | Left Trigger + Left A |
| 4 | Left Trigger + Left Y | Left Trigger + Left B |
| 5 | Left Trigger + Left Grip + Right A | Left Trigger + Left Grip + Right A |
| 6 | Left Trigger + Left Grip + Right B | Left Trigger + Left Grip + Right B |
| 7 | Left Trigger + Left Grip + Left X | Left Trigger + Left Grip + Left A |
| 8 | Left Trigger + Left Grip + Left Y | Left Trigger + Left Grip + Left B |

## Menus: laser and native buttons

Both methods work in the main menu and native game panels, including inventory,
stash and merchants. Keep the D2R window focused on the PC.

Use a **stick or face button** to start native button navigation and follow the
game's displayed prompts. The idle laser will not move your native selection.
To return to the laser, release the native controls and deliberately move your
aim across the panel. A small hand tremor will not change methods.

### Laser controls

| Action | VR control |
| --- | --- |
| Hover / point | Aim the right controller's laser |
| Select / drag an item | Right trigger; hold while moving the item and release to place it |
| Context / alternate click | Left Grip + Right Trigger (hold grip first) |
| Native Ctrl-click shortcut | Hold Left Trigger, then click with Right Trigger; the game chooses drop, sell or transfer for the current panel |
| Native Shift modifier | Hold Right Grip while using the laser |
| Back / close | Right B, or point and click the panel's close/back button |
| Tabs / page controls | Point and click the native on-screen controls |

Dragging keeps the item held if the ray briefly leaves the panel. Releasing
the trigger ends the drag at the last valid pointer position. Tracking or
desktop focus loss releases the injected mouse button; release the trigger
before starting again. Pressing or releasing Left Grip during a drag will
not convert it into a context-click.

### Native menu prompts

| Game's displayed input | Quest control | Steam Frame control |
| --- | --- | --- |
| A | Right A | Right A |
| B / back | Right B | Right B |
| X | Left X | Left A |
| Y | Left Y | Left B |
| Left stick / right stick | Left stick / right stick | Left stick / right stick |
| LS / RS click | Left stick click / right stick click | Left stick click / right stick click |
| Inventory: X / drop selected item | Left X; hold if the native prompt requires it | Left A; hold if the native prompt requires it |
| Inventory: hold RS / sort | Press and hold right stick click | Press and hold right stick click |
| LB / RB | Left Grip / Right Grip | Left Grip / Right Grip |
| LT / RT | Left Trigger / Right Trigger | Left Trigger / Right Trigger |
| Menu / Start, where available to the game | Controller menu button | Controller menu button |

Native menu actions use the same buttons in the main menu, tabbed menu, stash,
inventory and merchant panels. **Left primary opens the tabbed menu during
gameplay; inside a native menu it performs the displayed X action.**
**Left Grip + left secondary** stays reserved for VR settings.
Right-stick click does not swap weapons during gameplay. Use the shoulder
gesture or **EQUIPMENT / BELT** wheel for weapon configurations.

Quest controllers do not need a separate D-pad or Xbox controller: use sticks
for native navigation and the laser for the on-screen controls. When using
the laser, Right Trigger is a mouse click and the grips/triggers above provide
its modifiers; once native navigation is selected, they perform LB/RB/LT/RT.
Release buttons, triggers and sticks when opening or closing menus so a held
input cannot also act in the next screen or in gameplay.

The mod's own settings and calibration panels use their VR laser controls:
right laser + trigger to select, Right B to close/cancel. Native gamepad input
does not pass through those panels.
