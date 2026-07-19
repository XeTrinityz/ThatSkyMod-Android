#include <ui/core/ThemeManager.h>
#include <ui/core/colors.h>
#include <ui/data/UIData.h>
#include <Cipher/CipherUtils.h>
#include <algorithm>
#include <cmath>

namespace tsm { namespace ui {

ThemeManager& ThemeManager::Get() {
    static ThemeManager instance;
    return instance;
}

void ThemeManager::Initialize() {
    ApplyModernTheme();
}

void ThemeManager::UpdateRainbow(float delta_time, bool rainbow_enabled) {
    if (!rainbow_enabled && !rainbow_prev_) {
        return;
    }

    if (rainbow_enabled && !rainbow_prev_) {
        manual_accent_ = accent_color_;
        rainbow_index_ = FindNearestPaletteIndex(manual_accent_);
        rainbow_t_ = 0.0f;
    }
    else if (!rainbow_enabled && rainbow_prev_) {
        accent_color_ = manual_accent_;
        ApplyAccentColorsOnly();
    }

    if (rainbow_enabled) {
        const float per_step_sec = 3.0f;
        const int n = GetPaletteCount();
        if (n <= 0) return;

        rainbow_t_ += (per_step_sec > 0.0f) ? (delta_time / per_step_sec) : 1.0f;
        while (rainbow_t_ >= 1.0f) {
            rainbow_t_ -= 1.0f;
            rainbow_index_ = (rainbow_index_ + 1) % n;
        }

        int next = (rainbow_index_ + 1) % n;

        ImVec4 a_accent = kAccentPalette[rainbow_index_];
        ImVec4 b_accent = kAccentPalette[next];
        float t = rainbow_t_;

        accent_color_ = ImVec4(
            a_accent.x + (b_accent.x - a_accent.x) * t,
            a_accent.y + (b_accent.y - a_accent.y) * t,
            a_accent.z + (b_accent.z - a_accent.z) * t,
            1.0f
        );

        ApplyAccentColorsOnly();
    }

    rainbow_prev_ = rainbow_enabled;
}

void ThemeManager::SetAccentColor(const ImVec4& color) {
    accent_color_ = color;
    if (!rainbow_prev_) {
        manual_accent_ = color;
    }
    ApplyAccentColorsOnly();
}

ImVec4 ThemeManager::GetAccentColor() const {
    return accent_color_;
}

const ImVec4* ThemeManager::GetAccentPalette() const {
    return kAccentPalette;
}

int ThemeManager::GetPaletteCount() const {
    return sizeof(kAccentPalette) / sizeof(kAccentPalette[0]);
}

int ThemeManager::FindNearestPaletteIndex(const ImVec4& color) const {
    int best = 0;
    float best_dist = 1e9f;
    const int n = GetPaletteCount();

    for (int i = 0; i < n; ++i) {
        ImVec4 p = kAccentPalette[i];
        float dx = p.x - color.x;
        float dy = p.y - color.y;
        float dz = p.z - color.z;
        float dist = dx * dx + dy * dy + dz * dz;

        if (dist < best_dist) {
            best_dist = dist;
            best = i;
        }
    }

    return best;
}

void ThemeManager::ApplyModernTheme() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 0.0f;
    s.FrameRounding = 6.0f;
    s.GrabRounding = 6.0f;
    s.TabRounding = 6.0f;
    s.ChildRounding = 12.0f;
    s.ScrollbarRounding = 6.0f;
    s.ScrollbarSize = s.ScrollbarSize * 0.7f;
    s.FrameBorderSize = 0.0f;
    s.WindowBorderSize = 0.0f;
    s.ChildBorderSize = 1.0f;
    s.DisplaySafeAreaPadding = ImVec2(0, 0);
    s.DisplayWindowPadding = ImVec2(0, 0);
    s.ItemSpacing = ImVec2(10, 8);
    s.ItemInnerSpacing = ImVec2(8, 6);
    s.IndentSpacing = 14.0f;

    using modui::Color;
    const Color Primary = Color(MODUI_COLOR_HEX(0xFF0B835F));
    const Color OnPrimary = Color(MODUI_COLOR_HEX(0xFF0A0A0A));
    const Color PrimaryContainer = Color(MODUI_COLOR_HEX(0xFF0E6B52));
    const Color OnPrimaryContainer = Color(MODUI_COLOR_HEX(0xFFE8F6F1));

