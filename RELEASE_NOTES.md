# D2R PCVR v0.3.16 - Windows preview

This update focuses on physical combat, bows and crossbows, shield movement,
and clearer visuals. Choose between Unleashed and Classic melee rules, stab
with your weapon, and wipe blood from your visor.

Download **D2R-PCVR-Setup-0.3.16.exe** for graphical installation, or
**d2r-pcvr-windows.zip** for portable setup. Existing users should choose
**Update / repair and keep calibration and video settings**, keeping their
existing helper folders and setup backups.

You need your own purchased Diablo II: Resurrected: Steam 3.3.93787 or
Battle.net 3.3.93847 (experimental). Use local offline characters only.
No Blizzard game assets are included; icons and potion models are read locally
from your installed game. SteamVR and Virtual Desktop/VDXR remain supported.
The installer is unsigned; SHA-256 checksums accompany both downloads.

## Melee and shields

- **Choose your melee rules - contributed by ChuckRibbits:**
  **VR Settings > Combat > Melee rules** switches live between **Unleashed**
  and **Classic**. Unleashed is the default: a physical stroke can strike each
  creature it reaches once, without attack-rating rolls or an attack-speed
  cooldown. Classic uses native attack-rating, defense, block and dodge checks,
  plus the class/weapon/IAS attack cycle and one target per physical stroke.
  Both modes still require deliberate weapon movement; arrows and spells keep
  their existing rules.
- **Thrust attacks:** forward stabs now register independently of swing speed.
  **VR Settings > Combat > Melee thrust speed** defaults to **2 m/s**, adjustable
  from **0.75 to 10 m/s** in **0.25 m/s** steps. Pull back between thrusts; resting
  a weapon against an enemy does not repeatedly hit it.
- **Freer shield movement:** the shield arm can extend farther from the body
  to follow your tracked hand. The shield keeps its size and the contact used
  for shield attacks follows the displayed pose.

## Bows and crossbows

- **More reliable nocked arrows:** recover the native bow preview when its
  updates stall, and keep the nocked arrow aligned while walking and turning.
  This fixes the arrow lagging behind and snapping back into place. Unsupported
  bow rigs can still use the fallback preview.
- **Crossbow aim and bolt origin:** basic bolts launch from the rendered
  crossbow's muzzle and use the bow's physical free-aim flight path.
- **Move while firing:** basic Right Trigger crossbow attacks no longer lock
  movement behind the native firing animation. Equip bolts and assign **Attack**
  to Right Trigger. Native ammo and damage still apply, along with the existing
  class/weapon/IAS firing cadence.
- **Better support-hand placement:** the supporting hand sits lower under the
  stock and retains its tracked wrist orientation.
- **Release the support hand after swapping:** switching from sword/shield to
  crossbow over the shoulder no longer traps both hands on the crossbow. Release
  **Left Grip** after the swap, then grip near the stock to support it again.

## Visor, loot and indoor visuals

- **Blood on the visor - contributed by ChuckRibbits:** life dropping below
  **60%, 40% and 20%** adds splatters around the edges of your view. Healing back
  above a band fades its blood away. Changes to maximum life alone do not cause
  a splash or fade, and the centre of the view stays clear.
- **Wipe it away:** scrub either hand side to side in front of your face with
  the trigger released. Each stroke removes some blood and gives a small haptic
  tick. **VR Settings > HUD > Blood on visor** and **Face wipe gesture** control
  the effect and gesture separately; both default On.
- **Steam Frame visibility:** splatters stay within a visible part of wider
  headset views. The adjustment was checked on Steam Frame and Quest 3.
- **Original loot-label size restored, with an adjustment:** the forced 150%
  enlargement is removed. **VR Settings > HUD > Loot label size** defaults to
  the original **45%**, with a **20-150%** range in five-point steps. Existing
  saved sizes are respected. The darker backing and hold-to-show loot shortcut
  remain available.
- **Clearer indoor effects:** added indoor fog defaults Off, avoiding dark
  scenery silhouettes through Tyrael's wings and preserving indoor lighting.
  The indoor-fog control is hidden for now. Outdoor fog remains available;
  an explicitly saved indoor-fog value is preserved on update.

Existing saved settings and calibration are preserved. Melee rules is a new
choice that defaults to Unleashed when unset; an old `attack_speed` setting does
not select Classic. Choose Classic in Combat settings if that is your preference.

A huge thank-you to **ChuckRibbits** for the blood visor and melee rules work,
and to everyone who reported issues and tested the fixes across headsets!

## Validation and remaining limits

The plain-main gameplay build passed **68/68 native checks**. This release
packages the exact DLL built and installed from `1d7ae63`; the release follow-up
changes documentation and installer version only. The DEV menu, equipment
grants, teleport shortcuts and unmerged elemental-hands prototype are excluded.

The owner verified the bow movement fix, melee rules, thrusts, shield reach,
loot sizing, indoor wing rendering and crossbow fixes during development.
Blood visibility was checked on Steam Frame through SteamVR and Quest 3;
the original Quest baseline was tested through both SteamVR and VDXR. The blood
was also reported to fade after respawning. Later confirmations did not specify
every headset/runtime or individual scenario. The final plain-main artifact
has not received a separate complete headset pass.

All **114 Python checks** passed for setup, repair/removal, launchers, local
asset preparation, diagnostics and package staging. The compiled installer
lifecycle fixture passed installation, registration, failed-removal helper
retention and successful removal using an isolated stub backend. These checks
do not replace a fresh end-to-end installer run against a real game installation.

Installer behavior is unchanged in this update; the installer carries the new
version and payload. Battle.net remains experimental. Special crossbow skills keep
their existing casting route. Airborne enemy targeting and the reported Act III
resource-exhaustion crash are not fixed by this release. Skill-based attack-speed
modifiers remain outside the modeled physical attack cadence.

See [installation instructions](INSTALL-WINDOWS.md) and [controls](CONTROLS.md).

[Full source comparison](https://github.com/bearstonem/d2r-pcvr/compare/v0.3.15-windows-preview...v0.3.16-windows-preview)
