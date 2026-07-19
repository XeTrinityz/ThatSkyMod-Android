#include <ui/overlays/MusicPlaybackOverlay.h>
#include <ui/core/App.h>
#include <ui/widgets/Cards.h>
#include <ui/widgets/Controls.h>
#include <ui/widgets/Buttons.h>
#include <ui/widgets/MusicPlayback.h>
#include <ui/widgets/Icons.h>
#include <ui/core/metrics.h>
#include <ui/core/Localization.h>
#include <iconloader/IconLoader.h>
#include <game/hooks/MusicKeyHook.h>
#include <features/music/MusicSheetPlayer.h>
#include <state/ModState.h>
#include <imgui/imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>

namespace tsm { namespace ui { namespace overlays {

static std::vector<tsm::utils::storage::MusicSheet> s_sheets;
static int s_selectedIndex = -1;

void UpdateMusicPlaybackState(const std::vector<tsm::utils::storage::MusicSheet>& sheets,
                              int selectedIndex) {
    s_sheets = sheets;
    s_selectedIndex = selectedIndex;
}

static int GetSelectedIndex() { return s_selectedIndex; }
static void SetSelectedIndex(int i) { s_selectedIndex = i; }

int GetMusicPlaybackOverlaySelectedIndex() {
    return s_selectedIndex;
}

static bool OverlaySlider(const char* id, const char* label, const char* iconName,
                          float* value, float vMin, float vMax, const char* format) {
    using namespace tsm::ui::widgets;
    ImVec4 acc = tsm::ui::GetAccentColor();

    auto WithAlpha = [](const ImVec4& c, float a) {
        return ImVec4(c.x, c.y, c.z, a);
    };

    float contentW = ImGui::GetContentRegionAvail().x;
    float pad = DP(10.0f);
    float iconSize = DP(16.0f);

    ImVec2 rowStart = ImGui::GetCursorScreenPos();

    ImGui::SetCursorScreenPos(ImVec2(rowStart.x + pad, rowStart.y + 1.0f));
    Icon(iconName, iconSize, WithAlpha(acc, 0.7f));

    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + DP(4.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 0.65f));
    ImGui::SetWindowFontScale(0.95f);
    ImGui::TextUnformatted(tsm::ui::i18n::Tr(label));
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    char valBuf[32];
    snprintf(valBuf, sizeof(valBuf), format, *value);
    ImVec2 valSize = ImGui::CalcTextSize(valBuf);
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMin().x + contentW - pad - valSize.x);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 0.50f));
    ImGui::SetWindowFontScale(0.95f);
    ImGui::TextUnformatted(valBuf);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, DP(2.0f)));

    float sliderPad = pad;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + sliderPad);
    ImGui::PushItemWidth(contentW - sliderPad * 2.0f);

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(1.0f, 1.0f, 1.0f, 0.06f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.10f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(1.0f, 1.0f, 1.0f, 0.10f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(acc.x, acc.y, acc.z, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(acc.x, acc.y, acc.z, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 3.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 10.0f);

    bool changed = ImGui::SliderFloat(id, value, vMin, vMax, "");
    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(5);
    ImGui::PopItemWidth();

    return changed;
}

void RenderMusicPlaybackOverlay() {
    using namespace tsm::ui::widgets;

    auto& ms = tsm::state::ModState::Get();
    if (!ms.ui.showMusicPlaybackOverlay) return;

    auto WithAlpha = [](const ImVec4& c, float a) {
        return ImVec4(c.x, c.y, c.z, a);
    };

    int keyCount = tsm::game::hooks::musickey::GetRecordedKeyCount();
    auto& player = tsm::features::music::MusicSheetPlayer::Get();
    bool canPlay = (keyCount >= 15);
    bool isPlaying = player.IsPlaying();
    bool isPaused = player.IsPaused();

    const char* songName = "No song selected";
    int currentTimeMs = 0;
    int totalTimeMs = 0;
    float progress = 0.0f;

    bool hasValidSheet = (GetSelectedIndex() >= 0 &&
                          GetSelectedIndex() < static_cast<int>(s_sheets.size()));

    if (isPlaying && hasValidSheet) {
        songName = s_sheets[GetSelectedIndex()].name.c_str();
        currentTimeMs = player.GetCurrentTime();
        totalTimeMs = player.GetTotalTime();
        progress = player.GetProgress();
    }

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowSize(ImVec2(DP(340.0f), 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowPos(
        ImVec2(vp->WorkPos.x + vp->WorkSize.x - DP(360.0f), vp->WorkPos.y + DP(80.0f)),
        ImGuiCond_FirstUseEver);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(DP(14.0f), DP(14.0f)));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                             ImGuiWindowFlags_AlwaysAutoResize;

    if (ImGui::Begin("Playback Overlay##music", nullptr, flags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 winPos = ImGui::GetWindowPos();
        ImVec2 winSize = ImGui::GetWindowSize();
        ImVec4 acc = tsm::ui::GetAccentColor();

        float rounding = DP(14.0f);
        ImVec4 panelBg = ImGui::GetStyle().Colors[ImGuiCol_FrameBg];
        panelBg.w = 0.92f;

        ImVec2 panelMax = ImVec2(winPos.x + winSize.x, winPos.y + winSize.y);
        dl->AddRectFilled(winPos, panelMax, ImGui::GetColorU32(panelBg), rounding);

        float bandH = std::min<float>(DP(60.0f), winSize.y * 0.25f);
        ImU32 gradTop = ImGui::GetColorU32(WithAlpha(acc, 0.12f));
        ImU32 gradBot = ImGui::GetColorU32(WithAlpha(acc, 0.0f));
        dl->AddRectFilledMultiColor(winPos, ImVec2(panelMax.x, winPos.y + bandH),
                                    gradTop, gradTop, gradBot, gradBot);

        dl->AddRect(winPos, panelMax, ImGui::GetColorU32(ImVec4(1, 1, 1, 0.06f)), rounding, 0, 1.0f);

        if (!canPlay) ImGui::BeginDisabled();

        auto playbackAction = DrawPlaybackControls(
            isPlaying, isPaused, progress, songName,
            currentTimeMs, totalTimeMs, !s_sheets.empty(),
            &ms.ui.musicAutoPlayNext, &ms.ui.musicShuffle);

        if (!canPlay) ImGui::EndDisabled();

        auto pickRandomIndex = []() -> int {
            int n = static_cast<int>(s_sheets.size());
            if (n <= 0) return -1;
            static std::mt19937 rng(std::random_device{}());
            std::uniform_int_distribution<int> dist(0, n - 1);
            return dist(rng);
        };

        switch (playbackAction) {
        case PlaybackAction::Play:
            if (isPlaying && isPaused) {
                player.Resume();
            } else if (hasValidSheet && canPlay) {
                player.SetBPMMultiplier(std::clamp(ms.ui.musicBpmMultiplier, 0.5f, 2.0f));
                player.Play(s_sheets[GetSelectedIndex()]);
            } else if (!s_sheets.empty() && canPlay) {
                int targetIndex = ms.ui.musicShuffle ? pickRandomIndex() : 0;
                if (targetIndex >= 0) {
                    SetSelectedIndex(targetIndex);
                    player.SetBPMMultiplier(std::clamp(ms.ui.musicBpmMultiplier, 0.5f, 2.0f));
                    player.Play(s_sheets[targetIndex]);
                }
            }
            break;
        case PlaybackAction::Pause:
            player.Pause();
            break;
        case PlaybackAction::Stop:
            player.Stop();
            SetSelectedIndex(-1);
            break;
        case PlaybackAction::Previous:
            if (!s_sheets.empty()) {
                bool wasPlaying = isPlaying;
                if (isPlaying) player.Stop();
                if (ms.ui.musicShuffle) {
                    SetSelectedIndex(pickRandomIndex());
                } else {
                    int idx = GetSelectedIndex();
                    if (idx < 0) idx = 0;
                    else idx--;
                    if (idx < 0) idx = static_cast<int>(s_sheets.size()) - 1;
                    SetSelectedIndex(idx);
                }
                if (wasPlaying && GetSelectedIndex() >= 0 && canPlay) {
                    player.SetBPMMultiplier(std::clamp(ms.ui.musicBpmMultiplier, 0.5f, 2.0f));
                    player.Play(s_sheets[GetSelectedIndex()]);
                }
            }
            break;
        case PlaybackAction::Next:
            if (!s_sheets.empty()) {
                bool wasPlaying = isPlaying;
                if (isPlaying) player.Stop();
                if (ms.ui.musicShuffle) {
                    SetSelectedIndex(pickRandomIndex());
                } else {
                    int idx = GetSelectedIndex();
                    if (idx < 0) idx = 0;
                    else idx++;
                    if (idx >= static_cast<int>(s_sheets.size())) idx = 0;
                    SetSelectedIndex(idx);
                }
                if (wasPlaying && GetSelectedIndex() >= 0 && canPlay) {
                    player.SetBPMMultiplier(std::clamp(ms.ui.musicBpmMultiplier, 0.5f, 2.0f));
                    player.Play(s_sheets[GetSelectedIndex()]);
                }
            }
            break;
        default:
            break;
        }


        if (keyCount < 15) {
            ImGui::Dummy(ImVec2(0, DP(4.0f)));

            float pillPad = DP(10.0f);
            ImVec2 pillStart = ImGui::GetCursorScreenPos();
            pillStart.x += pillPad;
            float pillW = ImGui::GetContentRegionAvail().x - pillPad * 2.0f;
            float pillH = DP(42.0f);

            dl->AddRectFilled(pillStart, ImVec2(pillStart.x + pillW, pillStart.y + pillH),
                              ImGui::GetColorU32(ImVec4(1.0f, 0.75f, 0.2f, 0.10f)), DP(8.0f));
            dl->AddRect(pillStart, ImVec2(pillStart.x + pillW, pillStart.y + pillH),
                        ImGui::GetColorU32(ImVec4(1.0f, 0.75f, 0.2f, 0.25f)), DP(8.0f), 0, 1.0f);

            float iconSz = DP(16.0f);
            float textY = pillStart.y + (pillH - ImGui::GetTextLineHeight() * 2 - 2.0f) * 0.5f;
            ImGui::SetCursorScreenPos(ImVec2(pillStart.x + DP(10.0f), pillStart.y + (pillH - iconSz) * 0.5f));
            Icon("UiMiscQuestion", iconSz, ImVec4(1.0f, 0.85f, 0.4f, 1.0f));

            ImGui::SetCursorScreenPos(ImVec2(pillStart.x + DP(10.0f) + iconSz + DP(6.0f), textY));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.92f, 0.7f, 0.90f));
            ImGui::SetWindowFontScale(0.95f);
            ImGui::TextUnformatted(tsm::ui::i18n::Tr("Record all 15 keys to play"));
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();

            char keysBuf[32];
            snprintf(keysBuf, sizeof(keysBuf), "%d / 15 recorded", keyCount);
            ImGui::SetCursorScreenPos(ImVec2(pillStart.x + DP(10.0f) + iconSz + DP(6.0f), textY + ImGui::GetTextLineHeight() + 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.5f, 0.55f));
            ImGui::SetWindowFontScale(0.88f);
            ImGui::TextUnformatted(keysBuf);
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();

            ImGui::SetCursorScreenPos(ImVec2(pillStart.x - pillPad, pillStart.y + pillH));
            ImGui::Dummy(ImVec2(0, DP(4.0f)));
        }

        ImGui::Dummy(ImVec2(0, DP(4.0f)));
        {
            float sepPad = DP(10.0f);
            ImVec2 sp = ImGui::GetCursorScreenPos();
            dl->AddLine(
                ImVec2(sp.x + sepPad, sp.y),
                ImVec2(sp.x + ImGui::GetContentRegionAvail().x - sepPad, sp.y),
                ImGui::GetColorU32(ImVec4(1, 1, 1, 0.06f)), 1.0f);
        }
        ImGui::Dummy(ImVec2(0, DP(6.0f)));

        if (OverlaySlider("##SpeedSlider",
                          "Playback Speed",
                          "UiMenuTimer",
                          &ms.ui.musicBpmMultiplier, 0.5f, 2.0f, "%.2fx")) {
            ms.ui.musicBpmMultiplier = std::clamp(ms.ui.musicBpmMultiplier, 0.5f, 2.0f);
            player.SetBPMMultiplier(ms.ui.musicBpmMultiplier);
        }

        ImGui::Dummy(ImVec2(0, DP(4.0f)));

        if (OverlaySlider("##SustainSlider",
                          "Note Sustain",
                          "UiMiscMusicNote",
                          &ms.ui.musicSustainDuration, 0.0f, 10000.0f, "%.0fms")) {
        }

        ImGui::Dummy(ImVec2(0, DP(4.0f)));
    }
    ImGui::End();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

}}}
