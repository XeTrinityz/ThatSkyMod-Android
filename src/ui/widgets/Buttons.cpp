
#include <ui/widgets/Buttons.h>
#include <ui/widgets/Icons.h>
#include <ui/widgets/Layout.h>
#include <ui/widgets/WidgetInternal.h>
#include <ui/core/Localization.h>
#include <iconloader/IconLoader.h>
#include <ui/core/metrics.h>
#include <ui/helpers/Animations.h>
#include <ui/helpers/VisualEffects.h>
#include <imgui/imgui.h>
#include <unordered_map>
#include <cstdio>
#include <cmath>

namespace tsm { namespace ui { namespace widgets {

bool AccentButton(const char* label, const ImVec2& size) {
    const char* localized = tsm::ui::i18n::Tr(label);
    ImGui::PushID(localized);
    ImVec4 acc = GetAccentColor();

    using namespace tsm::ui::helpers;
    static std::unordered_map<std::string, AnimatedFloat> s_button_scales;
    ImGui::PushStyleColor(ImGuiCol_Button,        acc);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, acc);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(acc.x - 0.02f, acc.y - 0.02f, acc.z - 0.02f, acc.w));

    bool pressed = ImGui::Button(localized, size);

    ImGui::PopStyleColor(3);
    ImGui::PopID();
    return pressed;
}

bool SecondaryButton(const char* label, const ImVec2& size) {
    const char* localized = tsm::ui::i18n::Tr(label);
    ImVec4 bg   = kInactive();
    ImVec4 hov  = ImVec4(bg.x + 0.03f, bg.y + 0.03f, bg.z + 0.03f, 1.0f);
    ImVec4 act  = ImVec4(bg.x + 0.01f, bg.y + 0.01f, bg.z + 0.01f, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, bg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, act);
    ImGui::PushStyleColor(ImGuiCol_Border, kBorder());
    bool pressed = ImGui::Button(localized, size);
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();
    return pressed;
}

bool StandardButton(const char* label, bool topSpacer) {
    const char* localized = tsm::ui::i18n::Tr(label);
    ImGui::PushID(localized);

    if (topSpacer) {
        ImGui::Dummy(ImVec2(0, DP(0.3f)));
    }

    const float frame_h   = ImGui::GetFrameHeight();
    const float pad_y     = DP(6.0f);
    const float min_row_h = DP(36.0f);
    const float row_h     = std::max(frame_h + pad_y * 2.0f, min_row_h);

    const float avail_w = ImGui::GetContentRegionAvail().x;
    const bool clicked = ImGui::InvisibleButton("##stdbtn", ImVec2(avail_w, row_h));
    const bool active  = ImGui::IsItemActive();

    static std::unordered_map<ImGuiID, float> s_pressScrollY_std;
    ImGuiID stdId = ImGui::GetID("##stdbtn");
    if (ImGui::IsItemActivated()) {
        s_pressScrollY_std[stdId] = ImGui::GetScrollY();
    }

    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    float s_cardPad = GetCardPadding();
    ImVec2 bg_min = ImVec2(min.x + s_cardPad, min.y);
    ImVec2 bg_max = ImVec2(max.x - s_cardPad, max.y);
    ImVec4 acc = GetAccentColor();
    float alpha = active ? 0.95f : 0.65f;
    ImU32 bgc = ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, alpha));
    float r = DP(8.0f);
    dl->AddRectFilled(bg_min, bg_max, bgc, r);

    ImVec2 ts = ImGui::CalcTextSize(localized);
    float cx = bg_min.x + (bg_max.x - bg_min.x - ts.x) * 0.5f;
    float cy = min.y + (row_h - ts.y) * 0.5f;
    dl->AddText(ImVec2(cx, cy), ImGui::GetColorU32(ImVec4(1,1,1,1)), localized);

    ImGui::SetCursorScreenPos(ImVec2(min.x, max.y));
    bool acceptClick = clicked;
    auto itPress = s_pressScrollY_std.find(stdId);
    if (acceptClick && itPress != s_pressScrollY_std.end()) {
        float dy = fabsf(ImGui::GetScrollY() - itPress->second);
        if (dy > DP(10.0f)) acceptClick = false;
    }
    if (ImGui::IsItemDeactivated()) {
        s_pressScrollY_std.erase(stdId);
    }

    ImGui::Dummy(ImVec2(0, DP(0.3f)));
    ImGui::PopID();
    return acceptClick;
}

bool PillTab(const char* label, bool active, float padX, float padY) {
    ImGui::PushID(label);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 999.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(padX, padY));
    if (active) {
        ImVec4 acc = GetAccentColor();
        ImGui::PushStyleColor(ImGuiCol_Button, acc);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(acc.x + 0.05f, acc.y + 0.05f, acc.z + 0.05f, acc.w));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(acc.x - 0.02f, acc.y - 0.02f, acc.z - 0.02f, acc.w));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,1,1,1));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.10f,0.12f,0.12f,0.5f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f,0.12f,0.12f,0.8f));
        ImGui::PushStyleColor(ImGuiCol_Text, kMutedText());
    }
    bool clicked = ImGui::Button(label);
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
    ImGui::PopID();
    return clicked;
}

void HeaderBadge(const char* iconLabel, const char* title, const char* subtitle) {
    if (BeginCard("HeaderBadge", 10.0f)) {
        const float s = DP(18.0f);
        UIIcon icon{};
        IconLoader::getUIIcon(iconLabel, &icon);
        if (icon.textureId != IL_NO_TEXTURE) {
            IconLoader::icon(iconLabel, s);
        } else {
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("##hdrico", ImVec2(s, s));
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddCircleFilled(ImVec2(p.x + s*0.5f, p.y + s*0.5f), s*0.3f, ImGui::GetColorU32(GetAccentColor()), 24);
        }
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextUnformatted(title);
        if (subtitle && *subtitle) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.68f,0.72f,0.78f,1.0f));
            ImGui::TextUnformatted(subtitle);
            ImGui::PopStyleColor();
        }
        ImGui::EndGroup();
        EndCard();
    }
}

