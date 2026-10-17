# Native environment roof retention

Status: owner verified roof retention in Kurast Docks on 2026-10-16. The implementation
intercepts the shared environmental cutaway request, without per-area preset
overrides. First-person VR is the supported use case; overhead-view support is
not a requirement.

## Shared runtime fix

`vr/vrcam.cpp` hooks the game's opacity-target setter, but changes only calls
from the verified `FadeTargetComponent` hide branch. When the mod's world camera
is active, that branch requests alpha 1 instead of 0. The original setter still
owns interpolation and receives the original duration. Creature fades and other
opacity callers keep their original arguments. No entity storage is edited.

This covers any area's pieces using that native cutaway mechanism, including
attached decorations. It does not identify roofs by filename, repair missing
undersides, or manufacture ceilings. Retained geometry may expose lighting or
model defects that need a separate fix.

Evidence from the local Steam D2R 3.3.93787 executable:

- `FadeTargetComponent` has type hash `0xD94BE0C2`, size `0x58`, and storage
  getter RVA `0x6DFB30`. Its update loop calls the per-entity handler `0xF52090`.
- The handler compares the component's target name with active cutaway boxes.
  On a match, call RVA `0xF52216` requests alpha 0, duration 0.5 seconds. With no
  match, `0xF5223A` requests alpha 1 through the same setter `0xEEA920`.
- The detour captures its own return address and only changes the hide caller
  (`0xF5221B`). It checks both the setter prologue and the exact hide setup/call
  bytes before installation. A mismatch leaves native behavior alone; no broad
  pattern guess or unchecked alternate-build offset is used.
- `environment_cutaway_test.cpp` checks both fingerprints byte by byte, the
  relative call destination, inactive behavior, all other observed callers and
  preservation of nonzero opacity arguments.

Runtime log messages distinguish `environment roof cutaway hook ready` from
`environment roof cutaway suppressed a native hide request`. Installation of a
hook alone does not prove that the desired scene ran through it.

The owner's 17:20 clip was **Kurast Docks**, not Kurast Bazaar. The earlier preset
preview below did not cover the town, so that clip did not validate it. Town data
attaches fade targets to Asheara's front roof and Alkor's front roof; Asheara's
back roof has no such link, matching the half-visible roof in the clip.

Before testing the native DLL, remove the earlier data preview using its
`--remove` command below, with the game closed. Start the isolated developer
build and enter Asheara's hut in Kurast Docks. Check the whole roof from inside
and outside, then another building in town and a Kurast Bazaar building. Look
for visibility, underside lighting and doorway obstruction.

The owner confirmed the roof fix works in the 17:34 test, with a clip showing
Asheara's roof remaining visible on entry. Steam D2R 3.3.93787, SteamVR/OpenXR,
offline developer Paladin, integration `b033f19` (native change `b3dcb12`). Logs
confirm hook installation at 17:31:59 and a suppressed hide request at 17:33:25.
The user also reports erratic hanging-fixture movement throughout the game;
track that separately rather than treating this report as verification of light
or physics behavior. Wider building/area coverage and other game editions/builds
remain unverified. No overhead test is required.

## Earlier data-only diagnostic (superseded)

The helper below is retained for reproducing/removing the earlier experiment.
Do not combine its generated overrides with the native test or wire it into
normal installation. Only source code belongs in the PR; native image dumps
and generated game presets remain local.

The owner reports that Act III / Kurast buildings lose their roofs when entered,
leaving the sky visible from inside. The native Kurast HD presets attach
`FadeTargetComponent` to roof models. In Steam D2R 3.3.93787, 33 Kurast presets
contain 728 such roof entities. This provides a concrete cutaway mechanism to
test separately from fog and wall opacity.

`vr/vrcam.cpp`'s `solidwalls` code changes `transparentWallAlpha`; it does not
remove these roof fade links. Added fog cannot restore absent geometry. The
sampled native stone roof center and slum hut roof corner meshes both contain
downward-facing triangles, but that does not prove every roof has a complete,
correctly lit underside.

## What the experiment changes

`steamvr/prepare_roof_preview.py` reads the owner's local game archives and
generates loose preset overrides exclusively in the existing isolated
`mods/pcvr-dev/pcvr-dev.mpq` environment. It removes `FadeTargetComponent` only
from entities whose entire model selection consists of recognized Kurast roof
models. It keeps geometry, transforms, physics, walls, other fade targets and
dependencies intact. Entities with mixed or unknown model variations are left
alone. This deliberately tests only the native roof cutaway links; it does not
add replacement ceilings or change the outdoor sky.

No game assets belong in the repository, PR or release attachments. The generated
presets stay on the owner's machine and are identified by a local receipt. The
script refuses existing overrides, changed receipt-owned files, linked paths and
development metadata that points at normal saves. Removal only deletes matching
receipt-owned files. It neither reads nor edits character saves.

Do not automatically add this experiment to normal installation. Use the native
hook above for the current test, after removing these data overrides.

### Reproduce or remove the old diagnostic

Use an already configured developer build with exact pcvr-dev metadata and its
own plugin/settings. Close D2R and D2RLoader first. From the topic checkout:

```powershell
.\deps\python\python.exe steamvr/prepare_roof_preview.py --game "YOUR GAME DIRECTORY"
```

No DLL rebuild is needed because this experiment changes local preset data only.
Restart using the isolated developer shortcut (`-mod pcvr-dev`). Use an offline
test character and select **DEV > Scenic Locations > Kurast Bazaar**. Walk into
several buildings, look up, and walk back out. Check for retained roofs, missing
undersides, dark patches, doorway visibility and any unusual performance cost.
Lower and Upper Kurast have different roof families and need separate checks.

To restore native preset behavior, close the game and run:

```powershell
.\deps\python\python.exe steamvr/prepare_roof_preview.py --game "YOUR GAME DIRECTORY" --remove
```

### Verification of the old diagnostic

- Ten synthetic Python checks pass: selective removal, mixed variations,
  preservation, preset scope, collision refusal, install/remove lifecycle,
  partial-install cleanup, modified-file refusal and save-path isolation.
- Read-only comparison against the installed Steam archives finds 33 changed
  presets / 728 roof entities. The only parsed-data changes are removing their
  fade components. None of these selected entities has `WallTransparencyComponent`.
- No new native checks are needed for this Python/data-only experiment.
- Headset appearance, native acceptance of these overrides, runtime performance
  and complete underside coverage remain unverified.

The native hook now replaces this experiment. Its own build/check results and
headset status are recorded in the PR; the data-only checks above do not validate
the hook.
