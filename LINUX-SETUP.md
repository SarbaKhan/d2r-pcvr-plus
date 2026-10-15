# D2R PCVR Plus — Linux Qt Setup (0.2.3 preview)

An English-language setup wizard for the Steam edition of Diablo II: Resurrected.
The portable archive includes Python, Qt and the Windows mod DLL. You do not need
to install Python, Qt development tools or a compiler. No Blizzard game binaries or archives are
included. Promotional library artwork is credited in `artwork/README.md`. Native Steam and a working Proton installation are required.

## Supported scope

- Linux x86-64, native Steam; initial hardware target: CachyOS with KDE.
- Steam edition of D2R, in any selected Steam library. The installer validates
  the game build using the existing installer checks.
- Flat Screen, SteamVR (including Steam Link), and WiVRn.
- ALVR can use the SteamVR route but has not been tested for this project.
- Battle.net, Flatpak Steam and Flatpak WiVRn are outside this preview's scope.
- The GUI is Qt Widgets and can run on other desktops; it does not require KDE.
  It still needs normal desktop graphics libraries, glibc 2.35+ and XWayland for
  the validated VR route. This is not a promise of compatibility with every distro.

## Install on a clean game

1. Install D2R through Steam, choose a working Proton version, launch it once,
   then exit. Keep the existing Proton prefix and Saved Games folder.
2. Download the **D2R-PCVR-PLUS-Setup-0.2.3-Linux-x86_64** Actions artifact.
   Extract the GitHub ZIP, then extract the `.tar.gz` inside it. Keep all files
   in the extracted folder together.
3. Run `D2R-PCVR-Plus-Setup` from that folder (double-click it, or run
   `./D2R-PCVR-Plus-Setup` in a terminal). Do not run it as root.
4. Confirm or browse for the Steam **data folder**, Linux Steam **executable**,
   game folder and existing Saved Games folder. The setup destination is automatically
   `<game folder>/D2R-PCVR-Plus`; there is no destination field.
   Steam data is commonly `~/.local/share/Steam`; the executable is usually
   `/usr/bin/steam`. They are separate fields. Select the correct Steam account.
5. Select **Steam Link / SteamVR**, **WiVRn**, **Flat Screen (no VR)**, or **All**.
   Runtime fields are required only for the selected VR modes. Select Linux
   manifests, not SteamVR's Windows manifest.
6. Choose **QWERTY — WASD** or **AZERTY — ZQSD** for Flat. Choose text and audio
   languages independently. **Default (game language)** writes an empty override.
   Existing preferences are loaded from the selected game folder.
7. Exit Steam completely using **Steam > Exit**. Review the old D2R launch
   options and approve their replacement on the review page. Click **Install**.
8. After success, start Steam. The selected Plus shortcuts appear in the library.
   Keep compatibility forcing OFF on those helper shortcuts: the purchased D2R
   entry supplies the existing Proton version and prefix.

The download folder can be removed after success. Keep the permanent setup
folder: the original D2R Steam entry calls its launcher. Do not move it manually.
To update, extract a new download outside the game folder and run setup for the
same game. Do not extract the download over the managed installation.

## Launch behavior

| Entry | Behavior |
| --- | --- |
| Original purchased D2R | Original executable, no Plus mode selected |
| Plus - Flat Screen | D2RLoader, Flat profile; native Wayland on a Wayland desktop |
| Plus - SteamVR | D2RLoader, original VR profile; explicit SteamVR runtime; no Proton Wayland |
| Plus - WiVRn | D2RLoader, original VR profile; system OpenXR import; no forced manifest or Proton Wayland |

Connect the headset to the selected VR server before launching its shortcut.
For WiVRn, the server must select its runtime when the headset connects and its
OpenVR compatibility provider must be configured. The installer does not switch
the global OpenXR runtime or install SteamVR/WiVRn/Proton for you.

Do not keep the temporary `env -u XR_RUNTIME_JSON` workaround or a global
`PROTON_ENABLE_WAYLAND=1` prefix in D2R's launch options. The installed launcher
now handles these differences per mode. The review page displays existing test
arguments before replacement. Custom third-party launch wrappers are not merged
automatically: save them if you need to integrate them later.

## Preferences and updates

