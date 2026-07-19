#include <ui/core/WindowManager.h>
#include <ui/core/metrics.h>
#include <ui/core/App.h>
#include <ui/widgets/Icons.h>
#include <state/ModState.h>
#include <Cipher/CipherUtils.h>
#include <algorithm>
#include <cmath>

namespace tsm { namespace ui {

WindowManager& WindowManager::Get() {
    static WindowManager instance;
    return instance;
}

void WindowManager::Initialize() {
    auto& ms = tsm::state::ModState::Get();
    dock_right_ = ms.ui.dockRight;
    handle_y_norm_ = ms.ui.handleYNorm;
    active_tab_ = 0;
}

bool WindowManager::BeginFrame(bool* menu_open, bool auto_scale, const ImVec4& accent_color) {
    if (!menu_open || !*menu_open) return false;

    ImGuiIO& io = ImGui::GetIO();
    ImGuiViewport* vp = ImGui::GetMainViewport();

    if (first_frame_) {
        Initialize();
    }

    if (auto_scale) {
        ApplyAutoScaling();
    }

    UpdateSlideAnimation(io.DeltaTime);

    const float header_h = DP(56.0f);
    const float collapsed_h = header_h + DP(28.0f);
    const float margin_px = DP(12.0f);

    int panel_dp = tsm::state::ModState::Get().ui.panelWidthDp;
    panel_dp = std::clamp(panel_dp, 360, 720);
    const float kFixedWidth = DP((float)panel_dp);
    const float kFixedHeight = DP(545.0f);
    const float min_total_h = DP(545.0f);

    if (win_pos_.x < 0.0f || win_pos_.y < 0.0f) {
        const float init_w = kFixedWidth;
        const float init_h = collapsed_ ? collapsed_h : kFixedHeight;
        win_pos_ = ImVec2((io.DisplaySize.x - init_w) * 0.5f,
                          (io.DisplaySize.y - init_h) * 0.5f);
    }

    win_size_.x = kFixedWidth;
    win_size_.y = std::max(min_total_h, vp->Size.y - margin_px * 2.0f);

    float anchor_x, off_x;
    if (dock_right_) {
        anchor_x = vp->Pos.x + vp->Size.x - win_size_.x - margin_px;
        off_x = vp->Pos.x + vp->Size.x + DP(4.0f);
    } else {
        anchor_x = vp->Pos.x + margin_px;
        off_x = vp->Pos.x - win_size_.x - DP(4.0f);
    }

    const float slide_x = off_x + (anchor_x - off_x) * slide_t_;
    win_pos_ = ImVec2(slide_x, vp->Pos.y + margin_px);

    ImGui::SetNextWindowPos(win_pos_, ImGuiCond_Always);
    ImGui::SetNextWindowSize(win_size_, ImGuiCond_Always);

    ImVec4 grip = accent_color;
    ImGui::PushStyleColor(ImGuiCol_ResizeGrip, ImVec4(grip.x, grip.y, grip.z, 0.55f));
    ImGui::PushStyleColor(ImGuiCol_ResizeGripHovered, ImVec4(grip.x, grip.y, grip.z, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_ResizeGripActive, ImVec4(grip.x, grip.y, grip.z, 1.00f));
    pushed_colors_ = 3;

    if (slide_t_ <= 0.0f) {
        RenderCollapsedHandle(accent_color);
        ImGui::PopStyleColor(pushed_colors_);
        pushed_colors_ = 0;
        return false;
    }

    ImVec4 bg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(bg.x, bg.y, bg.z, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    pushed_colors_++;
    pushed_vars_++;

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

    return ImGui::Begin("ThatSkyMod", menu_open, flags);
}

void WindowManager::EndFrame() {
    if (pushed_vars_ > 0) {
        ImGui::PopStyleVar(pushed_vars_);
        pushed_vars_ = 0;
    }
    if (pushed_colors_ > 0) {
        ImGui::PopStyleColor(pushed_colors_);
        pushed_colors_ = 0;
    }

    ImGui::End();

    if (slide_t_ > 0.0f) {
        RenderCloseHandle();
    }

    first_frame_ = false;
}

void WindowManager::RenderCloseHandle() {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    auto& ms = tsm::state::ModState::Get();
    int style = ms.ui.collapseHandleStyle;
    if (style < 0 || style > 4) style = 0;

    float handle_w = DP(40.0f);
    float handle_h = DP(96.0f);
    const float handle_gap = DP(16.0f);
    if (style >= 2) {
        handle_w = DP(44.0f);
        handle_h = DP(80.0f);
    }

    float y = vp->Pos.y + std::clamp(handle_y_norm_, 0.0f, 1.0f) * (vp->Size.y - handle_h);
    float gap_with_anim = handle_gap * slide_t_;
    float x = dock_right_ ? (win_pos_.x - handle_w - gap_with_anim)
                          : (win_pos_.x + win_size_.x + gap_with_anim);

    ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(handle_w, handle_h), ImGuiCond_Always);

    ImGuiWindowFlags hf = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground;

    if (ImGui::Begin("TSM_SlideCloseHandle", nullptr, hf)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetWindowPos();
        ImVec2 s = ImGui::GetWindowSize();

        if (style == 0) {
            float r = DP(12.0f);
            ImU32 bg = ImGui::GetColorU32(ImVec4(0, 0, 0, 0.35f));

            ImDrawFlags f = dock_right_ ? ImDrawFlags_RoundCornersRight : ImDrawFlags_RoundCornersLeft;
            dl->AddRectFilled(p, ImVec2(p.x + s.x, p.y + s.y), bg, r, f);

            float ah = DP(14.0f);
            ImU32 col = ImGui::GetColorU32(ImVec4(1, 1, 1, 0.9f));
            ImVec2 c = ImVec2(p.x + s.x * (dock_right_ ? 0.6f : 0.4f), p.y + s.y * 0.5f);

            if (dock_right_) {
                dl->AddTriangleFilled(
                    ImVec2(c.x - ah * 0.5f, c.y - ah),
                    ImVec2(c.x - ah * 0.5f, c.y + ah),
                    ImVec2(c.x + ah * 0.5f, c.y),
                    col
                );
            } else {
                dl->AddTriangleFilled(
                    ImVec2(c.x + ah * 0.5f, c.y - ah),
                    ImVec2(c.x + ah * 0.5f, c.y + ah),
                    ImVec2(c.x - ah * 0.5f, c.y),
                    col
                );
            }
        } else if (style == 1) {
            float r = DP(10.0f);
            ImU32 bg = ImGui::GetColorU32(ImVec4(0, 0, 0, 0.30f));
            ImVec4 acc = tsm::ui::GetAccentColor();
            ImU32 border = ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 0.75f));

            ImDrawFlags f = dock_right_ ? ImDrawFlags_RoundCornersRight : ImDrawFlags_RoundCornersLeft;
            dl->AddRectFilled(p, ImVec2(p.x + s.x, p.y + s.y), bg, r, f);
            dl->AddRect(p, ImVec2(p.x + s.x, p.y + s.y), border, r, f, DP(1.5f));

            float cx = dock_right_ ? (p.x + s.x * 0.35f) : (p.x + s.x * 0.65f);
            float top = p.y + s.y * 0.22f;
            float bottom = p.y + s.y * 0.78f;
            dl->AddLine(ImVec2(cx, top), ImVec2(cx, bottom), border, DP(3.0f));
        } else if (style == 2) {
            ImVec4 acc = tsm::ui::GetAccentColor();
            ImU32 bg = ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 0.85f));
            ImU32 border = ImGui::GetColorU32(ImVec4(1, 1, 1, 0.85f));
            float r = s.y * 0.5f;

