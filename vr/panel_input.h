#pragma once
#include <cstdint>
namespace panelinput {inline constexpr uintptr_t InputTag=0x44325256;}
namespace panelinput {
void ObserveControllerMode(); // read-only mode observer for Flat; no input ownership
bool NativeControllerActive(); // cached on the native UI thread
void Install(); // timer thread; retries while native code is still encrypted
void OwnPointer(); // before native menu pointer injection; no game-thread calls
void UseController(); // restore native focus/snapping, retain protection for queued mouse releases
void WorldInput(bool active); // XR thread: bounded ownership of tracked gameplay input
void MaintainWorldController(bool player,bool menu); // UI thread: restore native interaction targeting
bool BlocksWorld(); // includes queued release after the menu closes
bool ControllerReady(); // UI thread: request native controller mode, report when committed
bool ControllerAction(void (*action)(void*),void*); // synchronous native binding operation, controller bank only
}
