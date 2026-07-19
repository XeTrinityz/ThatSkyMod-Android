#pragma once

#include <cstdint>

namespace tsm { namespace game { namespace hooks { namespace shout {

bool Install();

std::uintptr_t GetPlayerShout();

bool IsShoutEditorEnabled();
void SetShoutEditorEnabled(bool enabled);

bool IsRainbowModeEnabled();
void SetRainbowMode(bool enabled);

void GetShoutColor(float& r, float& g, float& b, float& a);
void SetShoutColor(float r, float g, float b, float a);

std::uint8_t GetVoiceType();
void SetVoiceType(std::uint8_t voice);

}}}}
