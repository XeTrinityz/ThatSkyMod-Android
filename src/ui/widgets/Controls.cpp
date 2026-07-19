
#include <ui/widgets/Controls.h>
#include <ui/widgets/WidgetInternal.h>
#include <ui/core/metrics.h>
#include <ui/core/Localization.h>
#include <imgui/imgui.h>
#include <algorithm>
#include <cstdio>

namespace tsm { namespace ui { namespace widgets {


void SectionHeader(const char* title) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.80f,0.82f,0.88f,1.0f));
    ImGui::TextUnformatted(tsm::ui::i18n::Tr(title));
    ImGui::PopStyleColor();
    ImGui::Separator();
}

void CenterSeparator(const char* text) {
    if (!text || !*text) return;
    ImGui::Dummy(ImVec2(0, DP(4.0f)));

    float cont_min_x = ImGui::GetWindowContentRegionMin().x;
    float cont_max_x = ImGui::GetWindowContentRegionMax().x;
    float cont_w = std::max(0.0f, cont_max_x - cont_min_x);
    const char* display = tsm::ui::i18n::Tr(text);
    ImVec2 ts = ImGui::CalcTextSize(display);
    float x = cont_min_x + std::max(0.0f, (cont_w - ts.x) * 0.5f);

    float y = ImGui::GetCursorPosY();
    ImGui::SetCursorPos(ImVec2(x, y));
    ImGui::PushStyleColor(ImGuiCol_Text, kMutedText());
    ImGui::TextUnformatted(display);
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, DP(4.0f)));
}

void CenteredText(const char* text) {
    if (!text || !*text) return;

    float cont_min_x = ImGui::GetWindowContentRegionMin().x;
    float cont_max_x = ImGui::GetWindowContentRegionMax().x;
    float cont_w = std::max(0.0f, cont_max_x - cont_min_x);
    const char* display = tsm::ui::i18n::Tr(text);
    ImVec2 ts = ImGui::CalcTextSize(display);
    float x = cont_min_x + std::max(0.0f, (cont_w - ts.x) * 0.5f);

    float y = ImGui::GetCursorPosY();
    ImGui::SetCursorPos(ImVec2(x, y));
    ImGui::TextUnformatted(display);
}

void PaddedTextDisabled(const char* text) {
    if (!text || !*text) return;
    const char* display = tsm::ui::i18n::Tr(text);
    float start_y = ImGui::GetCursorPosY();
    float min_x = ImGui::GetWindowContentRegionMin().x + GetCardPadding();
    float max_x = ImGui::GetWindowContentRegionMax().x - GetCardPadding();
    float avail_width = max_x - min_x;

    ImGui::SetCursorPos(ImVec2(min_x, start_y));
    ImGui::PushStyleColor(ImGuiCol_Text, kMutedText());
    ImGui::PushTextWrapPos(max_x);

    ImVec2 text_size = ImGui::CalcTextSize(display, nullptr, false, avail_width);
    float center_x = min_x + std::max(0.0f, (avail_width - text_size.x) * 0.5f);
    ImGui::SetCursorPos(ImVec2(center_x, start_y));

    ImGui::TextUnformatted(display);
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
}

void PaddedText(const char* text) {
    if (!text || !*text) return;
    const char* display = tsm::ui::i18n::Tr(text);
    float start_y = ImGui::GetCursorPosY();
    float min_x = ImGui::GetWindowContentRegionMin().x + GetCardPadding();
    float max_x = ImGui::GetWindowContentRegionMax().x - GetCardPadding();
    float avail_width = max_x - min_x;

    ImGui::SetCursorPos(ImVec2(min_x, start_y));
    ImGui::PushTextWrapPos(max_x);

    ImVec2 text_size = ImGui::CalcTextSize(display, nullptr, false, avail_width);
    float center_x = min_x + std::max(0.0f, (avail_width - text_size.x) * 0.5f);
    ImGui::SetCursorPos(ImVec2(center_x, start_y));

    ImGui::TextUnformatted(display);
    ImGui::PopTextWrapPos();
}


