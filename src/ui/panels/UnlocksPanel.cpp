#include <ui/panels/UnlocksPanel.h>
#include <ui/widgets/Layout.h>
#include <ui/widgets/Cards.h>
#include <ui/widgets/Controls.h>
#include <ui/widgets/Buttons.h>
#include <ui/widgets/Icons.h>
#include <ui/widgets/Popups.h>
#include <ui/helpers/SubTabRenderer.h>
#include <game/data/RealmData.h>
#include <game/data/UnlockData.h>
#include <utils/common/vec3.h>
#include <network/QuestApi.h>
#include <network/CollectiblesApi.h>
#include <network/ShopApi.h>
#include <network/helpers/WorldQuestHelper.h>
#include <network/helpers/DailyQuestHelper.h>
#include <imgui/imgui.h>
#include <game/memory/Patch.h>
#include <game/memory/Address.h>
#include <game/memory/offsets.h>
#include <game/memory/api.h>
#include <ui/core/App.h>
#include <ui/core/metrics.h>
#include <ui/core/Localization.h>
#include <cstring>
#include <game/interop/LuaScriptQueue.h>
#include <game/interop/LuaHelpers.h>
#include <network/SocialManager.h>
#include <nlohmann/json.hpp>
#include <network/ApiClient.h>
#include <game/hooks/HookManager.h>
#include <data/DataManager.h>
#include <state/ModState.h>
#include <features/manager/FeatureManager.h>
#include <ui/helpers/Toast.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdio>
#include <chrono>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace tsm { namespace ui { namespace tabs {

static int s_unlocksSubIndex = 0;

static std::mutex s_dailyUiMu;
static float s_dailyPct = 0.0f;
static std::string s_dailyMsg;

static std::mutex s_wqUiMu;
static float s_wqPct = 0.0f;
static std::string s_wqMsg;
static bool s_wqCollectCollectibles = false;

static void Daily_UpdateProgress(float pct, const char* msg) {
    std::lock_guard<std::mutex> lk(s_dailyUiMu);
    s_dailyPct = pct;
    s_dailyMsg = msg ? msg : "";
}

static void WQ_UpdateProgress(float pct, const char* msg) {
    std::lock_guard<std::mutex> lk(s_wqUiMu);
    s_wqPct = pct;
    s_wqMsg = msg ? msg : "";
}