            ImDrawFlags f = dock_right_ ? ImDrawFlags_RoundCornersRight : ImDrawFlags_RoundCornersLeft;
            dl->AddRectFilled(p, ImVec2(p.x + s.x, p.y + s.y), bg, r, f);
            dl->AddRect(p, ImVec2(p.x + s.x, p.y + s.y), border, r, f, DP(1.5f));

            float ah = DP(12.0f);
            ImU32 col = ImGui::GetColorU32(ImVec4(1, 1, 1, 0.95f));
            ImVec2 c = ImVec2(p.x + s.x * (dock_right_ ? 0.60f : 0.40f), p.y + s.y * 0.5f);

            if (dock_right_) {
                dl->AddTriangleFilled(
                    ImVec2(c.x - ah * 0.5f, c.y - ah),
                    ImVec2(c.x - ah * 0.5f, c.y + ah),
                    ImVec2(c.x + ah * 0.5f, c.y),
                    col
                );
            } else {
                dl->AddTriangleFilled(
                    ImVec2(c.x + ah * 0.5f, c.y - ah),
                    ImVec2(c.x + ah * 0.5f, c.y + ah),
                    ImVec2(c.x - ah * 0.5f, c.y),
                    col
                );
            }
        } else if (style == 3) {
            ImVec4 acc = tsm::ui::GetAccentColor();
            ImU32 bg = ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 0.85f));
            ImU32 border = ImGui::GetColorU32(ImVec4(1, 1, 1, 0.95f));

            float r = std::min(s.x, s.y) * 0.5f * 0.90f;
            ImVec2 c = ImVec2(p.x + s.x * 0.5f, p.y + s.y * 0.5f);

            dl->AddCircleFilled(c, r, bg, 48);
            dl->AddCircle(c, r, border, 48, DP(1.8f));

            float icon_sz = r * 1.15f;
            ImVec2 icon_pos = ImVec2(c.x - icon_sz * 0.5f,
                                     c.y - icon_sz * 0.5f);
            ImGui::SetCursorScreenPos(icon_pos);
            const std::string& iconName = ms.ui.collapseHandleIconName;
            const char* iconLabel = iconName.empty() ? "UiMenuArrowCollapse" : iconName.c_str();
            tsm::ui::widgets::Icon(iconLabel, icon_sz, ImVec4(1,1,1,1), "UiMenuArrowCollapse", nullptr);
        } else {
            ImVec4 acc = tsm::ui::GetAccentColor();
            ImU32 lineCol = ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 0.90f));

            float pad_x = s.x * 0.25f;
            float start_y = p.y + s.y * 0.30f;
            float spacing = s.y * 0.16f;
            for (int i = 0; i < 3; ++i) {
                float y = start_y + spacing * i;
                dl->AddLine(ImVec2(p.x + pad_x, y), ImVec2(p.x + s.x - pad_x, y), lineCol, DP(2.0f));
            }
        }

        ImGui::SetCursorPos(ImVec2(0, 0));
        if (ImGui::InvisibleButton("##close_panel", s)) {
            collapsed_ = true;
        }

        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImVec2 mp = ImGui::GetIO().MousePos;
            handle_y_norm_ = std::clamp((mp.y - vp->Pos.y - s.y * 0.5f) / (vp->Size.y - s.y), 0.0f, 1.0f);

            float edge = DP(60.0f);
            if (mp.x <= vp->Pos.x + edge) dock_right_ = false;
            else if (mp.x >= vp->Pos.x + vp->Size.x - edge) dock_right_ = true;
        }
    }
    ImGui::End();
}

