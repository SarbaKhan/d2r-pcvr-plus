# Spatial VR UI prototype

Branch: `feature/vr-spatial-ui`. This is a headset-validation build, not a release.

## Install for testing

Close the game. Keep a copy of your currently validated `d2rloader/plugins/d2rl-vrcam.dll`, then replace it with the DLL from this artifact. The shader is unchanged from the validated Diorama build. No setup, launcher, save, character or Steam shortcut changes are required. Restore the previous DLL to roll back.

## Intended behavior

- F1: native gamepad gameplay. NPC dialogue uses a horizontal panel next to the last native interaction target. Management menus keep their existing flat presentation.
- F2: native gamepad gameplay and right-stick camera. NPC dialogue uses a vertical panel above the last interaction target. Management menus keep their existing flat presentation.
- F3: existing immersive gameplay and NPC controls. Management menus use the existing native controller canvas and VR laser, with the stereo world retained around the panel.
- F4: native gamepad menus appear above the miniature. World bounds and optional WiVRn passthrough remain active. VR controllers do not inject gameplay or menu actions in this mode.
- Physical keyboard/mouse gameplay is disabled in a VR game area. Function-key mod shortcuts still work, as does text chat. Flat Screen keyboard/mouse behavior is unchanged. The added VR desktop-mouse cursor has been removed; the native controller cursor is retained.
- F4 HUD: the original HUD texture is displayed along an outward-facing arc at the miniature edge. Viewer position determines the arc location; head orientation does not. Movement is smoothed, with a stable direction inside the center dead zone.

The HUD uses up to 16 narrow native OpenXR quads, bounded by the runtime's layer limit. This avoids requiring a cylinder extension. It is a segmented curved surface, not new artwork or a new cursor. If HUD transport is unavailable, the original screen HUD is retained/restored. UI extraction failures retain the existing flat-menu fallback.

## Optional settings

Existing configurations use these defaults without manual changes. Add to `[diorama]` to customize:

```ini
hud_tilt_deg=10
hud_arc_deg=103
panel_width_m=1.2
panel_above_m=0.7
```

Tilt ranges from 0 to 60 degrees, arc from 30 to 150 degrees. Panel width is 0.5–2 m. Panel center height above the miniature is 0.3–1.5 m. These are separate from the existing placement height and scale.

## Headset test protocol (not yet performed)

Use offline test characters. Test Windows SteamVR and VDXR, and Linux SteamVR and WiVRn.

1. In F1/F2, talk to different NPCs, scroll speech, select dialogue with stick/D-pad and A. Open trade, inventory and quests; verify the expected flat management-menu fallback. Check panels on sloped ground and after area changes.
2. In F3, verify existing NPC controls, then open inventory, skills, quests, vendor and stash. Look sideways and move your head: the world must remain live and stereoscopic, not a cached image. Test laser hover, selection, drag/drop, modifiers, tabs and confirmation dialogs. No attacks should leak through menus.
3. In F4, repeat menus and dialogue from each side of the table. The native controller cursor must match selection. Move the table with Select before opening a panel; the panel must follow its new anchor and height. Verify the MR edge and real surroundings remain visible on WiVRn.
4. Close panels and inspect HUD life, mana, skills and belt. Walk around the miniature: HUD follows position smoothly, faces outward and tilts upward. Turn only your head: HUD stays at the same edge. Inspect seams, text size, texture proportions and clipping at the widest/narrowest table sizes.
5. Switch F1/F2/F3/F4 repeatedly, including after closing a menu with A/B held. Check focus loss, controller disconnect/reconnect, gamepad UI navigation, and Select hold/height adjustment. No input should remain stuck.
6. Compare frame time against the previous DLL, especially F3 steady gameplay and F4 with HUD. Verify Flat F1/F2/F3 keyboard, mouse and native gamepad behavior.

## Known validation limits

NPC anchoring samples the existing native interaction target, checks the unit type and reads its dynamic-path coordinates while the target callback is executing. It never stores a raw target pointer for the render thread. A fresh target is required in the same player session; otherwise a viewer-facing panel is used instead of guessing an NPC position. Ground elevation uses the orbit rig's player ground height, so unusual NPC elevation needs headset verification.

The existing verified SDR world/UI compositor is reused. Fullscreen native menus must be checked for a genuinely live world in both eyes; automated math and compilation cannot certify this. HDR or unsupported game builds may use the existing flat fallback. Curve readability, controller pointing under Proton, and GPU/runtime layer performance require real hardware testing.
