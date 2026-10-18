#pragma once
namespace framevr::uiworld {
void Register(void (*log)(const char*));
bool Ready();
bool Used(); // verified replacement actually bound since the last inactive reset
void SetActive(bool active);
}
