
#include <ui/widgets/Cards.h>
#include <ui/widgets/Icons.h>
#include <ui/widgets/Layout.h>
#include <ui/widgets/WidgetInternal.h>
#include <iconloader/IconLoader.h>
#include <ui/core/App.h>
#include <ui/core/metrics.h>
#include <ui/core/Localization.h>
#include <ui/helpers/Animations.h>
#include <ui/helpers/VisualEffects.h>
#include <imgui/imgui.h>
#include <unordered_map>
#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <string>
#include <cmath>
#include <vector>

namespace tsm { namespace ui { namespace widgets {


ImVec4 GetAccentColor() { return tsm::ui::GetAccentColor(); }

void TextEllipsisSingleLine(const char* text, float max_w) {
    if (!text || !*text || max_w <= 0.0f) return;
    const char* localized = tsm::ui::i18n::Tr(text);
    ImVec2 full = ImGui::CalcTextSize(localized);
    if (full.x <= max_w) {
        ImGui::TextUnformatted(localized);
        return;
    }
    const char* ell = "...";
    ImVec2 ell_sz = ImGui::CalcTextSize(ell);
    float target = std::max(0.0f, max_w - ell_sz.x);
    if (target <= 0.0f) { ImGui::TextUnformatted(ell); return; }
    int lo = 0; int hi = 0; while (localized[hi] != '\0') ++hi;
    while (lo < hi) {
        int mid = lo + (hi - lo + 1) / 2;
        ImVec2 ts = ImGui::CalcTextSize(localized, localized + mid);
        if (ts.x <= target) lo = mid; else hi = mid - 1;
    }
    std::string out(localized, localized + lo);
    out += ell;
    ImGui::TextUnformatted(out.c_str());
}

bool ToggleCard(const char* title, const char* description, bool* v) {
    ImGui::PushID(title);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    float s_cardPad = GetCardPadding();
    float cont_w = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
    float h_ctrl = ImGui::GetFrameHeight() * 0.9f;
    float switch_w = h_ctrl * 1.9f;
    float label_w = std::max(0.0f, cont_w - (s_cardPad * 2.0f + switch_w));
    float top_pad = DP(8.0f);
    float bottom_pad = DP(8.0f);
    float between = DP(2.0f);
    float title_h = ImGui::GetTextLineHeight();
    float desc_h = (description && *description) ? ImGui::GetTextLineHeight() : 0.0f;
    float text_h = top_pad + title_h + (desc_h > 0.0f ? (between + desc_h) : 0.0f) + bottom_pad;
    float row_h = std::max(text_h, h_ctrl + DP(12.0f));

    float avail_w = ImGui::GetContentRegionAvail().x;

    float card_button_w = avail_w - switch_w - s_cardPad;

    ImVec2 full_min = ImGui::GetCursorScreenPos();
    ImVec2 full_max = ImVec2(full_min.x + avail_w, full_min.y + row_h);

    ImGui::InvisibleButton("##card", ImVec2(card_button_w, row_h));
    bool cardClicked = ImGui::IsItemClicked();
    bool active = ImGui::IsItemActive();

    ImVec2 min = full_min;
    ImVec2 max = full_max;

    static std::unordered_map<ImGuiID, float> s_pressScrollY_toggle;
    ImGuiID cardId = ImGui::GetID("##card");
    if (ImGui::IsItemActivated()) {
        s_pressScrollY_toggle[cardId] = ImGui::GetScrollY();
    }

    using namespace tsm::ui::helpers;
    ImGuiID uniqueId = cardId;
    auto& bg_anim = GetAnimatedFloat((void*)(intptr_t)uniqueId, *v ? 0.18f : 0.0f);

    bg_anim.SetTarget(*v ? 0.18f : 0.0f);
    float bg_alpha = bg_anim.Update(ImGui::GetIO().DeltaTime);

    ImVec4 acc = GetAccentColor();

    if (bg_alpha > 0.001f) {
        ImVec4 bg_on = ImVec4(acc.x, acc.y, acc.z, bg_alpha);
        dl->AddRectFilled(min, max, ImGui::GetColorU32(bg_on), 0.0f);
    }

    auto& hold_anim = GetAnimatedFloat((void*)(intptr_t)(uniqueId + 5000), 0.0f);
    float hold_target = active ? 0.15f : 0.0f;
    hold_anim.SetTarget(hold_target);
    float hold_alpha = hold_anim.Update(ImGui::GetIO().DeltaTime);

    if (hold_alpha > 0.001f) {
        ImVec4 hold_bg = ImVec4(acc.x, acc.y, acc.z, hold_alpha);
        dl->AddRectFilled(min, max, ImGui::GetColorU32(hold_bg), 0.0f);
    }

    void* ripple_id = (void*)(intptr_t)(uniqueId + 1000);
    if (ImGui::IsItemActivated()) {
        StartRipple(ripple_id, ImGui::GetMousePos());
    }
    UpdateAndDrawRipples(ripple_id, dl, min, max, GetAccentColor(), 0.0f);

    float text_x = min.x + s_cardPad;
    float title_y = min.y + top_pad;
    ImGui::SetCursorScreenPos(ImVec2(text_x, title_y));
    TextEllipsisSingleLine(title, label_w);
    if (description && *description) {
        ImGui::PushStyleColor(ImGuiCol_Text, kMutedText());
        float desc_y = title_y + title_h + between;
        ImGui::SetCursorScreenPos(ImVec2(text_x, desc_y));
        ImGui::SetWindowFontScale(0.70f);
        TextEllipsisSingleLine(description, label_w);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
    }

    float h = h_ctrl;
    const float sw_w = h * 1.9f;
    const float sw_r = h * 0.5f;
    float x_local = ImGui::GetWindowContentRegionMax().x - s_cardPad - sw_w;
    float y_center = min.y + (row_h - h) * 0.5f;
    ImVec2 switch_pos = ImVec2(min.x + x_local, y_center);

    ImGui::SetCursorScreenPos(switch_pos);
    bool switchClicked = ImGui::InvisibleButton("##switch", ImVec2(sw_w, h));

    ImGuiID switchId = ImGui::GetID("##switch");
    if (ImGui::IsItemActivated()) {
        s_pressScrollY_toggle[switchId] = ImGui::GetScrollY();
    }

    bool switchHovered = ImGui::IsItemHovered();

    static std::unordered_map<const void*, float> s_anim;
    static std::unordered_map<const void*, float> s_pulse;
    if (s_anim.find(v) == s_anim.end()) s_anim[v] = *v ? 1.0f : 0.0f;
    if (s_pulse.find(v) == s_pulse.end()) s_pulse[v] = 0.0f;

    float& t = s_anim[v];
    float& pulse = s_pulse[v];
    if (t != t) t = 0.0f;
    float target = *v ? 1.0f : 0.0f;
    float speed = 15.0f;
    t += (target - t) * std::min(1.0f, speed * ImGui::GetIO().DeltaTime);
    if (t < 0.001f) t = 0.0f; else if (t > 0.999f) t = 1.0f;

    if (pulse > 0.0f) {
        pulse -= ImGui::GetIO().DeltaTime * 3.0f;
        if (pulse < 0.0f) pulse = 0.0f;
    }

    ImVec2 sw_min = ImGui::GetItemRectMin();
    ImVec2 sw_max = ImGui::GetItemRectMax();

    ImVec4 on  = ImVec4(acc.x, acc.y, acc.z, 0.95f);
    ImVec4 off = ImVec4(0.28f, 0.31f, 0.36f, 0.85f);
    ImVec4 bgv = ImVec4(off.x + (on.x - off.x) * t,
                        off.y + (on.y - off.y) * t,
                        off.z + (on.z - off.z) * t,
                        off.w + (on.w - off.w) * t);

    if (t > 0.1f) {
        float glow_strength = t * (0.4f + pulse * 0.3f);
        ShadowConfig glow = ShadowConfig::Glow(acc);
        glow.blur = 10.0f + pulse * 8.0f;
        glow.color.w = glow_strength;
        DrawShadow(dl, sw_min, sw_max, glow, h * 0.5f);
    }

    ImU32 bg = ImGui::GetColorU32(bgv);
    dl->AddRectFilled(sw_min, sw_max, bg, h * 0.5f);

    float left_cx  = sw_min.x + sw_r;
    float right_cx = sw_max.x - sw_r;
    float cx = left_cx + (right_cx - left_cx) * t;
    float cy = (sw_min.y + sw_max.y) * 0.5f;

    float knob_scale = 1.0f + pulse * 0.15f;
    float knob_r = (sw_r - 2.0f) * knob_scale;

    DrawCircleShadow(dl, ImVec2(cx, cy + 1.0f), knob_r, ShadowConfig{{0, 1}, 3.0f, ImVec4(0,0,0,0.3f), ShadowStyle::Soft});

    ImU32 knob = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    dl->AddCircleFilled(ImVec2(cx, cy), knob_r, knob, 32);

    if (t > 0.5f) {
        float check_alpha = (t - 0.5f) * 2.0f;
        ImU32 check_color = ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, check_alpha));
        float check_size = sw_r * 0.4f;
        ImVec2 check_center = ImVec2(left_cx + (right_cx - left_cx) * 0.25f, cy);
        dl->AddLine(ImVec2(check_center.x - check_size * 0.3f, check_center.y),
                   ImVec2(check_center.x - check_size * 0.05f, check_center.y + check_size * 0.3f),
                   check_color, 2.0f);
        dl->AddLine(ImVec2(check_center.x - check_size * 0.05f, check_center.y + check_size * 0.3f),
                   ImVec2(check_center.x + check_size * 0.4f, check_center.y - check_size * 0.3f),
                   check_color, 2.0f);
    }