bool NumberCircleButton(int number, bool active, float diameterDp, const ImVec4& color) {
    ImGui::PushID(number);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float d = DP(diameterDp);
    const float r = d * 0.5f;
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton("##numbtn", ImVec2(d, d));

    bool hasCustomColor = (color.w > 0.001f);
    ImVec4 baseColor = hasCustomColor ? color : GetAccentColor();

    float fill_alpha = active ? 0.28f : 0.10f;
    float border_alpha = active ? 0.70f : 0.18f;

    ImVec4 bg_fill, bg_brd;
    if (hasCustomColor || active) {
        bg_fill = ImVec4(baseColor.x, baseColor.y, baseColor.z, fill_alpha);
        bg_brd  = ImVec4(baseColor.x, baseColor.y, baseColor.z, border_alpha);
    } else {
        bg_fill = ImVec4(1, 1, 1, fill_alpha);
        bg_brd  = ImVec4(1, 1, 1, border_alpha);
    }

    ImVec2 c = ImVec2(p.x + r, p.y + r);

    ImU32 fill = ImGui::GetColorU32(bg_fill);
    ImU32 brd  = ImGui::GetColorU32(bg_brd);
    dl->AddCircleFilled(c, r, fill, 48);
    dl->AddCircle(c, r, brd, 48, 1.5f);

    char buf[8];
    std::snprintf(buf, sizeof(buf), "%d", number);
    ImVec2 ts = ImGui::CalcTextSize(buf);
    ImU32 tc = ImGui::GetColorU32(active ? ImVec4(1,1,1,1) : ImVec4(0.90f,0.92f,0.96f,1));
    dl->AddText(ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), tc, buf);
    ImGui::PopID();
    return clicked;
}

bool ColorCircleButton(const ImVec4& color, float diameterDp) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float d = DP(diameterDp);
    const float r = d * 0.5f;
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##colorbtn", ImVec2(d, d));

    static std::unordered_map<ImGuiID, float> s_scrollY;
    ImGuiID btnId = ImGui::GetID("##colorbtn");
    bool acceptClick = false;

    if (ImGui::IsItemActivated()) {
        s_scrollY[btnId] = ImGui::GetScrollY();
    }

    if (ImGui::IsItemDeactivated()) {
        auto itScroll = s_scrollY.find(btnId);
        if (itScroll != s_scrollY.end()) {
            float scrollDelta = fabsf(ImGui::GetScrollY() - itScroll->second);
            if (scrollDelta <= DP(5.0f)) {
                acceptClick = true;
            }
            s_scrollY.erase(btnId);
        }
    }

    ImVec2 c = ImVec2(p.x + r, p.y + r);
    ImU32 fill = ImGui::GetColorU32(color);
    ImU32 border = ImGui::GetColorU32(ImVec4(1,1,1,0.28f));
    dl->AddCircleFilled(c, r, fill, 48);
    dl->AddCircle(c, r, border, 48, 1.0f);

    return acceptClick;
}

CycleClick CycleCard(const char* id,
                     const char* leftIcon,
                     const char* centerIcon,
                     const char* rightIcon,
                     float iconSizeDp) {
    ImGui::PushID(id);
    CycleClick result = CycleClick::None;

    const float icon_sz   = DP(iconSizeDp);
    const float top_pad   = DP(6.0f);
    const float bot_pad   = DP(6.0f);
    const float min_row_h = DP(36.0f);
    const float row_h     = std::max(icon_sz + top_pad + bot_pad, min_row_h);

    const float avail_w = ImGui::GetContentRegionAvail().x;
    ImGui::Dummy(ImVec2(avail_w, row_h));

    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    const float y = min.y + (row_h - icon_sz) * 0.5f;

    float row_min_x = min.x;
    float row_max_x = max.x;
    ImGuiStyle& style = ImGui::GetStyle();
    float btn_w = icon_sz + style.FramePadding.x * 2.0f;
    float s_cardPad = GetCardPadding();
    float inner_w = (row_max_x - row_min_x) - s_cardPad * 2.0f;
    float left_x   = row_min_x + s_cardPad;
    float right_x  = row_max_x - s_cardPad - btn_w;
    float center_x = row_min_x + s_cardPad + (inner_w - btn_w) * 0.5f;

    {
        ImGui::SetCursorScreenPos(ImVec2(left_x, y));
        bool clicked = IconButton(leftIcon ? leftIcon : "UiMiscArrowLeft", icon_sz, ImVec4(1,1,1,1));
        if (clicked) result = CycleClick::Prev;
    }

    {
        ImGui::SetCursorScreenPos(ImVec2(center_x, y));
        bool clicked = IconButton(centerIcon ? centerIcon : "UiMiscTarget", icon_sz, ImVec4(1,1,1,1));
        if (clicked) result = CycleClick::Center;
    }

    {
        ImGui::SetCursorScreenPos(ImVec2(right_x, y));
        bool clicked = IconButton(rightIcon ? rightIcon : "UiMiscArrowRight", icon_sz, ImVec4(1,1,1,1));
        if (clicked) result = CycleClick::Next;
    }

    ImGui::SetCursorScreenPos(ImVec2(min.x, max.y));
    ImGui::PopID();
    return result;
}

}}}
