# D2R PCVR Plus — gamepad stage 1 test DLL

This delivery implements native gamepad support in Flat F1/F2/F3 and right-stick
camera rotation in VR F2. F4 diorama and passthrough are NOT implemented yet.
It is based on validated main 61ad69d, independent of the direct-launcher branch.

## Install for testing

Exit the game. Keep a copy of your current `d2rloader/plugins/d2rl-vrcam.dll`,
then replace ONLY that DLL with this artifact. Use the same DLL on Windows and
Linux/Proton. Keep your current profiles, runtime, shortcuts and saves. No setup
rerun is needed. To revert, exit the game and restore the previous DLL.

Gamepad support defaults to enabled, even with an existing `flat_no_pad=1`.
The new section supersedes that legacy flag. Optional settings go into
`d2rloader/plugins/d2r_pcvr_plus_flat.ini` for Flat and
`d2rloader/plugins/d2r_vr.ini` for VR (edit existing section if present):

```ini
[gamepad]
enabled=1
deadzone=0.20
yaw_speed=120
pitch_speed=90
invert_y=0
```

Speeds are degrees/second. `enabled=0` hides the physical pad in Flat and disables
the new VR F2 camera input; it does not disable immersive VR physical controls.
Menus retain the game's native pad actions. No virtual controller is installed.
Use the same XInput/Steam Input configuration that already worked in VR F1; do
not stack an additional gamepad emulator. A DirectInput-only device not exposed
as XInput is outside this patch's scope.

## Hardware acceptance — pending

Use an offline test character. Confirm each item on Windows and Linux/Proton:

1. Flat F1: native left-stick walking, combat, buttons, inventory and gamepad
   cursor. Switch between mouse and gamepad without UI flicker.
2. Flat F2/F3: left-stick movement relative to the camera; right stick yaw/pitch
   with correct signs; gradual deflection and no drift when released. Verify
   movement, attacks and interactions while rotating.
3. VR F1: same controls and cursor as the validated release; right stick retains
   native behavior. VR grips/triggers must not inject gameplay.
4. VR F2: right-stick yaw/pitch and left-stick movement. Mouse still rotates the
   camera after moving/clicking it; switching back to gamepad preserves native UI.
5. Inventory, stash, NPC dialogue, shop, pause, chat and character screen: right
   stick must not move the camera; native navigation remains available. Hold a
   stick while opening/closing a panel; release it and check for stuck movement.
6. Disconnect/reconnect the gamepad; connect another idle pad; Alt-Tab; cycle
   F1/F2/F3 repeatedly, including while holding buttons. No stale camera motion.
7. VR F3: original Quest controls, snap/smooth turning, combat, inventory and UI
   distances remain unchanged. Repeat under SteamVR and VDXR on Windows and
   SteamVR/WiVRn on Linux. Keep the existing Flat-only Wayland configuration.
8. Compare camera direction/speed, responsiveness and frame time against the old
   DLL. Report mode/runtime, gamepad mode and any problem; include
   `d2rloader/logs/d2r-vr-frame.log` where available.

## Automated coverage

Portable C++ camera fixtures test dead zone, both signs, time integration, idle
secondary slots, release, disconnect, stale samples, UI epochs and view gates.
CI builds the Windows DLL and runs CPU fixtures and immersive source guards.
Linux runs the portable camera math plus existing launcher regressions. These
checks do NOT simulate a headset, Proton game execution or native gamepad UI.
