# Upstream refresh + Plus integration

This branch starts from **bearstonem/d2r-pcvr 772d33720984616517da075f4b4c1570cab190dc** (2026-10-17), not from the older 0.3.16 release DLL. The Plus source reference is **0d51f359a0f1a611fe8b23e0fd37cd0798e0cf02**. This is an integration preview requiring fresh headset acceptance, not a replacement for the validated main build.

## Upstream behavior retained

- Current dual skill wheels, controller bindings, casting, physical melee, haptics, hand minimap, blood visor, and mercenary potion throws. No Plus right-stick run/walk binding.
- Act III renderer pool growth, roof/cutaway behavior, and stereo hanging-object physics fixes.
- D3D12-only ReShade handling, private scene depth, render-resolution management, and upstream native UI compositor.
- Mod coexistence checks, ownership-aware migration, loader preservation, installer backup discovery, HDR warning and Windows security recovery guidance.
- Mod identity and storage remain `D2R-PCVR`: `mods/D2R-PCVR/d2rloader/plugins/d2rl-pcvr.dll` and `d2r_pcvr.ini`. ReShade is under `mods/D2R-PCVR/reshade`. Launchers select `-mod D2R-PCVR`. Do not put the new DLL beside an old global `d2rl-vrcam.dll`.

## Plus additions adapted

- Separate Flat session/profile with no OpenXR startup, F1/F2/F3 cameras, configurable WASD/ZQSD and native classic controller support.
- Native OpenXR F1/F2/F3/F4 perspective rigs; physical VR gameplay remains exclusive to F3. F2 right-stick camera, head-independent F1 picking, tabletop stereo, classic-controller placement/height control, and WiVRn alpha passthrough detection with opaque fallback.
- Diorama HUD and native controller cursor capture. Native controller rumble is not suppressed in external perspectives.
- Windows wizard Flat/All choices, keyboard and separate text/audio languages, named Steam shortcuts and artwork.
- Qt Linux setup, custom game/Steam locations, game-local helper bundle, Flat/SteamVR/WiVRn Steam and desktop entries, preferences and artwork. Native Wayland is restricted to Flat; VR uses XWayland.
- Existing game save paths are used. Installation/configuration rollback snapshots are not character backups.

## Deliberately not carried over

| Addition | Decision / reason |
|---|---|
| Right VR stick click toggles run/walk | Explicitly excluded. Current upstream bindings win. |
| Plus opaque menus floating over a visible world in F3/F4 | Deferred. Keep upstream black-menu composition and handheld quest-journal behavior in F3; do not restore the old UI source-clearing workaround over the new compositor. |
| Plus physical keyboard/mouse suppression in F3 | Deferred: upstream laser positioning uses untagged mouse movement. Keep upstream F3 event handling so inventory hover/selection remains available. Suppression remains limited to F1/F2/F4. |
| Legacy portable DLL-only updater and rollback CMD package | Deferred. It targeted a global DLL/INI and cannot safely migrate the new per-mod layout. Use the full installer; replacing only the old DLL is not supported. |
| Historical 0.3.12 first-person source hash lock | Replaced by upstream-control provenance checks and runtime/launcher tests. Applying it would reject the requested upstream fixes. |
| Experimental Steam-independent launcher branch | Never merged into Plus main; intentionally not imported. |
| Other upstream mods / character migration | No import, save relocation or character changes. |

## Hardware acceptance required

Use a disposable offline character and test Windows SteamVR/VDXR and Linux SteamVR/WiVRn. CI cannot validate headset rendering, controller bindings, GPU alpha composition, or in-game installation migration.

1. Start stock Steam Play, then each Plus shortcut. Confirm correct mod selection, no duplicate plugins, and Flat without any active XR runtime.
2. F3: body calibration, both skill wheels, movement/casting, inventory sorting, mercenary potion toss and upstream controls. Right stick must not trigger the old Plus run/walk command.
3. F1/F2: native gamepad, right-stick F2 camera, native menu navigation/cursor, NPC dialogue size and visibility, HUD, no VR gameplay injection.
4. F4: 1.5 m width / 60-unit radius, Select hold placement, 10 cm / 1 cm D-pad height adjustments, stable stereo/parallax, HUD/cursor. WiVRn passthrough; black background on SteamVR/VDXR.
5. Open/close every menu in both eyes; switch perspectives while controls are held. Check upstream opaque-menu behavior explicitly.
6. Act III exterior/interior transitions, roofs and hanging fixtures, long sessions, resolution changes, GPU performance.
7. Fresh install and existing-install update: Steam shortcuts/art, settings persistence, languages, keyboard, Linux desktop/WiVRn entries, rollback/uninstall preserving unrelated mods and original saves.
