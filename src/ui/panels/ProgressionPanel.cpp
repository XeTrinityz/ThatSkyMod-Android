#include <ui/panels/ProgressionPanel.h>
#include <ui/widgets/Layout.h>
#include <ui/widgets/Cards.h>
#include <ui/widgets/Controls.h>
#include <ui/widgets/Buttons.h>
#include <ui/widgets/Icons.h>
#include <ui/widgets/Popups.h>
#include <ui/helpers/SubTabRenderer.h>
#include <ui/helpers/Toast.h>
#include <imgui/imgui.h>
#include <ui/core/metrics.h>
#include <ui/core/App.h>
#include <ui/core/Localization.h>
#include <iconloader/IconLoader.h>
#include <game/memory/Address.h>
#include <game/memory/Memory.h>
#include <game/memory/Patch.h>
#include <game/memory/offsets.h>
#include <game/memory/api.h>
#include <game/hooks/HookManager.h>
#include <game/hooks/ProgressionHooks.h>
#include <game/interop/LuaHelpers.h>
#include <game/data/WingLightData.h>
#include <utils/common/vec3.h>
#include <utils/strings/StringUtils.h>
#include <data/DataManager.h>
#include <progression/WaxRunner.h>
#include <progression/AutoWaxTools.h>
#include <progression/CandleRunner.h>
#include <network/ApiClient.h>
#include <network/SocialManager.h>
#include <network/WingBuffManager.h>
#include <network/EdenAutomation.h>
#include <game/interop/LuaScriptQueue.h>
#include <nlohmann/json.hpp>
#include "../../../resources/data/candles_json.h"
#include "../../../resources/data/statues_json.h"
#include <algorithm>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <state/ModState.h>

