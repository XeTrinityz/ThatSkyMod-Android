#include <ui/overlays/QuickTeleportOverlay.h>
#include <ui/helpers/Toast.h>
#include <ui/widgets/Layout.h>
#include <ui/widgets/Cards.h>
#include <ui/widgets/Controls.h>
#include <ui/widgets/Buttons.h>
#include <ui/widgets/Icons.h>
#include <ui/widgets/Popups.h>
#include <ui/core/metrics.h>
#include <ui/core/Localization.h>
#include <game/memory/api.h>
#include <game/interop/LuaHelpers.h>
#include <imgui/imgui.h>
#include <cmath>

namespace tsm { namespace ui { namespace overlays {

static bool s_showQuickMenuWindow = false;
static float s_quickTpDistance = 10.0f;
static bool s_rapidTeleport = true;

bool IsQuickTeleportVisible() {
    return s_showQuickMenuWindow;
}

void SetQuickTeleportVisible(bool visible) {
    s_showQuickMenuWindow = visible;
}

void RenderQuickTeleport()
{
    using namespace tsm::ui::widgets;

    if (!s_showQuickMenuWindow) return;

    ImGui::SetNextWindowSize(ImVec2(DP(180.0f), DP(290.0f)), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(DP(12.0f), DP(12.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, DP(8.0f));

    if (ImGui::Begin("Quick Teleport", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground)) {
        float slider_width = ImGui::GetContentRegionAvail().x;
        ImGui::SetNextItemWidth(slider_width);
        ImGui::SliderFloat("##distance", &s_quickTpDistance, 0.1f, 100.0f, "%.1f m");

        ImGui::Checkbox(tsm::ui::i18n::Tr("Rapid Teleport"), &s_rapidTeleport);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", tsm::ui::i18n::Tr("Hold buttons to continuously teleport"));
        }

        ImGui::Dummy(ImVec2(0, DP(4.0f)));

        auto IconButton = [&](const char* tooltip, float x_offset, float y_offset, float z_offset, float rotation = 0.0f) {
            const float btn_size = DP(44.0f);
            ImGui::PushID(tooltip);
            ImVec2 cursor_pos = ImGui::GetCursorScreenPos();

            static float lastActionTime = 0.0f;
            static float repeatInterval = 0.1f;
            float currentTime = ImGui::GetTime();
            bool trigger = (currentTime - lastActionTime) >= repeatInterval;
            if (!s_rapidTeleport) lastActionTime = currentTime;

            ImGui::InvisibleButton("##btn", ImVec2(btn_size, btn_size));
            bool hovered = ImGui::IsItemHovered();

            bool clicked = ImGui::IsItemClicked();
            bool holding = s_rapidTeleport && ImGui::IsMouseDown(0) && hovered && trigger;

            if (clicked || holding) {
                if (trigger || clicked) {
                    lastActionTime = currentTime;

                    auto pos = tsm::game::api::LocalAvatarPosition();
                    float rotation = tsm::game::api::LocalAvatarRotation();

                    vec3 newPos = pos;

                    if (x_offset != 0.0f) {
                        newPos.x = pos.x + (x_offset * s_quickTpDistance) * std::sin(rotation);
                        newPos.z = pos.z + (x_offset * s_quickTpDistance) * std::cos(rotation);
                    }

                    if (z_offset != 0.0f) {
                        float perpAngle = rotation - M_PI / 2.0f;
                        newPos.x = pos.x + (z_offset * s_quickTpDistance) * std::sin(perpAngle);
                        newPos.z = pos.z + (z_offset * s_quickTpDistance) * std::cos(perpAngle);
                    }

                    if (y_offset != 0.0f) {
                        newPos.y += y_offset * s_quickTpDistance;
                    }

                    tsm::lua::helpers::SetPlayerPosition(newPos, false);
                }
            }

            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 center = ImVec2(cursor_pos.x + btn_size * 0.5f, cursor_pos.y + btn_size * 0.5f);
            ImU32 bg_col;
            if (ImGui::IsItemActive()) {
                bg_col = ImGui::GetColorU32(ImVec4(0.4f, 0.4f, 0.4f, 0.9f));
            } else if (hovered) {
                bg_col = ImGui::GetColorU32(ImVec4(0.3f, 0.3f, 0.3f, 0.8f));
            } else {
                bg_col = ImGui::GetColorU32(ImVec4(0.2f, 0.2f, 0.2f, 0.6f));
            }
            dl->AddCircleFilled(center, btn_size * 0.45f, bg_col, 32);

            ImGui::SetCursorScreenPos(ImVec2(center.x - DP(16.0f), center.y - DP(16.0f)));
            IconRotated("UiMiscArrowLeft", DP(32.0f), rotation, ImVec4(1, 1, 1, 1));

            if (hovered && tooltip) {
                const char* tooltipText = tsm::ui::i18n::Tr(tooltip);
                if (s_rapidTeleport) {
                    const char* holdHint = tsm::ui::i18n::Tr("Hold to repeat");
                    ImGui::SetTooltip("%s\n(%s)", tooltipText, holdHint);
                } else {
                    ImGui::SetTooltip("%s", tooltipText);
                }
            }

            ImGui::PopID();
        };

        const float btn_size = DP(44.0f);
        const float spacing = DP(2.0f);
        const float grid_w = btn_size * 3 + spacing * 2;
        const float start_x = (ImGui::GetContentRegionAvail().x - grid_w) * 0.5f;

        ImVec2 grid_start = ImGui::GetCursorPos();

        ImGui::SetCursorPos(ImVec2(grid_start.x + start_x + btn_size + spacing, grid_start.y));
        IconButton("Forward", 1, 0, 0, 90.0f);

        float row2_y = grid_start.y + btn_size + spacing;
        ImGui::SetCursorPos(ImVec2(grid_start.x + start_x, row2_y));
        IconButton("Left", 0, 0, -1, 0.0f);
        ImGui::SetCursorPos(ImVec2(grid_start.x + start_x + (btn_size + spacing) * 2, row2_y));
        IconButton("Right", 0, 0, 1, 180.0f);

        float row3_y = grid_start.y + (btn_size + spacing) * 2;
        ImGui::SetCursorPos(ImVec2(grid_start.x + start_x + btn_size + spacing, row3_y));
        IconButton("Backward", -1, 0, 0, 270.0f);

        ImGui::SetCursorPos(ImVec2(grid_start.x, grid_start.y + (btn_size + spacing) * 3 + DP(4.0f)));

        ImVec2 vertical_start = ImGui::GetCursorPos();
        float vert_y = vertical_start.y;
        ImGui::SetCursorPos(ImVec2(vertical_start.x + start_x, vert_y));
        IconButton("Up", 0, 1, 0, 90.0f);
        ImGui::SetCursorPos(ImVec2(vertical_start.x + start_x + (btn_size + spacing) * 2, vert_y));
        IconButton("Down", 0, -1, 0, 270.0f);

        ImGui::SetCursorPos(ImVec2(vertical_start.x, vert_y + btn_size));
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

}}}