bool LabeledSliderInt(const char* label, int* v, int v_min, int v_max) {
    ImGui::PushID(label);
    bool changed = false;

    const float top_pad    = DP(4.0f);
    const float bottom_pad = DP(4.0f);
    const float start_x = ImGui::GetCursorPosX();
    const float start_y = ImGui::GetCursorPosY();
    const float cont_max_x = ImGui::GetWindowContentRegionMax().x;
    const float slider_w   = std::max(0.0f, cont_max_x - start_x);

    ImGui::SetCursorPosY(start_y + top_pad);
    ImGui::SetCursorPosX(start_x);
    ImGui::PushItemWidth(slider_w);
    changed |= ImGui::SliderInt("##slider", v, v_min, v_max, "");
    ImVec2 smin = ImGui::GetItemRectMin();
    ImVec2 smax = ImGui::GetItemRectMax();
    float row_h = smax.y - smin.y;
    ImGui::PopItemWidth();

    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 lts = ImGui::CalcTextSize(label);
        float l_x = smin.x + DP(8.0f);
        float l_y = smin.y + (row_h - lts.y) * 0.5f;
        dl->AddText(ImVec2(l_x, l_y), ImGui::GetColorU32(kMutedText()), label);

        char vbuf[32];
        std::snprintf(vbuf, sizeof(vbuf), "%d", *v);
        ImVec2 vts = ImGui::CalcTextSize(vbuf);
        float v_x = smax.x - DP(8.0f) - vts.x;
        float v_y = smin.y + (row_h - vts.y) * 0.5f;
        dl->AddText(ImVec2(v_x, v_y), ImGui::GetColorU32(kMutedText()), vbuf);
    }

    ImGui::SetCursorPos(ImVec2(start_x, start_y + top_pad + row_h + bottom_pad));
    ImGui::PopID();
    return changed;
}

bool LabeledSliderFloat(const char* label, float* v, float v_min, float v_max, const char* fmt) {
    ImGui::PushID(label);
    bool changed = false;

    const float top_pad    = DP(4.0f);
    const float bottom_pad = DP(4.0f);
    const float start_x = ImGui::GetCursorPosX();
    const float start_y = ImGui::GetCursorPosY();
    const float cont_max_x = ImGui::GetWindowContentRegionMax().x;
    const float slider_w   = std::max(0.0f, cont_max_x - start_x);

    ImGui::SetCursorPosY(start_y + top_pad);
    ImGui::SetCursorPosX(start_x);
    ImGui::PushItemWidth(slider_w);
    changed |= ImGui::SliderFloat("##slider", v, v_min, v_max, "");
    ImVec2 smin = ImGui::GetItemRectMin();
    ImVec2 smax = ImGui::GetItemRectMax();
    float row_h = smax.y - smin.y;
    ImGui::PopItemWidth();

    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 lts = ImGui::CalcTextSize(label);
        float l_x = smin.x + DP(8.0f);
        float l_y = smin.y + (row_h - lts.y) * 0.5f;
        dl->AddText(ImVec2(l_x, l_y), ImGui::GetColorU32(kMutedText()), label);

        char vbuf[32];
        if (!fmt) fmt = "%.1f";
        std::snprintf(vbuf, sizeof(vbuf), fmt, *v);
        ImVec2 vts = ImGui::CalcTextSize(vbuf);
        float v_x = smax.x - DP(8.0f) - vts.x;
        float v_y = smin.y + (row_h - vts.y) * 0.5f;
        dl->AddText(ImVec2(v_x, v_y), ImGui::GetColorU32(kMutedText()), vbuf);
    }

    ImGui::SetCursorPos(ImVec2(start_x, start_y + top_pad + row_h + bottom_pad));
    ImGui::PopID();
    return changed;
}

}}}
