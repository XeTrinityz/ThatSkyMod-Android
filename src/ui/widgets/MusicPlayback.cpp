#include <ui/widgets/MusicPlayback.h>
#include <ui/widgets/Icons.h>
#include <ui/widgets/Cards.h>
#include <ui/widgets/Buttons.h>
#include <ui/widgets/Controls.h>
#include <ui/widgets/WidgetInternal.h>
#include <ui/core/metrics.h>
#include <iconloader/IconLoader.h>
#include <imgui/imgui.h>
#include <algorithm>
#include <cstdio>

namespace tsm { namespace ui { namespace widgets {

static void FormatTime(int milliseconds, char* buffer, size_t bufferSize) {
    int totalSeconds = milliseconds / 1000;
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;
    snprintf(buffer, bufferSize, "%d:%02d", minutes, seconds);
}

static bool TransportButton(const char* id, const char* iconName, float size, bool accentBg, bool flipHoriz = false) {
    ImGui::PushID(id);
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool pressed = ImGui::InvisibleButton("##btn", ImVec2(size, size));
    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec4 acc = GetAccentColor();
    float radius = size * 0.5f;
    ImVec2 center = ImVec2(p.x + radius, p.y + radius);

    ImVec4 fillNormal, fillHover, fillActive;
    if (accentBg) {
        fillNormal = ImVec4(acc.x, acc.y, acc.z, 0.85f);
        fillHover  = ImVec4(acc.x, acc.y, acc.z, 1.0f);
        fillActive = ImVec4(acc.x * 0.8f, acc.y * 0.8f, acc.z * 0.8f, 1.0f);
    } else {
        fillNormal = ImVec4(1.0f, 1.0f, 1.0f, 0.08f);
        fillHover  = ImVec4(1.0f, 1.0f, 1.0f, 0.14f);
        fillActive = ImVec4(1.0f, 1.0f, 1.0f, 0.22f);
    }
    ImVec4 fill = active ? fillActive : (hovered ? fillHover : fillNormal);
    dl->AddCircleFilled(center, radius, ImGui::GetColorU32(fill), 48);

    if (!accentBg && (hovered || active)) {
        float ringAlpha = active ? 0.25f : 0.12f;
        dl->AddCircle(center, radius, ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, ringAlpha)), 48, 1.5f);
    }

    float iconSize = size * 0.50f;
    ImVec2 iconMin = ImVec2(center.x - iconSize * 0.5f, center.y - iconSize * 0.5f);
    ImVec4 iconColor = accentBg ? ImVec4(1, 1, 1, 1) : ImVec4(1, 1, 1, 0.90f);

    ImGui::SetCursorScreenPos(iconMin);
    if (flipHoriz) {
        IconRotated(iconName, iconSize, 180.0f, iconColor);
    } else {
        Icon(iconName, iconSize, iconColor);
    }

    ImGui::PopID();
    return pressed;
}

static void DrawProgressBar(float fraction, float width, float height, float progressPadding) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec4 acc = GetAccentColor();
    float rounding = height * 0.5f;

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + progressPadding);
    ImVec2 p = ImGui::GetCursorScreenPos();
    float barW = width - progressPadding * 2.0f;

    ImVec4 trackColor = ImVec4(1.0f, 1.0f, 1.0f, 0.08f);
    dl->AddRectFilled(p, ImVec2(p.x + barW, p.y + height), ImGui::GetColorU32(trackColor), rounding);

    float fillW = std::max(height, barW * std::clamp(fraction, 0.0f, 1.0f));
    if (fraction > 0.001f) {
        dl->AddRectFilled(p, ImVec2(p.x + fillW, p.y + height), ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 0.85f)), rounding);
    }

    ImGui::Dummy(ImVec2(barW, height));
}

