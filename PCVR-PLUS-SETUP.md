# D2R PCVR Plus — Windows Setup 0.4.0 integration preview

The GitHub Actions artifact contains `D2R-PCVR-PLUS-Setup-0.4.0.exe`. This is a full installer for a fresh Windows x64 Diablo II: Resurrected installation, or an ownership-checked update of an existing PCVR installation. It bundles private Python, the compiled Plus DLL, profiles, OpenXR loader and CascLib. D2RLoader and ReShade are downloaded from their official sources and checked against approved hashes. Internet access is needed; installing Python or build tools manually is not.

## Install

1. Install the purchased Steam or Battle.net game, launch it once to complete account prompts, then exit the game and Steam.
2. Run Setup. Choose the game edition and headset.
3. Choose Steam Link / SteamVR, Virtual Desktop / VDXR, Flat Screen (no VR), or All (three Steam shortcuts).
4. Confirm the Steam, game, Saved Games and applicable runtime directories. Saved Games is used for game video settings; characters stay in the original location.
5. Select the Steam account, WASD or ZQSD keyboard layout, and separate text/audio languages. Default leaves language selection to the game.
6. Choose Full install for a clean game, Update / repair to retain existing settings, or Only configure Steam shortcuts for an already complete installation.
7. Start Steam and use `D2R PCVR Plus - Flat Screen`, `D2R PCVR Plus - SteamVR`, or `D2R PCVR Plus - Virtual Desktop`. Original Play remains unmodded.

The helper runtime stays under `%LOCALAPPDATA%\Programs\D2R PCVR Plus`, following the upstream installer design. Do not move it after configuring shortcuts. Linux uses a game-local helper directory.

## New mod layout and updates

- Plugin and profiles: `<game>/mods/D2R-PCVR/d2rloader/plugins/` (`d2rl-pcvr.dll`, `d2r_pcvr.ini`, `d2r_pcvr_plus_flat.ini`).
- ReShade/shader: `<game>/mods/D2R-PCVR/reshade/`.
- Launchers select the `D2R-PCVR` D2RLoader mod. Do not copy this DLL into the old global plugin directory.
- Upstream ownership checks decide whether an old global PCVR plugin can be migrated. Unknown or conflicting plugins are not silently overwritten. Keep previous installer snapshots available when asked.
- Update preserves VR calibration, existing destination Flat preferences, and owned legacy Flat preferences during migration. Installation/configuration snapshots do not contain character saves.

The optional Steam-independent launcher branch is not included. This preview keeps upstream VR bindings; right-stick click does not receive the old Plus run/walk mapping.

## Build and validation

Run `steamvr/setup_windows_tools.ps1`, `steamvr/build_windows.ps1 -ExperimentalPerspectives`, then `installer/build_plus_release.ps1`. The build verifies dependencies, compiles the installer and exercises its isolated install/uninstall lifecycle plus packaged runtime checks. CI runs Windows C++ and Python fixtures before publishing installers; Linux CI also builds the Qt bundle from the same DLL commit.

Use offline test characters for headset acceptance. Read [integration scope, deferred features and hardware checks](docs/upstream-refresh-plus.md). Passing CI does not confirm rendering or input behavior on physical headsets.
