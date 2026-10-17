# D2R PCVR Plus — upstream integration preview

This branch combines current upstream PCVR with Plus Flat cameras, alternative native OpenXR perspectives and Windows/Linux setup. See [the exact upstream commit, integration decisions and deferred features](docs/upstream-refresh-plus.md).

- Flat: F1 isometric, F2 third person, F3 first person. Mouse/keyboard and the game's native controller; right-stick camera in F2/F3. WASD or ZQSD selected during setup.
- VR: F1 isometric and F2 third person use the native classic controller. F3 uses upstream physical VR controls, including its skill wheels. F4 is a stereoscopic tabletop diorama.
- Diorama: native controller gameplay, hold Select to reposition; D-pad up/down changes height by 10 cm, right/left by 1 cm. Optional WiVRn alpha passthrough; black fallback on SteamVR/VDXR.
- Native controller cursor and separate profile preferences; no character relocation or backup.

The DLL is now `mods/D2R-PCVR/d2rloader/plugins/d2rl-pcvr.dll`, the VR profile is `d2r_pcvr.ini` in that folder, and Flat uses `d2r_pcvr_plus_flat.ini`. The DLL, shader and launchers must be updated together through setup. The old DLL-only overlay updater is not supported on this branch.

Use [Windows Setup](PCVR-PLUS-SETUP.md) or [Linux Qt Setup](LINUX-SETUP.md). No system Python or compiler is needed for the packaged installers. Only a separately purchased game is supported; game-generated caches remain local.

This integration needs new headset acceptance. Earlier Plus hardware validation does not certify this new upstream baseline. In particular, upstream opaque-menu composition takes priority over the previous Plus floating-menu implementation. See the linked checklist before merging.
