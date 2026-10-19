"""Package only the compiled plugin for an existing mod-folder installation."""
import argparse
import hashlib
from pathlib import Path
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--commit', required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    dll = root / 'build-frame/windows-native/d2rl-pcvr.dll'
    data = dll.read_bytes()
    if not data.startswith(b'MZ'):
        raise SystemExit('Expected a compiled Windows DLL')
    destination = 'mods/D2R-PCVR/d2rloader/plugins/d2rl-pcvr.dll'
    digest = hashlib.sha256(data).hexdigest()
    readme = f'''D2R PCVR Plus - DLL-only test update
Source commit: {args.commit}
DLL SHA-256: {digest}

For an EXISTING installation of the validated upstream-refresh Plus build.
The same Windows DLL is used on Windows and Linux through Proton.

1. Close Diablo II: Resurrected and its loader.
2. Locate your game folder, then {destination}.
3. Move the existing DLL outside the plugins directory for rollback.
4. Copy the enclosed DLL to the same path, replacing that one file only.
5. Extract your own HUD.zip into the plugins directory. It must contain:
   HUD/ExpbarBackground.png, HUD/ExpBar.png,
   HUD/SkillBackground.png, HUD/BeltStaminaBackground.png.
   These local game-derived/custom textures are not included in this artifact.
6. Launch using your existing shortcut.

Keep your existing configuration, shaders, launchers, dependencies and saves.
Do not install this alongside an older global d2rl-vrcam.dll installation.
To roll back: close the game and restore the previous DLL at the same path.

Optional: append to your existing d2r_pcvr.ini (defaults already apply):
[head_hud]
vitals_x_m=-0.40
vitals_y_m=-0.50
vitals_scale=1.0
xp_y_m=0.55
xp_scale=1.0
belt_y_m=-0.50
belt_scale=1.0
belt_x_m=0.40
skills_y_m=-0.50
skills_scale=1.2
Y: negative = lower; positive = higher. X: negative = left; positive = right.
Positions are metres from view centre, range -0.8..0.8.
Scale: 1.0 = current size, 0.5 = half, 1.5 = 150%, range 0.25..3.
Scaling preserves proportions and panel centre. XP/skills stay horizontally centred; belt has its own X position.
Restart after editing.

Changes:
- Diagnostic build: bounded Flat pick ray, NPC anchor and click target logging.
- NPC selection behavior is unchanged; this is NOT a targeting fix yet.
- Flat NPC choices and spoken dialogue retain sky, fog and ceiling effects.
- Shops and other menus keep their previous effect policy; VR is unchanged.
- Configurable F2/F3/F4 life/mana height, now lower by default.
- Custom XP/notification panel at the top; six skill cells at bottom centre.
- Belt/stamina panel on the right, item icons only (no names/counts).
- Native skill/item images use the existing local cache.
- Provisional Xbox-style legends are drawn, not extracted native glyphs.
- RT skill remains empty until its native analog binding reader is verified.
- Level-up uses a provisional + while unspent skill/stat points remain.
- Native notification art, quest notification state and idle run/walk indicator
  still require native-reader/asset validation; this is an incomplete test build.
- Hide the original HUDPanel in all native VR perspectives using the immersive F1 path.
- Retire completed GPU uploads in external views to keep refreshing the HUD.
- F1 first person (default), F2 third person, F3 isometric; F4 Diorama in VR only.
- Returning to the VR main menu restores tracked-controller input; Flat is unchanged.
- Saved mode IDs and custom HUD settings remain compatible.
- Conversations no longer fall back to a flat scene because of dialogue classification.
- F4 dialogues use the same NPC-anchor placement as F2/F3, with the world visible.
- Retain the already-tested skybox preview (existing shader/image required).

Verify Desktop 2560x1440 and square resolutions, including after restarting.
Check life/mana damage, recovery and maximum-value changes in F2/F3/F4.
Check menus/dialogues, F1/F2/F3/F4 transitions, loading and character changes.
Check that F1 hand gameplay panels and Flat Screen remain unchanged.
F4 temporarily uses the same head-attached HUD as F2/F3, independent of table scale.
Headset validation remains required; successful CI is not a visual test.
'''
    output = root / 'dist-plus/D2R-PCVR-Plus-DLL-Update.zip'
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED) as archive:
        archive.writestr(destination, data)
        archive.writestr('README.txt', readme)
        archive.writestr('SHA256SUMS.txt', f'{digest}  {destination}\n')
    print(output)


if __name__ == '__main__':
    main()
