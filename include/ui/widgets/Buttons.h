#pragma once

#include <imgui/imgui.h>

namespace tsm { namespace ui { namespace widgets {


bool AccentButton(const char* label, const ImVec2& size = ImVec2(-1, 0));
bool SecondaryButton(const char* label, const ImVec2& size = ImVec2(-1, 0));
bool StandardButton(const char* label, bool topSpacer = true);

bool PillTab(const char* label, bool active, float padX = 12.0f, float padY = 6.0f);

void HeaderBadge(const char* iconLabel, const char* title, const char* subtitle = nullptr);

bool NumberCircleButton(int number, bool active, float diameterDp = 32.0f, const ImVec4& color = ImVec4(0,0,0,0));

bool ColorCircleButton(const ImVec4& color, float diameterDp = 36.0f);

enum class CycleClick { None, Prev, Center, Next };

CycleClick CycleCard(const char* id,
                     const char* leftIcon = nullptr,
                     const char* centerIcon = nullptr,
                     const char* rightIcon = nullptr,
                     float iconSizeDp = 24.0f);

}}}