- `d2rloader/plugins/d2r_pcvr_plus_flat.ini`: `[input] flat_layout=WASD` or `ZQSD`.
- `d2rloader/config/d2rloader.toml`: `text_locale` and `audio_locale`.
- Languages: Default, English, German, Spanish (Spain), French, Italian, Korean,
  Polish, Russian, Chinese (Simplified), Chinese (Traditional), Spanish (Latin
  America), Japanese, Portuguese (Brazil).
- Existing VR calibration and settings are preserved. No headset-specific button
  remapping is introduced; the tested original VR input implementation is kept.
- Other Steam shortcuts and the game's Proton selection are preserved. Repeating
  setup updates matching Plus shortcuts instead of duplicating them. Previously
  created shortcuts for unselected modes are retained.
- Characters are never copied, relocated or restored. Setup uses the game's
  existing prefix and save location. Installation snapshots contain mod files
  and video settings, not character backups. Steam/configuration snapshots and
  previous setup folders are retained for recovery.
- Internet access is needed for pinned, hash-checked loader, ReShade and shader
  compiler downloads. A failed checksum is an error, not an automatic new pin.

## Validation and acceptance tests

The earlier CLI routes were tested by the project owner on CachyOS/KDE with a
Quest 2 and RTX 2080 SUPER. Flat mouse capture required Proton's native Wayland
mode; SteamVR required it disabled. WiVRn v26.9 passed the D3D12/OpenXR probe after
removing the forced host manifest. The owner confirmed working controls and panel
placement in both VR routes. These results validate those routes, **not this new
GUI's end-to-end installation**.

CI checks launcher isolation, external library paths, Steam binary VDF round-trip,
repeat installs, preference preservation, partial configuration rollback, frozen
runtime imports, stock launch exit status and offscreen Qt startup. Windows build
and regression checks run before packaging the Linux setup.

Hardware acceptance still required:

1. Clean install through the GUI with Steam on one disk and D2R on another.
2. Verify all three shortcuts and the original unmodded entry; exit and relaunch.
3. Flat: all cameras, mouse capture, menus/inventory and NPC interaction.
4. VR: A/B/X/Y, stick clicks, movement/combat, panel distances and NPC dialogue,
   separately in SteamVR and WiVRn. Confirm no SteamVR startup for WiVRn.
5. QWERTY/AZERTY and different text/audio languages, then Default for both.
6. Repeat setup with existing preferences; verify no duplicate shortcuts and no
   changes to unrelated shortcuts, custom loader preferences or VR calibration.
7. Confirm update errors are readable and that existing launch paths survive a
   failed dependency download. Test session exit, zone changes and longer play.

Use local offline characters for acceptance testing. Never delete a Proton prefix
as a troubleshooting step for this setup.

## Troubleshooting and recovery

Version 0.2.2 fixes HTTPS downloads from the frozen runtime on distributions such
as CachyOS/Arch: setup explicitly discovers the host CA bundle instead of relying
on certificate paths compiled into the build machine's OpenSSL. Certificate and
hostname verification remain mandatory, as do dependency SHA-256 checks.
Explicit `SSL_CERT_FILE` / `SSL_CERT_DIR` overrides are respected and validated.
No trust-store changes are made to the operating system or game launch environment.

If an earlier installation failed at the first HTTPS download, extract the new
setup and select the same game folder; no game cleanup is needed.
For TLS diagnostics, run `./D2R-PCVR-Plus-Setup --tls-check`. If system certificates
are missing, repair your distribution's `ca-certificates` package; never disable
certificate verification.

The progress page shows the installation worker's output. Copy it before closing
if setup fails. Game-side logs are under `d2rloader/logs`, including
`plus-linux-launch.jsonl`, `steam-2536520.log` and probe output when requested.
Steam's accepted launch request or Proton's process exit code alone does not prove
that OpenXR initialized; inspect the probe text or verify the headset image.

To diagnose a mode after installation, launch Steam, then run:

```sh
steam -applaunch 2536520 --plus-wivrn --plus-probe
steam -applaunch 2536520 --plus-steamvr --plus-probe
```

If the GUI cannot use the desktop platform plugin, try
`QT_QPA_PLATFORM=xcb ./D2R-PCVR-Plus-Setup` (XWayland must be installed).
A terminal launch also exposes missing host-library errors.

