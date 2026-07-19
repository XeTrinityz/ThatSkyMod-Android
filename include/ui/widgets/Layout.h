#pragma once

#include <imgui/imgui.h>
#include <functional>

namespace tsm { namespace ui { namespace widgets {


bool BeginCard(const char* id, float padding = 12.0f, bool withBorder = true);
void EndCard();

bool BeginPannableChild(const char* id,
                        const ImVec2& size = ImVec2(0, 0),
                        bool border = false,
                        ImGuiWindowFlags flags = 0,
                        bool styleNoPadding = false,
                        bool transparentBg = true,
                        float minPanThreshold = 0.0f);
void EndPannableChild();

void ResetPannableState(const char* id);

void NumberedButtonGrid(int count, int selectedIndex,
                        const std::function<void(int)>& onButtonClick,
                        const std::function<ImVec4(int)>& colorGetter = nullptr);

}}}