    bool acceptCardClick = cardClicked;
    auto itCardPress = s_pressScrollY_toggle.find(cardId);
    if (acceptCardClick && itCardPress != s_pressScrollY_toggle.end()) {
        float dy = fabsf(ImGui::GetScrollY() - itCardPress->second);
        if (dy > DP(10.0f)) acceptCardClick = false;
    }
    if (ImGui::IsItemDeactivated()) {
        s_pressScrollY_toggle.erase(cardId);
    }

    bool acceptSwitchClick = switchClicked;
    auto itSwitchPress = s_pressScrollY_toggle.find(switchId);
    if (acceptSwitchClick && itSwitchPress != s_pressScrollY_toggle.end()) {
        float dy = fabsf(ImGui::GetScrollY() - itSwitchPress->second);
        if (dy > DP(10.0f)) acceptSwitchClick = false;
    }
    if (ImGui::IsItemDeactivated()) {
        s_pressScrollY_toggle.erase(switchId);
    }

    if (acceptSwitchClick) {
        *v = !*v;
        s_pulse[v] = 1.0f;
    } else if (acceptCardClick && !switchHovered) {
        *v = !*v;
    }

    ImGui::SetCursorScreenPos(ImVec2(min.x, max.y));
    ImGui::PopID();
    return acceptCardClick || acceptSwitchClick;
}

bool ToggleCardIcon(const char* title, const char* description, const char* iconLabel, bool* v) {
    ImGui::PushID(title);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    float s_cardPad = GetCardPadding();
    float cont_w = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
    float h_ctrl = ImGui::GetFrameHeight() * 0.9f;
    float switch_w = h_ctrl * 1.9f;
    const float icon_sz = DP(24.0f);
    const float icon_gap = DP(8.0f);
    const bool  has_icon = (iconLabel && *iconLabel);
    float label_w = std::max(0.0f, cont_w - (s_cardPad * 2.0f + switch_w + (has_icon ? (icon_sz + icon_gap) : 0.0f)));
    float top_pad = DP(8.0f);
    float bottom_pad = DP(8.0f);
    float between = DP(2.0f);
    float title_h = ImGui::GetTextLineHeight();
    float desc_h = (description && *description) ? ImGui::GetTextLineHeight() : 0.0f;
    float text_h = top_pad + title_h + (desc_h > 0.0f ? (between + desc_h) : 0.0f) + bottom_pad;
    float row_h = std::max(text_h, h_ctrl + DP(12.0f));

    float avail_w = ImGui::GetContentRegionAvail().x;

    float card_button_w = avail_w - switch_w - s_cardPad;

    ImVec2 full_min = ImGui::GetCursorScreenPos();
    ImVec2 full_max = ImVec2(full_min.x + avail_w, full_min.y + row_h);

    bool cardClicked = ImGui::InvisibleButton("##card", ImVec2(card_button_w, row_h));
    bool active = ImGui::IsItemActive();

    ImVec2 min = full_min;
    ImVec2 max = full_max;

    static std::unordered_map<ImGuiID, float> s_pressScrollY_toggle;
    ImGuiID cardId = ImGui::GetID("##card");
    if (ImGui::IsItemActivated()) {
        s_pressScrollY_toggle[cardId] = ImGui::GetScrollY();
    }

    using namespace tsm::ui::helpers;
    ImGuiID uniqueId = cardId;
    auto& bg_anim = GetAnimatedFloat((void*)(intptr_t)uniqueId, *v ? 0.18f : 0.0f);

    bg_anim.SetTarget(*v ? 0.18f : 0.0f);
    float bg_alpha = bg_anim.Update(ImGui::GetIO().DeltaTime);

    ImVec4 acc = GetAccentColor();

    if (bg_alpha > 0.001f) {
        ImVec4 bg_on = ImVec4(acc.x, acc.y, acc.z, bg_alpha);
        dl->AddRectFilled(min, max, ImGui::GetColorU32(bg_on), 0.0f);
    }

    auto& hold_anim = GetAnimatedFloat((void*)(intptr_t)(uniqueId + 5000), 0.0f);
    float hold_target = active ? 0.15f : 0.0f;
    hold_anim.SetTarget(hold_target);
    float hold_alpha = hold_anim.Update(ImGui::GetIO().DeltaTime);

    if (hold_alpha > 0.001f) {
        ImVec4 hold_bg = ImVec4(acc.x, acc.y, acc.z, hold_alpha);
        dl->AddRectFilled(min, max, ImGui::GetColorU32(hold_bg), 0.0f);
    }

    void* ripple_id = (void*)(intptr_t)(uniqueId + 1000);
    if (ImGui::IsItemActivated()) {
        StartRipple(ripple_id, ImGui::GetMousePos());
    }
    UpdateAndDrawRipples(ripple_id, dl, min, max, acc, 0.0f);

    float cursor_x = min.x + s_cardPad;
    float icon_y = min.y + (row_h - icon_sz) * 0.5f;
    if (has_icon) {
        ImGui::SetCursorScreenPos(ImVec2(cursor_x, icon_y));
        Icon(iconLabel, icon_sz, ImVec4(1,1,1,1));
        cursor_x += icon_sz + icon_gap;
    }

    float text_x = cursor_x;
    float title_y = min.y + top_pad;
    ImGui::SetCursorScreenPos(ImVec2(text_x, title_y));
    TextEllipsisSingleLine(title, label_w);
    if (description && *description) {
        ImGui::PushStyleColor(ImGuiCol_Text, kMutedText());
        float desc_y = title_y + title_h + between;
        ImGui::SetCursorScreenPos(ImVec2(text_x, desc_y));
        ImGui::SetWindowFontScale(0.70f);
        TextEllipsisSingleLine(description, label_w);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
    }

    float h = h_ctrl;
    const float sw_w = h * 1.9f;
    const float sw_r = h * 0.5f;
    float x_local = ImGui::GetWindowContentRegionMax().x - s_cardPad - sw_w;
    float y_center = min.y + (row_h - h) * 0.5f;
    ImVec2 switch_pos = ImVec2(min.x + x_local, y_center);

    ImGui::SetCursorScreenPos(switch_pos);
    bool switchClicked = ImGui::InvisibleButton("##switch", ImVec2(sw_w, h));

    ImGuiID switchId = ImGui::GetID("##switch");
    if (ImGui::IsItemActivated()) {
        s_pressScrollY_toggle[switchId] = ImGui::GetScrollY();
    }

    bool switchHovered = ImGui::IsItemHovered();

    static std::unordered_map<const void*, float> s_anim;
    static std::unordered_map<const void*, float> s_pulse;
    if (s_anim.find(v) == s_anim.end()) s_anim[v] = *v ? 1.0f : 0.0f;
    if (s_pulse.find(v) == s_pulse.end()) s_pulse[v] = 0.0f;

    float& t = s_anim[v];
    float& pulse = s_pulse[v];
    if (t != t) t = 0.0f;
    float target = *v ? 1.0f : 0.0f;
    float speed = 15.0f;
    t += (target - t) * std::min(1.0f, speed * ImGui::GetIO().DeltaTime);
    if (t < 0.001f) t = 0.0f; else if (t > 0.999f) t = 1.0f;

    if (pulse > 0.0f) {
        pulse -= ImGui::GetIO().DeltaTime * 3.0f;
        if (pulse < 0.0f) pulse = 0.0f;
    }

    ImVec2 sw_min = ImGui::GetItemRectMin();
    ImVec2 sw_max = ImGui::GetItemRectMax();

    ImVec4 on  = ImVec4(acc.x, acc.y, acc.z, 0.95f);
    ImVec4 off = ImVec4(0.28f, 0.31f, 0.36f, 0.85f);
    ImVec4 bgv = ImVec4(off.x + (on.x - off.x) * t,
                        off.y + (on.y - off.y) * t,
                        off.z + (on.z - off.z) * t,
                        off.w + (on.w - off.w) * t);

    if (t > 0.1f) {
        float glow_strength = t * (0.4f + pulse * 0.3f);
        ShadowConfig glow = ShadowConfig::Glow(acc);
        glow.blur = 10.0f + pulse * 8.0f;
        glow.color.w = glow_strength;
        DrawShadow(dl, sw_min, sw_max, glow, h * 0.5f);
    }

    ImU32 bg = ImGui::GetColorU32(bgv);
    dl->AddRectFilled(sw_min, sw_max, bg, h * 0.5f);

    float left_cx  = sw_min.x + sw_r;
    float right_cx = sw_max.x - sw_r;
    float cx = left_cx + (right_cx - left_cx) * t;
    float cy = (sw_min.y + sw_max.y) * 0.5f;

    float knob_scale = 1.0f + pulse * 0.15f;
    float knob_r = (sw_r - 2.0f) * knob_scale;

    DrawCircleShadow(dl, ImVec2(cx, cy + 1.0f), knob_r, ShadowConfig{{0, 1}, 3.0f, ImVec4(0,0,0,0.3f), ShadowStyle::Soft});

    ImU32 knob = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    dl->AddCircleFilled(ImVec2(cx, cy), knob_r, knob, 32);

    if (t > 0.5f) {
        float check_alpha = (t - 0.5f) * 2.0f;
        ImU32 check_color = ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, check_alpha));
        float check_size = sw_r * 0.4f;
        ImVec2 check_center = ImVec2(left_cx + (right_cx - left_cx) * 0.25f, cy);
        dl->AddLine(ImVec2(check_center.x - check_size * 0.3f, check_center.y),
                   ImVec2(check_center.x - check_size * 0.05f, check_center.y + check_size * 0.3f),
                   check_color, 2.0f);
        dl->AddLine(ImVec2(check_center.x - check_size * 0.05f, check_center.y + check_size * 0.3f),
                   ImVec2(check_center.x + check_size * 0.4f, check_center.y - check_size * 0.3f),
                   check_color, 2.0f);
    }

    bool acceptCardClick = cardClicked;
    auto itCardPress = s_pressScrollY_toggle.find(cardId);
    if (acceptCardClick && itCardPress != s_pressScrollY_toggle.end()) {
        float dy = fabsf(ImGui::GetScrollY() - itCardPress->second);
        if (dy > DP(10.0f)) acceptCardClick = false;
    }
    if (ImGui::IsItemDeactivated()) {
        s_pressScrollY_toggle.erase(cardId);
    }

    bool acceptSwitchClick = switchClicked;
    auto itSwitchPress = s_pressScrollY_toggle.find(switchId);
    if (acceptSwitchClick && itSwitchPress != s_pressScrollY_toggle.end()) {
        float dy = fabsf(ImGui::GetScrollY() - itSwitchPress->second);
        if (dy > DP(10.0f)) acceptSwitchClick = false;
    }
    if (ImGui::IsItemDeactivated()) {
        s_pressScrollY_toggle.erase(switchId);
    }

    if (acceptSwitchClick) {
        *v = !*v;
        s_pulse[v] = 1.0f;
    } else if (acceptCardClick && !switchHovered) {
        *v = !*v;
    }

    ImGui::SetCursorScreenPos(ImVec2(min.x, max.y));
    ImGui::PopID();
    return acceptCardClick || acceptSwitchClick;
}