void WindowManager::SyncToModState() {
    auto& ms = tsm::state::ModState::Get();
    ms.ui.dockRight = dock_right_;
    ms.ui.handleYNorm = handle_y_norm_;
}

void WindowManager::RenderCollapsedHandle(const ImVec4& accent_color) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    auto& ms = tsm::state::ModState::Get();
    int style = ms.ui.collapseHandleStyle;
    if (style < 0 || style > 4) style = 0;

    float handle_w = DP(40.0f);
    float handle_h = DP(96.0f);
    const float handle_gap = DP(16.0f);
    if (style >= 2) {
        handle_w = DP(44.0f);
        handle_h = DP(80.0f);
    }

    float y = vp->Pos.y + std::clamp(handle_y_norm_, 0.0f, 1.0f) * (vp->Size.y - handle_h);
    float x = dock_right_ ? (vp->Pos.x + vp->Size.x - handle_w - handle_gap)
                          : (vp->Pos.x + handle_gap);

    ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(handle_w, handle_h), ImGuiCond_Always);

    ImGuiWindowFlags hf = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground;

    if (ImGui::Begin("TSM_SlideHandle", nullptr, hf)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetWindowPos();
        ImVec2 s = ImGui::GetWindowSize();

        if (style == 0) {
            float r = DP(12.0f);
            ImU32 bg = ImGui::GetColorU32(ImVec4(0, 0, 0, 0.35f));

            ImDrawFlags f = dock_right_ ? ImDrawFlags_RoundCornersLeft : ImDrawFlags_RoundCornersRight;
            dl->AddRectFilled(p, ImVec2(p.x + s.x, p.y + s.y), bg, r, f);

            float ah = DP(14.0f);
            ImU32 col = ImGui::GetColorU32(ImVec4(1, 1, 1, 0.9f));
            ImVec2 c = ImVec2(p.x + s.x * (dock_right_ ? 0.4f : 0.6f), p.y + s.y * 0.5f);

            if (dock_right_) {
                dl->AddTriangleFilled(
                    ImVec2(c.x + ah * 0.5f, c.y - ah),
                    ImVec2(c.x + ah * 0.5f, c.y + ah),
                    ImVec2(c.x - ah * 0.5f, c.y),
                    col
                );
            } else {
                dl->AddTriangleFilled(
                    ImVec2(c.x - ah * 0.5f, c.y - ah),
                    ImVec2(c.x - ah * 0.5f, c.y + ah),
                    ImVec2(c.x + ah * 0.5f, c.y),
                    col
                );
            }
        } else if (style == 1) {
            float r = DP(10.0f);
            ImU32 bg = ImGui::GetColorU32(ImVec4(0, 0, 0, 0.30f));
            ImVec4 acc = accent_color;
            ImU32 border = ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 0.75f));

            ImDrawFlags f = dock_right_ ? ImDrawFlags_RoundCornersLeft : ImDrawFlags_RoundCornersRight;
            dl->AddRectFilled(p, ImVec2(p.x + s.x, p.y + s.y), bg, r, f);
            dl->AddRect(p, ImVec2(p.x + s.x, p.y + s.y), border, r, f, DP(1.5f));

            float cx = dock_right_ ? (p.x + s.x * 0.35f) : (p.x + s.x * 0.65f);
            float top = p.y + s.y * 0.22f;
            float bottom = p.y + s.y * 0.78f;
            dl->AddLine(ImVec2(cx, top), ImVec2(cx, bottom), border, DP(3.0f));
        } else if (style == 2) {
            ImVec4 acc = accent_color;
            ImU32 bg = ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 0.85f));
            ImU32 border = ImGui::GetColorU32(ImVec4(1, 1, 1, 0.85f));
            float r = s.y * 0.5f;

            ImDrawFlags f = dock_right_ ? ImDrawFlags_RoundCornersLeft : ImDrawFlags_RoundCornersRight;
            dl->AddRectFilled(p, ImVec2(p.x + s.x, p.y + s.y), bg, r, f);
            dl->AddRect(p, ImVec2(p.x + s.x, p.y + s.y), border, r, f, DP(1.5f));

            float ah = DP(12.0f);
            ImU32 col = ImGui::GetColorU32(ImVec4(1, 1, 1, 0.95f));
            ImVec2 c = ImVec2(p.x + s.x * (dock_right_ ? 0.40f : 0.60f), p.y + s.y * 0.5f);

            if (dock_right_) {
                dl->AddTriangleFilled(
                    ImVec2(c.x + ah * 0.5f, c.y - ah),
                    ImVec2(c.x + ah * 0.5f, c.y + ah),
                    ImVec2(c.x - ah * 0.5f, c.y),
                    col
                );
            } else {
                dl->AddTriangleFilled(
                    ImVec2(c.x - ah * 0.5f, c.y - ah),
                    ImVec2(c.x - ah * 0.5f, c.y + ah),
                    ImVec2(c.x + ah * 0.5f, c.y),
                    col
                );
            }
        } else if (style == 3) {
            ImVec4 acc = accent_color;
            ImU32 bg = ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 0.85f));
            ImU32 border = ImGui::GetColorU32(ImVec4(1, 1, 1, 0.95f));

            float r = std::min(s.x, s.y) * 0.5f * 0.90f;
            ImVec2 c = ImVec2(p.x + s.x * 0.5f, p.y + s.y * 0.5f);

            dl->AddCircleFilled(c, r, bg, 48);
            dl->AddCircle(c, r, border, 48, DP(1.8f));

            float icon_sz = r * 1.15f;
            ImVec2 icon_pos = ImVec2(c.x - icon_sz * 0.5f,
                                     c.y - icon_sz * 0.5f);
            ImGui::SetCursorScreenPos(icon_pos);
            const std::string& iconName = ms.ui.collapseHandleIconName;
            const char* iconLabel = iconName.empty() ? "UiMenuArrowCollapse" : iconName.c_str();
            tsm::ui::widgets::Icon(iconLabel, icon_sz, ImVec4(1,1,1,1), "UiMenuArrowCollapse", nullptr);
        } else {
            ImVec4 acc = accent_color;
            ImU32 lineCol = ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 0.90f));

            float pad_x = s.x * 0.25f;
            float start_y = p.y + s.y * 0.30f;
            float spacing = s.y * 0.16f;
            for (int i = 0; i < 3; ++i) {
                float y = start_y + spacing * i;
                dl->AddLine(ImVec2(p.x + pad_x, y), ImVec2(p.x + s.x - pad_x, y), lineCol, DP(2.0f));
            }
        }

        ImGui::SetCursorPos(ImVec2(0, 0));
        if (ImGui::InvisibleButton("##open_panel", s)) {
            collapsed_ = false;
        }

        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImVec2 mp = ImGui::GetIO().MousePos;
            handle_y_norm_ = std::clamp((mp.y - vp->Pos.y - s.y * 0.5f) / (vp->Size.y - s.y), 0.0f, 1.0f);

            float edge = DP(60.0f);
            if (mp.x <= vp->Pos.x + edge) dock_right_ = false;
            else if (mp.x >= vp->Pos.x + vp->Size.x - edge) dock_right_ = true;
        }
    }
    ImGui::End();
}