    const Color Secondary = Color(MODUI_COLOR_HEX(0xFF3A9C80));
    const Color OnSecondary = Color(MODUI_COLOR_HEX(0xFF0A0A0A));
    const Color SecondaryContainer = Color(MODUI_COLOR_HEX(0xFF145A45));
    const Color OnSecondaryContainer = Color(MODUI_COLOR_HEX(0xFFE8F6F1));

    const Color Tertiary = Color(MODUI_COLOR_RGBA(0x7D, 0x52, 0x60, 0xFF));
    const Color OnTertiary = Color(MODUI_COLOR_RGBA(0xFF, 0xFF, 0xFF, 0xFF));
    const Color TertiaryContainer = Color(MODUI_COLOR_RGBA(0xFF, 0xD8, 0xE4, 0xFF));
    const Color OnTertiaryContainer = Color(MODUI_COLOR_RGBA(0x31, 0x11, 0x1D, 0xFF));

    const Color Error = Color(MODUI_COLOR_RGBA(0xB3, 0x26, 0x1E, 0xFF));
    const Color OnError = Color(MODUI_COLOR_RGBA(0xFF, 0xFF, 0xFF, 0xFF));
    const Color ErrorContainer = Color(MODUI_COLOR_RGBA(0xF9, 0xDE, 0xDC, 0xFF));
    const Color OnErrorContainer = Color(MODUI_COLOR_RGBA(0x41, 0x0E, 0x0B, 0xFF));

    const Color Surface = Color(MODUI_COLOR_HEX(0xFF1C2023));
    const Color OnSurface = Color(MODUI_COLOR_HEX(0xFFE0E5E8));
    const Color SurfaceVariant = Color(MODUI_COLOR_HEX(0xFF262B2F));
    const Color OnSurfaceVariant = Color(MODUI_COLOR_HEX(0xFFB3BEC4));
    const Color SurfaceContainerHighest = Color(MODUI_COLOR_HEX(0xFF262B2E));
    const Color SurfaceContainerHigh = Color(MODUI_COLOR_HEX(0xFF23282B));
    const Color SurfaceContainer = Color(MODUI_COLOR_HEX(0xFF212528));
    const Color SurfaceContainerLow = Color(MODUI_COLOR_HEX(0xFF1F2326));
    const Color SurfaceContainerLowest = Color(MODUI_COLOR_HEX(0xFF1C2023));
    const Color InverseSurface = Color(MODUI_COLOR_HEX(0xFFE7ECEF));
    const Color InverseOnSurface = Color(MODUI_COLOR_HEX(0xFF1A1E21));
    const Color SurfaceTint = Primary;

    const Color Outline = Color(MODUI_COLOR_HEX(0xFF445057));
    const Color OutlineVariant = Color(MODUI_COLOR_HEX(0xFF2A3136));

    auto ToImVec4 = [](modui::Color color) -> ImVec4 {
        modui::Col32 v = color.get();
        float r = ((v >> 24) & 0xFF) / 255.0f;
        float g = ((v >> 16) & 0xFF) / 255.0f;
        float b = ((v >> 8) & 0xFF) / 255.0f;
        float a = ((v >> 0) & 0xFF) / 255.0f;
        return ImVec4(r, g, b, a);
    };

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg] = ToImVec4(Surface);
    {
        ImVec4 sc = ToImVec4(SurfaceContainer);
        sc.w = 0.8f;
        c[ImGuiCol_ChildBg] = sc;
    }
    c[ImGuiCol_PopupBg] = ToImVec4(SurfaceContainerHigh);
    c[ImGuiCol_Border] = ToImVec4(Outline);
    c[ImGuiCol_Text] = ToImVec4(OnSurface);
    c[ImGuiCol_TextDisabled] = ToImVec4(OnSurfaceVariant);

    ApplyAccentColorsOnly();

