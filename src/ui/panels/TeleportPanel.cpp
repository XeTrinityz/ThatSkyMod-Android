#include <ui/panels/TeleportPanel.h>
#include <ui/widgets/Layout.h>
#include <ui/widgets/Cards.h>
#include <ui/widgets/Controls.h>
#include <ui/widgets/Buttons.h>
#include <ui/widgets/Icons.h>
#include <ui/widgets/Popups.h>
#include <imgui/imgui.h>
#include <ui/core/metrics.h>
#include <ui/core/Localization.h>
#include <imgui/imgui_internal.h>
#include <iconloader/IconLoader.h>
#include <ui/core/App.h>
#include <ui/helpers/Toast.h>
#include <ui/overlays/QuickTeleportOverlay.h>
#include <ui/overlays/POIOverlay.h>
#include <ui/helpers/ImGuiHelpers.h>
#include <ui/helpers/SubTabRenderer.h>
#include <utils/storage/LocationStorage.h>

#include <game/memory/api.h>
#include <data/DataManager.h>

#include <game/interop/LuaScriptQueue.h>
#include <game/interop/LuaHelpers.h>
#include <resources/data/map_json.h>
#include "resources/data/OOB/Home - Empty World.h"
#include "resources/data/OOB/Aviary Village - Empty World.h"
#include "resources/data/OOB/Concert Hall - Boats.h"
#include "resources/data/OOB/Concert Hall - Cello.h"
#include "resources/data/OOB/Concert Hall - Piano.h"
#include "resources/data/OOB/Eye of Eden - Boar Statue.h"
#include "resources/data/OOB/Eye of Eden - Empty World.h"
#include "resources/data/OOB/Eye of Eden - Rebirth.h"
#include "resources/data/OOB/Hidden Forest - Airplane.h"
#include "resources/data/OOB/Hidden Forest - Empty World.h"
#include "resources/data/OOB/Isle of Dawn - Eden Gate.h"
#include "resources/data/OOB/TGC Office - Satellite.h"
#include "resources/data/OOB/Valley of Triumph - Penguin.h"
#include "resources/data/OOB/Valley of Triumph - Sunset.h"
#include "resources/data/OOB/Vault of Knowledge  - Elders.h"
#include "resources/data/OOB/Vault of Knowledge - Empty World.h"
#include "resources/data/OOB/Vault of Knowledge - Exterior.h"

#include <nlohmann/json.hpp>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstring>
#include <cstdio>