bool InputWithPlaceholder(const char* id, char* buf, size_t buf_size, const char* placeholder);

bool IntCard(const char* title, const char* description, int* v, int v_min, int v_max) {
    ImGui::PushID(title);
    float s_cardPad = GetCardPadding();
    float cont_w = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
    float h_ctrl = ImGui::GetFrameHeight() * 0.9f;
    float slider_w = std::max(DP(140.0f), cont_w * 0.35f);
    float label_w  = std::max(0.0f, cont_w - (s_cardPad * 2.0f + slider_w));
    float top_pad = DP(8.0f);
    float bottom_pad = DP(8.0f);
    float between = DP(2.0f);
    float title_h = ImGui::GetTextLineHeight();
    float desc_h  = (description && *description) ? ImGui::GetTextLineHeight() : 0.0f;
    float text_h  = top_pad + title_h + (desc_h > 0.0f ? (between + desc_h) : 0.0f) + bottom_pad;
    float row_h   = std::max(text_h, h_ctrl + DP(12.0f));
    float avail_w = ImGui::GetContentRegionAvail().x;
    ImGui::Dummy(ImVec2(avail_w, row_h));
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();

    float text_x = min.x + s_cardPad;
    float title_y = min.y + top_pad;
    ImGui::SetCursorScreenPos(ImVec2(text_x, title_y));
    TextEllipsisSingleLine(title, label_w);
    if (description && *description) {
        ImGui::PushStyleColor(ImGuiCol_Text, kMutedText());
        float desc_y = title_y + title_h + between;
        ImGui::SetCursorScreenPos(ImVec2(text_x, desc_y));
        ImGui::SetWindowFontScale(0.70f);
        TextEllipsisSingleLine(description, label_w);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
    }
    float h = h_ctrl;
    float x_local = ImGui::GetWindowContentRegionMax().x - s_cardPad - slider_w;
    float y_center = min.y + (row_h - h) * 0.5f;
    ImGui::SetCursorScreenPos(ImVec2(min.x + x_local, y_center));
    ImGui::PushItemWidth(slider_w);
    bool changed = ImGui::SliderInt("##slider", v, v_min, v_max);
    ImGui::PopItemWidth();
    ImGui::SetCursorScreenPos(ImVec2(min.x, max.y));
    ImGui::PopID();
    return changed;
}

bool FloatCard(const char* title, const char* description, float* v, float v_min, float v_max, const char* fmt) {
    ImGui::PushID(title);
    float s_cardPad = GetCardPadding();
    float cont_w = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
    float h_ctrl = ImGui::GetFrameHeight() * 0.9f;
    float slider_w = std::max(DP(140.0f), cont_w * 0.35f);
    float label_w  = std::max(0.0f, cont_w - (s_cardPad * 2.0f + slider_w));
    float top_pad = DP(8.0f);
    float bottom_pad = DP(8.0f);
    float between = DP(2.0f);
    float title_h = ImGui::GetTextLineHeight();
    float desc_h  = (description && *description) ? ImGui::GetTextLineHeight() : 0.0f;
    float text_h  = top_pad + title_h + (desc_h > 0.0f ? (between + desc_h) : 0.0f) + bottom_pad;
    float row_h   = std::max(text_h, h_ctrl + DP(12.0f));
    float avail_w = ImGui::GetContentRegionAvail().x;
    ImGui::Dummy(ImVec2(avail_w, row_h));
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    float text_x = min.x + s_cardPad;
    float title_y = min.y + top_pad;
    ImGui::SetCursorScreenPos(ImVec2(text_x, title_y));
    TextEllipsisSingleLine(title, label_w);
    if (description && *description) {
        ImGui::PushStyleColor(ImGuiCol_Text, kMutedText());
        float desc_y = title_y + title_h + between;
        ImGui::SetCursorScreenPos(ImVec2(text_x, desc_y));
        ImGui::SetWindowFontScale(0.70f);
        TextEllipsisSingleLine(description, label_w);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
    }
    float h = h_ctrl;
    float x_local = ImGui::GetWindowContentRegionMax().x - s_cardPad - slider_w;
    float y_center = min.y + (row_h - h) * 0.5f;
    ImGui::SetCursorScreenPos(ImVec2(min.x + x_local, y_center));
    ImGui::PushItemWidth(slider_w);
    if (!fmt) fmt = "%.1f";
    bool changed = ImGui::SliderFloat("##slider", v, v_min, v_max, fmt);
    ImGui::PopItemWidth();
    ImGui::SetCursorScreenPos(ImVec2(min.x, max.y));
    ImGui::PopID();
    return changed;
}

bool IntCardIcon(const char* title, const char* description, const char* iconLabel, int* v, int v_min, int v_max) {
    ImGui::PushID(title);
    float s_cardPad = GetCardPadding();
    float cont_w = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
    float h_ctrl = ImGui::GetFrameHeight() * 0.9f;
    float slider_w = std::max(DP(140.0f), cont_w * 0.35f);
    const float icon_sz = DP(24.0f);
    const float icon_gap = DP(8.0f);
    const bool  has_icon = (iconLabel && *iconLabel);
    float label_w  = std::max(0.0f, cont_w - (s_cardPad * 2.0f + slider_w + (has_icon ? (icon_sz + icon_gap) : 0.0f)));
    float top_pad = DP(8.0f);
    float bottom_pad = DP(8.0f);
    float between = DP(2.0f);
    float title_h = ImGui::GetTextLineHeight();
    float desc_h  = (description && *description) ? ImGui::GetTextLineHeight() : 0.0f;
    float text_h  = top_pad + title_h + (desc_h > 0.0f ? (between + desc_h) : 0.0f) + bottom_pad;
    float row_h   = std::max(text_h, h_ctrl + DP(12.0f));
    float avail_w = ImGui::GetContentRegionAvail().x;
    ImGui::Dummy(ImVec2(avail_w, row_h));
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    float cursor_x = min.x + s_cardPad;
    if (has_icon) {
        float icon_y = min.y + (row_h - icon_sz) * 0.5f;
        ImGui::SetCursorScreenPos(ImVec2(cursor_x, icon_y));
        Icon(iconLabel, icon_sz, ImVec4(1,1,1,1));
        cursor_x += icon_sz + icon_gap;
    }
    float text_x = cursor_x;
    float title_y = min.y + top_pad;
    ImGui::SetCursorScreenPos(ImVec2(text_x, title_y));
    TextEllipsisSingleLine(title, label_w);
    if (description && *description) {
        ImGui::PushStyleColor(ImGuiCol_Text, kMutedText());
        float desc_y = title_y + title_h + between;
        ImGui::SetCursorScreenPos(ImVec2(text_x, desc_y));
        ImGui::SetWindowFontScale(0.70f);
        TextEllipsisSingleLine(description, label_w);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
    }
    float h = h_ctrl;
    float x_local = ImGui::GetWindowContentRegionMax().x - s_cardPad - slider_w;
    float y_center = min.y + (row_h - h) * 0.5f;
    ImGui::SetCursorScreenPos(ImVec2(min.x + x_local, y_center));
    ImGui::PushItemWidth(slider_w);

    float scroll_y = ImGui::GetScrollY();
    bool changed = ImGui::SliderInt("##slider", v, v_min, v_max);

    if (ImGui::IsItemActive()) {
        ImGui::SetScrollY(scroll_y);
    }

    ImGui::PopItemWidth();
    ImGui::SetCursorScreenPos(ImVec2(min.x, max.y));
    ImGui::PopID();
    return changed;
}

