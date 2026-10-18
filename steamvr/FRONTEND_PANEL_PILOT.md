# Frontend UI separation pilot

VR only. This branch keeps the main-menu character and scenery on a mono quad.
The complete native UI canvas is a transparent quad at the existing menu/laser
position. The backdrop is 12 cm farther away and scaled to preserve its angular
size at the initial placement. No camera, gameplay-state or stereo-pair gates
are relaxed. Stored menu distance presets still apply.

Only `frontend` biomes are eligible; creation (`ui_characterselectscreen`),
loading and gameplay are excluded. The verified SDR world-only compositor,
float UI swapchain support and native HUD layer must all be available. The
mono capture is armed after Present for the following frame, and is not reset
before copying that completed frame. World-only composition is enabled only
after a native UI snapshot is captured. If the verified compositor was not
actually used, no separated layers are submitted and the original menu stays.
A failed optional frontend capture disables this pilot until XR restarts.
GPU/UI conversion failures can still produce a transient incomplete frame;
this experimental path needs real-runtime validation.

This tests separation, not full VR scenery. It does not individually crop or
reposition buttons. Transparent native regions must reveal the scenery; native
opaque regions retain their original appearance. No replacement game assets.

## Headset checks

- Start in VR: side/bottom UI should sit slightly forward of the backdrop.
- Lean sideways: observe relative parallax without duplicated text/buttons.
- Point/click with tracked controllers, use menu navigation and change selection.
- Change Close/Medium/Far menu distance, Desktop/square resolution, restart.
- Enter/exit a game from every perspective; verify input ownership and dialogue.
- Visit character creation/loading and verify original combined presentation.
- Test Flat: identical presentation/input to main.
- Check `Frame frontend:` log lines if separation is absent or incorrect.

Success in CPU/CI checks does not validate the native frontend capture or visuals.