namespace tsm { namespace ui { namespace tabs {

static int s_tpSubIndex = 0;

static void EnsureOOBLocationsLoaded(std::vector<tsm::utils::storage::SavedLocation>& locations,
                                     std::vector<std::string>& categories)
{
    static bool s_oobLoaded = false;
    if (s_oobLoaded)
        return;

    s_oobLoaded = true;

    const std::string oobCategory = "OOB";

    if (std::find(categories.begin(), categories.end(), oobCategory) == categories.end()) {
        categories.push_back(oobCategory);
    }

    auto addOOB = [&](const char* name,
                      const unsigned char* data,
                      std::size_t size)
    {
        try {
            std::string jsonStr(reinterpret_cast<const char*>(data), size);
            nlohmann::json j = nlohmann::json::parse(jsonStr);
            if (!j.is_object()) return;

            tsm::utils::storage::SavedLocation loc{};
            std::strncpy(loc.name, name, sizeof(loc.name) - 1);
            loc.name[sizeof(loc.name) - 1] = '\0';
            loc.x = j.value("x", 0.0f);
            loc.y = j.value("y", 0.0f);
            loc.z = j.value("z", 0.0f);
            if (j.contains("level") && j["level"].is_string()) {
                loc.level = j["level"].get<std::string>();
            } else {
                loc.level.clear();
            }
            loc.category = oobCategory;
            loc.builtin = true;

            bool exists = false;
            for (const auto& existing : locations) {
                if (existing.category == oobCategory && std::string(existing.name) == loc.name) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                locations.push_back(loc);
            }
        } catch (...) {
        }
    };

    addOOB("Home - Empty World", kHome___Empty_WorldData, kHome___Empty_WorldSize);
    addOOB("Aviary Village - Empty World", kAviary_Village___Empty_WorldData, kAviary_Village___Empty_WorldSize);
    addOOB("Concert Hall - Boats", kConcert_Hall___BoatsData, kConcert_Hall___BoatsSize);
    addOOB("Concert Hall - Cello", kConcert_Hall___CelloData, kConcert_Hall___CelloSize);
    addOOB("Concert Hall - Piano", kConcert_Hall___PianoData, kConcert_Hall___PianoSize);
    addOOB("Eye of Eden - Boar Statue", kEye_of_Eden___Boar_StatueData, kEye_of_Eden___Boar_StatueSize);
    addOOB("Eye of Eden - Empty World", kEye_of_Eden___Empty_WorldData, kEye_of_Eden___Empty_WorldSize);
    addOOB("Eye of Eden - Rebirth", kEye_of_Eden___RebirthData, kEye_of_Eden___RebirthSize);
    addOOB("Hidden Forest - Airplane", kHidden_Forest___AirplaneData, kHidden_Forest___AirplaneSize);
    addOOB("Hidden Forest - Empty World", kHidden_Forest___Empty_WorldData, kHidden_Forest___Empty_WorldSize);
    addOOB("Isle of Dawn - Eden Gate", kIsle_of_Dawn___Eden_GateData, kIsle_of_Dawn___Eden_GateSize);
    addOOB("TGC Office - Satellite", kTGC_Office___SatelliteData, kTGC_Office___SatelliteSize);
    addOOB("Valley of Triumph - Penguin", kValley_of_Triumph___PenguinData, kValley_of_Triumph___PenguinSize);
    addOOB("Valley of Triumph - Sunset", kValley_of_Triumph___SunsetData, kValley_of_Triumph___SunsetSize);
    addOOB("Vault of Knowledge - Elders", kVault_of_Knowledge____EldersData, kVault_of_Knowledge____EldersSize);
    addOOB("Vault of Knowledge - Empty World", kVault_of_Knowledge___Empty_WorldData, kVault_of_Knowledge___Empty_WorldSize);
    addOOB("Vault of Knowledge - Exterior", kVault_of_Knowledge___ExteriorData, kVault_of_Knowledge___ExteriorSize);
}

void DrawTeleportTab()
{
    using namespace tsm::ui::widgets;
    using namespace tsm::ui::helpers;

    static const char* kSubIcons[] = { "UiMiscTarget", "UiMenuQuestHint", "UiMenuMap", "UiMenuSaveDisk" };
    DrawSubTabs(kSubIcons, 4, s_tpSubIndex);

    if (BeginCard("##teleport_card", 8.0f, false)) {
        struct PendingTP { bool active=false; std::string level; float x=0,y=0,z=0; std::string postLua; bool matched=false; std::chrono::steady_clock::time_point readyAt{}; };
        static PendingTP s_pendingTP;
        static std::vector<std::pair<std::string, std::chrono::steady_clock::time_point>> s_pendingRemove;

        auto is_zero_pos = [](double x, double y, double z) -> bool {
            const double eps = 1e-3;
            return std::fabs(x) < eps && std::fabs(y) < eps && std::fabs(z) < eps;
        };

        if (s_pendingTP.active) {
            const auto now = std::chrono::steady_clock::now();
            if (const char* cur = tsm::game::api::LevelName()) {
                if (!s_pendingTP.level.empty() && s_pendingTP.level == cur) {
                    if (!s_pendingTP.matched) {
                        s_pendingTP.matched = true;
                        s_pendingTP.readyAt = now + std::chrono::milliseconds(3000);
                    } else if (now >= s_pendingTP.readyAt) {
                        if (is_zero_pos(s_pendingTP.x, s_pendingTP.y, s_pendingTP.z)) {
                            s_pendingTP.active = false;
                            s_pendingTP.matched = false;
                            s_pendingTP.readyAt = std::chrono::steady_clock::time_point{};
                        } else {
                            tsm::lua::helpers::SetPlayerPosition(vec3{s_pendingTP.x, s_pendingTP.y, s_pendingTP.z}, false);
                            if (!s_pendingTP.postLua.empty()) {
                                tsm::lua::queue::Enqueue(s_pendingTP.postLua);
                            }
                            tsm::ui::helpers::ShowToastSuccess("Teleported");
                            s_pendingTP.active = false;
                            s_pendingTP.matched = false;
                            s_pendingTP.readyAt = std::chrono::steady_clock::time_point{};
                        }
                    }
                } else {
                    s_pendingTP.matched = false;
                    s_pendingTP.readyAt = std::chrono::steady_clock::time_point{};
                }
            }
        }

        if (!s_pendingRemove.empty()) {
            auto now = std::chrono::steady_clock::now();
            auto it = s_pendingRemove.begin();
            while (it != s_pendingRemove.end()) {
                if (now >= it->second) {
                    tsm::lua::helpers::RemoveSpell(it->first);
                    it = s_pendingRemove.erase(it);
                } else {
                    ++it;
                }
            }
        }

        switch (s_tpSubIndex) {
            case 0: {
                static float s_customX = 0.0f;
                static float s_customY = 0.0f;
                static float s_customZ = 0.0f;

                if (BeginPannableChild("##coords_scroll")) {
                    CenterSeparator("Current Position");

                    auto currentPos = tsm::game::api::LocalAvatarPosition();
                    char posBuf[256];
                    std::snprintf(posBuf, sizeof(posBuf), "X: %.2f  |  Y: %.2f  |  Z: %.2f",
                                 currentPos.x, currentPos.y, currentPos.z);

                    float text_w = ImGui::CalcTextSize(posBuf).x;
                    float avail_w = ImGui::GetContentRegionAvail().x;
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail_w - text_w) * 0.5f);
                    ImGui::TextUnformatted(posBuf);

                    if (StandardButton("Copy Current Position")) {
                        s_customX = currentPos.x;
                        s_customY = currentPos.y;
                        s_customZ = currentPos.z;
                        tsm::ui::helpers::ShowToastSuccess("Position copied to inputs");
                    }

                    CenterSeparator("Custom Teleport");

                    InputCardIcon("X Coordinate", "Horizontal position", "UiButtonRight", &s_customX, "%.2f");
                    InputCardIcon("Y Coordinate", "Vertical position", "UiButtonTop", &s_customY, "%.2f");
                    InputCardIcon("Z Coordinate", "Depth position", "UiButtonBottom", &s_customZ, "%.2f");

                    if (StandardButton("Teleport to Coordinates")) {
                        vec3 targetPos{s_customX, s_customY, s_customZ};
                        tsm::lua::helpers::SetPlayerPosition(targetPos, false);
                        tsm::ui::helpers::ShowToastSuccess("Teleported");
                    }

                    CenterSeparator("Quick Teleport Actions");

                    bool quickTpVisible = tsm::ui::overlays::IsQuickTeleportVisible();
                    if (ToggleCardIcon("Quick Teleport Menu", "Show floating directional teleport controls", "UiMenuQuestHint", &quickTpVisible)) {
                        tsm::ui::overlays::SetQuickTeleportVisible(quickTpVisible);
                    }

                    bool poiVisible = tsm::ui::overlays::IsPOIVisible();
                    if (ToggleCardIcon("POI Menu", "Show nearby points of interest in current level", "UiMenuMap", &poiVisible)) {
                        tsm::ui::overlays::SetPOIVisible(poiVisible);
                    }
                }
                EndPannableChild();                break; }

            case 1: {
                using namespace tsm::ui::widgets;
                struct TeleDest {
                    const char* name;
                    const char* dynamicLua;
                    const char* levelName;
                    float pos[3];
                    const char* category;
                    const char* postLua;
                    const char* commandName;
                };

                static const TeleDest kTeleports[] = {
                    {"Active NPC",              "WarpToNextActiveNpc(game)",      nullptr, {0,0,0},     "Dynamic", nullptr, nullptr},
                    {"Closest Manta",           "WarpToManta(game)",              nullptr, {0,0,0},     "Dynamic", nullptr, nullptr},
                    {"Closest Meditation",      "WarpToNextMeditationArea(game)", nullptr, {0,0,0},     "Dynamic", nullptr, nullptr},
                    {"Reload Level",            nullptr,                          nullptr, {0,0,0},     "Dynamic", nullptr, "reload_level_with_position"},
                    {"Home",                    "TTitleGate()", nullptr, {0,0,0},     "Dynamic", nullptr, nullptr},

                    {"Geyser",                  nullptr, "Prairie_Island",    {144.49f, 2.81f, 416.67f}, "Events", nullptr, nullptr},
                    {"Grandma",                 nullptr, "RainShelter",       {-30.80f, 87.64f, -0.42f}, "Events", nullptr, nullptr},

                    {"Water Trial Skip",        nullptr, "Dawn_TrialsWater",  { 44.20f, 67.94f, -273.01f}, "Trials", "PlayTimeline(game,\"Revelation\")", nullptr},
                    {"Earth Trial Skip",        nullptr, "Dawn_TrialsEarth",  { 1.71f, 128.28f, 15.72f}, "Trials", "PlayTimeline(game,\"Revelation\")", nullptr},
                    {"Air Trial Skip",          nullptr, "Dawn_TrialsAir",    {-11.44f,106.22f, -130.33f}, "Trials", "PlayTimeline(game,\"Revelation\")", nullptr},
                    {"Fire Trial Skip",         nullptr, "Dawn_TrialsFire",   {-12.22f, 50.33f, -257.27f}, "Trials", "PlayTimeline(game,\"Revelation\")", nullptr},

                    {"Wonderland Cafe",         nullptr, "MainStreet_Cafe_Wonderland", {0,0,0}, "Unique", nullptr, nullptr},
                    {"Aurora Concert",          nullptr, nullptr, {0,0,0}, "Unique", nullptr, "teleport_aurora_concert"},
                    {"Birthday Crab",           nullptr, nullptr, {0,0,0}, "Unique", nullptr, "teleport_birthdaycrab"},
                    {"Little Prince Figurine",  nullptr, nullptr, {0,0,0}, "Unique", nullptr, "teleport_figurine_littleprince"},
                    {"Sky Kid SE Figurine",     nullptr, nullptr, {0,0,0}, "Unique", nullptr, "teleport_figurineskykid_se"},
                    {"Nature Turtle",           nullptr, nullptr, {0,0,0}, "Unique", nullptr, "teleport_nature_turtle"},
                    {"Oreo",                    nullptr, nullptr, {0,0,0}, "Unique", nullptr, "teleport_oreo"},
                    {"Wisteria Tree",           nullptr, nullptr, {0,0,0}, "Unique", nullptr, "teleport_wisteria_tree"},
                    {"You And I",               nullptr, nullptr, {0,0,0}, "Unique", nullptr, "teleport_you_and_i"},

                    {"Forest Elder",            nullptr, nullptr, {0,0,0}, "Elders", nullptr, "teleport_forest_elder"},
                    {"Isle Elder",              nullptr, nullptr, {0,0,0}, "Elders", nullptr, "teleport_isle_elder"},
                    {"Prairie Elder",           nullptr, nullptr, {0,0,0}, "Elders", nullptr, "teleport_prairie_elder"},
                    {"Valley Elder",            nullptr, nullptr, {0,0,0}, "Elders", nullptr, "teleport_valley_elder"},
                    {"Vault Elder",             nullptr, nullptr, {0,0,0}, "Elders", nullptr, "teleport_vault_elder"},
                    {"Wasteland Elder",         nullptr, nullptr, {0,0,0}, "Elders", nullptr, "teleport_wasteland_elder"},

                    {"Forest Cave",             nullptr, nullptr, {0,0,0}, "Other", nullptr, "teleport_forest_cave"},
                    {"Prophecy Cave",           nullptr, nullptr, {0,0,0}, "Other", nullptr, "teleport_prophecy_cave"},
                    {"Hermit Valley",           nullptr, nullptr, {0,0,0}, "Other", nullptr, "teleport_hermit_valley"},
                    {"Sanctuary Island",        nullptr, nullptr, {0,0,0}, "Other", nullptr, "teleport_sanctuary_island"},
                    {"Starlight Desert",        nullptr, nullptr, {0,0,0}, "Other", nullptr, "teleport_starlight_desert"},
                };

                static const std::vector<std::string> kOrder = { "Dynamic", "Events", "Trials", "Unique", "Elders", "Other" };

                if (BeginPannableChild("##quick_scroll")) {
                    auto iconForCategory = [](const char* cat) -> const char* {
                        if (!cat) return "UiMenuMap";
                        if (std::strcmp(cat, "Dynamic") == 0) return "UiMenuMap";
                        if (std::strcmp(cat, "Events")  == 0) return "UiMenuQuestHint";
                        if (std::strcmp(cat, "Trials")  == 0) return "UiMenuSpellWater";
                        if (std::strcmp(cat, "Unique")  == 0) return "UiMenuStarScan";
                        if (std::strcmp(cat, "Elders")  == 0) return "UiMenuShield";
                        if (std::strcmp(cat, "Other")   == 0) return "UiMenuGate";
                        return "UiMenuMap";
                    };
                    for (const auto& cat : kOrder) {
                        CenterSeparator(cat.c_str());
                        for (const auto& td : kTeleports) {
                            if (cat != td.category) continue;
                            const char* iconC = iconForCategory(td.category);
                            if (ButtonCard(td.name, nullptr, iconC, "GO")) {
                                if (td.dynamicLua && *td.dynamicLua) {
                                    tsm::lua::queue::Enqueue(td.dynamicLua);
                                } else if (td.levelName && *td.levelName) {
                                    bool handledDirect = false;
                                    if (td.category && std::strcmp(td.category, "Trials") == 0) {
                                        if (const char* cur = tsm::game::api::LevelName()) {
                                            if (std::strcmp(cur, td.levelName) == 0) {
                                                tsm::lua::helpers::SetPlayerPosition(vec3{td.pos[0], td.pos[1], td.pos[2]}, false);
                                                if (td.postLua && *td.postLua) {
                                                    tsm::lua::queue::Enqueue(td.postLua);
                                                }
                                                handledDirect = true;
                                            }
                                        }
                                    }
                                    if (!handledDirect) {
                                        s_pendingTP.active = true;
                                        s_pendingTP.level  = td.levelName;
                                        s_pendingTP.x = td.pos[0]; s_pendingTP.y = td.pos[1]; s_pendingTP.z = td.pos[2];
                                        s_pendingTP.postLua = td.postLua ? td.postLua : "";
                                        tsm::lua::helpers::ChangeLevel(td.levelName, true);
                                    }
                                } else if (td.commandName && *td.commandName) {
                                    if (std::strcmp(td.commandName, "reload_level_with_position") == 0) {
                                        auto currentPos = tsm::game::api::LocalAvatarPosition();
                                        const char* currentLevel = tsm::game::api::LevelName();

                                        if (currentLevel && *currentLevel) {
                                            s_pendingTP.active = true;
                                            s_pendingTP.level = currentLevel;
                                            s_pendingTP.x = currentPos.x;
                                            s_pendingTP.y = currentPos.y;
                                            s_pendingTP.z = currentPos.z;
                                            s_pendingTP.postLua = "";

                                            tsm::lua::helpers::ChangeLevel(currentLevel, true);
                                            tsm::ui::helpers::ShowToastInfo("Reloading level...");
                                        } else {
                                            tsm::ui::helpers::ShowToastError("Cannot reload: No level loaded");
                                        }
                                    } else if (cat == std::string("Elders") || cat == std::string("Other") || cat == std::string("Unique")) {
                                        tsm::lua::helpers::GrantSpell(td.commandName);
                                        s_pendingRemove.emplace_back(std::string(td.commandName), std::chrono::steady_clock::now() + std::chrono::milliseconds(1500));
                                    } else {
                                        std::string fn = td.commandName;
                                        if (fn.find('(') == std::string::npos) fn += "(game)";
                                        tsm::lua::queue::Enqueue(fn);
                                    }
                                }
                            }
                        }
                    }
                }
                EndPannableChild();                break; }

            case 2: {
                static bool s_attemptedLoad = false;
                if (!s_attemptedLoad && (tsm::data::DataManager::Get().GetLevels().is_null() || tsm::data::DataManager::Get().GetLevels().empty())) {
                    (void)tsm::data::DataManager::Get().LoadLevels();
                    s_attemptedLoad = true;
                }

                CenterSeparator("Configuration");
                static bool s_useFade = true;
                ToggleCardIcon("Fade Transition", "Use fade when changing level", "UiMenuBlur", &s_useFade);
                static bool s_spawnPortal = false;
                ToggleCardIcon("Create Portal Instead", "Spawn a portal near you to enter the selected level", "UiMenuGate", &s_spawnPortal);

                static int s_realmIndex = 0;
                static const char* kRealmKeys[] = {
                    "dawn", "prairie", "rain", "sunset", "dusk", "night", "storm"
                };
                static const char* kRealmFriendly[] = {
                    "Isle of Dawn",
                    "Daylight Prairie",
                    "Hidden Forest",
                    "Valley of Triumph",
                    "Golden Wasteland",
                    "Vault of Knowledge",
                    "Eye of Eden"
                };
                static const char* kRealmIcons[] = {
                    "UiMiscTempleDawn",
                    "UiMiscTempleDay",
                    "UiMiscTempleRain",
                    "UiMiscTempleSunset",
                    "UiMiscTempleDusk",
                    "UiMiscTempleNight",
                    "UiMiscTempleStorm"
                };
                if (s_realmIndex < 0 || s_realmIndex >= (int)IM_ARRAYSIZE(kRealmKeys)) s_realmIndex = 0;

                {
                    CenterSeparator("Realm");
                    const int realm_count = (int)IM_ARRAYSIZE(kRealmKeys);
                    float avail_w2 = ImGui::GetContentRegionAvail().x;
                    const float realm_base_d = DP(44.0f) * 0.8f;
                    float base_gap2 = ImGui::GetStyle().ItemSpacing.x;

                    const ImVec4 acc = tsm::ui::GetAccentColor();
                    const ImU32 bg_sel = ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 0.28f));
                    const ImU32 brd_sel = ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 0.70f));
                    const ImU32 bg_nrm = ImGui::GetColorU32(ImVec4(1,1,1,0.10f));
                    const ImU32 brd_nrm = ImGui::GetColorU32(ImVec4(1,1,1,0.18f));

