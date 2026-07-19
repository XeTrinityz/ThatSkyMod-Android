#include <ui/core/TabBar.h>
#include <ui/core/metrics.h>
#include <ui/core/App.h>
#include <iconloader/IconLoader.h>

namespace tsm { namespace ui {

int TabBar::Render(int current_tab, const ImVec4& accent_color) {
    const float base_sidebar_icon_d = DP(44.0f);
    const float base_sidebar_gap = DP(12.0f);
    const float base_sidebar_w = base_sidebar_icon_d + base_sidebar_gap * 2.0f;

    static UIIcon cached_icons[kTabCount];
    static bool icons_loaded = false;
    if (!icons_loaded) {
        for (int i = 0; i < kTabCount; ++i) {
            IconLoader::getUIIcon(kTabIcons[i], &cached_icons[i]);
        }
        icons_loaded = true;
    }

    int new_tab = current_tab;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, base_sidebar_gap));
    ImGui::BeginChild("##sidebar", ImVec2(base_sidebar_w, 0), false, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();
    {
        const float avail_h = ImGui::GetContentRegionAvail().y;

        const float base_total_h = (base_sidebar_icon_d + base_sidebar_gap) * kTabCount - base_sidebar_gap;

        float sidebar_icon_d = base_sidebar_icon_d;
        float sidebar_gap = base_sidebar_gap;
        float total_h = base_total_h;
        bool scaling_applied = false;

        if (base_total_h > avail_h) {
            float scale_factor = avail_h / base_total_h;
            sidebar_icon_d = base_sidebar_icon_d * scale_factor;
            sidebar_gap = base_sidebar_gap * scale_factor;
            total_h = (sidebar_icon_d + sidebar_gap) * kTabCount - sidebar_gap;
            scaling_applied = true;
        }

        const float icon_size = sidebar_icon_d * 0.62f;

        float start_y = 0.0f;
        if (!scaling_applied && total_h < avail_h) {
            start_y = (avail_h - total_h) * 0.5f;
            if (start_y < 0.0f) start_y = 0.0f;
        }

        if (start_y > 0.0f) ImGui::Dummy(ImVec2(0, start_y));

        for (int i = 0; i < kTabCount; ++i) {
            ImGui::PushID(i);

            bool is_active = (i == current_tab);

            float avail_w = ImGui::GetContentRegionAvail().x;
            float base_x = ImGui::GetCursorPosX();
            ImGui::SetCursorPosX(base_x + (avail_w - sidebar_icon_d) * 0.5f);

            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("##circle", ImVec2(sidebar_icon_d, sidebar_icon_d));
            bool clicked = ImGui::IsItemClicked();

            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImU32 fill = ImGui::GetColorU32(is_active
                ? ImVec4(accent_color.x, accent_color.y, accent_color.z, 0.30f)
                : ImVec4(1, 1, 1, 0.10f));
            ImU32 border = ImGui::GetColorU32(is_active
                ? ImVec4(accent_color.x, accent_color.y, accent_color.z, 0.80f)
                : ImVec4(1, 1, 1, 0.18f));

            float radius = sidebar_icon_d * 0.5f;
            ImVec2 center = ImVec2(p.x + radius, p.y + radius);
            dl->AddCircleFilled(center, radius, fill, 48);
            dl->AddCircle(center, radius, border, 48, 1.0f);

            ImVec2 icon_pos = ImVec2(p.x + (sidebar_icon_d - icon_size) * 0.5f,
                                     p.y + (sidebar_icon_d - icon_size) * 0.5f);
            ImGui::SetCursorScreenPos(icon_pos);

            const UIIcon& uiIcon = cached_icons[i];

            if (uiIcon.textureId != IL_NO_TEXTURE) {
                IconLoader::icon(kTabIcons[i], icon_size, ImVec4(1,1,1,1));
            } else {
                char label[16];
                std::snprintf(label, sizeof(label), "%d", i + 1);
                ImGui::Text("%s", label);
            }

            if (clicked) {
                new_tab = i;
            }

            if (i < kTabCount - 1) {
                ImGui::Dummy(ImVec2(0, sidebar_gap));
            }

            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    return new_tab;
}

}}
