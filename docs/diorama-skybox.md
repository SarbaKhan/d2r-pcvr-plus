# F4 skybox preview

Based on the owner-validated editor/floating-menu merge. This branch adds the
approved generated panorama as a virtual environment around F4 only. It has no
physical floor, table, sound or animation yet.

`reshade/sky/diorama-night.png` is the owner's approved AI-generated artwork,
not an extracted Blizzard asset. It is preserved byte-for-byte at 1774 x 887.
The known horizontal seam and zenith appearance need headset review.

The existing Diorama mask blends game pixels into the panorama with the same
soft boundary. Per-eye ray bases come from the latched OpenXR view orientations
and FOVs, directly in tracking space. No table transform, scale, hero movement,
head position or IPD translation affects this infinitely distant background.
Looking around changes the visible direction; moving/resizing the table does
not move the sky. Tracking-space resets can still change its apparent heading.

OpenXR composition stays OPAQUE for this preview, even on WiVRn with a previously
enabled passthrough preference. Settings are preserved; the preview gate blocks
alpha passthrough without deleting its implementation. ReShade writes opaque
output. Other perspectives retain their existing behavior.

## Update

The CI artifact contains only the compiled DLL, D2R_DepthFog.fx and panorama,
plus instructions and SHA-256 hashes. Extract its mods folder into an existing
Plus game directory, with the game closed. Keep the previous DLL and shader
outside the plugin/effect directories for rollback. No configuration, saves,
launchers, runtimes or dependencies are replaced.

The existing installer and full-package sky glob includes the new texture for
future builds; no complete installer is produced for this trial.

## Validation

CPU tests cover center/asymmetric FOV rays, yaw, pitch and the forced opaque
preview policy. DLL compilation and existing Windows/Linux fixtures run in CI.
The modified ReShade effect still requires compilation by ReShade in the game;
CI DLL compilation alone does not validate the full effect or headset output.

Headset acceptance:
- F4: night sky, correct horizon/up/down, both eyes, no passthrough on WiVRn.
- Translate and rotate the head: distant sky stable, no IPD-depth distortion.
- Edit/recenter/move/resize the miniature: sky independent, soft boundary intact.
- Floating menus and HUD retain their positions; background visible in both eyes.
- F1/F2/F3 and Flat retain their previous appearance.
- Check frame time, seam visibility and pole distortion before any merge.
