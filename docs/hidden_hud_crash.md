# Hidden HUD update crash

During the Quest / Virtual Desktop / SteamVR test on October 14, firing
Fireball shortly after enabling planar aim ended in an access violation at
`D2RLoader.exe+0x1501782`. The saved exception context has `RAX=0`; the failing
instruction reads `[RAX+0x145]`. This is the native HUD update's visibility
check on `ExpFlashLeft`, held at panel offset `0x170`. The adjacent check uses
`ExpFlashRight` at `0x178`. Native layout binding explicitly permits either
lookup to fail, but the update assumes both succeeded.

The last vitals sample was level 4, XP 7845/7875. A level-up notification is
a plausible trigger; neither a level-up nor the cause of the absent widget
is proven by the dump. The fault itself is in HUD presentation, not the
projectile trajectory calculation.

The fix validates the native update entry and both widget reads, then defers
that update only while the VR toolbar is hidden and either widget is absent
or its pointers cannot be read. It retries on the next native update without
caching widget addresses. Fully initialized HUDs and desktop play retain the
original update. Native HUD hiding requires this guard to install; incompatible
game code falls back to the existing rendered-toolbar suppression.

This can defer native toolbar level-up/experience animations while its layout
is incomplete. It does not write character stats, spell state, settings, saves,
or the HUD's widget pointers.

Validation: focused Windows build and all 56 native checks passed, including
either missing widget, unreadable layout, ready layout, and desktop pass-through.
The combined tester build passed all 57 checks. On October 14 the owner
confirmed that Fireball casting and leveling up no longer crashed on Quest
through Virtual Desktop with SteamVR/OpenXR, using combined build `4d56cca`.
Runtime logs confirm the guard deferred incomplete HUD updates and the wrist
HUD continued reporting level 5 and XP progress. Repeated planar toggles and
menu-transition stress testing remain unverified. Raw dumps and native code
inspection artifacts remain local.