void DrawUnlocksTab()
{
    using namespace tsm::ui::widgets;
    using namespace tsm::ui::helpers;

    static const char* kSubIcons[] = { "UiMenuTimer", "UiMenuQuest", "UiEmotePoint", "UiMenuShop" };
    DrawSubTabs(kSubIcons, 4, s_unlocksSubIndex);

    if (BeginCard("##unlocks_card", 12.0f, false)) {
        switch (s_unlocksSubIndex) {
            case 0: {
                CenterSeparator("Temporary Unlocks");

                if (BeginPannableChild("##temp_scroll")) {
                    auto& ms = tsm::state::ModState::Get();
                    bool uAll    = ms.unlocks.unlockAll;
                    bool uEmotes = ms.unlocks.unlockEmoteLevels;
                    bool uRel    = ms.unlocks.unlockRelationshipAbilities;

                    if (ToggleCardIcon("Unlock All", "Temporarily bypass all \"Has Unlock\" checks", "UiMenuLock", &uAll)) {
                        ms.unlocks.unlockAll = uAll;
                        tsm::features::FeatureManager::Get().ApplyUnlockFeatures();
                    }

                    if (ToggleCardIcon("Unlock All Emote Levels", "Allow using all emote level variants", "UiEmoteStanceHero", &uEmotes)) {
                        ms.unlocks.unlockEmoteLevels = uEmotes;
                        tsm::features::FeatureManager::Get().ApplyUnlockFeatures();
                    }

                    if (ToggleCardIcon("Unlock All Relationship Abilities", "Enable all relationship actions", "UiEmoteSocialHeart", &uRel)) {
                        ms.unlocks.unlockRelationshipAbilities = uRel;
                        tsm::features::FeatureManager::Get().ApplyUnlockFeatures();
                    }

                    static bool s_autoCompleteQuests = false;
                    static tsm::game::memory::Patch s_autoCompleteQuestsPatch;
                    if (ToggleCardIcon("Auto Complete Quests", "Automatically complete quests", "UiMenuQuestComplete", &s_autoCompleteQuests)) {
                        if (s_autoCompleteQuests) {
                            if (tsm::game::memory::GetBase() == 0) tsm::game::memory::InitializeBase();
                            void* target = tsm::game::memory::RvaToPtr(tsm::game::Offsets::kAutoCompleteQuests);
                            if (target) {
                                const std::uint8_t kPatch[4] = { 0x20, 0x00, 0x80, 0x52 };
                                s_autoCompleteQuestsPatch = tsm::game::memory::CreatePatch(target, kPatch, 4);
                                s_autoCompleteQuestsPatch.Apply();
                            } else {
                                s_autoCompleteQuests = false;
                            }
                        } else {
                            if (s_autoCompleteQuestsPatch.address) {
                                s_autoCompleteQuestsPatch.Restore();
                            }
                        }
                    }

                    CenterSeparator("Account Unlocks");
                    if (ButtonCard("Unlock All Realms", "Add account unlocks for all realms", "UiMenuGate", "UNLOCK")) {
                        std::thread([]{
                            for (const auto& realm : tsm::game::data::kRealmNames) {
                                std::string script = std::string("AccountUnlockCmd(game, \"add\", \"") + realm + "\")";
                                tsm::lua::queue::Enqueue(script);
                                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                            }
                        }).detach();
                        tsm::ui::helpers::ShowToastSuccess("Unlocked All Realms");
                    }

                    if (ButtonCard("Unlock All Map Shrines", "Add account unlocks for all map shrines", "UiMenuMap", "UNLOCK")) {
                        std::thread([]{
                            for (const auto& id : tsm::game::data::kShrineUnlocks) {
                                std::string script = std::string("AccountUnlockCmd(game, \"add\", \"") + id + "\")";
                                tsm::lua::queue::Enqueue(script);
                                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                            }
                        }).detach();
                        tsm::ui::helpers::ShowToastSuccess("Unlocked All Map Shrines");
                    }

                    if (ButtonCard("Skip Intro Sequence", "Skip the intro/tutorial sequence", "UiMenuCheckpoint", "SKIP")) {
                        std::thread([]{
                            for (const auto& id : tsm::game::data::kIntroUnlocks) {
                                std::string script = std::string("AccountUnlockCmd(game, \"add\", \"") + id + "\")";
                                tsm::lua::queue::Enqueue(script);
                                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                            }
                        }).detach();
                        tsm::ui::helpers::ShowToastSuccess("Intro Sequence Skipped");
                    }

                    if (ButtonCard("Unlock All Music Sheets", "Add account unlocks for all music sheets", "UiMiscMusicNote", "UNLOCK")) {
                        std::thread([]{
                            for (const auto& id : tsm::game::data::kMusicSheetUnlocks) {
                                tsm::lua::helpers::Unlock(id, true);
                                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                            }
                            tsm::ui::helpers::ShowToastSuccess("Unlocked All Music Sheets");
                        }).detach();
                    }
                }
                EndPannableChild();
                break; }
            case 1: {
                static int s_questSubTab = 0;
                ImGui::Dummy(ImVec2(0, DP(4.0f)));
                {
                    const float base_d   = DP(44.0f);
                    float base_gap = ImGui::GetStyle().ItemSpacing.x;
                    const char* kQuestSubIcons[] = { "UiMenuQuestComplete", "UiMenuQuestNew" };
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
                        ImGui::PushID(i + 5000);
                        ImVec2 p2 = ImGui::GetCursorScreenPos();
                        ImGui::InvisibleButton("##questsubico", ImVec2(slot_d_sub, slot_d_sub));
                        bool clicked2 = ImGui::IsItemClicked();
                        ImDrawList* dl2 = ImGui::GetWindowDrawList();
                        float r2 = slot_d_sub * 0.5f;
                        ImVec2 c2 = ImVec2(p2.x + r2, p2.y + r2);
                        ImVec4 acc = tsm::ui::GetAccentColor();
                        ImU32 bg_sel  = ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 0.28f));
                        ImU32 brd_sel = ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 0.70f));
                        ImU32 bg_nrm  = ImGui::GetColorU32(ImVec4(1,1,1,0.10f));
                        ImU32 brd_nrm = ImGui::GetColorU32(ImVec4(1,1,1,0.18f));
                        dl2->AddCircleFilled(c2, r2, (s_questSubTab == i) ? bg_sel : bg_nrm, 48);
                        dl2->AddCircle(c2, r2, (s_questSubTab == i) ? brd_sel : brd_nrm, 48, 1.0f);
                        ImVec2 icon_pos2 = ImVec2(c2.x - icon_sz_sub * 0.5f, c2.y - icon_sz_sub * 0.5f);
                        ImGui::SetCursorScreenPos(icon_pos2);
                        Icon(kQuestSubIcons[i], icon_sz_sub, ImVec4(1,1,1,1));
                        if (clicked2) s_questSubTab = i;
                        ImGui::PopID();
                    }
                }
                ImGui::Dummy(ImVec2(0, DP(6.0f)));

                if (s_questSubTab == 0) {

                    const std::string& uid = tsm::game::hooks::HookManager::Get().GetCapturedUserId();
                    const std::string& sid = tsm::game::hooks::HookManager::Get().GetCapturedSessionId();
                    const bool loggedIn = !uid.empty() && !sid.empty();

                    if (!loggedIn) {
                        CenterSeparator("Login required");
                        PaddedTextDisabled("No user/session detected.");
                        if (StandardButton("Check Again")) {
                        }
                        break;
                    }

                    CenterSeparator("Daily Quests");

                    auto& dailyHelper = tsm::network::helpers::DailyQuestHelper::Get();
                    if (dailyHelper.IsLoading()) {
                        ImGui::BeginDisabled(true);
                        ButtonCard("Reload Daily Quests", "Fetching active quests...", "UiMiscView", "RELOAD");
                        ImGui::EndDisabled();
                    } else {
                        if (ButtonCard("Reload Daily Quests", "Fetch active daily quests from server", "UiMiscView", "RELOAD")) {
                            dailyHelper.FetchAsync();
                        }
                    }

                    if (dailyHelper.IsCompleting() || dailyHelper.IsCollecting()) {
                        std::string msg; { std::lock_guard<std::mutex> lk(s_dailyUiMu); msg = s_dailyMsg; }
                        if (ButtonCard("Cancel", msg.c_str(), "UiMiscX")) {
                            dailyHelper.Cancel();
                        }
                    } else {
                        auto local = dailyHelper.GetQuests();
                        const char* trailing = nullptr;
                        char trailBuf[32];
                        if (!local.empty()) {
                            const char* fmt = tsm::ui::i18n::Tr("%zu Quests");
                            std::snprintf(trailBuf, sizeof(trailBuf), fmt, local.size());
                            trailing = trailBuf;
                        }
                        if (ButtonCard("Complete All Daily Quests", "Attempt to complete each active daily quest", "UiMenuQuestComplete", trailing)) {
                            if (local.empty()) { dailyHelper.FetchAsync(true); }
                            else { dailyHelper.CompleteAll(Daily_UpdateProgress); }
                        }

                        bool hasSnap = dailyHelper.HasReloadSnapshot();
                        if (!hasSnap) ImGui::BeginDisabled(true);
                        if (ButtonCard("Collect All Daily Quest Rewards", "Collect rewards for quests completed since last reload", "UiOutfitPropAnniversaryTrophy", hasSnap?nullptr:"RELOAD FIRST")) {
                            dailyHelper.CollectAllCompletedSinceReload(Daily_UpdateProgress);
                        }
                        if (!hasSnap) ImGui::EndDisabled();
                    }

                    float dq_h = ImGui::GetContentRegionAvail().y; if (dq_h < 0.0f) dq_h = 0.0f;
                    if (BeginPannableChild("##quick_scroll")) {
                        auto local = dailyHelper.GetQuests();
                        if (local.empty()) {
                            PaddedTextDisabled("No active daily quests.");
                        } else {
                            for (const auto& it : local) {
                                char trail[32];
                                const char* trailFmt = tsm::ui::i18n::Tr("x%d");
                                std::snprintf(trail, sizeof(trail), trailFmt, std::max(1, it.second));
                                std::string displayName = tsm::network::GetQuestDescription(it.first);
                                if (ButtonCard(displayName.c_str(), "Run quest completion now", "UiMenuQuestHint", trail)) {
                                    std::string name = it.first;
                                    int count = std::max(1, it.second);
                                    std::thread([name, count]{
                                        for (int i = 0; i < count; ++i) {
                                            tsm::lua::helpers::TryCompleteQuestOrIncrementStat(name);
                                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                                        }
                                        tsm::network::helpers::DailyQuestHelper::Get().RemoveQuest(name);
                                    }).detach();
                                }
                            }
                        }
                    }
                    EndPannableChild();
                } else {
                    const std::string& uid = tsm::game::hooks::HookManager::Get().GetCapturedUserId();
                    const std::string& sid = tsm::game::hooks::HookManager::Get().GetCapturedSessionId();
                    const bool loggedIn = !uid.empty() && !sid.empty();

                    if (!loggedIn) {
                        CenterSeparator("Login required");
                        PaddedTextDisabled("No user/session detected.");
                        if (StandardButton("Check Again")) {
                        }
                        break;
                    }

                    CenterSeparator("World Quests");
                    ToggleCardIcon("Collect Quest Collectibles", "After claiming rewards, collect associated quest collectibles", "UiMiscPlusMedium", &s_wqCollectCollectibles);

                    auto& wqHelper = tsm::network::helpers::WorldQuestHelper::Get();
                    if (wqHelper.IsStatusLoading()) {
                        ImGui::BeginDisabled(true);
                        ButtonCard("Reload World Quest Status", "Fetching completed world quests...", "UiMiscView", "RELOAD");
                        ImGui::EndDisabled();
                    } else {
                        size_t completedCnt = wqHelper.GetCompleted().size();
                        char trail[48];
                        const char* fmt = tsm::ui::i18n::Tr("%zu Done");
                        std::snprintf(trail, sizeof(trail), fmt, completedCnt);
                        if (ButtonCard("Reload World Quest Status", "Get current completed world quests from server", "UiMiscView", completedCnt>0?trail:nullptr)) {
                            wqHelper.FetchStatusAsync();
                            wqHelper.SetManuallyReloaded(true);
                        }
                    }

                    if (wqHelper.IsRunning()) {
                        std::string msg; { std::lock_guard<std::mutex> lk(s_wqUiMu); msg = s_wqMsg; }
                        if (ButtonCard("Cancel", msg.c_str(), "UiMiscX")) { wqHelper.Cancel(); }
                    } else {
                        if (wqHelper.HasManuallyReloaded() && wqHelper.GetDefinitions().empty()) wqHelper.BuildDefinitions(false);
                        size_t remaining = 0;
                        if (wqHelper.HasManuallyReloaded()) {
                            const auto& completed = wqHelper.GetCompleted();
                            const auto& defs = wqHelper.GetDefinitions();
                            auto isQuestFlag = [&defs](const std::string& qname){
                                auto it = defs.find(qname); if (it == defs.end()) return false; for (const auto& rw : it->second) { if (!rw.collectible.empty() && rw.collectible.find("_quest_done") != std::string::npos) return true; } return false;
                            };
                            auto hasCollectible = [&defs](const std::string& qname){
                                auto it = defs.find(qname); if (it == defs.end()) return false; for (const auto& rw : it->second) { if (!rw.collectible.empty() && rw.collectible.find("_quest_done") == std::string::npos) return true; } return false;
                            };
                            for (const auto& kv : defs) {
                                const std::string& q = kv.first;
                                if (completed.count(q) > 0) continue;
                                if (isQuestFlag(q)) continue;
                                if (!s_wqCollectCollectibles && hasCollectible(q)) continue;
                                ++remaining;
                            }
                        }
                        char trail[48];
                        const char* fmt = tsm::ui::i18n::Tr("%zu Quests");
                        std::snprintf(trail, sizeof(trail), fmt, remaining);
                        if (ButtonCard("Run World Quests", "Claim rewards for incomplete world quests", "UiMiscStar", trail)) {
                            wqHelper.StartRun(s_wqCollectCollectibles, WQ_UpdateProgress);
                        }
                    }

                    float list_h = ImGui::GetContentRegionAvail().y; if (list_h < 0.0f) list_h = 0.0f;
                    if (BeginPannableChild("##quick_scroll")) {
                        if (!wqHelper.HasManuallyReloaded()) {
                            PaddedTextDisabled("No data loaded. Please reload world quest status first.");
                        } else {
                            if (wqHelper.GetDefinitions().empty()) wqHelper.BuildDefinitions(false);
                            const auto& completed = wqHelper.GetCompleted();
                            const auto& defs = wqHelper.GetDefinitions();
                            auto isQuestFlag = [&defs](const std::string& qname){
                                auto it = defs.find(qname); if (it == defs.end()) return false; for (const auto& rw : it->second) { if (!rw.collectible.empty() && rw.collectible.find("_quest_done") != std::string::npos) return true; } return false;
                            };
                            auto hasCollectible = [&defs](const std::string& qname){
                                auto it = defs.find(qname); if (it == defs.end()) return false; for (const auto& rw : it->second) { if (!rw.collectible.empty() && rw.collectible.find("_quest_done") == std::string::npos) return true; } return false;
                            };
                            std::vector<std::string> items; items.reserve(defs.size());
                            for (const auto& kv : defs) {
                                const std::string& q = kv.first;
                                if (completed.count(q) > 0) continue;
                                if (isQuestFlag(q)) continue;
                                if (!s_wqCollectCollectibles && hasCollectible(q)) continue;
                                items.push_back(q);
                            }
                            if (items.empty()) {
                                PaddedTextDisabled("No world quests to process.");
                            } else {
                                std::sort(items.begin(), items.end());
                                for (const auto& q : items) {
                                    int colCount = 0; auto it = defs.find(q); if (it != defs.end()) {
                                        for (const auto& rw : it->second) { if (!rw.collectible.empty() && rw.collectible.find("_quest_done") == std::string::npos) ++colCount; }
                                    }
                                    const char* trailing = nullptr; char trailBuf[32]; if (colCount > 0) {
                                        const char* trailFmt = tsm::ui::i18n::Tr("x%d");
                                        std::snprintf(trailBuf, sizeof(trailBuf), trailFmt, colCount);
                                        trailing = trailBuf;
                                    }
                                    std::string displayName = tsm::network::GetQuestDescription(q);
                                    if (ButtonCard(displayName.c_str(), "Claim reward (and collect items if enabled)", "UiMenuQuestHint", trailing)) {
                                        std::string name = q; bool collect = s_wqCollectCollectibles;
                                        auto defsCopy = defs;
                                        std::thread([name, collect, defsCopy]{
                                            tsm::network::ClaimQuestReward(name);
                                            if (collect) {
                                                auto it = defsCopy.find(name); if (it != defsCopy.end()) {
                                                    for (const auto& rw : it->second) {
                                                        if (!rw.collectible.empty() && rw.collectible.find("_quest_done") == std::string::npos) {
                                                            tsm::lua::helpers::CollectCollectible(rw.collectible, "emote");
                                                            std::this_thread::sleep_for(std::chrono::seconds(2));
                                                        }
                                                    }
                                                }
                                            }
                                            tsm::network::helpers::WorldQuestHelper::Get().MarkCompleted(name);
                                        }).detach();
                                    }
                                }
                            }
                        }
                    }
                    EndPannableChild();
                }
                break; }
            case 2: {

                const std::string& uid = tsm::game::hooks::HookManager::Get().GetCapturedUserId();
                const std::string& sid = tsm::game::hooks::HookManager::Get().GetCapturedSessionId();
                const bool loggedIn = !uid.empty() && !sid.empty();

                if (!loggedIn) {
                    CenterSeparator("Login required");
                    PaddedTextDisabled("No user/session detected.");
                    if (StandardButton("Check Again")) {
                    }
                    break;
                }

                CenterSeparator("Collectibles");

                {
                    static std::atomic<bool> s_clearDotsRunning{false};
                    static std::atomic<bool> s_clearDotsCancel{false};
                    static std::mutex s_clearUiMu;
                    static float s_clearPct = 0.0f;
                    static std::string s_clearMsg;

                    auto Clear_UpdateProgress = [&](float pct, const char* msg){
                        std::lock_guard<std::mutex> lk(s_clearUiMu);
                        s_clearPct = pct;
                        s_clearMsg = msg ? msg : "";
                    };

                    auto StartClearRedDots = [&](){
                        if (s_clearDotsRunning.exchange(true)) return;
                        s_clearDotsCancel.store(false);
                        Clear_UpdateProgress(10.0f, "Retrieving collectibles...");
                        std::thread([&](){
                            size_t cleared = 0;
                            size_t total = 0;
                            bool cancelled = false;
                            try {
                                auto resp = tsm::network::GetCollectibles();
                                if (!resp.is_object() || !resp.contains("collectibles") || !resp["collectibles"].is_array()) {
                                    Clear_UpdateProgress(95.0f, tsm::ui::i18n::Tr("Failed to retrieve collectibles"));
                                } else {
                                    Clear_UpdateProgress(20.0f, tsm::ui::i18n::Tr("Processing collectibles..."));
                                    std::vector<std::string> toProcess; toProcess.reserve(resp["collectibles"].size());
                                    for (const auto& it : resp["collectibles"]) {
                                        try {
                                            if (!it.is_object()) continue;
                                            if (!it.contains("id") || !it["id"].is_string()) continue;
                                            if (it.contains("used") && it["used"].is_boolean() && !it["used"].get<bool>()) {
                                                toProcess.push_back(it["id"].get<std::string>());
                                            }
                                        } catch (...) {}
                                    }

                                    total = toProcess.size();
                                    if (total == 0) {
                                        Clear_UpdateProgress(95.0f, tsm::ui::i18n::Tr("No unacknowledged collectibles found"));
                                    } else {
                                        const size_t BATCH = 10;
                                        for (size_t i = 0; i < toProcess.size(); i += BATCH) {
                                            if (s_clearDotsCancel.load()) { cancelled = true; break; }
                                            size_t end = std::min<size_t>(i + BATCH, toProcess.size());
                                            for (size_t j = i; j < end; ++j) {
                                                if (s_clearDotsCancel.load()) { cancelled = true; break; }
                                                const std::string& id = toProcess[j];
                                                tsm::network::AcknowledgeCollectible(id);
                                                ++cleared;
                                                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                                            }
                                            float frac = (total > 0) ? (static_cast<float>(cleared) / static_cast<float>(total)) : 1.0f;
                                            float pct = 20.0f + 75.0f * frac;
                                            char msg[96];
                                            const char* msgFmt = tsm::ui::i18n::Tr("Clearing dots (%zu/%zu)");
                                            std::snprintf(msg, sizeof(msg), msgFmt, cleared, total);
                                            Clear_UpdateProgress(pct, msg);
                                            if (cancelled) break;
                                        }
                                        if (!cancelled) {
                                            char done[96];
                                            const char* doneFmt = tsm::ui::i18n::Tr("Cleared %zu items");
                                            std::snprintf(done, sizeof(done), doneFmt, cleared);
                                            Clear_UpdateProgress(95.0f, done);
                                        }
                                    }
                                }
                            } catch (...) {
                                Clear_UpdateProgress(95.0f, tsm::ui::i18n::Tr("Error while clearing dots"));
                            }
                            if (s_clearDotsCancel.load()) { cancelled = true; }
                            s_clearDotsRunning.store(false);
                            Clear_UpdateProgress(100.0f, cancelled ? tsm::ui::i18n::Tr("Canceled") : s_clearMsg.c_str());
                        }).detach();
                    };

                    using namespace tsm::ui::widgets;
                    if (s_clearDotsRunning.load()) {
                        std::string msg; { std::lock_guard<std::mutex> lk(s_clearUiMu); msg = s_clearMsg; }
                        if (ButtonCard("Cancel", msg.c_str(), "UiMiscX", "CANCEL")) { s_clearDotsCancel.store(true); }
                    } else {
                        if (ButtonCard("Clear Red Dots", "Acknowledge new collectibles to remove red dot markers", "UiMiscView", "CLEAR")) {
                            StartClearRedDots();
                        }
                    }
                }

                static std::atomic<bool> s_ownedLoading{false};
                static std::mutex s_ownedMtx;
                static std::unordered_set<std::string> s_ownedSet;
                static int s_catIdx = 0;
                static bool s_collectiblesHasManuallyReloaded = false;

                auto FetchOwnedCollectiblesAsync = [](){
                    if (s_ownedLoading.exchange(true)) return;
                    std::thread([](){
                        std::unordered_set<std::string> owned;
                        try {
                            auto resp = ([&](){ static tsm::network::ApiClient client; return &client; })()->PostJson("/account/get_collectibles");
                            if (resp.is_object() && resp.contains("collectibles") && resp["collectibles"].is_array()) {
                                for (const auto& it : resp["collectibles"]) {
                                    try {
                                        if (it.is_object() && it.contains("id") && it["id"].is_string())
                                            owned.insert(it["id"].get<std::string>());
                                    } catch (...) {}
                                }
                            }
                        } catch (...) {}
                        {
                            std::lock_guard<std::mutex> lk(s_ownedMtx);
                            s_ownedSet = std::move(owned);
                        }
                        s_ownedLoading.store(false);
                    }).detach();
                };

                {
                    static const char* kCatLabels[] = { "Emote", "Attitude", "Voice", "Action" };
                    (void)SelectCardIcon("Category", "Select a collectible category", "UiMiscPlusMedium", &s_catIdx, kCatLabels, 4);
                }

                if (s_ownedLoading.load()) {
                    ImGui::BeginDisabled(true);
                    ButtonCard("Reload Collectibles", "Loading collectible data...", "UiMiscView", "RELOAD");
                    ImGui::EndDisabled();
                } else {
                    if (ButtonCard("Reload Collectibles", "Load defs and fetch owned items", "UiMiscView", "RELOAD")) {
                        (void)tsm::data::DataManager::Get().LoadCollectibleDefs();
                        FetchOwnedCollectiblesAsync();
                        s_collectiblesHasManuallyReloaded = true;
                    }
                }

                static const char* kCats[] = { "emote", "attitude", "voice", "action" };
                const std::string cat = kCats[((s_catIdx % 4) + 4) % 4];

                float ch = ImGui::GetContentRegionAvail().y; if (ch < 0.0f) ch = 0.0f;
                if (BeginPannableChild("##quick_scroll")) {
                    if (!s_collectiblesHasManuallyReloaded) {
                        PaddedTextDisabled("No data loaded. Please reload collectibles first.");
                    } else {
                        const auto& defs = tsm::data::DataManager::Get().GetCollectibleDefs();
                        if (!defs.is_array() || defs.empty()) {
                            PaddedTextDisabled("No data loaded (tap reload)");
                        } else {
                        std::unordered_set<std::string> owned; { std::lock_guard<std::mutex> lk(s_ownedMtx); owned = s_ownedSet; }
                        std::vector<std::pair<std::string,std::string>> items; items.reserve(defs.size());
                        static const std::unordered_set<std::string> kEmotesToHide = {
                            "conduct", "silentclap", "skip", "penguindance"
                        };
                        for (const auto& e : defs) {
                            if (!e.is_object()) continue;
                            if (!e.contains("type") || !e["type"].is_string()) continue;
                            if (e["type"].get<std::string>() != cat) continue;
                            std::string name = e.value("name", std::string());
                            if (name.empty()) continue;
                            if (owned.find(name) != owned.end()) continue;
                            if (cat == "emote" && kEmotesToHide.find(name) != kEmotesToHide.end()) continue;
                            std::string icon = e.value("icon", std::string("UiEmoteCallDefault"));
                            if (!icon.empty()) {
                                static const char* kAnim = "Anim";
                                if (icon.size() >= 4 && icon.compare(icon.size() - 4, 4, kAnim) == 0) {
                                    icon.replace(icon.size() - 4, 4, "0");
                                }
                            }
                            items.emplace_back(std::move(name), std::move(icon));
                        }
                        std::sort(items.begin(), items.end(), [](const auto& a, const auto& b){ return a.first < b.first; });
                            if (items.empty()) {
                                PaddedTextDisabled("You already have all items in this category");
                            } else {
                                for (const auto& it : items) {
                                    if (ButtonCard(it.first.c_str(), "Tap to collect", it.second.c_str(), "COLLECT")) {
                                        std::string nm = it.first; std::string catStr = cat;
                                        std::thread([nm, catStr]{
                                            tsm::lua::helpers::CollectCollectible(nm, catStr);
                                            {
                                                std::lock_guard<std::mutex> lk(s_ownedMtx);
                                                s_ownedSet.insert(nm);
                                            }
                                            std::this_thread::sleep_for(std::chrono::milliseconds(150));
                                        }).detach();
                                    }
                                }
                            }
                        }
                    }
                }
                EndPannableChild();                break; }
            case 3: {
                const std::string& uid = tsm::game::hooks::HookManager::Get().GetCapturedUserId();
                const std::string& sid = tsm::game::hooks::HookManager::Get().GetCapturedSessionId();
                const bool loggedIn = !uid.empty() && !sid.empty();

                if (!loggedIn) {
                    CenterSeparator("Login required");
                    PaddedTextDisabled("No user/session detected.");
                    if (StandardButton("Check Again")) {}
                    break;
                }

                static int s_shopSubTab = 0;
                static const char* kShopSubIcons[] = { "UiMenuShop", "UiMenuBuffEventCurrency" };
                DrawSubTabs(kShopSubIcons, 2, s_shopSubTab, 0.75f);

                if (s_shopSubTab == 0) {
                CenterSeparator("Spirit Shops");

                if (ButtonCard("Constellation Area", "Spawn constellation area at your position", "UiMenuConstellationHome", "SPAWN")) {
                    vec3 pos = tsm::game::api::LocalAvatarPosition();
                    std::ostringstream script;
                    script << "TConstellationArea(" << pos.x << ", " << pos.y << ", " << pos.z << ")";
                    tsm::lua::queue::Enqueue(script.str());
                }

                static std::atomic<bool> s_shopsLoading{false};
                static std::mutex s_shopsMtx;
                static std::vector<std::string> s_spiritShops;
                static char s_shopSearch[128] = "";

                auto FetchShopsAsync = [](){
                    if (s_shopsLoading.exchange(true)) return;
                    std::thread([](){
                        std::vector<std::string> shops;
                        try {
                            auto resp = tsm::network::GetSpiritShops();
                            if (resp.is_object() && resp.contains("spirit_shops") && resp["spirit_shops"].is_array()) {
                                std::unordered_set<std::string> uniqueShops;
                                for (const auto& item : resp["spirit_shops"]) {
                                    try {
                                        if (item.is_object() && item.contains("spirit") && item["spirit"].is_string()) {
                                            uniqueShops.insert(item["spirit"].get<std::string>());
                                        }
                                    } catch (...) {}
                                }
                                shops.assign(uniqueShops.begin(), uniqueShops.end());
                                std::sort(shops.begin(), shops.end());
                            }
                        } catch (...) {}
                        {
                            std::lock_guard<std::mutex> lk(s_shopsMtx);
                            s_spiritShops = std::move(shops);
                        }
                        s_shopsLoading.store(false);
                    }).detach();
                };

                if (s_shopsLoading.load()) {
                    ImGui::BeginDisabled(true);
                    ButtonCard("Reload Spirit Shops", "Loading shop data...", "UiMiscView", "RELOAD");
                    ImGui::EndDisabled();
                } else {
                    if (ButtonCard("Reload Spirit Shops", "Fetch available spirit shops from server", "UiMiscView", "RELOAD")) {
                        FetchShopsAsync();
                    }
                }

                SearchCard("Search...", s_shopSearch, sizeof(s_shopSearch));

                float list_h = ImGui::GetContentRegionAvail().y;
                if (list_h < 0.0f) list_h = 0.0f;

                if (BeginPannableChild("##shop_list")) {
                    std::vector<std::string> shops;
                    {
                        std::lock_guard<std::mutex> lk(s_shopsMtx);
                        shops = s_spiritShops;
                    }

                    if (shops.empty()) {
                        PaddedTextDisabled("No shops loaded (tap reload)");
                    } else {
                        vec3 playerPos = tsm::game::api::LocalAvatarPosition();
                        std::string searchLower = s_shopSearch;
                        std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

                        for (const auto& shopName : shops) {
                            if (!searchLower.empty()) {
                                std::string nameLower = shopName;
                                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                                if (nameLower.find(searchLower) == std::string::npos) continue;
                            }

                            std::string icon = "UiMenuShop";
                            auto collectible = tsm::data::DataManager::Get().FindCollectibleByName(shopName);
                            if (collectible.has_value()) {
                                const auto& obj = collectible.value();
                                if (obj.contains("icon") && obj["icon"].is_string()) {
                                    std::string foundIcon = obj["icon"].get<std::string>();
                                    if (!foundIcon.empty()) {
                                        static const char* kAnim = "Anim";
                                        if (foundIcon.size() >= 4 && foundIcon.compare(foundIcon.size() - 4, 4, kAnim) == 0) {
                                            foundIcon.replace(foundIcon.size() - 4, 4, "0");
                                        }
                                        icon = foundIcon;
                                    }
                                }
                            }

                            if (ButtonCard(shopName.c_str(), "Tap to open this spirit shop in-game", icon.c_str(), "OPEN")) {
                                tsm::lua::helpers::OpenConstellationShop(shopName, playerPos);
                            }
                        }
                    }
                }
                EndPannableChild();
                } else {
                CenterSeparator("Generic Shops");
                static std::atomic<bool> s_genericLoading{false};
                static std::mutex s_genericMtx;
                static std::vector<tsm::network::GenericShopDef> s_genericShops;
                static char s_genericSearch[128] = "";

                auto FetchGenericShopsAsync = [](){
                    if (s_genericLoading.exchange(true)) return;
                    std::thread([](){
                        auto shops = tsm::network::GetGenericShops();
                        { std::lock_guard<std::mutex> lk(s_genericMtx); s_genericShops = std::move(shops); }
                        s_genericLoading.store(false);
                    }).detach();
                };

                if (s_genericLoading.load()) {
                    ImGui::BeginDisabled(true);
                    ButtonCard("Reload Generic Shops", "Loading shop data...", "UiMiscView", "RELOAD");
                    ImGui::EndDisabled();
                } else {
                    if (ButtonCard("Reload Generic Shops", "Fetch available generic shops", "UiMiscView", "RELOAD")) {
                        FetchGenericShopsAsync();
                    }
                }
                SearchCard("Search...", s_genericSearch, sizeof(s_genericSearch));

                float list_h2 = ImGui::GetContentRegionAvail().y;
                if (list_h2 < 0.0f) list_h2 = 0.0f;
                if (BeginPannableChild("##generic_shop_list")) {
                    std::vector<tsm::network::GenericShopDef> shops;
                    { std::lock_guard<std::mutex> lk(s_genericMtx); shops = s_genericShops; }
                    if (shops.empty()) {
                        PaddedTextDisabled("No shops loaded (tap reload)");
                    } else {
                        vec3 playerPos = tsm::game::api::LocalAvatarPosition();
                        std::string searchLower = s_genericSearch;
                        std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);
                        for (const auto& shop : shops) {
                            if (!searchLower.empty()) {
                                std::string nameLower = shop.name;
                                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                                if (nameLower.find(searchLower) == std::string::npos) continue;
                            }
                            const char* iconName = shop.icon.empty() ? "UiMenuShop" : shop.icon.c_str();
                            if (ButtonCard(shop.name.c_str(), "Spawn shop hint at your position", iconName, "HINT")) {
                                tsm::lua::helpers::GenericShopNpcHint(shop.name, shop.icon, playerPos);
                            }
                        }
                    }
                }
                EndPannableChild();
                }
                break; }
            default:
                break;
        }
        EndCard();
    }
}

}}}