bool FloatCardIcon(const char* title, const char* description, const char* iconLabel, float* v, float v_min, float v_max, float v_default, const char* fmt) {
    ImGui::PushID(title);
    float s_cardPad = GetCardPadding();
    float cont_w = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
    float h_ctrl = ImGui::GetFrameHeight() * 0.9f;

    const float reset_btn_sz = DP(30.0f);
    const float reset_gap = DP(6.0f);
    float slider_w = std::max(DP(140.0f), cont_w * 0.35f);

    const float icon_sz = DP(24.0f);
    const float icon_gap = DP(8.0f);
    const bool  has_icon = (iconLabel && *iconLabel);

    float label_w = std::max(0.0f, cont_w - (s_cardPad * 2.0f + slider_w + reset_btn_sz + reset_gap + (has_icon ? (icon_sz + icon_gap) : 0.0f)));

    float top_pad = DP(8.0f);
    float bottom_pad = DP(8.0f);
    float between = DP(2.0f);
    float title_h = ImGui::GetTextLineHeight();
    float desc_h = (description && *description) ? ImGui::GetTextLineHeight() : 0.0f;
    float text_h = top_pad + title_h + (desc_h > 0.0f ? (between + desc_h) : 0.0f) + bottom_pad;
    float row_h = std::max(text_h, h_ctrl + DP(12.0f));
    float avail_w = ImGui::GetContentRegionAvail().x;
    ImGui::Dummy(ImVec2(avail_w, row_h));
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    float cursor_x = min.x + s_cardPad;

    if (has_icon) {
        float icon_y = min.y + (row_h - icon_sz) * 0.5f;
        ImGui::SetCursorScreenPos(ImVec2(cursor_x, icon_y));
        Icon(iconLabel, icon_sz, ImVec4(1, 1, 1, 1));
        cursor_x += icon_sz + icon_gap;
    }

    float text_x = cursor_x;
    float title_y = min.y + top_pad;
    ImGui::SetCursorScreenPos(ImVec2(text_x, title_y));
    TextEllipsisSingleLine(title, label_w);

    if (description && *description) {
        ImGui::PushStyleColor(ImGuiCol_Text, kMutedText());
        float desc_y = title_y + title_h + between;
        ImGui::SetCursorScreenPos(ImVec2(text_x, desc_y));
        ImGui::SetWindowFontScale(0.70f);
        TextEllipsisSingleLine(description, label_w);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
    }

    float h = h_ctrl;
    float slider_x_local = ImGui::GetWindowContentRegionMax().x - s_cardPad - slider_w;
    float y_center = min.y + (row_h - h) * 0.5f;
    ImGui::SetCursorScreenPos(ImVec2(min.x + slider_x_local, y_center));
    ImGui::PushItemWidth(slider_w);

    float scroll_y = ImGui::GetScrollY();
    bool changed = ImGui::SliderFloat("##slider", v, v_min, v_max, fmt);

    if (ImGui::IsItemActive()) {
        ImGui::SetScrollY(scroll_y);
    }

    ImGui::PopItemWidth();

    float reset_x_local = slider_x_local - reset_gap - reset_btn_sz;
    float reset_y = min.y + (row_h - reset_btn_sz) * 0.5f;
    ImGui::SetCursorScreenPos(ImVec2(min.x + reset_x_local, reset_y));

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.2f));

    if (ImGui::Button("##reset", ImVec2(reset_btn_sz, reset_btn_sz))) {
        *v = v_default;
        changed = true;
    }

    ImGui::PopStyleColor(3);

    ImVec2 btn_min = ImGui::GetItemRectMin();
    ImGui::SetCursorScreenPos(ImVec2(btn_min.x + (reset_btn_sz - reset_btn_sz * 0.8f) * 0.5f,
        btn_min.y + (reset_btn_sz - reset_btn_sz * 0.8f) * 0.5f));
    Icon("UiMenuRewind", reset_btn_sz * 0.8f, ImVec4(1, 1, 1, 0.6f));

    ImGui::SetCursorScreenPos(ImVec2(min.x, max.y));
    ImGui::PopID();
    return changed;
}

bool InputCardIcon(const char* title, const char* description, const char* iconLabel, float* v, const char* fmt) {
    ImGui::PushID(title);
    float s_cardPad = GetCardPadding();
    float cont_w = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
    float h_ctrl = ImGui::GetFrameHeight() * 0.9f;
    float input_w = std::max(DP(140.0f), cont_w * 0.35f);
    const float icon_sz = DP(24.0f);
    const float icon_gap = DP(8.0f);
    const bool  has_icon = (iconLabel && *iconLabel);
    float label_w  = std::max(0.0f, cont_w - (s_cardPad * 2.0f + input_w + (has_icon ? (icon_sz + icon_gap) : 0.0f)));
    float top_pad = DP(8.0f);
    float bottom_pad = DP(8.0f);
    float between = DP(2.0f);
    float title_h = ImGui::GetTextLineHeight();
    float desc_h  = (description && *description) ? ImGui::GetTextLineHeight() : 0.0f;
    float text_h  = top_pad + title_h + (desc_h > 0.0f ? (between + desc_h) : 0.0f) + bottom_pad;
    float row_h   = std::max(text_h, h_ctrl + DP(12.0f));
    float avail_w = ImGui::GetContentRegionAvail().x;
    ImGui::Dummy(ImVec2(avail_w, row_h));
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    float cursor_x = min.x + s_cardPad;
    if (has_icon) {
        float icon_y = min.y + (row_h - icon_sz) * 0.5f;
        ImGui::SetCursorScreenPos(ImVec2(cursor_x, icon_y));
        Icon(iconLabel, icon_sz, ImVec4(1,1,1,1));
        cursor_x += icon_sz + icon_gap;
    }
    float text_x = cursor_x;
    float title_y = min.y + top_pad;
    ImGui::SetCursorScreenPos(ImVec2(text_x, title_y));
    TextEllipsisSingleLine(title, label_w);
    if (description && *description) {
        ImGui::PushStyleColor(ImGuiCol_Text, kMutedText());
        float desc_y = title_y + title_h + between;
        ImGui::SetCursorScreenPos(ImVec2(text_x, desc_y));
        ImGui::SetWindowFontScale(0.70f);
        TextEllipsisSingleLine(description, label_w);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
    }
    float h = h_ctrl;
    float x_local = ImGui::GetWindowContentRegionMax().x - s_cardPad - input_w;
    float y_center = min.y + (row_h - h) * 0.5f;
    ImGui::SetCursorScreenPos(ImVec2(min.x + x_local, y_center));
    ImGui::PushItemWidth(input_w);
    if (!fmt) fmt = "%.2f";
    bool changed = ImGui::InputFloat("##input", v, 0.0f, 0.0f, fmt);
    ImGui::PopItemWidth();
    ImGui::SetCursorScreenPos(ImVec2(min.x, max.y));
    ImGui::PopID();
    return changed;
}

