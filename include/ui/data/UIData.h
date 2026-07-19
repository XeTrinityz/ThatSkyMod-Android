#pragma once

#include <imgui/imgui.h>

namespace tsm { namespace ui { namespace data {

static const ImVec4 kAccentColorPalette[] = {
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
    ImVec4(0.039f, 0.463f, 0.336f, 1.0f),
    ImVec4(0.07f, 0.63f, 0.54f, 1.0f),
    ImVec4(0.18f, 0.78f, 0.64f, 1.0f),
    ImVec4(0.95f, 0.46f, 0.10f, 1.0f),
    ImVec4(0.91f, 0.19f, 0.55f, 1.0f),
    ImVec4(0.65f, 0.74f, 0.99f, 1.0f),
    ImVec4(0.98f, 0.72f, 0.22f, 1.0f),
    ImVec4(0.40f, 0.80f, 0.90f, 1.0f),
    ImVec4(0.55f, 0.87f, 0.30f, 1.0f),
    ImVec4(0.98f, 0.36f, 0.30f, 1.0f),
    ImVec4(0.43f, 0.58f, 0.18f, 1.0f),
};

constexpr int kAccentColorPaletteSize = sizeof(kAccentColorPalette) / sizeof(kAccentColorPalette[0]);

static const ImVec4 kBackgroundColorPalette[] = {
    ImVec4(0.11f, 0.13f, 0.14f, 1.0f),
    ImVec4(0.08f, 0.08f, 0.08f, 1.0f),
    ImVec4(0.10f, 0.10f, 0.12f, 1.0f),
    ImVec4(0.12f, 0.10f, 0.12f, 1.0f),
    ImVec4(0.12f, 0.10f, 0.10f, 1.0f),
    ImVec4(0.10f, 0.12f, 0.10f, 1.0f),
    ImVec4(0.09f, 0.11f, 0.12f, 1.0f),
    ImVec4(0.15f, 0.15f, 0.15f, 1.0f),
};

constexpr int kBackgroundColorPaletteSize = sizeof(kBackgroundColorPalette) / sizeof(kBackgroundColorPalette[0]);

static const char* kSkyClockPositions[] = {
    "Top-Left",
    "Top-Center",
    "Top-Right",
    "Bottom-Left",
    "Bottom-Center",
    "Bottom-Right"
};

constexpr int kSkyClockPositionCount = sizeof(kSkyClockPositions) / sizeof(kSkyClockPositions[0]);

}}}
