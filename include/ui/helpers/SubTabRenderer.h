#pragma once

#include <imgui/imgui.h>
#include <ui/core/metrics.h>
#include <ui/core/App.h>
#include <ui/widgets/Icons.h>
#include <algorithm>

namespace tsm { namespace ui { namespace helpers {

inline bool DrawSubTabs(const char** icons, int count, int& activeIndex, float sizeScale = 1.0f) {
    using namespace tsm::ui::widgets;

    int originalIndex = activeIndex;

    ImGui::Dummy(ImVec2(0, 10.0f));

    const float base_d   = DP(44.0f) * sizeScale;
    const ImVec4 acc     = tsm::ui::GetAccentColor();
    const ImU32  bg_sel  = ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 0.28f));
    const ImU32  brd_sel = ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 0.70f));
    const ImU32  bg_nrm  = ImGui::GetColorU32(ImVec4(1,1,1,0.10f));
    const ImU32  brd_nrm = ImGui::GetColorU32(ImVec4(1,1,1,0.18f));

    float avail_w = ImGui::GetContentRegionAvail().x;
    float base_gap = ImGui::GetStyle().ItemSpacing.x;
    float s = 1.0f;
    float need_w = count * base_d + (count - 1) * base_gap;
    if (need_w > avail_w && count > 0) {
        s = std::max(0.6f, (avail_w - (count - 1) * base_gap) / (count * base_d));
    }
    float slot_d = base_d * s;
    float icon_sz = slot_d * 0.62f;
    float row_w = count * slot_d + (count - 1) * base_gap;
    float left_pad = std::max(0.0f, (avail_w - row_w) * 0.5f);
    float gap = base_gap;

    ImGui::BeginGroup();
    ImVec2 start = ImGui::GetCursorPos();
    for (int i = 0; i < count; ++i) {
        ImGui::SetCursorPos(ImVec2(start.x + left_pad + i * (slot_d + gap), start.y));
        ImGui::PushID(i);
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##subico", ImVec2(slot_d, slot_d));
        bool clicked = ImGui::IsItemClicked();

        ImDrawList* dl = ImGui::GetWindowDrawList();
        float r = slot_d * 0.5f;
        ImVec2 c = ImVec2(p.x + r, p.y + r);
        dl->AddCircleFilled(c, r, (activeIndex == i) ? bg_sel : bg_nrm, 48);
        dl->AddCircle(c, r, (activeIndex == i) ? brd_sel : brd_nrm, 48, 1.0f);

        ImVec2 icon_pos = ImVec2(c.x - icon_sz * 0.5f, c.y - icon_sz * 0.5f);
        ImGui::SetCursorScreenPos(icon_pos);
        Icon(icons[i], icon_sz, ImVec4(1,1,1,1));

        if (clicked) activeIndex = i;
        ImGui::PopID();
    }
    ImGui::EndGroup();

    return activeIndex != originalIndex;
}

template<typename IconSelectorFn, typename ClickCallbackFn>
inline bool DrawSubTabsEx(const char** icons, int count, int& activeIndex,
                          IconSelectorFn iconSelector, ClickCallbackFn clickCallback,
                          bool useCustomAccent = false, ImVec4 customAccent = ImVec4(0.95f, 0.26f, 0.21f, 1.0f)) {
    using namespace tsm::ui::widgets;

    int originalIndex = activeIndex;

    ImGui::Dummy(ImVec2(0, 10.0f));

    const float base_d   = DP(44.0f);
    const ImVec4 acc     = useCustomAccent ? customAccent : tsm::ui::GetAccentColor();
    const ImU32  bg_sel  = ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 0.28f));
    const ImU32  brd_sel = ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 0.70f));
    const ImU32  bg_nrm  = ImGui::GetColorU32(ImVec4(1,1,1,0.10f));
    const ImU32  brd_nrm = ImGui::GetColorU32(ImVec4(1,1,1,0.18f));

    float avail_w = ImGui::GetContentRegionAvail().x;
    float base_gap = ImGui::GetStyle().ItemSpacing.x;
    float s = 1.0f;
    float need_w = count * base_d + (count - 1) * base_gap;
    if (need_w > avail_w && count > 0) {
        s = std::max(0.6f, (avail_w - (count - 1) * base_gap) / (count * base_d));
    }
    float slot_d = base_d * s;
    float icon_sz = slot_d * 0.62f;
    float row_w = count * slot_d + (count - 1) * base_gap;
    float left_pad = std::max(0.0f, (avail_w - row_w) * 0.5f);
    float gap = base_gap;

    ImGui::BeginGroup();
    ImVec2 start = ImGui::GetCursorPos();
    for (int i = 0; i < count; ++i) {
        ImGui::SetCursorPos(ImVec2(start.x + left_pad + i * (slot_d + gap), start.y));
        ImGui::PushID(i);
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##subico", ImVec2(slot_d, slot_d));
        bool clicked = ImGui::IsItemClicked();

        ImDrawList* dl = ImGui::GetWindowDrawList();
        float r = slot_d * 0.5f;
        ImVec2 c = ImVec2(p.x + r, p.y + r);
        dl->AddCircleFilled(c, r, (activeIndex == i) ? bg_sel : bg_nrm, 48);
        dl->AddCircle(c, r, (activeIndex == i) ? brd_sel : brd_nrm, 48, 1.0f);

        const char* iconName = iconSelector(i, activeIndex);
        ImVec2 icon_pos = ImVec2(c.x - icon_sz * 0.5f, c.y - icon_sz * 0.5f);
        ImGui::SetCursorScreenPos(icon_pos);
        Icon(iconName, icon_sz, ImVec4(1,1,1,1));

        if (clicked) {
            clickCallback(i, activeIndex);
        }
        ImGui::PopID();
    }
    ImGui::EndGroup();

    return activeIndex != originalIndex;
}

}}}
