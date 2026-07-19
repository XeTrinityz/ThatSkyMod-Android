#include <ui/overlays/DebugOverlay.h>
#include <imgui/imgui.h>
#include <state/ModState.h>
#include <ui/core/App.h>
#include <ui/core/metrics.h>
#include <ui/widgets/Layout.h>
#include <ui/widgets/Cards.h>
#include <ui/widgets/Controls.h>
#include <ui/widgets/Buttons.h>
#include <ui/widgets/Icons.h>
#include <ui/widgets/Popups.h>
#include <game/hooks/HookManager.h>
#include <game/memory/api.h>
#include <game/data/VoiceData.h>
#include <game/data/StanceData.h>

namespace tsm { namespace ui { namespace overlays {

static const char* VoiceToName(std::uint8_t voice) {
    if (voice >= 0 && voice < tsm::game::data::kVoiceCount) {
        return tsm::game::data::kVoiceNames[voice].c_str();
    }
    return "Unknown";
}

static const char* StanceToName(int stance) {
    if (stance >= 0 && stance < tsm::game::data::kStanceCount) {
        return tsm::game::data::kStanceNames[stance];
    }
    return "Unknown";
}

void RenderDebugOverlay()
{
    auto& ms = tsm::state::ModState::Get();
    using namespace tsm::ui::widgets;
    if (!ms.debug.showOverlay) return;

    const float pad = DP(16.0f);
    if (ms.debug.overlayPosX < 0.0f || ms.debug.overlayPosY < 0.0f) {
        ImGui::SetNextWindowPos(ImVec2(pad, pad), ImGuiCond_FirstUseEver);
    } else {
        ImGui::SetNextWindowPos(ImVec2(ms.debug.overlayPosX, ms.debug.overlayPosY), ImGuiCond_FirstUseEver);
    }

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.3f, 0.3f, 0.3f, 0.8f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, DP(8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(DP(12.0f), DP(10.0f)));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::Begin("##DebugOverlay", nullptr, flags)) {
        ImGui::SetWindowFontScale(0.9f);

        ImGui::PushStyleColor(ImGuiCol_Text, GetAccentColor());
        ImGui::TextUnformatted("DEBUG INFO");
        ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 0.4f));
        ImGui::SetWindowFontScale(0.7f);
        ImGui::TextUnformatted(" [Drag to move]");
        ImGui::SetWindowFontScale(0.9f);
        ImGui::PopStyleColor();

        ImGui::Separator();

        const auto& userId = tsm::game::hooks::HookManager::Get().GetCapturedUserId();
        const auto& sessionId = tsm::game::hooks::HookManager::Get().GetCapturedSessionId();

        CenterSeparator("Network Data");
        if (!userId.empty()) {
            ImGui::Text("User ID: %s", userId.c_str());
        } else {
            ImGui::TextUnformatted("User ID: N/A");
        }

        if (!sessionId.empty()) {
            ImGui::Text("Session: %s", sessionId.c_str());
        } else {
            ImGui::TextUnformatted("Session: N/A");
        }

        CenterSeparator("Game Data");
        const char* levelName = tsm::game::api::LevelName();
        if (levelName && levelName[0] != '\0') {
            ImGui::Text("Level: %s", levelName);
        } else {
            ImGui::TextUnformatted("Level: Unknown");
        }

        float gameSpeed = tsm::game::api::GameSpeed();
        ImGui::Text("Game Speed: %.2fx", gameSpeed);

        CenterSeparator("Avatar Data");
        auto avatarInfo = tsm::game::api::GetAvatarInfoByIndex(0);

        if (avatarInfo.avatarScale) {
            ImGui::Text("Scale: %.3f", *avatarInfo.avatarScale);
        } else {
            ImGui::TextUnformatted("Scale: N/A");
        }

        if (avatarInfo.avatarHeight) {
            ImGui::Text("Height: %.3f", *avatarInfo.avatarHeight);
        } else {
            ImGui::TextUnformatted("Height: N/A");
        }

        if (avatarInfo.avatarVoice) {
            ImGui::Text("Voice: %s", VoiceToName(*avatarInfo.avatarVoice));
        } else {
            ImGui::TextUnformatted("Voice: N/A");
        }

        if (avatarInfo.avatarStance) {
            ImGui::Text("Stance: %s", StanceToName(*avatarInfo.avatarStance));
        } else {
            ImGui::TextUnformatted("Stance: N/A");
        }

        if (avatarInfo.avatarPosition) {
            ImGui::Text("Position: (%.1f, %.1f, %.1f)",
                       avatarInfo.avatarPosition->x,
                       avatarInfo.avatarPosition->y,
                       avatarInfo.avatarPosition->z);
        } else {
            ImGui::TextUnformatted("Position: N/A");
        }

        ImVec2 windowPos = ImGui::GetWindowPos();
        ms.debug.overlayPosX = windowPos.x;
        ms.debug.overlayPosY = windowPos.y;
    }
    ImGui::End();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

}}}
