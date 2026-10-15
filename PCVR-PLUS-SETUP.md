# D2R PCVR Plus — Windows Setup 0.1.4 preview

`D2R-PCVR-PLUS-Setup-0.1.4.exe` installs the complete mod into a clean Windows
x64 copy of Diablo II: Resurrected. It bundles the Flat/native OpenXR plugin,
profiles, private Python, OpenXR loader and CascLib. D2RLoader and ReShade are
downloaded from their official sources and checked against pinned hashes.
Internet access is required during setup; no system Python/compiler is needed.
No Blizzard game binaries, archives or extracted caches are distributed.

## Installation

1. Install the purchased Steam or Battle.net game, launch it once to complete
   account prompts and create Settings.json, then exit the game and Steam.
2. Run Setup and select the store, headset and connection: SteamVR, Virtual
   Desktop / VDXR, Flat Screen, or All. Flat requires no headset or OpenXR runtime.
3. Verify the detected Steam, game, Saved Games and optional SteamVR paths.
   Saved Games is used for settings only; characters are not copied or relocated.
4. Select the local Steam account, keyboard layout and text/audio languages.
   Default languages follow the original game configuration.
5. Use Full install for a clean game, or Update / repair to keep calibration
   and existing profiles. Shortcuts only requires an already complete mod.
6. Run the direct EXEs from the game's `D2R-PCVR-Plus` folder or the Windows
   Start menu. The selected Steam library shortcuts remain available too.

The original Steam/Battle.net game entry remains unmodded. Setup does not change
the global OpenXR runtime. VR profiles and Flat profiles are separate:
`d2rloader/plugins/d2r_vr.ini` and `d2rloader/plugins/d2r_pcvr_plus_flat.ini`.
The installer on this branch includes the native F1/F2/F3 VR perspectives merged
into main after user hardware validation. F3 remains immersive VR by default.

## Repair and removal

Setup journals replaced mod files, video settings and Steam configuration for
rollback. Those snapshots exclude character progress. The registered Windows
uninstaller restores tracked files and removes managed Steam shortcuts while
retaining characters, current video settings and installation backups.
Keep the original installation backups when upgrading an older PCVR installation;
the wizard can adopt a verified backup chain. Save and exit D2R and fully exit
Steam before installation, repair or removal. Test with offline local characters.

## Version 0.1.2: preferences and library artwork

The English wizard now offers QWERTY (WASD) or AZERTY (ZQSD) for Flat movement,
and independent text/audio language choices. Both language lists include Default
(game language), which writes an empty locale override. Existing preferences are
read from the selected installation. Other loader settings and VR controls remain
unchanged by these selections, including when updating or configuring shortcuts.

The selected Steam shortcuts now use these names:

- D2R PCVR Plus - Flat Screen
- D2R PCVR Plus - SteamVR
- D2R PCVR Plus - Virtual Desktop

Each shortcut gets its matching icon, portrait/horizontal covers, hero and logo.
Matching legacy shortcuts are renamed without duplicating them or changing their
Steam IDs, history or tags. Steam and Battle.net editions remain independently
tracked. Artwork and preference writes participate in setup's rollback handling.

Before accepting this version, test an update from 0.1.1 and a clean installation,
verify all three Steam shortcuts and visuals, test both keyboard layouts, choose
different text/audio languages, then select Default again. Confirm unchanged VR
controls and panel placement on the actual headset. Automated fixtures do not
replace those checks.

## Game-local installation and direct Windows launchers (0.1.4 preview)

Setup now places its private runtime, scripts and installation records in
`<Diablo II Resurrected>/D2R-PCVR-Plus`. There is no additional destination to
choose. Select **Update / repair** to retain VR calibration and existing profiles.
The game-local folder must not be a symlink or junction.

Open that folder or the D2R PCVR Plus Start menu group and run:

- `D2R-PCVR-Plus-Flat.exe`
- `D2R-PCVR-Plus-SteamVR.exe`
- `D2R-PCVR-Plus-VDXR.exe`

These native executables use the included private Python and existing D2RLoader
routes. They do not launch the game through a Steam library shortcut. Only modes
selected during setup are enabled. Steam shortcuts and artwork remain available.
No system Python or build tools are needed.

This does not remove storefront authentication: the Steam edition may still
require the Steam client/account, and SteamVR still requires its runtime. VDXR
requires Virtual Desktop Streamer. Launch the original purchased game once to
complete its normal sign-in requirements. There is no DRM bypass or new prefix.

When upgrading an older AppData installation, setup adopts verified installation
backups and repoints Steam shortcuts to the new helper directory. It deliberately
retains the old AppData files. Do not run the old copy's uninstaller; use the new
registered uninstaller. Keep the old folder until the new installation is tested.
Characters remain in the game's original Saved Games folder.

Acceptance checks: install/repair in a Steam library with spaces/non-ASCII paths;
launch all selected EXEs without opening the Steam library; verify profiles,
controller calibration and menus; repair again; test removal on a disposable
installation. Automated fixtures do not certify storefront/headset behavior.
