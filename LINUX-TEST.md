# D2R PCVR Plus — Linux launch validation kit

For the new graphical installer, see [Linux Qt Setup](LINUX-SETUP.md).
This CLI kit remains available for diagnostics. It requires host Python 3.10+;
the Qt setup bundles its own Python and does not have that requirement.

Supported test target: Linux x86-64, native Steam, Steam edition of D2R. The game
can live in any Steam library. Run the unmodded game once before installing.
Neither character saves nor the Proton prefix are relocated.

## Configure and install

Extract the kit into a permanent location, then run:

```sh
python3 steamvr/linux_test.py configure \
  --game "/path/to/library/steamapps/common/Diablo II Resurrected" \
  --steam "/usr/bin/steam" \
  --saves "/path/to/existing/Saved Games/Diablo II Resurrected" \
  --steamvr-runtime "/path/to/SteamVR/steamxr_linux64.json" \
  --wivrn-runtime "/usr/share/openxr/1/openxr_wivrn.json"
python3 steamvr/linux_test.py install
```

Omit runtime options for modes you do not use. `--steam` is the Linux executable,
not the Steam data folder. Copy the printed wrapper line into D2R's Steam launch
options. This kit does not automatically edit Steam configuration.

The installer downloads hash-checked dependencies and prepares assets locally
from the purchased game. No Blizzard assets are shipped. Mod/video-setting
snapshots exclude character saves.

## Test each route

```sh
python3 steamvr/linux_test.py flat
python3 steamvr/linux_test.py steamvr --probe
python3 steamvr/linux_test.py steamvr
python3 steamvr/linux_test.py wivrn --probe
python3 steamvr/linux_test.py wivrn
```

Connect the headset through the relevant server first. Inspect probe output before
launching the VR game. A probe creates a D3D12/OpenXR session without launching D2R.
The launcher now enables native Wayland for Flat on Wayland desktops, removes it
for VR, and lets Steam import WiVRn's system runtime without a forced host JSON.
Do not retain the older `env -u XR_RUNTIME_JSON` workaround in Steam launch options.

Logs: `d2rloader/logs/plus-linux-launch.jsonl`, `steam-2536520.log`, and
`plus-steamvr-probe.txt` / `plus-wivrn-probe.txt` in the game folder. Startup errors
also appear in `linux-launch-error.txt` beside the kit. A successful Steam request
is not proof that the game or VR session started.

The owner validated Flat, SteamVR and WiVRn on CachyOS/KDE, Quest 2, RTX 2080 SUPER,
including VR controls and panel placement. See the setup guide for the remaining
GUI installation acceptance tests and recovery instructions. ALVR and Flatpak
installations are not claimed as tested.
