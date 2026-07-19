#pragma once

#include <imgui/imgui.h>

namespace tsm { namespace ui {

class ThemeManager {
public:
    static ThemeManager& Get();

    ThemeManager(const ThemeManager&) = delete;
    ThemeManager& operator=(const ThemeManager&) = delete;

    void Initialize();

    void UpdateRainbow(float delta_time, bool rainbow_enabled);


    void SetAccentColor(const ImVec4& color);

    ImVec4 GetAccentColor() const;

    const ImVec4* GetAccentPalette() const;

    int GetPaletteCount() const;

    int FindNearestPaletteIndex(const ImVec4& color) const;

private:
    ThemeManager() = default;
    ~ThemeManager() = default;

    void ApplyModernTheme();
    void ApplyAccentColorsOnly();

    ImVec4 accent_color_{0.00f, 0.75f, 0.50f, 1.0f};
    ImVec4 manual_accent_{0.00f, 0.75f, 0.50f, 1.0f};

    bool rainbow_prev_{false};
    int rainbow_index_{0};
    float rainbow_t_{0.0f};

    static constexpr ImVec4 kAccentPalette[] = {
        ImVec4(0.00f, 0.75f, 0.50f, 1.0f),
        ImVec4(0.12f, 0.47f, 0.95f, 1.0f),
        ImVec4(0.29f, 0.39f, 0.95f, 1.0f),
        ImVec4(0.60f, 0.40f, 0.80f, 1.0f),
        ImVec4(0.92f, 0.33f, 0.71f, 1.0f),
        ImVec4(0.95f, 0.26f, 0.21f, 1.0f),
        ImVec4(1.00f, 0.59f, 0.00f, 1.0f),
        ImVec4(1.00f, 0.76f, 0.03f, 1.0f),
        ImVec4(0.80f, 0.86f, 0.22f, 1.0f),
        ImVec4(0.30f, 0.69f, 0.31f, 1.0f),
        ImVec4(0.00f, 0.74f, 0.83f, 1.0f),
        ImVec4(0.26f, 0.54f, 0.96f, 1.0f),
    };
};

}}