namespace tsm { namespace ui { namespace tabs {

using namespace tsm::game::Signatures;

static int s_progSubIndex = 0;

static std::atomic<bool> s_wingShowRelog{false};
static std::atomic<bool> s_wingBusy{false};
static bool s_wingCollectMode = true;
static std::atomic<int> s_wingLastMode{0};
static std::atomic<bool> s_wingListNeedsRefresh{true};

static std::atomic<bool> s_edenSeasonConvertBusy{false};

static const char* kSeasonTimelines[] = {
    "Ap20_Intro_Ending",
    "Ap21_Intro_Ending",
    "Ap22_Intro_Ending",
    "Ap23_Intro_Ending",
    "Ap24_Intro_Ending",
    "Ap25_Intro_Ending",
    "Ap26_Intro_Ending",
    "Ap27_Intro_Ending",
    "Ap28_Intro_Ending",
    "Ap29_Intro_Ending",
    "Ap30_Intro_Ending",
};
static const int kSeasonTimelineCount = sizeof(kSeasonTimelines) / sizeof(kSeasonTimelines[0]);
static int s_selectedSeasonIndex = 7;

static void Wing_HandleWingBuffAction(tsm::network::WingBuffOperation operation, const std::vector<std::string>& names)
{
    auto result = tsm::network::WingBuffManager::HandleWingBuffs(operation, names);
    if (result.success) {
        bool isCollect = (operation == tsm::network::WingBuffOperation::Collect);
        s_wingLastMode.store(isCollect ? 1 : 2);
        s_wingShowRelog.store(result.needsRelog);
    }
}

static void Eden_RunSeasonCandleConversion(const char* timeline)
{
    if (s_edenSeasonConvertBusy.exchange(true)) {
        return;
    }

    std::string timelineStr(timeline);
    std::thread([timelineStr](){
        try {
            const char* targetLevel = "CandleSpace";
            bool inCandleSpace = false;

            bool needLoad = true;
            if (const char* cur = tsm::game::api::LevelName()) {
                if (std::string(cur) == targetLevel) {
                    needLoad = false;
                    inCandleSpace = true;
                }
            }

            if (needLoad) {
                tsm::lua::helpers::ChangeLevel(targetLevel, true);
				inCandleSpace = true;
            }

            if (needLoad) {
                for (int i = 0; i < 60; ++i) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
                    if (const char* cur = tsm::game::api::LevelName()) {
                        if (std::string(cur) == targetLevel) {
                            inCandleSpace = true;
                            break;
                        }
                    }
                }
			}

            if (!inCandleSpace) {
                tsm::ui::helpers::ShowToastError(tsm::ui::i18n::Tr("Failed to load CandleSpace for season candle conversion"));
                s_edenSeasonConvertBusy.store(false);
                return;
            }

            float originalSpeed = tsm::game::api::GameSpeed();
            float fastSpeed = 20.0f;
            tsm::game::api::SetGameSpeed(fastSpeed);

            std::string luaCmd = "PlayTimeline(game, \"" + timelineStr + "\")";
            tsm::lua::queue::Enqueue(luaCmd.c_str());

            std::this_thread::sleep_for(std::chrono::seconds(3));

            tsm::game::api::SetGameSpeed(originalSpeed);
            tsm::lua::helpers::ChangeLevel(targetLevel, true);

        } catch (...) {
            tsm::game::api::SetGameSpeed(1.0f);
        }

        s_edenSeasonConvertBusy.store(false);
    }).detach();
}

void DrawProgressionTab()
{
    using namespace tsm::ui::widgets;

    struct CandlePoint { std::string name; std::string map; float x=0,y=0,z=0; };
    static bool s_candlesParsed = false;
    static std::vector<CandlePoint> s_candles;
    if (!s_candlesParsed) {
        s_candlesParsed = true;
        try {
            const char* begin = reinterpret_cast<const char*>(kCandlesData);
            const char* end   = begin + (sizeof(kCandlesData) / sizeof(kCandlesData[0]));
            nlohmann::json arr = nlohmann::json::parse(begin, end);
            if (arr.is_array()) {
                s_candles.reserve(arr.size());
                for (const auto& it : arr) {
                    if (!it.is_object()) continue;
                    CandlePoint cp{};
                    if (auto k = it.find("name"); k != it.end() && k->is_string()) cp.name = k->get<std::string>();
                    if (auto k = it.find("map");  k != it.end() && k->is_string()) cp.map  = k->get<std::string>();
                    if (auto k = it.find("x");    k != it.end() && k->is_number()) cp.x    = k->get<float>();
                    if (auto k = it.find("y");    k != it.end() && k->is_number()) cp.y    = k->get<float>();
                    if (auto k = it.find("z");    k != it.end() && k->is_number()) cp.z    = k->get<float>();
                    if (!cp.map.empty()) s_candles.push_back(std::move(cp));
                }
            }
        } catch (...) {
            s_candles.clear();
        }
    }

    struct StatuePoint { std::string name; std::string map; float x=0,y=0,z=0; };
    static bool s_statuesParsed = false;
    static std::vector<StatuePoint> s_statues;
    if (!s_statuesParsed) {
        s_statuesParsed = true;
        try {
            const char* begin = reinterpret_cast<const char*>(kEdenStatuesData);
            const char* end   = begin + (sizeof(kEdenStatuesData) / sizeof(kEdenStatuesData[0]));
            nlohmann::json arr = nlohmann::json::parse(begin, end);
            if (arr.is_array()) {
                s_statues.reserve(arr.size());
                for (const auto& it : arr) {
                    if (!it.is_object()) continue;
                    StatuePoint sp{};
                    if (auto k = it.find("name"); k != it.end() && k->is_string()) sp.name = k->get<std::string>();
                    if (auto k = it.find("map");  k != it.end() && k->is_string()) sp.map  = k->get<std::string>();
                    if (auto k = it.find("x");    k != it.end() && k->is_number()) sp.x    = k->get<float>();
                    if (auto k = it.find("y");    k != it.end() && k->is_number()) sp.y    = k->get<float>();
                    if (auto k = it.find("z");    k != it.end() && k->is_number()) sp.z    = k->get<float>();
                    if (!sp.map.empty()) s_statues.push_back(std::move(sp));
                }
            }
        } catch (...) {
            s_statues.clear();
        }
    }

    using namespace tsm::ui::helpers;
    using namespace tsm::utils;

    static const char* kSubIcons[] = {
        "UiMenuBuffDoubleWax",
        "UiMenuWingLockOpen",
        "UiMenuDyeAutoCollect",
        "UiMiscTempleStorm",
        "UiMenuQuestHint"
    };
    constexpr int sub_count = 5;

    DrawSubTabs(kSubIcons, sub_count, s_progSubIndex);

    const float base_d   = DP(44.0f);
    const ImVec4 acc     = tsm::ui::GetAccentColor();
    const ImU32  bg_sel  = ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 0.28f));
    const ImU32  brd_sel = ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 0.70f));
    const ImU32  bg_nrm  = ImGui::GetColorU32(ImVec4(1,1,1,0.10f));
    const ImU32  brd_nrm = ImGui::GetColorU32(ImVec4(1,1,1,0.18f));
    float base_gap = ImGui::GetStyle().ItemSpacing.x;

    if (s_progSubIndex == 0 && BeginCard("##progression_wax", 8.0f, false)) {
        static int s_waxSubTab = 0;
        ImGui::Dummy(ImVec2(0, DP(4.0f)));
        {
            const char* kWaxSubIcons[] = { "UiMenuBuffCandle", "UiMenuDevCorner", "UiMenuMap" };
            const int sub_count2 = 3;
            float slot_d_sub = base_d * 0.75f;
            float icon_sz_sub = slot_d_sub * 0.62f;
            float gap2 = base_gap;
            float cont_w2 = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
            float row_w2 = sub_count2 * slot_d_sub + (sub_count2 - 1) * gap2;
            float left_pad2 = std::max(0.0f, (cont_w2 - row_w2) * 0.5f);

            ImVec2 start2 = ImGui::GetCursorPos();
            for (int i = 0; i < sub_count2; ++i) {
                ImGui::SetCursorPos(ImVec2(start2.x + left_pad2 + i * (slot_d_sub + gap2), start2.y));
                ImGui::PushID(i + 1000);
                ImVec2 p2 = ImGui::GetCursorScreenPos();
                ImGui::InvisibleButton("##waxsubico", ImVec2(slot_d_sub, slot_d_sub));
                bool clicked2 = ImGui::IsItemClicked();

                ImDrawList* dl2 = ImGui::GetWindowDrawList();
                float r2 = slot_d_sub * 0.5f;
                ImVec2 c2 = ImVec2(p2.x + r2, p2.y + r2);
                dl2->AddCircleFilled(c2, r2, (s_waxSubTab == i) ? bg_sel : bg_nrm, 48);
                dl2->AddCircle(c2, r2, (s_waxSubTab == i) ? brd_sel : brd_nrm, 48, 1.0f);

                ImVec2 icon_pos2 = ImVec2(c2.x - icon_sz_sub * 0.5f, c2.y - icon_sz_sub * 0.5f);
                ImGui::SetCursorScreenPos(icon_pos2);
                Icon(kWaxSubIcons[i], icon_sz_sub, ImVec4(1,1,1,1));

                if (clicked2) s_waxSubTab = i;
                ImGui::PopID();
            }
        }
        ImGui::Dummy(ImVec2(0, DP(6.0f)));

        static bool s_autoCollectWax = false;
        static bool s_autoBurnCandles = false;
        static bool s_autoBurnPlants = false;
        static bool s_autoCollectFragments = false;
        static bool s_warpToFragments = false;
        static bool s_fastPlantBurn = false;

        if (s_waxSubTab == 0) {
            float awt_child_h = ImGui::GetContentRegionAvail().y;
            if (awt_child_h < 0.0f) awt_child_h = 0.0f;
            if (BeginPannableChild("##wax_awt_child", ImVec2(0, awt_child_h))) {
                CenterSeparator("Auto Wax Tools");
                static bool s_runIncludePlants = true;
                static bool s_runIncludeDyes = false;
                bool active = tsm::progression::WaxRunner::Get().IsActive();
                if (active) {
                    std::string runDesc = tsm::progression::WaxRunner::Get().GetStatus();
                    if (runDesc.empty()) runDesc = "Running...";
                    if (ButtonCard("Cancel Auto Wax Tools Run", runDesc.c_str(), "UiMenuBuffCandle")) {
                        tsm::progression::WaxRunner::Get().Cancel();
                    }
                } else {
                    if (ButtonCard("Start from Beginning", "Cycle levels to collect wax", "UiMenuBuffCandle", "START")) {
                        tsm::progression::RunConfig config;
                        config.start_mode = tsm::progression::StartMode::FromBeginning;
                        config.include_plants = s_runIncludePlants;
                        config.include_dyes = s_runIncludeDyes;
                        tsm::progression::WaxRunner::Get().Start(config);
                    }
                    if (ButtonCard("Start from Current Level", "Continue from the current level", "UiMenuBuffCandle", "START")) {
                        tsm::progression::RunConfig config;
                        config.start_mode = tsm::progression::StartMode::FromCurrentLevel;
                        config.include_plants = s_runIncludePlants;
                        config.include_dyes = s_runIncludeDyes;
                        tsm::progression::WaxRunner::Get().Start(config);
                    }
                    ToggleCardIcon("Include plants", "Teleport to plant clusters during run", "UiOutfitHornAP15DarkPlant", &s_runIncludePlants);
                    ToggleCardIcon("Include dyes", "Teleport to dyes and auto-collect during run", "UiMenuDyeAutoCollect", &s_runIncludeDyes);

                    CenterSeparator("Timing");
                    {
                        auto& ms = tsm::state::ModState::Get();
                        int pre = ms.progression.prePlantDelaySec;
                        if (IntCardIcon("Pre-Plant Delay", "Delay before starting plant teleports (sec)", "UiMenuTimer", &pre, 0, 60)) {
                            ms.progression.prePlantDelaySec = pre;
                        }
                        int waitDye = ms.progression.waitForDyeInitSec;
                        if (IntCardIcon("Wait For Dyes", "Delay to wait for dyes to initialize (sec)", "UiMenuTimer", &waitDye, 0, 60)) {
                            ms.progression.waitForDyeInitSec = waitDye;
                        }
                        int plantItv = ms.progression.plantIntervalSec;
                        if (IntCardIcon("Plant Interval", "Delay between plant teleports (sec)", "UiMenuTimer", &plantItv, 0, 30)) {
                            ms.progression.plantIntervalSec = plantItv;
                        }
                        int dyeItv = ms.progression.dyeIntervalSec;
                        if (IntCardIcon("Dye Interval", "Delay between dye teleports (sec)", "UiMenuTimer", &dyeItv, 0, 30)) {
                            ms.progression.dyeIntervalSec = dyeItv;
                        }
                        int between = ms.progression.betweenLevelsSec;
                        if (IntCardIcon("Between Levels", "Delay between levels (sec)", "UiMenuTimer", &between, 0, 120)) {
                            ms.progression.betweenLevelsSec = between;
                        }
                    }
                }
                EndPannableChild();
            }
        }

        if (s_waxSubTab == 1) {
            float wax_child_h = ImGui::GetContentRegionAvail().y;
            if (wax_child_h < 0.0f) wax_child_h = 0.0f;
            if (BeginPannableChild("##wax_automation_child", ImVec2(0, wax_child_h))) {
                CenterSeparator("Tools");
                bool runnerActive = tsm::progression::WaxRunner::Get().IsActive();

                s_autoCollectWax   = tsm::progression::IsAutoCollectWaxEnabled();
                s_autoBurnCandles  = tsm::progression::IsAutoBurnCandlesEnabled();
                s_autoBurnPlants   = tsm::progression::IsAutoBurnPlantsEnabled();

                if (runnerActive) ImGui::BeginDisabled(true);

                if (ToggleCardIcon("Auto Collect Wax", "Automatically collect wax nearby", "UiMenuBuffCandle", &s_autoCollectWax)) {
                    tsm::progression::SetAutoCollectWaxEnabled(s_autoCollectWax);
                }

                if (ToggleCardIcon("Auto Burn Candles", "Continuously burn candles in range", "UiMiscFlame", &s_autoBurnCandles)) {
                    tsm::progression::SetAutoBurnCandlesEnabled(s_autoBurnCandles);
                }

                if (ToggleCardIcon("Auto Burn Plants", "Continuously burn plants in range", "UiOutfitHornAP15DarkPlant", &s_autoBurnPlants)) {
                    tsm::progression::SetAutoBurnPlantsEnabled(s_autoBurnPlants);
                }

                if (ToggleCardIcon("Auto Collect Fragments", "Auto-collect all dropped fragments", "UiMenuStarScan", &s_autoCollectFragments)) {
                    tsm::game::memory::WriteByte(tsm::game::Offsets::kAutoCollectAllFragments, s_autoCollectFragments ? 1 : 0);
                }

                if (ToggleCardIcon("Warp To Fragments", "Automatically warp to nearby fragments", "UiMiscTarget", &s_warpToFragments)) {
                    tsm::game::memory::WriteByte(tsm::game::Offsets::kAutoFragmentWarp, s_warpToFragments ? 1 : 0);
                }

                if (ToggleCardIcon("Faster Plant Burning", "Increase plant burning speed", "UiOutfitHornAP15DarkPlant", &s_fastPlantBurn)) {
                    tsm::game::memory::WriteByte(tsm::game::Offsets::kFastBurn, s_fastPlantBurn ? 1 : 0);
                }
                if (runnerActive) ImGui::EndDisabled();
                EndPannableChild();
            }
        }

        if (s_waxSubTab == 2) {
            CenterSeparator("Semi Candle Run");

            auto& cr = tsm::progression::CandleRunner::Get();
            cr.RefreshCache();

            const auto& regular = cr.GetRegularCoords();
            const auto& named = cr.GetNamedCoords();

            const char* lvl = tsm::game::api::LevelName();
            std::string curLevel = (lvl && *lvl) ? std::string(lvl) : std::string();

            static std::unordered_map<std::string, int> s_selByLevel;
            int& sel = s_selByLevel[curLevel];
            if (sel < 0) sel = 0;
            if (!regular.empty() && sel >= (int)regular.size()) sel = 0;

            if (!regular.empty() || !named.empty()) {
                if (!regular.empty()) {
                    CycleClick cc = CycleCard("##wax_cycle", "UiMiscArrowLeft", "UiMenuBuffCandle", "UiMiscArrowRight", 28.0f);
                    auto handleTp = [&](int idx) {
                        const auto& cp = regular[idx];
                        tsm::lua::helpers::SetPlayerPosition(vec3{cp.position.x, cp.position.y, cp.position.z}, false);
                    };

                    if (cc == CycleClick::Prev) {
                        sel = (sel + (int)regular.size() - 1) % (int)regular.size();
                        handleTp(sel);
                    } else if (cc == CycleClick::Next) {
                        sel = (sel + 1) % (int)regular.size();
                        handleTp(sel);
                    } else if (cc == CycleClick::Center) {
                        handleTp(sel);
                    }

                    ImGui::Dummy(ImVec2(0, DP(8.0f)));
                }

                float cr_child_h = ImGui::GetContentRegionAvail().y;
                if (cr_child_h < 0.0f) cr_child_h = 0.0f;
                if (BeginPannableChild("##semi_cr_numbers", ImVec2(0, cr_child_h))) {
                    if (!regular.empty()) {
                        auto colorGetter = [&](int idx) -> ImVec4 {
                            const auto& cp = regular[idx];
                            if (cp.name == "Candle") return ImVec4(0.5216f, 0.0353f, 0.0235f, 1.0f);
                            if (cp.name == "Season Candle") return ImVec4(0.6196f, 0.4235f, 0.1490f, 1.0f);
                            if (cp.name == "Plant") return ImVec4(0.1412f, 0.4196f, 0.4980f, 1.0f);
                            return ImVec4(0,0,0,0);
                        };

                        NumberedButtonGrid((int)regular.size(), sel, [&](int idx) {
                            sel = idx;
                            const auto& cp = regular[sel];
                            tsm::lua::helpers::SetPlayerPosition(vec3{cp.position.x, cp.position.y, cp.position.z}, false);
                        }, colorGetter);
                    }

                    for (const auto& cp : named) {
                        if (!cp.name.empty()) {
                            if (StandardButton(cp.name.c_str())) {
                                tsm::lua::helpers::SetPlayerPosition(vec3{cp.position.x, cp.position.y, cp.position.z}, false);
                            }
                        }
                    }
                }
                EndPannableChild();
            } else {
                PaddedTextDisabled("No candle locations for this level.");
            }
        }
        EndCard();
    }

    if (s_progSubIndex == 2 && BeginCard("##progression_dyes", 8.0f, false)) {
        static int s_dyeSubTab = 0;
        static bool s_autoBurnDyes = false;
        static std::vector<tsm::game::memory::Patch> s_abDyesPatches;
        static bool s_autoCollectDyes = false;

        ImGui::Dummy(ImVec2(0, DP(4.0f)));
        {
            const char* kDyeSubIcons[] = { "UiMenuDyeAutoCollect", "UiMenuMap" };
            const int sub_count2 = 2;
            float slot_d_sub = base_d * 0.75f;
            float icon_sz_sub = slot_d_sub * 0.62f;
            float gap2 = base_gap;
            float cont_w2 = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
            float row_w2 = sub_count2 * slot_d_sub + (sub_count2 - 1) * gap2;
            float left_pad2 = std::max(0.0f, (cont_w2 - row_w2) * 0.5f);

            ImVec2 start2 = ImGui::GetCursorPos();
            for (int i = 0; i < sub_count2; ++i) {
                ImGui::SetCursorPos(ImVec2(start2.x + left_pad2 + i * (slot_d_sub + gap2), start2.y));
                ImGui::PushID(i + 4000);
                ImVec2 p2 = ImGui::GetCursorScreenPos();
                ImGui::InvisibleButton("##dyesubico", ImVec2(slot_d_sub, slot_d_sub));
                bool clicked2 = ImGui::IsItemClicked();

                ImDrawList* dl2 = ImGui::GetWindowDrawList();
                float r2 = slot_d_sub * 0.5f;
                ImVec2 c2 = ImVec2(p2.x + r2, p2.y + r2);
                dl2->AddCircleFilled(c2, r2, (s_dyeSubTab == i) ? bg_sel : bg_nrm, 48);
                dl2->AddCircle(c2, r2, (s_dyeSubTab == i) ? brd_sel : brd_nrm, 48, 1.0f);

                ImVec2 icon_pos2 = ImVec2(c2.x - icon_sz_sub * 0.5f, c2.y - icon_sz_sub * 0.5f);
                ImGui::SetCursorScreenPos(icon_pos2);
                Icon(kDyeSubIcons[i], icon_sz_sub, ImVec4(1,1,1,1));

                if (clicked2) s_dyeSubTab = i;
                ImGui::PopID();
            }
        }
        ImGui::Dummy(ImVec2(0, DP(6.0f)));

        if (s_dyeSubTab == 0) {
            CenterSeparator("Automation");

            if (ToggleCardIcon("Auto Burn Dyes", "Automatically burn dyes in range", "UiMenuDyeAutoCollect", &s_autoBurnDyes)) {
                if (s_autoBurnDyes) {
                    if (tsm::game::memory::GetBase() == 0) tsm::game::memory::InitializeBase();
                    s_abDyesPatches.clear();
                    const std::uint8_t kToInstr[4] = { 0xE0, 0x03, 0x27, 0x1E };
                    const std::uintptr_t kTargets[] = {
                        tsm::game::Offsets::kAutoBurnPlants1,
                        tsm::game::Offsets::kAutoBurnPlants2,
                        tsm::game::Offsets::kAutoBurnPlants3
                    };
                    for (std::uintptr_t rva : kTargets) {
                        void* target = tsm::game::memory::RvaToPtr(rva);
                        if (!target) continue;
                        std::uint32_t cur = *reinterpret_cast<std::uint32_t*>(target);
                        if (cur == 0xBD424940u || cur == 0xBD424920u) {
                            s_abDyesPatches.emplace_back(tsm::game::memory::CreatePatch(target, kToInstr, 4));
                        }
                    }
                    if (!s_abDyesPatches.empty()) {
                        for (auto& p : s_abDyesPatches) p.Apply();
                    } else {
                        s_autoBurnDyes = false;
                    }
                } else {
                    for (auto& p : s_abDyesPatches) if (p.address) p.Restore();
                }
            }

            ToggleCardIcon("Auto Collect Dyes", "Automatically collect dyes when teleporting in Semi Dye Run", "UiPersonalityButterfly", &s_autoCollectDyes);
        } else {
            CenterSeparator("Semi Dye Run");

            int dyeCount = 0;
            if (int* p = tsm::game::hooks::HookManager::Get().GetDyeCountPtr()) { dyeCount = *p; }
            std::uintptr_t first = tsm::game::hooks::HookManager::Get().GetFirstDye();
            constexpr std::uintptr_t kDyeStride = 0x40;

            const char* lvl = tsm::game::api::LevelName();
            std::string curLevel = (lvl && *lvl) ? std::string(lvl) : std::string();

            struct DyeCluster { float x, y, z; };
            static std::unordered_map<std::string, std::vector<DyeCluster>> s_dyeClustersByLevel;
            static std::unordered_map<std::string, int> s_dyeCountsByLevel;

            if (first != 0 && dyeCount > 0) {
                int prevCount = 0;
                auto itCount = s_dyeCountsByLevel.find(curLevel);
                if (itCount != s_dyeCountsByLevel.end()) {
                    prevCount = itCount->second;
                }
                if (s_dyeClustersByLevel.find(curLevel) == s_dyeClustersByLevel.end() || s_dyeClustersByLevel[curLevel].empty() || prevCount != dyeCount) {
                    std::vector<DyeCluster> allDyes;
                    for (int i = 0; i < dyeCount; ++i) {
                        std::uintptr_t addr = first + i * kDyeStride;
                        const float* v = reinterpret_cast<const float*>(addr);
                        allDyes.push_back(DyeCluster{v[0], v[1], v[2]});
                    }

                    auto distance3D = [](float x1, float y1, float z1, float x2, float y2, float z2) -> float {
                        float dx = x2 - x1, dy = y2 - y1, dz = z2 - z1;
                        return std::sqrt(dx * dx + dy * dy + dz * dz);
                    };

                    std::vector<bool> visited(allDyes.size(), false);
                    std::vector<DyeCluster> clusters;
                    constexpr float clusterRadius = 10.0f;

                    for (size_t i = 0; i < allDyes.size(); ++i) {
                        if (visited[i]) continue;

                        std::vector<size_t> clusterIndices;
                        clusterIndices.push_back(i);
                        visited[i] = true;

                        for (size_t j = i + 1; j < allDyes.size(); ++j) {
                            if (visited[j]) continue;
                            float dist = distance3D(allDyes[i].x, allDyes[i].y, allDyes[i].z,
                                                   allDyes[j].x, allDyes[j].y, allDyes[j].z);
                            if (dist <= clusterRadius) {
                                clusterIndices.push_back(j);
                                visited[j] = true;
                            }
                        }

                        float sumX = 0, sumY = 0, sumZ = 0;
                        for (size_t idx : clusterIndices) {
                            sumX += allDyes[idx].x;
                            sumY += allDyes[idx].y;
                            sumZ += allDyes[idx].z;
                        }
                        float cnt = static_cast<float>(clusterIndices.size());
                        clusters.push_back(DyeCluster{sumX / cnt, sumY / cnt, sumZ / cnt});
                    }

                    s_dyeClustersByLevel[curLevel] = clusters;
                    s_dyeCountsByLevel[curLevel] = dyeCount;
                }
            }

            auto& clusters = s_dyeClustersByLevel[curLevel];
            int count = static_cast<int>(clusters.size());

            static std::unordered_map<std::string, int> s_dyeSelByLevel;
            int& sel = s_dyeSelByLevel[curLevel];
            if (sel < 0) sel = 0;
            if (count > 0 && sel >= count) sel = 0;

            if (first != 0 && count > 0) {
                CycleClick cc = CycleCard("##dye_cycle", "UiMiscArrowLeft", "UiMenuDyeAutoCollect", "UiMiscArrowRight", 28.0f);
                auto go_to_index = [&](int idx) {
                    const auto& cluster = clusters[idx];
                    tsm::lua::helpers::SetPlayerPosition(vec3{cluster.x, cluster.y, cluster.z}, false);
                    if (s_autoCollectDyes) {
                        tsm::lua::helpers::PlayEmote("butterfly", 1.0, 10.0, 1);
                    }
                };
                if (cc == CycleClick::Prev) { sel = (sel + count - 1) % count; go_to_index(sel); }
                else if (cc == CycleClick::Next) { sel = (sel + 1) % count; go_to_index(sel); }
                else if (cc == CycleClick::Center) { go_to_index(sel); }

                ImGui::Dummy(ImVec2(0, DP(8.0f)));

                float dye_child_h = ImGui::GetContentRegionAvail().y;
                if (dye_child_h < 0.0f) dye_child_h = 0.0f;
                if (BeginPannableChild("##dye_numbers", ImVec2(0, dye_child_h))) {
                    NumberedButtonGrid(count, sel, [&](int idx) {
                        sel = idx;
                        const auto& cluster = clusters[sel];
                        tsm::lua::helpers::SetPlayerPosition(vec3{cluster.x, cluster.y, cluster.z}, false);
                        if (s_autoCollectDyes) {
                            tsm::lua::helpers::PlayEmote("butterfly", 1.0, 10.0, 1);
                        }
                    });
                }
                EndPannableChild();
            } else {
                PaddedTextDisabled("No dye locations for this level.");
            }
        }
        EndCard();
    }

    if (s_progSubIndex == 3 && BeginCard("##progression_eden", 8.0f, false)) {
        static int s_edenSubTab = 0;
        ImGui::Dummy(ImVec2(0, DP(4.0f)));
        {
            const char* kEdenSubIcons[] = { "UiMenuWingBuff", "UiMenuMap" };
            const int sub_count2 = 2;
            float slot_d_sub = base_d * 0.75f;
            float icon_sz_sub = slot_d_sub * 0.62f;
            float gap2 = base_gap;
            float cont_w2 = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
            float row_w2 = sub_count2 * slot_d_sub + (sub_count2 - 1) * gap2;
            float left_pad2 = std::max(0.0f, (cont_w2 - row_w2) * 0.5f);

            ImVec2 start2 = ImGui::GetCursorPos();
            for (int i = 0; i < sub_count2; ++i) {
                ImGui::SetCursorPos(ImVec2(start2.x + left_pad2 + i * (slot_d_sub + gap2), start2.y));
                ImGui::PushID(i + 3000);
                ImVec2 p2 = ImGui::GetCursorScreenPos();
                ImGui::InvisibleButton("##edensubico", ImVec2(slot_d_sub, slot_d_sub));
                bool clicked2 = ImGui::IsItemClicked();

                ImDrawList* dl2 = ImGui::GetWindowDrawList();
                float r2 = slot_d_sub * 0.5f;
                ImVec2 c2 = ImVec2(p2.x + r2, p2.y + r2);
                dl2->AddCircleFilled(c2, r2, (s_edenSubTab == i) ? bg_sel : bg_nrm, 48);
                dl2->AddCircle(c2, r2, (s_edenSubTab == i) ? brd_sel : brd_nrm, 48, 1.0f);

                ImVec2 icon_pos2 = ImVec2(c2.x - icon_sz_sub * 0.5f, c2.y - icon_sz_sub * 0.5f);
                ImGui::SetCursorScreenPos(icon_pos2);
                Icon(kEdenSubIcons[i], icon_sz_sub, ImVec4(1,1,1,1));

                if (clicked2) s_edenSubTab = i;
                ImGui::PopID();
            }
        }
        ImGui::Dummy(ImVec2(0, DP(6.0f)));

        if (s_edenSubTab == 0) {
            using namespace tsm::ui::widgets;

            const std::string& uid = tsm::game::hooks::HookManager::Get().GetCapturedUserId();
            const std::string& sid = tsm::game::hooks::HookManager::Get().GetCapturedSessionId();
            const bool loggedIn = !uid.empty() && !sid.empty();

            if (!loggedIn) {
                CenterSeparator("Login required");
                PaddedTextDisabled("No user/session detected.");
                if (StandardButton("Check Again")) {
                }
                ImGui::EndChild();
                return;
            }

            float eden_child_h = ImGui::GetContentRegionAvail().y;
            if (BeginPannableChild("##eden_automation", ImVec2(0, eden_child_h))) {
                CenterSeparator("Automation");
                static bool s_recollectAfter = true;
                ToggleCardIcon("Recollect After Convert", "Collect wing light again after conversion", "UiMenuWingBuff", &s_recollectAfter);
                ImGui::Dummy(ImVec2(0, DP(4.0f)));

                if (tsm::network::EdenAutomation::IsRunning()) {
                    std::string msg = tsm::network::EdenAutomation::GetStatus();
                    float pct = tsm::network::EdenAutomation::GetProgress();
                    char buf[256];
                    std::snprintf(buf, sizeof(buf), "%s (%.0f%%)", msg.c_str(), pct);
                    if (ButtonCard("Cancel", buf, "UiMiscX")) {
                        tsm::network::EdenAutomation::Stop();
                    }
                } else {
                    if (ButtonCard("Run Auto Eden", "Deposit, convert to ascended candles, optional recollect, and run Eden sequence", "UiMiscStar")) {
                        tsm::network::EdenAutomation::Config config;
                        config.recollectAfterConvert = s_recollectAfter;
                        tsm::network::EdenAutomation::Start(config);
                    }
                }

                if (StringSliderCard("Convert Season Candles", "Convert season candles into regular candles", "UiMenuBuffCandle",
                    kSeasonTimelines, kSeasonTimelineCount, &s_selectedSeasonIndex)) {
                    ImGui::OpenPopup("##confirm_convert_season_candles");
                }

                if (ConfirmPopup("##confirm_convert_season_candles",
                                 "Convert Season Candles",
                                 "Convert season candles into regular candles now?\n\nThis will remove all of your season candles.",
                                 "Convert",
                                 "Cancel")) {
                    Eden_RunSeasonCandleConversion(kSeasonTimelines[s_selectedSeasonIndex]);
                }

                if (ButtonCard("Check Star Shrine", "Check if eden has reset", "UiMenuQuestHint", "CHECK")) {
                    tsm::lua::queue::Enqueue("TWingBuffDepositShrineResetHint()");
                }
            }
            EndPannableChild();
        } else {
            using namespace tsm::ui::widgets;
            CenterSeparator("Semi Eden");

            const char* lvl = tsm::game::api::LevelName();
            std::string curLevel = (lvl && *lvl) ? std::string(lvl) : std::string();
            bool inEdenLevel = (curLevel == "StormEnd");
            if (!inEdenLevel) {
                PaddedTextDisabled("Eden locations are available only in StormEnd.");
                if (ButtonCard("Load StormEnd", "Switch to StormEnd to view locations", "UiSocialTeleport", "GO")) {
                    tsm::lua::helpers::ChangeLevel("StormEnd", true);
                }
            } else {
                std::string matchMap = std::string("StormEnd");
                std::vector<int> indices; indices.reserve(64);
                for (int i = 0; i < (int)s_statues.size(); ++i) {
                    if (s_statues[i].map == matchMap) indices.push_back(i);
                }

                static std::unordered_map<std::string, int> s_selByLevel;
                int& sel = s_selByLevel[matchMap];
                if (sel < 0) sel = 0;
                if ((int)indices.size() > 0 && sel >= (int)indices.size()) sel = 0;

                if (!indices.empty()) {
                    CycleClick cc = CycleCard("##eden_cycle", "UiMiscArrowLeft", "UiMenuStarScan", "UiMiscArrowRight", 28.0f);
                    auto tp_to = [&](int idx){ const auto& sp = s_statues[indices[idx]]; tsm::lua::helpers::SetPlayerPosition(vec3{sp.x, sp.y, sp.z}, false); };
                    if (cc == CycleClick::Prev) { sel = (sel + (int)indices.size() - 1) % (int)indices.size(); tp_to(sel); }
                    else if (cc == CycleClick::Next) { sel = (sel + 1) % (int)indices.size(); tp_to(sel); }
                    else if (cc == CycleClick::Center) { tp_to(sel); }

                    ImGui::Dummy(ImVec2(0, DP(8.0f)));

                    float eden_child_h = ImGui::GetContentRegionAvail().y;
                    if (eden_child_h < 0.0f) eden_child_h = 0.0f;
                    if (BeginPannableChild("##eden_numbers", ImVec2(0, eden_child_h))) {
                        NumberedButtonGrid((int)indices.size(), sel, [&](int idx) {
                            sel = idx;
                            const auto& sp = s_statues[indices[sel]];
                            tsm::lua::helpers::SetPlayerPosition(vec3{sp.x, sp.y, sp.z}, false);
                        });
                    }
                    EndPannableChild();
                } else {
                    PaddedTextDisabled("No Eden statue locations for this level.");
                }
            }
        }
        EndCard();
    }

    if (s_progSubIndex == 1 && BeginCard("##progression_winglight", 8.0f, false)) {
        if (s_wingShowRelog.exchange(false)) {
            ImGui::OpenPopup("##confirm_relog_wing");
        }
        {
            int m = s_wingLastMode.load();
            const char* actionWord = (m == 2) ? tsm::ui::i18n::Tr("drop") : tsm::ui::i18n::Tr("collect");
            char msg[256];
            const char* fmt = tsm::ui::i18n::Tr("Wing Light %s applied. Relog now to take effect?");
            std::snprintf(msg, sizeof(msg), fmt, actionWord);
            if (tsm::ui::widgets::ConfirmPopup("##confirm_relog_wing", "Relog Now?", msg, "Relog now", "Later")) {
                std::thread([](){
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    tsm::network::WingBuffManager::TriggerRelog();
                }).detach();
            }
        }

        static int s_wingSubTab = 0;
        ImGui::Dummy(ImVec2(0, DP(4.0f)));
        {
            const char* kWingSubIcons[] = { "UiMenuWingBuff", "UiMenuMap" };
            const int sub_count2 = 2;
            float slot_d_sub = base_d * 0.75f;
            float icon_sz_sub = slot_d_sub * 0.62f;
            float gap2 = base_gap;
            float cont_w2 = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
            float row_w2 = sub_count2 * slot_d_sub + (sub_count2 - 1) * gap2;
            float left_pad2 = std::max(0.0f, (cont_w2 - row_w2) * 0.5f);

            ImVec2 start2 = ImGui::GetCursorPos();
            for (int i = 0; i < sub_count2; ++i) {
                ImGui::SetCursorPos(ImVec2(start2.x + left_pad2 + i * (slot_d_sub + gap2), start2.y));
                ImGui::PushID(i + 2000);
                ImVec2 p2 = ImGui::GetCursorScreenPos();
                ImGui::InvisibleButton("##wingsubico", ImVec2(slot_d_sub, slot_d_sub));
                bool clicked2 = ImGui::IsItemClicked();

                ImDrawList* dl2 = ImGui::GetWindowDrawList();
                float r2 = slot_d_sub * 0.5f;
                ImVec2 c2 = ImVec2(p2.x + r2, p2.y + r2);
                dl2->AddCircleFilled(c2, r2, (s_wingSubTab == i) ? bg_sel : bg_nrm, 48);
                dl2->AddCircle(c2, r2, (s_wingSubTab == i) ? brd_sel : brd_nrm, 48, 1.0f);

                ImVec2 icon_pos2 = ImVec2(c2.x - icon_sz_sub * 0.5f, c2.y - icon_sz_sub * 0.5f);
                ImGui::SetCursorScreenPos(icon_pos2);
                Icon(kWingSubIcons[i], icon_sz_sub, ImVec4(1,1,1,1));

                if (clicked2) s_wingSubTab = i;
                ImGui::PopID();
            }
        }
        ImGui::Dummy(ImVec2(0, DP(6.0f)));

        if (s_wingSubTab == 0) {
            using namespace tsm::ui::widgets;

            const std::string& uid = tsm::game::hooks::HookManager::Get().GetCapturedUserId();
            const std::string& sid = tsm::game::hooks::HookManager::Get().GetCapturedSessionId();
            const bool loggedIn = !uid.empty() && !sid.empty();

            if (!loggedIn) {
                CenterSeparator("Login required");
                PaddedTextDisabled("No user/session detected.");
                if (StandardButton("Check Again")) {
                }
                ImGui::EndChild();
                return;
            }

            CenterSeparator("Automation");
            if (s_wingBusy.load()) ImGui::BeginDisabled(true);
            {
                std::string modeDesc = std::string("Current mode: ") + (s_wingCollectMode ? "Collect" : "Drop");
                if (ToggleCardIcon("Collect Mode", modeDesc.c_str(), "UiMenuWingBuff", &s_wingCollectMode)) {
                    s_wingListNeedsRefresh.store(true);
                }
            }
            ImGui::Dummy(ImVec2(0, DP(4.0f)));
            {
                const char* modeWord = s_wingCollectMode ? "Collect" : "Drop";
                std::string titleLevel = std::string(modeWord) + " All Wing Light";

                if (ButtonCard(titleLevel.c_str(), "Collect/Drop all level wings automatically", "UiMenuWingBuff", s_wingCollectMode ? "COLLECT" : "DROP")) {
                    auto op = s_wingCollectMode ? tsm::network::WingBuffOperation::Collect : tsm::network::WingBuffOperation::Drop;
                    s_wingBusy.store(true);
                    std::thread([op]() {
                        std::vector<std::string> names;
                        names.insert(names.end(), tsm::game::data::kLevelWingNames.begin(), tsm::game::data::kLevelWingNames.end());
                        Wing_HandleWingBuffAction(op, names);
                        s_wingListNeedsRefresh.store(true);
                        s_wingBusy.store(false);
                    }).detach();
                }

                CenterSeparator("Level Wings");

                static std::vector<std::string> s_currentWings;
                static std::atomic<bool> s_wingsRefreshing{ false };

                if (s_wingListNeedsRefresh.exchange(false)) {
                    if (!s_wingsRefreshing.exchange(true)) {
                        std::thread([]() {
                            try {
                                static tsm::network::ApiClient client;
                                auto resp = client.PostJson("/account/wing_buffs/get");
                                std::unordered_set<std::string> ownedWings;
                                if (resp.is_object() && resp.contains("wing_buffs") && resp["wing_buffs"].is_array()) {
                                    for (const auto& item : resp["wing_buffs"]) {
                                        if (item.is_object() && item.contains("name") && item["name"].is_string()) {
                                            ownedWings.insert(item["name"].get<std::string>());
                                        }
                                    }
                                }

                                std::vector<std::string> filtered;
                                bool collectMode = s_wingCollectMode;
                                for (const auto& nm : tsm::game::data::kLevelWingNames) {
                                    bool owned = ownedWings.find(nm) != ownedWings.end();
                                    if ((collectMode && !owned) || (!collectMode && owned)) {
                                        filtered.push_back(nm);
                                    }
                                }
                                s_currentWings = std::move(filtered);
                            } catch (...) {}
                            s_wingsRefreshing.store(false);
                        }).detach();
                    }
                }

                if (ButtonCard("Refresh List", "Fetch the latest wing state", "UiMenuRestart", "REFRESH")) {
                    s_wingListNeedsRefresh.store(true);
                }

                if (s_wingsRefreshing.load()) {
                    ImGui::BeginDisabled(true);
                    ButtonCard("Loading...", "Fetching wing state", "UiMenuRestart", "WAIT");
                    ImGui::EndDisabled();
                } else {
                    float wingListH = ImGui::GetContentRegionAvail().y;
                    if (wingListH < 0.0f) wingListH = 0.0f;
                    if (!BeginPannableChild("##wing_list_dyn", ImVec2(0, wingListH))) {
                        EndPannableChild();
                    } else {
                        if (s_currentWings.empty()) {
                            PaddedTextDisabled("No wings to display.");
                        } else {
                            for (const auto& wingName : s_currentWings) {
                                if (ButtonCard(wingName.c_str(), "", "UiMenuWingBuff", s_wingCollectMode ? "COLLECT" : "DROP")) {
                                    auto op = s_wingCollectMode ? tsm::network::WingBuffOperation::Collect : tsm::network::WingBuffOperation::Drop;
                                    s_wingBusy.store(true);
                                    std::string wn = wingName;
                                    std::thread([op, wn]() {
                                        Wing_HandleWingBuffAction(op, {wn});
                                        s_wingListNeedsRefresh.store(true);
                                        s_wingBusy.store(false);
                                    }).detach();
                                }
                            }
                        }
                        EndPannableChild();
                    }
                }
            }
            if (s_wingBusy.load()) ImGui::EndDisabled();
        } else {
            CenterSeparator("Semi Wing Light");
            int count = 0;
            if (int* p = tsm::game::hooks::HookManager::Get().GetWingCountPtr()) { count = *p; }
            std::uintptr_t first = tsm::game::hooks::HookManager::Get().GetFirstWing();

            const char* lvl = tsm::game::api::LevelName();
            std::string curLevel = (lvl && *lvl) ? std::string(lvl) : std::string();
            static std::unordered_map<std::string, int> s_wlSelByLevel;
            int& sel = s_wlSelByLevel[curLevel];
            if (sel < 0) sel = 0;
            if (count > 0 && sel >= count) sel = 0;

            if (first != 0 && count > 0) {
                CycleClick cc = CycleCard("##wing_cycle", "UiMiscArrowLeft", "UiMenuStarScan", "UiMiscArrowRight", 28.0f);
                auto go_to_index = [&](int idx) {
                    std::uintptr_t addr = first + static_cast<std::uintptr_t>(idx) * 0x140u;
                    const float* v = reinterpret_cast<const float*>(addr);
                    tsm::lua::helpers::SetPlayerPosition(vec3{v[0], v[1], v[2]}, false);
                };
                if (cc == CycleClick::Prev) { sel = (sel + count - 1) % count; go_to_index(sel); }
                else if (cc == CycleClick::Next) { sel = (sel + 1) % count; go_to_index(sel); }
                else if (cc == CycleClick::Center) { go_to_index(sel); }

                ImGui::Dummy(ImVec2(0, DP(8.0f)));

                float wing_child_h = ImGui::GetContentRegionAvail().y;
                if (wing_child_h < 0.0f) wing_child_h = 0.0f;
                if (BeginPannableChild("##wing_numbers", ImVec2(0, wing_child_h))) {
                    NumberedButtonGrid(count, sel, [&](int idx) {
                        sel = idx;
                        std::uintptr_t addr = first + static_cast<std::uintptr_t>(sel) * 0x140u;
                        const float* v = reinterpret_cast<const float*>(addr);
                        tsm::lua::helpers::SetPlayerPosition(vec3{v[0], v[1], v[2]}, false);
                    });
                }
                EndPannableChild();
            } else {
                PaddedTextDisabled("No wing light locations for this level.");
            }
        }
        EndCard();
    }

    if (s_progSubIndex == 4 && BeginCard("##progression_stats", 12.0f, false)) {
        static bool s_statsLoaded = false;
        static bool s_statsLoading = false;
        static std::mutex s_statsMutex;
        static std::vector<tsm::network::StatDef> s_statDefs;
        static char s_statSearch[128] = "";
        static bool s_showInputModal = false;
        static std::string s_selectedStatName;
        static float s_selectedStatValue = 0.0f;
        static char s_inputBuffer[64] = "";

        const std::string& uid = tsm::game::hooks::HookManager::Get().GetCapturedUserId();
        const std::string& sid = tsm::game::hooks::HookManager::Get().GetCapturedSessionId();
        const bool loggedIn = !uid.empty() && !sid.empty();

        if (!loggedIn) {
            CenterSeparator("Login required");
            PaddedTextDisabled("No user/session detected.");
            if (StandardButton("Check Again")) {
                s_statsLoaded = false;
                std::lock_guard<std::mutex> lk(s_statsMutex);
                s_statDefs.clear();
            }
        } else {
            bool shouldStart = false;
            {
                std::lock_guard<std::mutex> lk(s_statsMutex);
                shouldStart = (!s_statsLoaded && !s_statsLoading);
                if (shouldStart) s_statsLoading = true;
            }
            if (shouldStart) {
                std::thread([&]() {
                    static tsm::network::ApiClient client;
                    auto defs = client.GetAchievementStats();
                    {
                        std::lock_guard<std::mutex> lk(s_statsMutex);
                        s_statDefs = std::move(defs);
                        s_statsLoaded = true;
                        s_statsLoading = false;
                    }
                }).detach();
            }

            CenterSeparator("Stats");
            if (StandardButton("Reload Stats")) {
                s_statsLoading = true;
                std::thread([&]() {
                    static tsm::network::ApiClient client;
                    auto defs = client.GetAchievementStats();
                    {
                        std::lock_guard<std::mutex> lk(s_statsMutex);
                        s_statDefs = std::move(defs);
                        s_statsLoaded = true;
                        s_statsLoading = false;
                    }
                }).detach();
            }

            SearchCard("Search...", s_statSearch, sizeof(s_statSearch));

            if (BeginPannableChild("##stats_scroll")) {

                std::string q = ToLower(s_statSearch);
                const bool doFilter = !q.empty();

                for (const auto& def : s_statDefs) {
                    std::string displayName;
                    if (!def.displayName.empty()) {
                        displayName = def.displayName;
                    } else {
                        displayName = def.type;
                        ReplaceAll(displayName, '_', ' ');
                        displayName = CapitalizeWords(displayName);
                    }
                    if (doFilter) {
                        std::string lname = ToLower(displayName);
                        std::string rid = ToLower(def.type);
                        if (lname.find(q) == std::string::npos && rid.find(q) == std::string::npos) continue;
                    }

                    char valueStr[64];
                    const char* valueFmt = tsm::ui::i18n::Tr("Current: %.0f");
                    std::snprintf(valueStr, sizeof(valueStr), valueFmt, def.value);

                    if (ButtonCard(displayName.c_str(), valueStr, "UiMenuQuestHint", "EDIT")) {
                        s_selectedStatName = def.type;
                        s_selectedStatValue = def.value;
                        std::snprintf(s_inputBuffer, sizeof(s_inputBuffer), "%.0f", def.value);
                        s_showInputModal = true;
                    }
                }

                if (s_statsLoaded && s_statDefs.empty()) {
                    PaddedText("No stats available.");
                }
            }
            EndPannableChild();
        }

        if (s_showInputModal) {
            ImGui::OpenPopup("##EditStatValue");
            s_showInputModal = false;
        }

        std::string displayName;
        {
            auto it = std::find_if(s_statDefs.begin(), s_statDefs.end(),
                [&](const auto& d){ return d.type == s_selectedStatName; });
            if (it != s_statDefs.end() && !it->displayName.empty()) {
                displayName = it->displayName;
            } else {
                displayName = s_selectedStatName;
                ReplaceAll(displayName, '_', ' ');
                displayName = CapitalizeWords(displayName);
            }
        }
        char titleBuf[256];
        std::snprintf(titleBuf, sizeof(titleBuf), "%s", displayName.c_str());
        if (InputPopup("##EditStatValue", titleBuf, "Enter new value", s_inputBuffer, sizeof(s_inputBuffer), "Apply", "Cancel")) {
            try {
                float newValue = std::stof(s_inputBuffer);
                float difference = newValue - s_selectedStatValue;
                tsm::lua::helpers::AddFloatToStat(s_selectedStatName, difference);

                for (auto& stat : s_statDefs) {
                    if (stat.type == s_selectedStatName) {
                        stat.value = newValue;
                        break;
                    }
                }
            } catch (...) {
            }
        }

        EndCard();
    }
}

}}}
