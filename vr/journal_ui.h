#pragma once
#include <cstdint>
namespace gamestate {
bool JournalOnly();
bool OpaqueMenuOpen();
bool DialogueOnly();
bool DialogueRect(int32_t out[4]);
void ReadDialogueRect();
bool JournalRect(int32_t out[4]);
void ReadJournalRect(); // UI thread only
}
namespace hud {
bool LayerFound();
void DioramaHudFrame(bool active,int eye);
uint64_t DioramaHudTexture(uint32_t& width,uint32_t& height);
void UiFrame(bool active,bool workspace,int eye,bool dialogue=false);
// Menu map lease is maintained by the game's UI thread. Each XR pair must
// capture a new native map before its texture can be submitted in either eye.
void MenuMapCapture(bool active);
void MapFrame();
uint64_t MapTexture(uint32_t& width,uint32_t& height);
// Owned copy of UI only, in R16G16B16A16_FLOAT, shader-resource state.
// RGB is premultiplied, alpha is world transmittance; convert before XR use.
uint64_t UiTexture(uint32_t& width,uint32_t& height,int32_t crop[4]);
}