Before removing the permanent setup folder, clear D2R's Plus launch options and
remove the matching helper shortcuts in Steam. To restore mod files, the bundled
installer can use a snapshot path printed during installation:

```sh
/path/to/installed/D2R-PCVR-Plus-Setup --install-game \
  --game "/path/to/steamapps/common/Diablo II Resurrected" \
  --restore "/path/to/installation-backups/SNAPSHOT"
```

Exit the game first. This restores tracked mod/video-setting files, never character
progress. Steam file verification does not remove extra mod files.

## Build from source

Use a Linux x86-64 host (CI uses Ubuntu 22.04) and Python 3.12:

```sh
python3 -m venv .venv-linux
. .venv-linux/bin/activate
pip install -r installer/linux-requirements.txt
python -m unittest discover -s steamvr -p setup_linux_test.py
python -m unittest discover -s steamvr -p launch_linux_test.py
QT_QPA_PLATFORM=offscreen python steamvr/linux_setup_entry.py --gui-smoke
python installer/build_linux_setup.py --kit dist-plus/D2R-PCVR-Plus-Linux-Test.tar.gz
```

The Linux test kit must come from the **same commit**, built by the Windows CI job
with the real DLL and OpenXR probe. The builder verifies its manifest hashes.
The source GUI can be previewed without installing; a frozen bundle is required
for installation so the launcher never depends on a developer Python environment.

Qt/PySide and Python remain shared, replaceable components in `_internal`.
See `THIRD_PARTY.md` and the packaged runtime notices for licenses, source links
and rebuilding/replacement instructions.

## Version 0.2.2: Steam artwork and application-menu entries

Selected modes now receive their own icon, portrait cover, horizontal cover,
hero background and transparent logo in the selected Steam account. Setup also
creates application-menu entries in `$XDG_DATA_HOME/applications` (normally
`~/.local/share/applications`), with absolute icon paths. These entries work
with KDE and other desktops that implement the Desktop Entry Specification.

The WiVRn entry includes `Categories=Game;X-WiVRn-VR;`, allowing it to appear in
the headset launcher. It does not request SteamVR. If WiVRn has already cached
its application list, restart WiVRn and reconnect the headset. The previously
documented manual WiVRn entry is adopted on update.

Menu entries use the same tested Steam routing as the library shortcuts. The
wrapper continues to enable native Wayland only for Flat on a Wayland desktop;
SteamVR and WiVRn continue to disable that Proton option. Rerunning setup updates
selected entries without duplicating them or deleting unselected entries.
Preferences, Steam artwork and desktop-entry edits are included in the existing
configuration transaction and its rollback snapshots. Character saves are not
backed up or relocated.

### Acceptance checks for this update

- Update an existing 0.2.1 installation with Steam closed; verify all selected
  modes have their five matching visuals and no duplicate library entries.
- Find Flat, SteamVR and WiVRn in KDE's application menu and launch each selected
  mode. Confirm WiVRn is also listed in its headset launcher with the green icon.
- Repeat setup, including an installation folder containing spaces or accents.
- Confirm Flat mouse look on Wayland and the previously validated VR controls,
  panel placement and NPC dialogue menus. These still require the real machine.

## Version 0.2.3: installation inside the game folder

The complete launcher, bundled Python/Qt runtime, dependencies and setup files
now live in `<game folder>/D2R-PCVR-Plus`. The review page shows this automatic
path. Game plugins remain in their normal D2RLoader locations. Character saves
and the Proton prefix are not moved.

For an existing external installation, run the new download, select the same
game and all previously used modes, and approve replacing the old launch options.
Steam and application-menu shortcuts are refreshed to use the new installation.
The old external folder is not modified or deleted; retain it for its historical
installation/configuration snapshots. Existing game preferences are preserved.
A symlink or an unrelated nonempty folder at the new destination is rejected.

Validation: automated fixtures cover automatic destination selection, paths with
spaces and Unicode, unmanaged-folder/symlink rejection, launcher routing and
failed-update restoration. Confirm Flat, SteamVR and WiVRn on the real machine
after upgrading; CI cannot validate headset launches.
