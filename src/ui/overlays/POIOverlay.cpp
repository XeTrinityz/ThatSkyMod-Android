#include <ui/overlays/POIOverlay.h>
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
#include <resources/data/map_json.h>
#include <imgui/imgui.h>
#include <nlohmann/json.hpp>
#include <vector>
#include <string>
#include <cstdio>

namespace tsm { namespace ui { namespace overlays {

static bool s_showPOIMenu = false;

bool IsPOIVisible() {
    return s_showPOIMenu;
}

void SetPOIVisible(bool visible) {
    s_showPOIMenu = visible;
}

void RenderPOI() {
    using namespace tsm::ui::widgets;

    if (!s_showPOIMenu) return;

    static std::vector<nlohmann::json> s_allPOIs;
    static bool s_mapDataParsed = false;

    if (!s_mapDataParsed) {
        try {
            std::string jsonStr(reinterpret_cast<const char*>(kMapData), sizeof(kMapData));
            nlohmann::json mapArray = nlohmann::json::parse(jsonStr);

            if (mapArray.is_array()) {
                for (const auto& item : mapArray) {
                    s_allPOIs.push_back(item);
                }
            }
            s_mapDataParsed = true;
        } catch (...) {
        }
    }

    const char* currentLevel = tsm::game::api::LevelName();
    std::string currentLevelStr = (currentLevel && *currentLevel) ? std::string(currentLevel) : "";

    std::vector<nlohmann::json> currentLevelPOIs;
    for (const auto& poi : s_allPOIs) {
        if (poi.contains("map") && poi["map"].is_string()) {
            std::string mapName = poi["map"].get<std::string>();
            if (mapName == currentLevelStr) {
                currentLevelPOIs.push_back(poi);
            }
        }
    }

    if (!currentLevelPOIs.empty()) {
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                                 ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoScrollbar |
                                 ImGuiWindowFlags_NoBackground;

        ImGui::SetNextWindowPos(ImVec2(DP(12.0f), DP(12.0f)), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(ImVec2(DP(200.0f), -1), ImVec2(DP(400.0f), -1));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(DP(12.0f), DP(12.0f)));

        if (ImGui::Begin("POI Menu", nullptr, flags)) {
            ImGui::PushItemWidth(-1);

            for (const auto& poi : currentLevelPOIs) {
                if (poi.contains("name") && poi.contains("x") && poi.contains("y") && poi.contains("z")) {
                    std::string name = poi["name"].get<std::string>();
                    float x = poi["x"].get<float>();
                    float y = poi["y"].get<float>();
                    float z = poi["z"].get<float>();

                    if (AccentButton(name.c_str(), ImVec2(-1, 0))) {
                        vec3 targetPos{x, y, z};
                        tsm::lua::helpers::SetPlayerPosition(targetPos, false);
                        char toastBuf[256];
                        const char* fmt = tsm::ui::i18n::Tr("Teleported to %s");
                        std::snprintf(toastBuf, sizeof(toastBuf), fmt, name.c_str());
                        tsm::ui::helpers::ShowToastSuccess(toastBuf);
                    }
                }
            }

            ImGui::PopItemWidth();
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }
}

}}}
