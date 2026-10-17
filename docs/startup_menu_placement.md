# Startup menu placement

The logo and flat main menu share an eye-level, world-fixed panel. Placement
waits until OpenXR is rendering, the session is focused, and the head position
and orientation are tracked. Valid but untracked poses are insufficient: a
runtime can supply its reference-space origin before headset tracking starts.
Both the regular menu path and the pre-game mono-frame fallback use this rule.
The laser cannot interact with an unplaced panel.

Returning to VR focus invalidates the flat and spatial menu anchors. Their next
tracked frame places them at the current head height; ordinary head movement
does not move an already open menu. This supports seated and standing users
without a fixed height offset. A placement diagnostic records the head height
in the frame log.

## Validation

The native panel fixture covers startup without focus/tracking/rendering,
standing placement, keeping a menu stationary during head movement, and seated
placement after reacquiring focus. The reported case is Quest 3 through Virtual
Desktop with VDXR, Steam edition, at 3072 by 3072. Physical verification of this
correction is pending: check the logo and main menu on a fresh launch, return
from the runtime overlay, then check menu lasers and an offline inventory.

## Release validation update (October 14)

The owner accepted startup placement and subsequently confirmed bottom-row
selection after the separate native cursor-bounds correction, on Quest 3 through
Virtual Desktop/VDXR. Repeated overlay/focus transitions remain a regression check.