bool StringSliderCard(const char* title,
    const char* description,
    const char* iconLabel,
    const char* const* options,
    int options_count,
    int* selected_index) {
    if (!options || options_count <= 0 || !selected_index) {
        return false;
    }

    int idx = *selected_index;
    if (idx < 0) idx = 0;
    if (idx >= options_count) idx = options_count - 1;
    *selected_index = idx;

    ImGui::PushID(title);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    float s_cardPad = GetCardPadding();
    float cont_w = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
    float h_ctrl = ImGui::GetFrameHeight() * 0.9f;
    float arrow_w = h_ctrl;
    float text_pad = DP(8.0f);
    float label_area_w = DP(120.0f);
    float ctrl_w = arrow_w * 2.0f + label_area_w + text_pad * 2.0f;

    const float icon_sz = DP(24.0f);
    const float icon_gap = DP(8.0f);
    const bool has_icon = (iconLabel && *iconLabel);
    float label_w = std::max(0.0f, cont_w - (s_cardPad * 2.0f + ctrl_w + (has_icon ? (icon_sz + icon_gap) : 0.0f)));

    float top_pad = DP(8.0f);
    float bottom_pad = DP(8.0f);
    float between = DP(2.0f);
    float title_h = ImGui::GetTextLineHeight();
    float desc_h = (description && *description) ? ImGui::GetTextLineHeight() : 0.0f;
    float text_h = top_pad + title_h + (desc_h > 0.0f ? (between + desc_h) : 0.0f) + bottom_pad;
    float row_h = std::max(text_h, h_ctrl + DP(12.0f));

    float avail_w = ImGui::GetContentRegionAvail().x;
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool card_clicked = ImGui::InvisibleButton("##card_slider", ImVec2(avail_w, row_h));
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();

    float ctrl_x = ImGui::GetWindowContentRegionMax().x - s_cardPad - ctrl_w;
    float ctrl_y = min.y + (row_h - arrow_w) * 0.5f;

    ImVec2 leftArrowMin = ImVec2(min.x + ctrl_x, ctrl_y);
    ImVec2 leftArrowMax = ImVec2(leftArrowMin.x + arrow_w, ctrl_y + arrow_w);
    ImVec2 rightArrowMin = ImVec2(min.x + ctrl_x + ctrl_w - arrow_w, ctrl_y);
    ImVec2 rightArrowMax = ImVec2(rightArrowMin.x + arrow_w, ctrl_y + arrow_w);

    auto containsPoint = [](ImVec2 rMin, ImVec2 rMax, ImVec2 pt) {
        return pt.x >= rMin.x && pt.x < rMax.x && pt.y >= rMin.y && pt.y < rMax.y;
        };

    ImVec2 clickPos = ImGui::GetIO().MouseClickedPos[0];
    bool pressOnArrow = containsPoint(leftArrowMin, leftArrowMax, clickPos) ||
        containsPoint(rightArrowMin, rightArrowMax, clickPos);

    static std::unordered_map<ImGuiID, float> s_pressScrollY;
    ImGuiID cardIdTrack = ImGui::GetID("##card_slider");
    if (ImGui::IsItemActivated()) {
        s_pressScrollY[cardIdTrack] = ImGui::GetScrollY();
    }
    bool acceptClick = card_clicked && !pressOnArrow;
    auto itPress = s_pressScrollY.find(cardIdTrack);
    if (acceptClick && itPress != s_pressScrollY.end()) {
        float dy = fabsf(ImGui::GetScrollY() - itPress->second);
        if (dy > DP(10.0f)) acceptClick = false;
    }
    if (ImGui::IsItemDeactivated()) {
        s_pressScrollY.erase(cardIdTrack);
    }

    bool active = ImGui::IsItemActive() && !pressOnArrow;
    using namespace tsm::ui::helpers;
    auto& bg_anim = GetAnimatedFloat((void*)(intptr_t)cardIdTrack, 0.0f);
    float target_alpha = active ? 0.15f : 0.0f;
    bg_anim.SetTarget(target_alpha);
    float bg_alpha = bg_anim.Update(ImGui::GetIO().DeltaTime);
    ImVec4 acc = GetAccentColor();
    if (bg_alpha > 0.001f) {
        ImVec4 bg = ImVec4(acc.x, acc.y, acc.z, bg_alpha);
        dl->AddRectFilled(min, max, ImGui::GetColorU32(bg), 0.0f);
    }
    void* ripple_id = (void*)(intptr_t)(cardIdTrack + 1);
    if (ImGui::IsItemClicked() && !pressOnArrow) {
        StartRipple(ripple_id, ImGui::GetMousePos());
    }
    UpdateAndDrawRipples(ripple_id, dl, min, max, acc, 0.0f);

    float cursor_x = min.x + s_cardPad;
    if (has_icon) {
        float icon_y = min.y + (row_h - icon_sz) * 0.5f;
        ImGui::SetCursorScreenPos(ImVec2(cursor_x, icon_y));
        Icon(iconLabel, icon_sz, ImVec4(1, 1, 1, 1));
        cursor_x += icon_sz + icon_gap;
    }

    float text_x = cursor_x;
    float title_y = min.y + top_pad;
    ImGui::SetCursorScreenPos(ImVec2(text_x, title_y));
    TextEllipsisSingleLine(title, label_w);
    if (description && *description) {
        ImGui::PushStyleColor(ImGuiCol_Text, kMutedText());
        float desc_y = title_y + title_h + between;
        ImGui::SetCursorScreenPos(ImVec2(text_x, desc_y));
        ImGui::SetWindowFontScale(0.70f);
        TextEllipsisSingleLine(description, label_w);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
    }

    bool prevPressed = card_clicked && containsPoint(leftArrowMin, leftArrowMax, clickPos);
    bool nextPressed = card_clicked && containsPoint(rightArrowMin, rightArrowMax, clickPos);

    if (prevPressed) {
        idx = (idx - 1 + options_count) % options_count;
        *selected_index = idx;
        acceptClick = false;
    }
    else if (nextPressed) {
        idx = (idx + 1) % options_count;
        *selected_index = idx;
        acceptClick = false;
    }

    auto drawArrow = [&](ImVec2 btnMin, ImVec2 btnMax, const char* iconName) {
        ImVec2 mousePos = ImGui::GetIO().MousePos;
        bool hovered = ImGui::IsWindowHovered() && containsPoint(btnMin, btnMax, mousePos);
        bool held = hovered && ImGui::IsMouseDown(0);

        float r = arrow_w * 0.5f;
        ImVec2 c = ImVec2(btnMin.x + r, btnMin.y + r);

        ImVec4 fill = hovered ? ImVec4(acc.x, acc.y, acc.z, 0.20f) : ImVec4(1, 1, 1, 0.10f);
        ImVec4 brd = hovered ? ImVec4(acc.x, acc.y, acc.z, 0.60f) : ImVec4(1, 1, 1, 0.18f);
        if (held) {
            fill = ImVec4(acc.x, acc.y, acc.z, 0.30f);
            brd = ImVec4(acc.x, acc.y, acc.z, 0.80f);
        }

        dl->AddCircleFilled(c, r, ImGui::GetColorU32(fill), 48);
        dl->AddCircle(c, r, ImGui::GetColorU32(brd), 48, 1.0f);

        ImVec2 icon_pos = ImVec2(c.x - arrow_w * 0.3f, c.y - arrow_w * 0.3f);
        ImGui::SetCursorScreenPos(icon_pos);
        Icon(iconName, arrow_w * 0.6f, ImVec4(1, 1, 1, 1));
        };

    drawArrow(leftArrowMin, leftArrowMax, "UiMiscArrowLeft");
    drawArrow(rightArrowMin, rightArrowMax, "UiMiscArrowRight");

    const char* label = options[idx] ? options[idx] : "";
    const char* displayLabel = tsm::ui::i18n::Tr(label);
    ImVec2 labelSize = ImGui::CalcTextSize(displayLabel);

    ImVec2 textMin = ImVec2(min.x + ctrl_x + arrow_w + text_pad, min.y);
    ImVec2 textMax = ImVec2(textMin.x + label_area_w, min.y + row_h);
    ImGui::PushClipRect(textMin, textMax, true);
    float labelX = textMin.x + std::max(0.0f, (label_area_w - labelSize.x) * 0.5f);
    ImVec2 labelPos = ImVec2(labelX, min.y + (row_h - labelSize.y) * 0.5f);
    dl->AddText(labelPos, ImGui::GetColorU32(ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]), displayLabel);
    ImGui::PopClipRect();

    ImGui::SetCursorScreenPos(ImVec2(min.x, max.y));
    ImGui::PopID();
    return acceptClick;
}

bool SelectCard(const char* title, const char* description, int* current_index, const char* const* options, int options_count) {
    ImGui::PushID(title);
    float s_cardPad = GetCardPadding();
    float cont_w = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
    float h_ctrl = ImGui::GetFrameHeight() * 0.9f;
    float combo_w = std::max(DP(140.0f), cont_w * 0.35f);
    float label_w = std::max(0.0f, cont_w - (s_cardPad * 2.0f + combo_w));
    float top_pad = DP(8.0f);
    float bottom_pad = DP(8.0f);
    float between = DP(2.0f);
    float title_h = ImGui::GetTextLineHeight();
    float desc_h  = (description && *description) ? ImGui::GetTextLineHeight() : 0.0f;
    float text_h  = top_pad + title_h + (desc_h > 0.0f ? (between + desc_h) : 0.0f) + bottom_pad;
    float row_h   = std::max(text_h, h_ctrl + DP(12.0f));
    float avail_w = ImGui::GetContentRegionAvail().x;
    ImGui::Dummy(ImVec2(avail_w, row_h));
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    float text_x = min.x + s_cardPad;
    float title_y = min.y + top_pad;
    ImGui::SetCursorScreenPos(ImVec2(text_x, title_y));
    TextEllipsisSingleLine(title, label_w);
    if (description && *description) {
        ImGui::PushStyleColor(ImGuiCol_Text, kMutedText());
        float desc_y = title_y + title_h + between;
        ImGui::SetCursorScreenPos(ImVec2(text_x, desc_y));
        ImGui::SetWindowFontScale(0.70f);
        TextEllipsisSingleLine(description, label_w);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
    }
    float h = h_ctrl;
    float x_local = ImGui::GetWindowContentRegionMax().x - s_cardPad - combo_w;
    float y_center = min.y + (row_h - h) * 0.5f;
    ImGui::SetCursorScreenPos(ImVec2(min.x + x_local, y_center));
    ImGui::PushItemWidth(combo_w);
    bool changed = false;
    if (options && options_count > 0 && current_index) {
        int idx = *current_index;
        if (idx < 0) idx = 0; if (idx >= options_count) idx = options_count - 1;

        std::vector<std::string> localized;
        localized.reserve(options_count);
        std::vector<const char*> optionPtrs;
        optionPtrs.reserve(options_count);
        for (int i = 0; i < options_count; ++i) {
            const char* src = (options[i] && *options[i]) ? options[i] : "";
            const char* loc = tsm::ui::i18n::Tr(src);
            localized.emplace_back(loc);
            optionPtrs.push_back(localized.back().c_str());
        }

        changed = ImGui::Combo("##select", &idx, optionPtrs.data(), options_count);
        if (changed) *current_index = idx;
    } else {
        const char* noneLocalized = tsm::ui::i18n::Tr("None");
        const char* kNoOptions[] = { noneLocalized };
        int dummy = 0;
        ImGui::BeginDisabled();
        ImGui::Combo("##select", &dummy, kNoOptions, 1);
        ImGui::EndDisabled();
    }
    ImGui::PopItemWidth();
    ImGui::SetCursorScreenPos(ImVec2(min.x, max.y));
    ImGui::PopID();
    return changed;
}