                    float need_w = realm_count * realm_base_d + (realm_count - 1) * base_gap2;
                    float s2 = 1.0f;
                    if (need_w > avail_w2 && realm_count > 0) {
                        s2 = std::max(0.6f, (avail_w2 - (realm_count - 1) * base_gap2) / (realm_count * realm_base_d));
                    }

                    float realm_slot_d = realm_base_d * s2;
                    float realm_icon_sz = realm_slot_d * 0.62f;
                    float row_w = realm_count * realm_slot_d + (realm_count - 1) * base_gap2;
                    float left_pad = std::max(0.0f, (avail_w2 - row_w) * 0.5f);

                    ImGui::BeginGroup();
                    ImVec2 realm_start = ImGui::GetCursorPos();
                    for (int i = 0; i < realm_count; ++i) {
                        ImGui::SetCursorPos(ImVec2(realm_start.x + left_pad + i * (realm_slot_d + base_gap2), realm_start.y));
                        ImGui::PushID(i + 2000);
                        ImVec2 p = ImGui::GetCursorScreenPos();
                        ImGui::InvisibleButton("##realmico", ImVec2(realm_slot_d, realm_slot_d));
                        bool clicked = ImGui::IsItemClicked();

                        const bool selected = (s_realmIndex == i);

                        ImDrawList* dl = ImGui::GetWindowDrawList();
                        float r = realm_slot_d * 0.5f;
                        ImVec2 c = ImVec2(p.x + r, p.y + r);
                        dl->AddCircleFilled(c, r, selected ? bg_sel : bg_nrm, 48);
                        dl->AddCircle(c, r, selected ? brd_sel : brd_nrm, 48, 1.0f);

                        ImVec2 icon_pos = ImVec2(c.x - realm_icon_sz * 0.5f, c.y - realm_icon_sz * 0.5f);
                        ImGui::SetCursorScreenPos(icon_pos);
                        Icon(kRealmIcons[i], realm_icon_sz, ImVec4(1,1,1,1));

                        if (clicked) {
                            s_realmIndex = i;
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndGroup();
                    ImGui::Dummy(ImVec2(0, DP(8.0f)));
                }

                if (BeginPannableChild("##levels_scroll")) {
                    const nlohmann::json& lv = tsm::data::DataManager::Get().GetLevels();
                    if (!lv.is_object() || lv.empty()) {
                        ImGui::TextDisabled("No level data available.");
                    } else {
                        static const std::vector<std::string> orderedRealmNames = {
                            "dawn", "prairie", "rain", "sunset", "dusk", "night", "storm"
                        };
                        static const std::unordered_map<std::string, std::string> displayNames = {
                            {"dawn",   "Isle of Dawn"},
                            {"prairie","Daylight Prairie"},
                            {"rain",   "Hidden Forest"},
                            {"sunset", "Valley of Triumph"},
                            {"dusk",   "Golden Wasteland"},
                            {"night",  "Vault of Knowledge"},
                            {"storm",  "Eye of Eden"}
                        };

                        const char* selectedRealmKey = kRealmKeys[s_realmIndex];
                        for (const auto& realmKey : orderedRealmNames) {
                            if (realmKey != selectedRealmKey) continue;
                            if (!lv.contains(realmKey)) continue;
                            const auto& arr = lv.at(realmKey);
                            if (!arr.is_array() || arr.empty()) continue;
                            auto itDisp = displayNames.find(realmKey);
                            const char* disp = (itDisp != displayNames.end()) ? itDisp->second.c_str() : realmKey.c_str();
                            CenterSeparator(disp);

                            for (const auto& n : arr) {
                                if (!n.is_string()) continue;
                                std::string levelName = n.get<std::string>();
                                if (ButtonCard(levelName.c_str(), nullptr, "UiSocialTeleport", "GO")) {
                                    if (s_spawnPortal) {
                                        auto pos2 = tsm::game::api::LocalAvatarPosition();
                                        vec3 pos{pos2.x + 4.0f, pos2.y + 1.5f, pos2.z};
                                        tsm::lua::helpers::CreatePortalAt(pos, levelName, levelName);
                                    } else {
                                        tsm::lua::helpers::ChangeLevel(levelName, s_useFade);
                                    }
                                }
                            }
                        }
                    }
                }
                EndPannableChild();                break; }