    c[ImGuiCol_FrameBg] = ToImVec4(SurfaceContainerLow);
    c[ImGuiCol_FrameBgHovered] = ToImVec4(SurfaceContainer);
    c[ImGuiCol_FrameBgActive] = ToImVec4(SurfaceContainerHigh);
    c[ImGuiCol_TitleBg] = ToImVec4(SurfaceContainerLowest);
    c[ImGuiCol_TitleBgActive] = ToImVec4(SurfaceContainer);
    c[ImGuiCol_TitleBgCollapsed] = ToImVec4(SurfaceContainerLowest);
    c[ImGuiCol_MenuBarBg] = ToImVec4(SurfaceContainer);
    c[ImGuiCol_Tab] = ToImVec4(SurfaceContainer);
    c[ImGuiCol_TabHovered] = ToImVec4(SurfaceContainerHighest);
    c[ImGuiCol_TabActive] = ToImVec4(PrimaryContainer);
    c[ImGuiCol_TabUnfocused] = ToImVec4(SurfaceContainerLow);
    c[ImGuiCol_TabUnfocusedActive] = ToImVec4(SurfaceContainer);
    c[ImGuiCol_PlotLines] = ToImVec4(Primary);
    c[ImGuiCol_PlotLinesHovered] = ToImVec4(OnPrimary);
    c[ImGuiCol_PlotHistogram] = ToImVec4(Primary);
    c[ImGuiCol_PlotHistogramHovered] = ToImVec4(OnPrimary);
    c[ImGuiCol_TableHeaderBg] = ToImVec4(SurfaceContainerHigh);
    c[ImGuiCol_TableBorderStrong] = ToImVec4(Outline);
    c[ImGuiCol_TableBorderLight] = ToImVec4(OutlineVariant);
    c[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt] = ToImVec4(SurfaceContainerLowest);
    c[ImGuiCol_TextSelectedBg] = ToImVec4(PrimaryContainer);
    c[ImGuiCol_DragDropTarget] = ToImVec4(Primary);
    c[ImGuiCol_NavHighlight] = ToImVec4(Primary);
    c[ImGuiCol_NavWindowingHighlight] = ToImVec4(OnSurface);
    c[ImGuiCol_NavWindowingDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.50f);
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.10f, 0.10f, 0.10f, 0.60f);
}

void ThemeManager::ApplyAccentColorsOnly() {
    ImGuiStyle& s = ImGui::GetStyle();
    ImVec4 acc = accent_color_;

    s.Colors[ImGuiCol_Header] = ImVec4(acc.x, acc.y, acc.z, 0.35f);
    s.Colors[ImGuiCol_HeaderHovered] = ImVec4(acc.x, acc.y, acc.z, 0.60f);
    s.Colors[ImGuiCol_HeaderActive] = ImVec4(acc.x, acc.y, acc.z, 0.80f);

    s.Colors[ImGuiCol_Separator] = ImVec4(acc.x, acc.y, acc.z, 0.40f);
    s.Colors[ImGuiCol_SeparatorHovered] = ImVec4(acc.x, acc.y, acc.z, 0.70f);
    s.Colors[ImGuiCol_SeparatorActive] = ImVec4(acc.x, acc.y, acc.z, 1.00f);

    s.Colors[ImGuiCol_ResizeGrip] = ImVec4(acc.x, acc.y, acc.z, 0.25f);
    s.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(acc.x, acc.y, acc.z, 0.67f);
    s.Colors[ImGuiCol_ResizeGripActive] = ImVec4(acc.x, acc.y, acc.z, 0.95f);

    s.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.15f, 0.15f, 0.15f, 0.3f);
    s.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(acc.x, acc.y, acc.z, 0.50f);
    s.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(acc.x, acc.y, acc.z, 0.70f);
    s.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(acc.x, acc.y, acc.z, 0.90f);

    s.Colors[ImGuiCol_SliderGrab] = acc;
    s.Colors[ImGuiCol_SliderGrabActive] = ImVec4(acc.x * 0.8f, acc.y * 0.8f, acc.z * 0.8f, 1.0f);
    s.Colors[ImGuiCol_CheckMark] = acc;

    s.Colors[ImGuiCol_Button] = ImVec4(acc.x, acc.y, acc.z, 0.40f);
    s.Colors[ImGuiCol_ButtonHovered] = ImVec4(acc.x, acc.y, acc.z, 0.60f);
    s.Colors[ImGuiCol_ButtonActive] = ImVec4(acc.x, acc.y, acc.z, 0.80f);
}

}}