bool SelectCardIcon(const char* title, const char* description, const char* iconLabel, int* current_index, const char* const* options, int options_count) {
    ImGui::PushID(title);
    float s_cardPad = GetCardPadding();
    float cont_w = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
    float h_ctrl = ImGui::GetFrameHeight() * 0.9f;
    float combo_w = std::max(DP(140.0f), cont_w * 0.35f);
    const float icon_sz = DP(24.0f);
    const float icon_gap = DP(8.0f);
    const bool  has_icon = (iconLabel && *iconLabel);
    float label_w = std::max(0.0f, cont_w - (s_cardPad * 2.0f + combo_w + (has_icon ? (icon_sz + icon_gap) : 0.0f)));
    float top_pad = DP(8.0f);
    float bottom_pad = DP(8.0f);
    float between = DP(2.0f);
    float title_h = ImGui::GetTextLineHeight();
    float desc_h  = (description && *description) ? ImGui::GetTextLineHeight() : 0.0f;
    float text_h  = top_pad + title_h + (desc_h > 0.0f ? (between + desc_h) : 0.0f) + bottom_pad;
    float row_h   = std::max(text_h, h_ctrl + DP(12.0f));
    float avail_w = ImGui::GetContentRegionAvail().x;
    ImGui::Dummy(ImVec2(avail_w, row_h));
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    float cursor_x = min.x + s_cardPad;
    if (has_icon) {
        float icon_y = min.y + (row_h - icon_sz) * 0.5f;
        ImGui::SetCursorScreenPos(ImVec2(cursor_x, icon_y));
        Icon(iconLabel, icon_sz, ImVec4(1,1,1,1));
        cursor_x += icon_sz + icon_gap;
    }
    float text_x = cursor_x;
    float title_y = min.y + top_pad;
    ImGui::SetCursorScreenPos(ImVec2(text_x, title_y));
    TextEllipsisSingleLine(title, label_w);
    if (description && *description) {
        ImGui::PushStyleColor(ImGuiCol_Text, kMutedText());
        float desc_y = title_y + title_h + between;
        ImGui::SetCursorScreenPos(ImVec2(text_x, desc_y));
        ImGui::SetWindowFontScale(0.70f);
        TextEllipsisSingleLine(description, label_w);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
    }
    float h = h_ctrl;
    float x_local = ImGui::GetWindowContentRegionMax().x - s_cardPad - combo_w;
    float y_center = min.y + (row_h - h) * 0.5f;
    ImGui::SetCursorScreenPos(ImVec2(min.x + x_local, y_center));
    ImGui::PushItemWidth(combo_w);
    bool changed = false;
    if (options && options_count > 0 && current_index) {
        int idx = *current_index;
        if (idx < 0) idx = 0; if (idx >= options_count) idx = options_count - 1;

        std::vector<std::string> localized;
        localized.reserve(options_count);
        std::vector<const char*> optionPtrs;
        optionPtrs.reserve(options_count);
        for (int i = 0; i < options_count; ++i) {
            const char* src = (options[i] && *options[i]) ? options[i] : "";
            const char* loc = tsm::ui::i18n::Tr(src);
            localized.emplace_back(loc);
            optionPtrs.push_back(localized.back().c_str());
        }

        changed = ImGui::Combo("##select", &idx, optionPtrs.data(), options_count, ImGuiComboFlags_HeightLargest);
        if (changed) *current_index = idx;
    } else {
        const char* noneLocalized = tsm::ui::i18n::Tr("None");
        const char* kNoOptions[] = { noneLocalized };
        int dummy = 0;
        ImGui::BeginDisabled();
        ImGui::Combo("##select", &dummy, kNoOptions, 1);
        ImGui::EndDisabled();
    }
    ImGui::PopItemWidth();
    ImGui::SetCursorScreenPos(ImVec2(min.x, max.y));
    ImGui::PopID();
    return changed;
}

bool SelectCardIconList(const char* title, const char* description, int* current_index, const char* const* iconNames, int iconCount) {
    ImGui::PushID(title);

    bool changed = false;

    if (!iconNames || iconCount <= 0 || !current_index) {
        float s_cardPad = GetCardPadding();
        float avail_w = ImGui::GetContentRegionAvail().x;
        float label_w = std::max(0.0f, avail_w - s_cardPad * 2.0f);

        ImVec2 start = ImGui::GetCursorScreenPos();
        float top_pad = DP(8.0f);
        float between = DP(2.0f);
        float title_h = ImGui::GetTextLineHeight();
        float desc_h  = (description && *description) ? ImGui::GetTextLineHeight() : 0.0f;

        ImGui::SetCursorScreenPos(ImVec2(start.x + s_cardPad, start.y + top_pad));
        TextEllipsisSingleLine(title, label_w);
        if (description && *description) {
            ImGui::PushStyleColor(ImGuiCol_Text, kMutedText());
            float desc_y = start.y + top_pad + title_h + between;
            ImGui::SetCursorScreenPos(ImVec2(start.x + s_cardPad, desc_y));
            ImGui::SetWindowFontScale(0.70f);
            TextEllipsisSingleLine(description, label_w);
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();
        }

        ImGui::Dummy(ImVec2(0, DP(8.0f)));
        ImGui::PopID();
        return false;
    }

    int idx = *current_index;
    if (idx < 0) idx = 0;
    if (idx >= iconCount) idx = iconCount - 1;
    *current_index = idx;

    float s_cardPad = GetCardPadding();
    float avail_w = ImGui::GetContentRegionAvail().x;
    float label_w = std::max(0.0f, avail_w - s_cardPad * 2.0f);

    float top_pad = DP(8.0f);
    float between = DP(2.0f);
    float title_h = ImGui::GetTextLineHeight();
    float desc_h  = (description && *description) ? ImGui::GetTextLineHeight() : 0.0f;

    ImVec2 start = ImGui::GetCursorScreenPos();

    ImGui::SetCursorScreenPos(ImVec2(start.x + s_cardPad, start.y + top_pad));
    TextEllipsisSingleLine(title, label_w);
    if (description && *description) {
        ImGui::PushStyleColor(ImGuiCol_Text, kMutedText());
        float desc_y = start.y + top_pad + title_h + between;
        ImGui::SetCursorScreenPos(ImVec2(start.x + s_cardPad, desc_y));
        ImGui::SetWindowFontScale(0.70f);
        TextEllipsisSingleLine(description, label_w);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
    }

    float grid_offset_y = top_pad + title_h + (desc_h > 0.0f ? (between + desc_h) : 0.0f) + DP(6.0f);
    ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + grid_offset_y));

    const float base_d = DP(44.0f);
    float avail_w2 = ImGui::GetContentRegionAvail().x;
    float base_gap2 = ImGui::GetStyle().ItemSpacing.x;

    int count = iconCount;
    int target_cols = (count <= 1) ? 1 : (count + 1) / 2;
    float denom2 = target_cols * base_d + (target_cols - 1) * base_gap2;
    float s2 = 1.0f;
    if (denom2 > 0.0f && avail_w2 > 0.0f) {
        s2 = std::min(1.0f, avail_w2 / denom2);
    }
    s2 = std::max(0.6f, s2);

    float slot_d = base_d * s2;
    float icon_sz = slot_d * 0.62f;
    float gap = base_gap2 * s2;

    int cols_fit = std::max(1, (int)floorf((avail_w2 + gap) / (slot_d + gap)));
    int cols = std::min(std::max(1, target_cols), cols_fit);
    int rows = (count + cols - 1) / cols;

    ImVec4 acc = GetAccentColor();
    const ImU32 bg_sel  = ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 0.28f));
    const ImU32 brd_sel = ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 0.70f));
    const ImU32 bg_nrm  = ImGui::GetColorU32(ImVec4(1,1,1,0.10f));
    const ImU32 brd_nrm = ImGui::GetColorU32(ImVec4(1,1,1,0.18f));

    ImGui::BeginGroup();
    ImVec2 grid_start = ImGui::GetCursorPos();

    for (int i = 0; i < count; ++i) {
        int row = i / cols;
        int col = i % cols;

        int items_in_row = (row == rows - 1) ? (count - row * cols) : cols;
        float row_w = items_in_row * slot_d + (items_in_row - 1) * gap;
        float left_pad_row = std::max(0.0f, (avail_w2 - row_w) * 0.5f);

        ImGui::SetCursorPos(ImVec2(grid_start.x + left_pad_row + col * (slot_d + gap),
                                   grid_start.y + row * (slot_d + gap)));

        ImGui::PushID(i);
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##icon", ImVec2(slot_d, slot_d));
        bool clicked = ImGui::IsItemClicked();
        bool is_selected = (i == idx);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        float r = slot_d * 0.5f;
        ImVec2 c = ImVec2(p.x + r, p.y + r);

        dl->AddCircleFilled(c, r, is_selected ? bg_sel : bg_nrm, 48);
        dl->AddCircle(c, r, is_selected ? brd_sel : brd_nrm, 48, 1.0f);

        ImVec2 icon_pos = ImVec2(c.x - icon_sz * 0.5f, c.y - icon_sz * 0.5f);
        ImGui::SetCursorScreenPos(icon_pos);
        Icon(iconNames[i], icon_sz, ImVec4(1,1,1,1));

        if (clicked && !is_selected) {
            idx = i;
            changed = true;
        }

        ImGui::PopID();
    }
    ImGui::EndGroup();

    ImGui::Dummy(ImVec2(0, DP(8.0f)));

    if (changed) {
        *current_index = idx;
    }

    ImGui::PopID();
    return changed;
}