void WindowManager::UpdateSlideAnimation(float delta_time) {
    if (collapsed_ && slide_t_ == 0.0f) return;
    if (!collapsed_ && slide_t_ == 1.0f) return;

    const float anim_speed = 6.0f;
    if (collapsed_) {
        slide_t_ = std::max(0.0f, slide_t_ - delta_time * anim_speed);
    } else {
        slide_t_ = std::min(1.0f, slide_t_ + delta_time * anim_speed);
    }
}

void WindowManager::ApplyAutoScaling() {
    ImGuiIO& io = ImGui::GetIO();

    static ImVec2 last_display_size = ImVec2(0, 0);
    if (last_display_size.x == io.DisplaySize.x && last_display_size.y == io.DisplaySize.y) {
        return;
    }
    last_display_size = io.DisplaySize;

    const float base_w_dp = 430.0f;
    const float base_h_dp = 545.0f;
    const float margins_dp = 24.0f;
    const float px_per_dp_raw = ::tsm::ui::metrics::dp_raw(1.0f);
    const float avail_w_px = io.DisplaySize.x - margins_dp * px_per_dp_raw;
    const float avail_h_px = io.DisplaySize.y - margins_dp * px_per_dp_raw;

    float s_fit_w = (base_w_dp > 0.0f) ? (avail_w_px / (base_w_dp * px_per_dp_raw)) : 1.0f;
    float s_fit_h = (base_h_dp > 0.0f) ? (avail_h_px / (base_h_dp * px_per_dp_raw)) : 1.0f;
    float s_fit = std::min(s_fit_w, s_fit_h);

    const float target_w_frac = 0.75f;
    const float target_h_frac = 0.80f;
    float s_frac_w = (base_w_dp > 0.0f) ? ((io.DisplaySize.x * target_w_frac) / (base_w_dp * px_per_dp_raw)) : 1.0f;
    float s_frac_h = (base_h_dp > 0.0f) ? ((io.DisplaySize.y * target_h_frac) / (base_h_dp * px_per_dp_raw)) : 1.0f;
    float s_frac = std::min(s_frac_w, s_frac_h);

    float s = std::min(1.0f, std::min(s_fit, s_frac));
    s = std::clamp(s, 0.6f, 1.0f);
    modui::set_ui_scale(s);
}

}}