            case 3: {
                static std::vector<tsm::utils::storage::SavedLocation> s_savedLocations;
                static std::vector<std::string> s_categories = {"Uncategorised"};
                static int s_selectedCategoryIdx = 0;
                static char s_newLocationName[128] = "";
                static char s_newCategoryName[128] = "";
                static bool s_showSaveModal = false;
                static bool s_showCategoryModal = false;
                static bool s_showEditModal = false;
                static bool s_showDeleteCategoryConfirm = false;
                static bool s_showLevelChangeConfirm = false;
                static int s_editLocationIdx = -1;
                static int s_editCategoryIdx = 0;
                static int s_pendingTeleportIdx = -1;
                static bool s_locationsLoaded = false;

                auto hasDuplicateName = [&](const std::string& name, const std::string& category, int excludeIdx = -1) {
                    for (size_t i = 0; i < s_savedLocations.size(); ++i) {
                        if ((int)i == excludeIdx) continue;
                        if (s_savedLocations[i].category == category &&
                            std::string(s_savedLocations[i].name) == name) {
                            return true;
                        }
                    }
                    return false;
                };

                if (!s_locationsLoaded) {
                    tsm::utils::storage::LoadLocationsFromFile(s_savedLocations, s_categories);
                    EnsureOOBLocationsLoaded(s_savedLocations, s_categories);
                    s_locationsLoaded = true;
                }

                if (BeginPannableChild("##saved_scroll")) {
                    CenterSeparator("Quick Save");

                    if (ButtonCard("Save Current Location", "Save your current position to this category", "UiMiscPlusMedium", "SAVE")) {
                        s_showSaveModal = true;
                        auto pos = tsm::game::api::LocalAvatarPosition();
                        std::snprintf(s_newLocationName, sizeof(s_newLocationName), "Location %d", (int)s_savedLocations.size() + 1);
                    }

                    CenterSeparator("Categories");

                    if (ButtonCard("Create New Category", "Add a new folder to organize your locations", "UiMiscPlusMedium", "CREATE")) {
                        s_showCategoryModal = true;
                        s_newCategoryName[0] = '\0';
                    }

                    if (!s_categories.empty()) {
                        if (s_selectedCategoryIdx >= (int)s_categories.size()) {
                            s_selectedCategoryIdx = 0;
                        }

                        std::vector<const char*> categoryPtrs;
                        for (const auto& cat : s_categories) {
                            categoryPtrs.push_back(cat.c_str());
                        }
                        SelectCardIcon("Current Category", "Filter locations by category", "UiMenuSortDown",
                                    &s_selectedCategoryIdx, categoryPtrs.data(), (int)categoryPtrs.size());

                        if (s_selectedCategoryIdx > 0 &&
                            s_selectedCategoryIdx < (int)s_categories.size() &&
                            s_categories[s_selectedCategoryIdx] != std::string("OOB")) {
                            if (ButtonCard("Delete Current Category", "Remove this category and move locations to Uncategorised", "UiMenuDeleteAll", "DELETE")) {
                                s_showDeleteCategoryConfirm = true;
                            }
                        }
                    }

                    CenterSeparator("Saved Locations");

                    std::string currentCategory = s_categories[s_selectedCategoryIdx];
                    bool foundAny = false;

                    static std::unordered_map<size_t, float> s_pressScrollY;

                    for (size_t i = 0; i < s_savedLocations.size(); ++i) {
                        const auto& loc = s_savedLocations[i];
                        if (loc.category != currentCategory) continue;
                        foundAny = true;

                        ImGui::PushID((int)i);

                        char coordsDesc[256];
                        std::snprintf(coordsDesc, sizeof(coordsDesc), "(%.1f, %.1f, %.1f)",
                                     loc.x, loc.y, loc.z);

                        auto result = EditableCard(loc.name, coordsDesc, "UiMenuCheckpoint");

                        if (result.main) {
                            const char* currentLevel = tsm::game::api::LevelName();
                            std::string currentLevelStr = (currentLevel && *currentLevel) ? std::string(currentLevel) : "";

                            bool differentLevel = !loc.level.empty() && !currentLevelStr.empty() && loc.level != currentLevelStr;

                            if (differentLevel) {
                                s_pendingTeleportIdx = (int)i;
                                s_showLevelChangeConfirm = true;
                            } else {
                                vec3 targetPos{loc.x, loc.y, loc.z};
                                tsm::lua::helpers::SetPlayerPosition(targetPos, false);
                                tsm::ui::helpers::ShowToastSuccess("Teleported");
                            }
                        }

                        if (result.edit) {
                            if (loc.builtin) {
                                tsm::ui::helpers::ShowToastInfo("Built-in locations cannot be edited");
                            } else {
                                s_editLocationIdx = (int)i;
                                s_showEditModal = true;
                                std::strncpy(s_newLocationName, loc.name, sizeof(s_newLocationName) - 1);
                                for (int ci = 0; ci < (int)s_categories.size(); ++ci) {
                                    if (s_categories[ci] == loc.category) {
                                        s_editCategoryIdx = ci;
                                        break;
                                    }
                                }
                            }
                        }

                        ImGui::PopID();
                    }

                    if (!foundAny) {
                        PaddedTextDisabled("No saved locations in this category");
                    }
                }
                EndPannableChild();

                if (s_showDeleteCategoryConfirm) {
                    ImGui::OpenPopup("##DeleteCategoryConfirm");
                    s_showDeleteCategoryConfirm = false;
                }

                std::string categoryToDelete = (s_selectedCategoryIdx > 0 && s_selectedCategoryIdx < (int)s_categories.size())
                    ? s_categories[s_selectedCategoryIdx] : "";
                char confirmMsg[256];
                const char* deleteFmt = tsm::ui::i18n::Tr("Delete category '%s'?\n\nAll locations will be moved to Uncategorised.");
                std::snprintf(confirmMsg, sizeof(confirmMsg),
                             deleteFmt,
                             categoryToDelete.c_str());

                if (ConfirmPopup("##DeleteCategoryConfirm", "Delete Category", confirmMsg, "Delete", "Cancel")) {
                    if (s_selectedCategoryIdx > 0 && s_selectedCategoryIdx < (int)s_categories.size()) {
                        std::string catToDelete = s_categories[s_selectedCategoryIdx];
                        if (catToDelete == "OOB") {
                            tsm::ui::helpers::ShowToastError("The OOB category cannot be deleted");
                        } else {
                            for (auto& loc : s_savedLocations) {
                                if (loc.category == catToDelete) {
                                    loc.category = "Uncategorised";
                                }
                            }
                            s_categories.erase(s_categories.begin() + s_selectedCategoryIdx);
                            s_selectedCategoryIdx = 0;
                            tsm::utils::storage::SaveLocationsToFile(s_savedLocations, s_categories);
                            tsm::ui::helpers::ShowToastSuccess("Category deleted");
                        }
                    }
                }

                if (s_showLevelChangeConfirm) {
                    ImGui::OpenPopup("##LevelChangeConfirm");
                    s_showLevelChangeConfirm = false;
                }

                if (s_pendingTeleportIdx >= 0 && s_pendingTeleportIdx < (int)s_savedLocations.size()) {
                    const auto& pendingLoc = s_savedLocations[s_pendingTeleportIdx];
                    char levelChangeMsg[512];
                    const char* levelFmt = tsm::ui::i18n::Tr("This location is in level: %s\n\nChange to this level first?");
                    std::snprintf(levelChangeMsg, sizeof(levelChangeMsg), levelFmt, pendingLoc.level.c_str());
                    if (ConfirmPopup("##LevelChangeConfirm", "Change Level?", levelChangeMsg, "Change Level", "Cancel")) {
                        s_pendingTP.active = true;
                        s_pendingTP.level = pendingLoc.level;
                        s_pendingTP.x = pendingLoc.x;
                        s_pendingTP.y = pendingLoc.y;
                        s_pendingTP.z = pendingLoc.z;
                        s_pendingTP.postLua = "";
                        tsm::lua::helpers::ChangeLevel(pendingLoc.level, true);
                        tsm::ui::helpers::ShowToastInfo("Changing level...");
                        s_pendingTeleportIdx = -1;
                    } else if (ImGui::IsPopupOpen("##LevelChangeConfirm") == false) {
                        s_pendingTeleportIdx = -1;
                    }
                }

                if (s_showSaveModal) {
                    ImGui::OpenPopup("##SaveLocationModal");
                    s_showSaveModal = false;
                }

                if (InputPopup("##SaveLocationModal", "Save Location", "Enter location name",
                              s_newLocationName, sizeof(s_newLocationName), "Save", "Cancel")) {
                    std::string targetCategory = s_categories[s_selectedCategoryIdx];

                    if (hasDuplicateName(std::string(s_newLocationName), targetCategory)) {
                        tsm::ui::helpers::ShowToastError("A location with this name already exists in this category");
                    } else {
                        tsm::utils::storage::SavedLocation loc{};
                        std::strncpy(loc.name, s_newLocationName, sizeof(loc.name) - 1);
                        loc.name[sizeof(loc.name) - 1] = '\0';
                        auto pos = tsm::game::api::LocalAvatarPosition();
                        loc.x = pos.x;
                        loc.y = pos.y;
                        loc.z = pos.z;
                        loc.category = targetCategory;
                        const char* currentLevel = tsm::game::api::LevelName();
                        loc.level = (currentLevel && *currentLevel) ? std::string(currentLevel) : "";
                        loc.builtin = false;
                        s_savedLocations.push_back(loc);
                        tsm::utils::storage::SaveLocationsToFile(s_savedLocations, s_categories);
                        tsm::ui::helpers::ShowToastSuccess("Location saved");
                    }
                }

                if (s_showCategoryModal) {
                    ImGui::OpenPopup("##CreateCategoryModal");
                    s_showCategoryModal = false;
                }

                if (InputPopup("##CreateCategoryModal", "Create Category", "Enter category name",
                              s_newCategoryName, sizeof(s_newCategoryName), "Create", "Cancel")) {
                    if (s_newCategoryName[0] != '\0') {
                        s_categories.push_back(std::string(s_newCategoryName));
                        tsm::utils::storage::SaveLocationsToFile(s_savedLocations, s_categories);
                        tsm::ui::helpers::ShowToastSuccess("Category created");
                    }
                }

                if (s_showEditModal) {
                    ImGui::OpenPopup("##EditLocationModal");
                    s_showEditModal = false;
                }

                ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                ImGui::SetNextWindowSize(ImVec2(DP(280.0f), 0), ImGuiCond_Always);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(DP(12.0f), DP(10.0f)));
                if (ImGui::BeginPopupModal("##EditLocationModal", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
                    if (s_editLocationIdx >= 0 && s_editLocationIdx < (int)s_savedLocations.size()) {
                        const char* title = tsm::ui::i18n::Tr("Edit Location");
                        float title_w = ImGui::CalcTextSize(title).x;
                        float window_w = ImGui::GetContentRegionAvail().x;
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (window_w - title_w) * 0.5f);
                        ImGui::Text("%s", title);
                        ImGui::Separator();

                        ImGui::Text("%s", tsm::ui::i18n::Tr("Name:"));
                        ImGui::PushItemWidth(-1);
                        InputWithPlaceholder("##editname", s_newLocationName, sizeof(s_newLocationName), "Location name");
                        ImGui::PopItemWidth();

                        ImGui::Text("%s", tsm::ui::i18n::Tr("Category:"));
                        std::vector<const char*> categoryPtrs;
                        for (const auto& cat : s_categories) {
                            categoryPtrs.push_back(cat.c_str());
                        }
                        ImGui::PushItemWidth(-1);
                        ImGui::Combo("##editcat", &s_editCategoryIdx, categoryPtrs.data(), (int)categoryPtrs.size());
                        ImGui::PopItemWidth();

                        ImGui::Dummy(ImVec2(0, DP(8.0f)));

                        float btn_w = (ImGui::GetContentRegionAvail().x - DP(8.0f)) / 2.0f;

                        if (AccentButton("Save", ImVec2(btn_w, 0))) {
                            std::string newName = std::string(s_newLocationName);
                            std::string newCategory = s_categories[s_editCategoryIdx];
                            std::string oldCategory = s_savedLocations[s_editLocationIdx].category;
                            std::string oldName = std::string(s_savedLocations[s_editLocationIdx].name);

                            bool nameOrCategoryChanged = (newName != oldName || newCategory != oldCategory);
                            bool hasDuplicate = nameOrCategoryChanged && hasDuplicateName(newName, newCategory, s_editLocationIdx);

                            if (hasDuplicate) {
                                tsm::ui::helpers::ShowToastError("A location with this name already exists in the target category");
                            } else {
                                std::strncpy(s_savedLocations[s_editLocationIdx].name, s_newLocationName,
                                           sizeof(s_savedLocations[s_editLocationIdx].name) - 1);
                                s_savedLocations[s_editLocationIdx].category = newCategory;
                                tsm::utils::storage::SaveLocationsToFile(s_savedLocations, s_categories);
                                tsm::ui::helpers::ShowToastSuccess("Location updated");
                                ImGui::CloseCurrentPopup();
                            }
                        }
                        ImGui::SameLine();
                        if (SecondaryButton("Cancel", ImVec2(btn_w, 0))) {
                            ImGui::CloseCurrentPopup();
                        }

                        ImGui::Dummy(ImVec2(0, DP(4.0f)));
                        ImGui::Separator();
                        ImGui::Dummy(ImVec2(0, DP(4.0f)));

                        bool isBuiltin = (s_editLocationIdx >= 0 && s_editLocationIdx < (int)s_savedLocations.size())
                            ? s_savedLocations[s_editLocationIdx].builtin
                            : false;
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, isBuiltin ? 0.2f : 0.6f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, isBuiltin ? 0.2f : 0.8f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.3f, 0.3f, isBuiltin ? 0.2f : 0.9f));
                        const char* delLabel = tsm::ui::i18n::Tr("Delete Location");
                        if (ImGui::Button(delLabel, ImVec2(-1, 0))) {
                            if (isBuiltin) {
                                tsm::ui::helpers::ShowToastInfo("Built-in locations cannot be deleted");
                            } else {
                                s_savedLocations.erase(s_savedLocations.begin() + s_editLocationIdx);
                                tsm::utils::storage::SaveLocationsToFile(s_savedLocations, s_categories);
                                tsm::ui::helpers::ShowToastSuccess("Location deleted");
                                ImGui::CloseCurrentPopup();
                            }
                        }
                        ImGui::PopStyleColor(3);
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopStyleVar();

                break; }
        }
        EndCard();
    }
}

}}}
