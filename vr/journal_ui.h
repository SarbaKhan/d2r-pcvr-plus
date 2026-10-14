#pragma once
#include <cstdint>
namespace gamestate {
bool JournalOnly();
bool OpaqueMenuOpen();
bool JournalRect(int32_t out[4]);
void ReadJournalRect(); // UI thread only
}
namespace hud {
bool LayerFound();
void UiFrame(bool active,bool workspace,int eye);
// Menu map lease is maintained by the game's UI thread. Each XR pair must
// capture a new native map before its texture can be submitted in either eye.
void MenuMapCapture(bool active);
void MapFrame();
uint64_t MapTexture(uint32_t& width,uint32_t& height);
// Owned copy of UI only, in R16G16B16A16_FLOAT, shader-resource state.
// RGB is premultiplied, alpha is world transmittance; convert before XR use.
uint64_t UiTexture(uint32_t& width,uint32_t& height,int32_t crop[4]);
}