bool ButtonCard(const char* title, const char* description, const char* iconLabel, const char* trailingText, const ImVec4& iconColor) {
    ImGui::PushID(title);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float s_cardPad = GetCardPadding();
    const float cont_w = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
    const float icon_sz = DP(24.0f);
    const float icon_gap = DP(8.0f);
    const bool  has_icon = (iconLabel && *iconLabel);
    const float trail_gap = DP(12.0f);
    const bool  has_trail = (trailingText && *trailingText);
    const char* trail_text_localized = has_trail ? tsm::ui::i18n::Tr(trailingText) : nullptr;
    const ImVec2 trail_ts = (has_trail && trail_text_localized) ? ImGui::CalcTextSize(trail_text_localized) : ImVec2(0,0);
    const float trail_w = has_trail ? trail_ts.x : 0.0f;
    const float trail_reserve = has_trail ? (trail_w + trail_gap) : 0.0f;
    const float text_w = std::max(0.0f, cont_w - s_cardPad * 2.0f - (has_icon ? (icon_sz + icon_gap) : 0.0f) - trail_reserve);
    const float top_pad = DP(8.0f);
    const float bottom_pad = DP(8.0f);
    const float between = DP(2.0f);
    const float title_h = ImGui::GetTextLineHeight();
    const float desc_h  = (description && *description) ? ImGui::GetTextLineHeight() : 0.0f;
    const float text_h  = top_pad + title_h + (desc_h > 0.0f ? (between + desc_h) : 0.0f) + bottom_pad;
    const float row_h   = std::max(text_h, icon_sz + DP(10.0f));
    const float avail_w = ImGui::GetContentRegionAvail().x;
    const bool clicked = ImGui::InvisibleButton("##btncard", ImVec2(avail_w, row_h));
    const bool active = ImGui::IsItemActive();

    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();

    static std::unordered_map<ImGuiID, float> s_pressScrollY;
    ImGuiID btnIdTrack = ImGui::GetID("##btncard");
    if (ImGui::IsItemActivated()) {
        s_pressScrollY[btnIdTrack] = ImGui::GetScrollY();
    }
    ImVec4 acc = GetAccentColor();

    using namespace tsm::ui::helpers;
    auto& bg_anim = GetAnimatedFloat((void*)(intptr_t)btnIdTrack, 0.0f);

    float target_alpha = active ? 0.15f : 0.0f;
    bg_anim.SetTarget(target_alpha);

    float bg_alpha = bg_anim.Update(ImGui::GetIO().DeltaTime);

    if (bg_alpha > 0.001f) {
        ImVec4 bg = ImVec4(acc.x, acc.y, acc.z, bg_alpha);
        dl->AddRectFilled(min, max, ImGui::GetColorU32(bg), 0.0f);
    }

    void* ripple_id = (void*)(intptr_t)(btnIdTrack + 1);
    if (ImGui::IsItemClicked()) {
        StartRipple(ripple_id, ImGui::GetMousePos());
    }
    UpdateAndDrawRipples(ripple_id, dl, min, max, acc, 0.0f);
    float cursor_x = min.x + s_cardPad;
    float cursor_y = min.y + top_pad;
    if (has_icon) {
        ImGui::SetCursorScreenPos(ImVec2(cursor_x, min.y + (row_h - icon_sz) * 0.5f));
        Icon(iconLabel, icon_sz, iconColor);
        cursor_x += icon_sz + icon_gap;
    }
    ImGui::SetCursorScreenPos(ImVec2(cursor_x, cursor_y));
    TextEllipsisSingleLine(title, text_w);
    if (description && *description) {
        ImGui::PushStyleColor(ImGuiCol_Text, kMutedText());
        ImGui::SetCursorScreenPos(ImVec2(cursor_x, cursor_y + title_h + between));
        ImGui::SetWindowFontScale(0.70f);
        TextEllipsisSingleLine(description, text_w);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
    }
    if (has_trail && trail_text_localized && *trail_text_localized) {
        float x_local = ImGui::GetWindowContentRegionMax().x - s_cardPad - trail_w;
        float y_center = min.y + (row_h - trail_ts.y) * 0.5f;
        ImGui::PushStyleColor(ImGuiCol_Text, kMutedText());
        ImGui::SetCursorScreenPos(ImVec2(min.x + x_local, y_center));
        ImGui::TextUnformatted(trail_text_localized);
        ImGui::PopStyleColor();
    }
    ImGui::SetCursorScreenPos(ImVec2(min.x, max.y));
    ImGuiID btnIdEval = ImGui::GetID("##btncard");
    bool acceptClick = clicked;
    auto itPress = s_pressScrollY.find(btnIdEval);
    if (acceptClick && itPress != s_pressScrollY.end()) {
        float dy = fabsf(ImGui::GetScrollY() - itPress->second);
        if (dy > DP(10.0f)) acceptClick = false;
    }
    if (ImGui::IsItemDeactivated()) {
        s_pressScrollY.erase(btnIdEval);
    }
    ImGui::PopID();
    return acceptClick;
}

bool ColorPickerCard(const char* title, const char* description, const char* iconLabel, ImVec4* color, float ) {
    if (!color) return false;
    ImGui::PushID(title);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    float s_cardPad = GetCardPadding();
    const float cont_w = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
    float h_ctrl = ImGui::GetFrameHeight() * 0.9f;
    const float sw_w = h_ctrl * 1.9f;
    const float icon_sz = DP(24.0f);
    const float icon_gap = DP(8.0f);
    const bool has_icon = (iconLabel && *iconLabel);
    float label_w = std::max(0.0f, cont_w - (s_cardPad * 2.0f + sw_w + (has_icon ? (icon_sz + icon_gap) : 0.0f)));
    float top_pad = DP(8.0f);
    float bottom_pad = DP(8.0f);
    float between = DP(2.0f);
    float title_h = ImGui::GetTextLineHeight();
    float desc_h = (description && *description) ? ImGui::GetTextLineHeight() : 0.0f;
    float text_h = top_pad + title_h + (desc_h > 0.0f ? (between + desc_h) : 0.0f) + bottom_pad;
    float row_h = std::max(text_h, h_ctrl + DP(12.0f));

    float avail_w = ImGui::GetContentRegionAvail().x;
    float card_button_w = avail_w - sw_w - s_cardPad;

    ImVec2 full_min = ImGui::GetCursorScreenPos();
    ImVec2 full_max = ImVec2(full_min.x + avail_w, full_min.y + row_h);
    ImVec2 min = full_min;
    ImVec2 max = full_max;

    bool cardClicked = ImGui::InvisibleButton("##card_color", ImVec2(card_button_w, row_h));
    bool active = ImGui::IsItemActive();
    ImVec4 acc = GetAccentColor();

    using namespace tsm::ui::helpers;
    ImGuiID cardId = ImGui::GetID("##card_color");
    auto& hold_anim = GetAnimatedFloat((void*)(intptr_t)cardId, 0.0f);
    float hold_target = active ? 0.15f : 0.0f;
    hold_anim.SetTarget(hold_target);
    float hold_alpha = hold_anim.Update(ImGui::GetIO().DeltaTime);
    if (hold_alpha > 0.001f) {
        ImVec4 hold_bg = ImVec4(acc.x, acc.y, acc.z, hold_alpha);
        dl->AddRectFilled(min, max, ImGui::GetColorU32(hold_bg), 0.0f);
    }
    void* ripple_id = (void*)(intptr_t)(cardId + 1);
    if (ImGui::IsItemClicked()) {
        StartRipple(ripple_id, ImGui::GetMousePos());
    }
    UpdateAndDrawRipples(ripple_id, dl, min, max, acc, 0.0f);

    float cursor_x = min.x + s_cardPad;
    float icon_y = min.y + (row_h - icon_sz) * 0.5f;
    if (has_icon) {
        ImGui::SetCursorScreenPos(ImVec2(cursor_x, icon_y));
        Icon(iconLabel, icon_sz, ImVec4(1,1,1,1));
        cursor_x += icon_sz + icon_gap;
    }

    float text_x = cursor_x;
    float title_y = min.y + top_pad;
    ImGui::SetCursorScreenPos(ImVec2(text_x, title_y));
    TextEllipsisSingleLine(title, label_w);
    if (description && *description) {
        ImGui::PushStyleColor(ImGuiCol_Text, kMutedText());
        float desc_y = title_y + title_h + between;
        ImGui::SetCursorScreenPos(ImVec2(text_x, desc_y));
        ImGui::SetWindowFontScale(0.70f);
        TextEllipsisSingleLine(description, label_w);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
    }

    float sw_x = min.x + avail_w - s_cardPad - sw_w;
    float y_center = min.y + (row_h - sw_w) * 0.5f;
    ImVec2 swPos = ImVec2(sw_x, y_center);
    ImGui::SetCursorScreenPos(swPos);
    ImGui::InvisibleButton("##swatch", ImVec2(sw_w, sw_w));
    bool swatchClicked = ImGui::IsItemClicked();
    bool swatchHovered = ImGui::IsItemHovered();
    bool swatchActive = ImGui::IsItemActive();
    ImVec2 swMin = ImGui::GetItemRectMin();
    ImVec2 swMax = ImGui::GetItemRectMax();

    ImVec4 base = ImGui::GetStyle().Colors[ImGuiCol_FrameBg];
    ImVec4 bg = swatchHovered ? ImVec4(acc.x, acc.y, acc.z, 0.14f) : ImVec4(base.x, base.y, base.z, 0.65f);
    ImVec4 brd = swatchActive ? ImVec4(acc.x, acc.y, acc.z, 0.85f) : (swatchHovered ? ImVec4(acc.x, acc.y, acc.z, 0.60f) : ImVec4(acc.x, acc.y, acc.z, 0.25f));
    dl->AddRectFilled(swMin, swMax, ImGui::GetColorU32(bg), 8.0f);
    dl->AddRect(swMin, swMax, ImGui::GetColorU32(brd), 8.0f, 0, 1.0f);

    const float inset = 3.0f;
    ImVec4 col = *color;
    col.w = 1.0f;
    dl->AddRectFilled(ImVec2(swMin.x + inset, swMin.y + inset), ImVec2(swMax.x - inset, swMax.y - inset), ImGui::GetColorU32(col), 6.0f);
    dl->AddRect(ImVec2(swMin.x + inset, swMin.y + inset), ImVec2(swMax.x - inset, swMax.y - inset), ImGui::GetColorU32(ImVec4(1,1,1,0.22f)), 6.0f, 0, 1.0f);

    if (cardClicked || swatchClicked) {
        ImGui::OpenPopup("##color_popup");
    }

    bool changed = false;
    ImGui::SetNextWindowSizeConstraints(ImVec2(220.0f, 0.0f), ImVec2(FLT_MAX, FLT_MAX));
    if (ImGui::BeginPopup("##color_popup")) {
        ImGui::TextUnformatted(tsm::ui::i18n::Tr("Custom Color"));
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::Separator();
        ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_NoInputs;
        changed = ImGui::ColorPicker4("##picker", &color->x, flags);
        ImGui::EndPopup();
    }

    ImGui::SetCursorScreenPos(ImVec2(min.x, max.y));
    ImGui::PopID();
    return changed;
}