PlaybackAction DrawPlaybackControls(bool isPlaying, bool isPaused, float progress,
                                     const char* songName, int currentTimeMs, int totalTimeMs,
                                     bool canNavigate, bool* autoPlayNext, bool* shuffle) {
    PlaybackAction action = PlaybackAction::None;
    ImVec4 acc = GetAccentColor();

    float contentWidth = ImGui::GetContentRegionAvail().x;
    float pad = DP(10.0f);

    {
        ImGui::Dummy(ImVec2(0, DP(2.0f)));

        float iconSize = DP(18.0f);
        ImVec2 iconPos = ImGui::GetCursorScreenPos();
        iconPos.x += pad;
        ImGui::SetCursorScreenPos(iconPos);
        Icon("UiMiscMusicNote", iconSize, ImVec4(acc.x, acc.y, acc.z, 0.85f));

        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + DP(4.0f));

        float maxNameW = contentWidth - pad * 2.0f - iconSize - DP(12.0f);
        const char* displayName = (songName && songName[0] != '\0') ? songName : "No song selected";

        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + maxNameW);
        if (isPlaying || isPaused) {
            ImGui::SetWindowFontScale(1.15f);
            ImGui::TextUnformatted(displayName);
            ImGui::SetWindowFontScale(1.0f);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 0.45f));
            ImGui::SetWindowFontScale(1.15f);
            ImGui::TextUnformatted(displayName);
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();
        }
        ImGui::PopTextWrapPos();

        ImGui::Dummy(ImVec2(0, DP(4.0f)));
    }

    {
        float barHeight = DP(4.0f);
        DrawProgressBar(progress, contentWidth, barHeight, pad);
    }

    if (totalTimeMs > 0) {
        ImGui::Dummy(ImVec2(0, DP(1.0f)));

        char currentStr[16];
        char totalStr[16];
        FormatTime(currentTimeMs, currentStr, sizeof(currentStr));
        FormatTime(totalTimeMs, totalStr, sizeof(totalStr));

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 0.45f));
        ImGui::SetWindowFontScale(0.92f);

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + pad);
        ImGui::Text("%s", currentStr);

        ImGui::SameLine();
        float slotW = ImGui::CalcTextSize("00:00").x;
        float rightEdge = ImGui::GetCursorPosX() + contentWidth - pad * 2.0f - slotW
                          - ImGui::CalcTextSize(currentStr).x - DP(8.0f);
        if (rightEdge > ImGui::GetCursorPosX()) {
            ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMin().x + contentWidth - pad - slotW);
        }
        ImGui::Text("%s", totalStr);

        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
    } else {
        ImGui::Dummy(ImVec2(0, DP(2.0f)));
    }

    {
        ImGui::Dummy(ImVec2(0, DP(4.0f)));

        float mainBtnSize  = DP(44.0f);
        float smallBtnSize = DP(36.0f);
        float toggleSize   = DP(28.0f);
        float spacing      = DP(12.0f);

        float totalW = smallBtnSize + spacing + mainBtnSize + spacing + smallBtnSize;
        if (shuffle) totalW += spacing + toggleSize;
        if (autoPlayNext) totalW += spacing + toggleSize;
        float offsetX = (contentWidth - totalW) * 0.5f;

        ImVec2 rowStart = ImGui::GetCursorPos();
        float centerY = rowStart.y + (mainBtnSize - smallBtnSize) * 0.5f;
        float toggleCenterY = rowStart.y + (mainBtnSize - toggleSize) * 0.5f;

        float curX = rowStart.x + offsetX;

        if (shuffle) {
            ImGui::SetCursorPos(ImVec2(curX, toggleCenterY));
            ToggleButton("shuffle", "UiMenuRandomGenerator", shuffle, toggleSize);
            curX += toggleSize + spacing;
        }

        if (!canNavigate) ImGui::BeginDisabled();
        ImGui::SetCursorPos(ImVec2(curX, centerY));
        if (TransportButton("prev", "UiMenuSkipNext", smallBtnSize, false, true)) {
            action = PlaybackAction::Previous;
        }
        if (!canNavigate) ImGui::EndDisabled();
        curX += smallBtnSize + spacing;

        const char* ppIcon = (isPlaying && !isPaused) ? "UiMiscMinus" : "UiMiscArrowRight";
        ImGui::SetCursorPos(ImVec2(curX, rowStart.y));
        if (TransportButton("playpause", ppIcon, mainBtnSize, true)) {
            action = (isPlaying && !isPaused) ? PlaybackAction::Pause : PlaybackAction::Play;
        }
        curX += mainBtnSize + spacing;

        if (!canNavigate) ImGui::BeginDisabled();
        ImGui::SetCursorPos(ImVec2(curX, centerY));
        if (TransportButton("next", "UiMenuSkipNext", smallBtnSize, false)) {
            action = PlaybackAction::Next;
        }
        if (!canNavigate) ImGui::EndDisabled();
        curX += smallBtnSize + spacing;

        if (autoPlayNext) {
            ImGui::SetCursorPos(ImVec2(curX, toggleCenterY));
            ToggleButton("auto_play_next", "UiMenuUndo", autoPlayNext, toggleSize);
        }

        ImGui::SetCursorPos(ImVec2(rowStart.x, rowStart.y + mainBtnSize + DP(2.0f)));

        if (isPlaying || isPaused) {
            ImGui::Dummy(ImVec2(0, DP(4.0f)));
            if (StandardButton("Stop", false)) {
                action = PlaybackAction::Stop;
            }
        }
    }

    return action;
}

}}}
