#ifndef TSM_V2_UI_APP_H
#define TSM_V2_UI_APP_H

#include <imgui/imgui.h>

namespace tsm { namespace ui {

void Initialize();

void Draw(bool* menu_open);

float GetUiScale();

void SetAccentColor(const ImVec4& c);
ImVec4 GetAccentColor();

void SetAutoScaleEnabled(bool enabled);
bool IsAutoScaleEnabled();

void SetDockedRight(bool right);
bool IsDockedRight();

}}

#endif