bool SearchCard(const char* placeholder, char* buf, size_t buf_size) {
    ImGui::PushID("SearchCard");
    float s_cardPad = GetCardPadding();
    const float cont_w = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
    const float pad_x  = s_cardPad;
    const float pad_y  = DP(6.0f);
    const float frame_h = ImGui::GetFrameHeight();
    const float min_row_h = DP(36.0f);
    const float row_h  = std::max(frame_h + pad_y * 2.0f, min_row_h);
    ImVec2 row_min = ImGui::GetCursorScreenPos();
    ImVec2 row_max = ImVec2(row_min.x + cont_w, row_min.y + row_h);
    ImVec2 bg_min = ImVec2(row_min.x + pad_x, row_min.y);
    ImVec2 bg_max = ImVec2(row_max.x - pad_x, row_max.y);
    const float in_pad_x = DP(10.0f);
    const float input_y = row_min.y + (row_h - frame_h) * 0.5f;
    ImGui::SetCursorScreenPos(ImVec2(bg_min.x + in_pad_x, input_y));
    ImGui::PushItemWidth((bg_max.x - bg_min.x) - in_pad_x * 2.0f);
    bool changed = InputWithPlaceholder("##search", buf, buf_size, placeholder ? placeholder : "Search...");
    ImGui::PopItemWidth();
    ImGui::SetCursorScreenPos(ImVec2(row_min.x, row_max.y));
    ImGui::PopID();
    return changed;
}

EditableCardClick EditableCard(const char* title, const char* description, const char* iconLabel) {
    return EditableCard(title, description, iconLabel, false);
}

EditableCardClick EditableCard(const char* title, const char* description, const char* iconLabel, bool selected) {
    ImGui::PushID(title);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float s_cardPad = GetCardPadding();
    const float cont_w = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
    const float icon_sz = DP(24.0f);
    const float icon_gap = DP(8.0f);
    const float edit_btn_sz = DP(40.0f);
    const float edit_gap = 0.0f;
    const float pad = s_cardPad;
    const bool has_icon = (iconLabel && *iconLabel);
    const float text_w = std::max(0.0f, cont_w - pad * 2.0f - (has_icon ? (icon_sz + icon_gap) : 0.0f) - edit_btn_sz - edit_gap);
    const float top_pad = DP(8.0f);
    const float bottom_pad = DP(8.0f);
    const float between = DP(2.0f);
    const float title_h = ImGui::GetTextLineHeight();
    const float desc_h = (description && *description) ? ImGui::GetTextLineHeight() : 0.0f;
    const float text_h = top_pad + title_h + (desc_h > 0.0f ? (between + desc_h) : 0.0f) + bottom_pad;
    const float row_h = std::max(text_h, icon_sz + DP(10.0f));

    EditableCardClick result = {false, false};

    float card_width = cont_w - edit_btn_sz - edit_gap;
    ImVec2 card_start = ImGui::GetCursorScreenPos();
    const bool card_clicked = ImGui::InvisibleButton("##maincard", ImVec2(card_width, row_h));
    const bool card_active = ImGui::IsItemActive();

    ImVec2 card_min = ImGui::GetItemRectMin();
    ImVec2 card_max = ImGui::GetItemRectMax();

    static std::unordered_map<ImGuiID, float> s_pressScrollY;
    ImGuiID cardIdTrack = ImGui::GetID("##maincard");
    if (ImGui::IsItemActivated()) {
        s_pressScrollY[cardIdTrack] = ImGui::GetScrollY();
    }
    ImVec4 acc = GetAccentColor();

    using namespace tsm::ui::helpers;
    auto& bg_anim = GetAnimatedFloat((void*)(intptr_t)cardIdTrack, 0.0f);

    float target_alpha = card_active ? 0.15f : 0.0f;
    bg_anim.SetTarget(target_alpha);

    float bg_alpha = bg_anim.Update(ImGui::GetIO().DeltaTime);

    if (bg_alpha > 0.001f) {
        ImVec4 bg = ImVec4(acc.x, acc.y, acc.z, bg_alpha);
        dl->AddRectFilled(card_min, card_max, ImGui::GetColorU32(bg), 0.0f);
    }

    void* ripple_id = (void*)(intptr_t)(cardIdTrack + 1);
    if (ImGui::IsItemClicked()) {
        StartRipple(ripple_id, ImGui::GetMousePos());
    }
    UpdateAndDrawRipples(ripple_id, dl, card_min, card_max, acc, 0.0f);

    float cursor_x = card_min.x + pad;
    float cursor_y = card_min.y + top_pad;
    if (has_icon) {
        ImGui::SetCursorScreenPos(ImVec2(cursor_x, card_min.y + (row_h - icon_sz) * 0.5f));
        ImVec4 iconColor = selected ? GetAccentColor() : ImVec4(1,1,1,1);
        Icon(iconLabel, icon_sz, iconColor);
        cursor_x += icon_sz + icon_gap;
    }

    ImGui::SetCursorScreenPos(ImVec2(cursor_x, cursor_y));
    TextEllipsisSingleLine(title, text_w);

    if (description && *description) {
        ImGui::PushStyleColor(ImGuiCol_Text, kMutedText());
        ImGui::SetCursorScreenPos(ImVec2(cursor_x, cursor_y + title_h + between));
        ImGui::SetWindowFontScale(0.70f);
        TextEllipsisSingleLine(description, text_w);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
    }

    ImGui::SetCursorScreenPos(ImVec2(card_max.x + edit_gap, card_min.y));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(acc.x, acc.y, acc.z, 0.15f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(acc.x, acc.y, acc.z, 0.3f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DP(8.0f));
    if (ImGui::Button("##edit", ImVec2(edit_btn_sz, row_h))) {
        result.edit = true;
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    float edit_icon_sz = DP(20.0f);
    ImVec2 edit_btn_min = ImVec2(card_max.x + edit_gap, card_min.y);
    ImGui::SetCursorScreenPos(ImVec2(edit_btn_min.x + (edit_btn_sz - edit_icon_sz) * 0.5f,
                                     edit_btn_min.y + (row_h - edit_icon_sz) * 0.5f));
    Icon("UiMenuEdit", edit_icon_sz, ImVec4(1,1,1,1));

    if (card_clicked) {
        bool acceptClick = true;
        auto itPress = s_pressScrollY.find(cardIdTrack);
        if (itPress != s_pressScrollY.end()) {
            float dy = fabsf(ImGui::GetScrollY() - itPress->second);
            if (dy > DP(10.0f)) acceptClick = false;
        }
        if (acceptClick) result.main = true;
    }

    if (ImGui::IsItemDeactivated()) {
        s_pressScrollY.erase(cardIdTrack);
    }

    ImGui::SetCursorScreenPos(ImVec2(card_start.x, card_max.y));
    ImGui::PopID();
    return result;
}

bool ToggleButton(const char* id, const char* iconName, bool* value, float size, const char* tooltip) {
    if (!id || !iconName || !value) return false;
    ImGui::PushID(id);
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool pressed = ImGui::InvisibleButton("##btn", ImVec2(size, size));
    bool hovered = ImGui::IsItemHovered();
    if (tooltip && tooltip[0]) ImGui::SetItemTooltip("%s", tooltip);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec4 acc = GetAccentColor();
    float radius = size * 0.5f;
    ImVec2 center = ImVec2(p.x + radius, p.y + radius);

    bool active = *value;
    ImVec4 fillNormal, fillHover, fillActive;
    if (active) {
        fillNormal = ImVec4(acc.x, acc.y, acc.z, 0.85f);
        fillHover  = ImVec4(acc.x, acc.y, acc.z, 1.0f);
        fillActive = ImVec4(acc.x * 0.8f, acc.y * 0.8f, acc.z * 0.8f, 1.0f);
    } else {
        fillNormal = ImVec4(1.0f, 1.0f, 1.0f, 0.08f);
        fillHover  = ImVec4(1.0f, 1.0f, 1.0f, 0.14f);
        fillActive = ImVec4(1.0f, 1.0f, 1.0f, 0.22f);
    }
    ImVec4 fill = ImGui::IsItemActive() ? fillActive : (hovered ? fillHover : fillNormal);
    dl->AddCircleFilled(center, radius, ImGui::GetColorU32(fill), 48);

    float icoSize = size * 0.50f;
    ImGui::SetCursorScreenPos(ImVec2(center.x - icoSize * 0.5f, center.y - icoSize * 0.5f));
    Icon(iconName, icoSize, ImVec4(1, 1, 1, active ? 1.0f : (hovered ? 0.9f : 0.65f)));

    if (pressed) *value = !*value;
    ImGui::PopID();
    return pressed;
}

}}}

