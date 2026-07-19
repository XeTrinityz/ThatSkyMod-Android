#include <ui/panels/SocialPanel.h>
#include <ui/widgets/Layout.h>
#include <ui/widgets/Cards.h>
#include <ui/widgets/Controls.h>
#include <ui/widgets/Buttons.h>
#include <ui/widgets/Icons.h>
#include <ui/widgets/Popups.h>
#include <imgui/imgui.h>
#include <ui/core/metrics.h>
#include <ui/core/App.h>
#include <network/SocialManager.h>
#include <iconloader/IconLoader.h>
#include <algorithm>
#include <game/hooks/HookManager.h>
#include <game/memory/api.h>
#include <game/memory/offsets.h>
#include <game/memory/Memory.h>
#include <game/data/OutfitData.h>
#include <game/data/VoiceData.h>
#include <game/data/StanceData.h>
#include <utils/strings/OutfitNameFormatter.h>
#include <utils/strings/DateFormatter.h>
#include <utils/cosmetics/CosmeticHelpers.h>
#include <ui/helpers/SubTabRenderer.h>
#include <game/interop/LuaScriptQueue.h>
#include <game/interop/LuaHelpers.h>
#include <data/DataManager.h>
#include <ui/core/Localization.h>
#include <cstdio>
#include <ctime>
#include <atomic>
#include <thread>
#include <set>
#include <ui/helpers/Toast.h>
#include <utils/common/esp.h>
#include <state/ModState.h>

namespace tsm { namespace ui { namespace tabs {

static int s_socialSubIndex = 0;
static std::string s_selectedFriendId;
static tsm::network::FriendInfo s_selectedFriendInfo{};
static int s_friendsPage = 0;
static int s_friendsSubTab = 1;
static int s_confirmAction = 0;
static char s_renameBuf[64] = {0};
static std::atomic<bool> s_sendGiftBusy{false};

static bool s_showGiftHeartsModal = false;
static std::set<std::string> s_selectedForHearts;
static std::atomic<bool> s_sendingHearts{false};
static int s_heartsSent = 0;
static int s_heartsTotal = 0;
static char s_giftHeartsSearchBuf[128] = {0};

static int s_selectedPlayerIndex = -1;

static int s_selectedNPCIndex = -1;

using namespace tsm::game::data;

static void WearOutfit(const char* meshName, const std::string& type) {
    std::string outfitName = tsm::utils::cosmetics::MeshToOutfitName(meshName, type);
    if (outfitName.empty()) return;

    std::string lua = std::string("OutfitSelectSetOutfit(game, \"") + outfitName + "\")";
    tsm::lua::queue::Enqueue(lua);
}

static bool GetCosmeticIcon(const char* outfitName, std::string& outIcon) {
    outIcon.clear();
    if (!outfitName || *outfitName == '\0') return false;

    auto optDef = tsm::data::DataManager::Get().FindOutfitByName(outfitName);
    if (optDef.has_value()) {
        const auto& def = *optDef;
        if (def.contains("icon") && def["icon"].is_string()) {
            outIcon = def["icon"].get<std::string>();
        }
        if (!outIcon.empty()) {
            return true;
        }
    }

    std::string fallback = tsm::utils::cosmetics::FindCosmeticIcon(outfitName);
    if (!fallback.empty()) {
        outIcon = std::move(fallback);
        return true;
    }

    return false;
}

static const char* GetAvatarStanceIcon(const tsm::game::api::AvatarInfo& info, const char* fallback) {
    if (!info.avatarStance) return fallback;
    unsigned stanceId = static_cast<unsigned>(*info.avatarStance);
    return tsm::game::data::GetStanceIcon(stanceId);
}

void DrawSocialTab()
{
    using namespace tsm::ui::widgets;
    using namespace tsm::ui::helpers;

    static const char* kSubIcons[] = { "UiMenuFriends", "UiEmoteStanceHero", "UiEmoteAP16ArmWave0" };

    const bool inDetailView = (s_socialSubIndex == 0 && s_friendsPage == 1) ||
                              (s_socialSubIndex == 1 && s_selectedPlayerIndex >= 0) ||
                              (s_socialSubIndex == 2 && s_selectedNPCIndex >= 0);

    auto iconSelector = [&](int i, int activeIdx) -> const char* {
        bool thisTabInDetail = (i == 0 && s_friendsPage == 1) ||
                               (i == 1 && s_selectedPlayerIndex >= 0) ||
                               (i == 2 && s_selectedNPCIndex >= 0);
        return (activeIdx == i && thisTabInDetail) ? "UiMenuExit" : kSubIcons[i];
    };

    auto clickCallback = [&](int i, int& activeIdx) {
        if (activeIdx == i) {
            if (i == 0 && s_friendsPage == 1) {
                s_friendsPage = 0;
                s_confirmAction = 0;
            }
            else if (i == 1 && s_selectedPlayerIndex >= 0) {
                s_selectedPlayerIndex = -1;
            }
            else if (i == 2 && s_selectedNPCIndex >= 0) {
                s_selectedNPCIndex = -1;
            }
        } else {
            activeIdx = i;
        }
    };

    DrawSubTabsEx(kSubIcons, 3, s_socialSubIndex, iconSelector, clickCallback,
                  inDetailView, ImVec4(0.95f, 0.26f, 0.21f, 1.0f));

    if (BeginCard("##social_card", 10.0f, false)) {
        switch (s_socialSubIndex) {
            case 0: {
                static char s_search[128] = {0};

                const std::string& uid = tsm::game::hooks::HookManager::Get().GetCapturedUserId();
                const std::string& sid = tsm::game::hooks::HookManager::Get().GetCapturedSessionId();
                const bool loggedIn = !uid.empty() && !sid.empty();
                if (!loggedIn) {
                    CenterSeparator("Login required");
                    PaddedTextDisabled("No user/session detected.");
                    if (StandardButton("Check Again")) {
                        tsm::network::SocialManager::Get().RefreshAsync();
                    }
                    EndCard();
                    break;
                }

                if (s_friendsPage == 0) {
                    static const char* kFriendsSubIcons[] = { "UiMenuSpellCandleBundle", "UiMenuSortDown" };
                    DrawSubTabs(kFriendsSubIcons, 2, s_friendsSubTab, 0.75f);
                }

                if (s_friendsPage == 0 && s_friendsSubTab == 0) {
                    auto& mgr = tsm::network::SocialManager::Get();
                    struct {
                        bool giftingRunning; int giftingCurrent; int giftingTotal; std::string giftingLastResult;
                        bool collectingRunning; int collectingCurrent; int collectingTotal; std::string collectingLastResult;
                    } snapshotBefore;
                    auto gp = mgr.GetGiftingProgress();
                    auto cp = mgr.GetCollectingProgress();
                    snapshotBefore.giftingRunning = gp.running;
                    snapshotBefore.giftingCurrent = gp.current;
                    snapshotBefore.giftingTotal = gp.total;
                    snapshotBefore.giftingLastResult = gp.result_message;
                    snapshotBefore.collectingRunning = cp.running;
                    snapshotBefore.collectingCurrent = cp.current;
                    snapshotBefore.collectingTotal = cp.total;
                    snapshotBefore.collectingLastResult = cp.result_message;

                    CenterSeparator("Actions");
                    ImGui::Dummy(ImVec2(0, DP(4.0f)));

                    std::string giftDesc = "Send Light to all eligible friends";
                    if (snapshotBefore.giftingRunning) {
                        char buf[64];
                        const char* fmt = tsm::ui::i18n::Tr("Sending... %d / %d");
                        std::snprintf(buf, sizeof(buf), fmt, snapshotBefore.giftingCurrent, snapshotBefore.giftingTotal);
                        giftDesc = buf;
                    } else if (!snapshotBefore.giftingLastResult.empty()) {
                        giftDesc = snapshotBefore.giftingLastResult;
                    }

                    if (snapshotBefore.giftingRunning) ImGui::BeginDisabled();
                    if (ButtonCard("Gift All Light", giftDesc.c_str(), "UiMenuBuffCandle", "GIFT")) {
                        tsm::network::SocialManager::Get().GiftAllLightAsync();
                    }
                    if (snapshotBefore.giftingRunning) ImGui::EndDisabled();

                    std::string collectDesc = "Claim all pending gifts and messages";
                    if (snapshotBefore.collectingRunning) {
                        char buf2[64];
                        const char* fmt = tsm::ui::i18n::Tr("Collecting... %d / %d");
                        std::snprintf(buf2, sizeof(buf2), fmt, snapshotBefore.collectingCurrent, snapshotBefore.collectingTotal);
                        collectDesc = buf2;
                    } else if (!snapshotBefore.collectingLastResult.empty()) {
                        collectDesc = snapshotBefore.collectingLastResult;
                    }

                    if (snapshotBefore.collectingRunning) ImGui::BeginDisabled();
                    if (ButtonCard("Collect Gifts", collectDesc.c_str(), "UiMenuAddCandle", "COLLECT")) {
                        tsm::network::SocialManager::Get().CollectAllGiftsAsync();
                    }
                    if (snapshotBefore.collectingRunning) ImGui::EndDisabled();

                    std::string heartsDesc = "Select friends to send Hearts to";
                    if (s_sendingHearts.load()) {
                        char buf[64];
                        const char* fmt = tsm::ui::i18n::Tr("Sending... %d / %d");
                        std::snprintf(buf, sizeof(buf), fmt, s_heartsSent, s_heartsTotal);
                        heartsDesc = buf;
                    }
                    if (s_sendingHearts.load()) ImGui::BeginDisabled();
                    if (ButtonCard("Gift Hearts", heartsDesc.c_str(), "UiMiscHeart", "SELECT")) {
                        s_showGiftHeartsModal = true;
                        s_selectedForHearts.clear();
                        s_giftHeartsSearchBuf[0] = '\0';
                    }
                    if (s_sendingHearts.load()) ImGui::EndDisabled();

                    static bool s_disableJoinable = false;
                    static std::uint32_t s_joinableOriginal = 0;
                    static bool s_joinableOrigSet = false;
                    static std::atomic<bool> s_togglingJoinable{false};

                    std::string joinableDesc = s_disableJoinable ? "Friends cannot join via constellation" : "Allow friends to join via constellation";
                    if (s_togglingJoinable.load()) {
                        joinableDesc = "Updating...";
                        ImGui::BeginDisabled();
                    }

                    if (ToggleCardIcon("Disable Joinable", joinableDesc.c_str(), "UiMenuDND", &s_disableJoinable)) {
                        s_togglingJoinable.store(true);
                        const bool newState = s_disableJoinable;

                        std::thread([newState](){
                            using namespace tsm::game;

                            bool success = tsm::network::SocialManager::Get().SetJoinable(!newState);

                            if (success) {
                                if (newState) {
                                    if (!s_joinableOrigSet) {
                                        s_joinableOriginal = tsm::game::memory::ReadU32(Offsets::kSetJoinableFunction);
                                        s_joinableOrigSet = true;
                                    }
                                    tsm::game::memory::WriteU32(Offsets::kSetJoinableFunction, Signatures::kRetInstruction);
                                    tsm::ui::helpers::ShowToastSuccess("Joinable disabled");
                                } else {
                                    if (s_joinableOrigSet) {
                                        tsm::game::memory::WriteU32(Offsets::kSetJoinableFunction, s_joinableOriginal);
                                        s_joinableOrigSet = false;
                                    }
                                    tsm::ui::helpers::ShowToastSuccess("Joinable enabled");
                                }
                            } else {
                                s_disableJoinable = !newState;
                                tsm::ui::helpers::ShowToastError("Failed to update joinable state");
                            }

                            s_togglingJoinable.store(false);
                        }).detach();
                    }

                    if (s_togglingJoinable.load()) {
                        ImGui::EndDisabled();
                    }
                }

                auto& mgr = tsm::network::SocialManager::Get();
                struct {
                    bool loading; bool ok; std::string err;
                    tsm::network::FriendList friends;
                    std::vector<std::string> online;
                    std::map<std::string, std::uint32_t> onlineLevels;
                    bool giftingRunning; int giftingCurrent; int giftingTotal; std::string giftingLastResult;
                    bool collectingRunning; int collectingCurrent; int collectingTotal; std::string collectingLastResult;
                } sshot;
                sshot.loading = mgr.IsLoading();
                sshot.ok = !sshot.loading;
                sshot.friends = mgr.GetFriends();
                sshot.online = mgr.GetOnlineFriends();
                sshot.onlineLevels = mgr.GetOnlineLevels();
                auto gp = mgr.GetGiftingProgress();
                auto cp = mgr.GetCollectingProgress();
                sshot.giftingRunning = gp.running;
                sshot.giftingCurrent = gp.current;
                sshot.giftingTotal = gp.total;
                sshot.giftingLastResult = gp.result_message;
                sshot.collectingRunning = cp.running;
                sshot.collectingCurrent = cp.current;
                sshot.collectingTotal = cp.total;
                sshot.collectingLastResult = cp.result_message;

                static bool s_autoRefreshRequested = false;
                if (!s_autoRefreshRequested && !sshot.loading && !sshot.ok) {
                    tsm::network::SocialManager::Get().RefreshAsync();
                    s_autoRefreshRequested = true;
                }

                if (!sshot.ok && !sshot.loading) {
                    PaddedTextDisabled(sshot.err.empty() ? "Not logged in or failed to load." : sshot.err.c_str());
                    EndCard();
                    break;
                }

                static bool s_prevGiftRunning = false;
                static std::string s_prevGiftResult;
                static bool s_prevCollectRunning = false;
                static std::string s_prevCollectResult;
                if (s_prevGiftRunning && !sshot.giftingRunning && !sshot.giftingLastResult.empty() && sshot.giftingLastResult != s_prevGiftResult) {
                    const std::string& msg = sshot.giftingLastResult;
                    if (msg.find("Failed") != std::string::npos || msg.find("No ") != std::string::npos || msg.find("Not logged") != std::string::npos)
                        tsm::ui::helpers::ShowToastError(msg);
                    else
                        tsm::ui::helpers::ShowToastSuccess(msg);
                }
                if (s_prevCollectRunning && !sshot.collectingRunning && !sshot.collectingLastResult.empty() && sshot.collectingLastResult != s_prevCollectResult) {
                    const std::string& msg = sshot.collectingLastResult;
                    if (msg.find("Failed") != std::string::npos || msg.find("No ") != std::string::npos || msg.find("Not logged") != std::string::npos)
                        tsm::ui::helpers::ShowToastError(msg);
                    else
                        tsm::ui::helpers::ShowToastSuccess(msg);
                }
                s_prevGiftRunning = sshot.giftingRunning;
                s_prevGiftResult = sshot.giftingLastResult;
                s_prevCollectRunning = sshot.collectingRunning;
                s_prevCollectResult = sshot.collectingLastResult;

                if (s_friendsPage == 0 && s_friendsSubTab == 1) {
                    CenterSeparator("Friends");
                    {
                        bool loadingList = sshot.loading;
                        if (!loadingList && StandardButton("Refresh")) { tsm::network::SocialManager::Get().RefreshAsync(); }
                        if (loadingList) { ImGui::BeginDisabled(); StandardButton("Refreshing..."); ImGui::EndDisabled(); }
                    }
                    SearchCard("Search...", s_search, sizeof(s_search));
                    if (sshot.ok && !sshot.loading && sshot.friends.empty()) {
                        PaddedTextDisabled("No friends found.");
                        EndCard();
                        break;
                    }
                }

                auto contains_ci = [](const std::string& hay, const char* needle){
                    if (!needle || *needle == '\0') return true;
                    std::string h = hay, n = needle;
                    std::transform(h.begin(), h.end(), h.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                    std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                    return h.find(n) != std::string::npos;
                };

                auto isOnline = [&](const std::string& id){ return std::find(sshot.online.begin(), sshot.online.end(), id) != sshot.online.end(); };

                std::vector<const std::pair<std::string, tsm::network::FriendInfo>*> onlineActive;
                std::vector<const std::pair<std::string, tsm::network::FriendInfo>*> offlineActive;
                std::vector<const std::pair<std::string, tsm::network::FriendInfo>*> blockedOrDeleted;
                for (const auto& p : sshot.friends) {
                    const auto& id = p.first; const auto& info = p.second;
                    std::string display = info.nickname.empty() ? std::string("(No name)") : info.nickname;
                    if (!(contains_ci(display, s_search) || contains_ci(id, s_search))) continue;
                    if (info.deleted || info.blocked || info.blocked_by) blockedOrDeleted.push_back(&p);
                    else if (isOnline(id)) onlineActive.push_back(&p);
                    else offlineActive.push_back(&p);
                }
                auto sortByNick = [](const auto* a, const auto* b){ return a->second.nickname < b->second.nickname; };
                std::sort(onlineActive.begin(), onlineActive.end(), sortByNick);
                std::sort(offlineActive.begin(), offlineActive.end(), sortByNick);
                std::sort(blockedOrDeleted.begin(), blockedOrDeleted.end(), sortByNick);

                auto addRow = [&](const std::pair<std::string, tsm::network::FriendInfo>& p, bool on){
                    const std::string& id = p.first; const auto& info = p.second;
                    std::string title = info.nickname.empty() ? std::string(tsm::ui::i18n::Tr("(No name)")) : info.nickname;
                    if (info.deleted) title += std::string(" ") + tsm::ui::i18n::Tr("(Deleted)");
                    else if (info.blocked && info.blocked_by) title += std::string(" ") + tsm::ui::i18n::Tr("(Mutual Block)");
                    else if (info.blocked) title += std::string(" ") + tsm::ui::i18n::Tr("(Blocked)");
                    else if (info.blocked_by) title += std::string(" ") + tsm::ui::i18n::Tr("(Blocked You)");
                    std::string desc;
                    if (info.deleted) desc = tsm::ui::i18n::Tr("Deleted");
                    else if (info.blocked && info.blocked_by) desc = tsm::ui::i18n::Tr("Mutually Blocked");
                    else if (info.blocked) desc = tsm::ui::i18n::Tr("Blocked");
                    else if (info.blocked_by) desc = tsm::ui::i18n::Tr("Blocked You");
                    else if (on) {
                        auto it = sshot.onlineLevels.find(id);
                        desc = std::string(tsm::ui::i18n::Tr("Online")) + (it != sshot.onlineLevels.end() ? std::string(" - ") + tsm::network::SocialManager::Get().GetLevelName(it->second) : std::string(""));
                    } else desc = tsm::ui::i18n::Tr("Offline");
                    if (ButtonCard(title.c_str(), desc.c_str(), "UiMenuFriends", "OPEN")) {
                        s_selectedFriendId = id;
                        s_selectedFriendInfo = info;
                        s_friendsPage = 1;
                        s_confirmAction = 0;
                        std::snprintf(s_renameBuf, sizeof(s_renameBuf), "%s", info.nickname.c_str());
                    }
                };
                if (s_friendsPage == 0 && s_friendsSubTab == 1) {
                    if (BeginPannableChild("##friends_scroll")) {
                        for (auto* pr : onlineActive) addRow(*pr, true);
                        for (auto* pr : offlineActive) addRow(*pr, false);
                        for (auto* pr : blockedOrDeleted) addRow(*pr, false);
                    }
                    EndPannableChild();
                } else if (s_friendsPage == 1) {
                    for (const auto& kv : sshot.friends) {
                        if (kv.first == s_selectedFriendId) { s_selectedFriendInfo = kv.second; break; }
                    }

                    if (BeginPannableChild("##friend_details_scroll")) {
                        const std::string title = s_selectedFriendInfo.nickname.empty() ? s_selectedFriendId : s_selectedFriendInfo.nickname;
                        CenterSeparator(title.c_str());

                        bool isOn = std::find(sshot.online.begin(), sshot.online.end(), s_selectedFriendId) != sshot.online.end();
                        auto lvlIt = sshot.onlineLevels.find(s_selectedFriendId);

                        std::string status;
                        if (s_selectedFriendInfo.deleted) status = tsm::ui::i18n::Tr("Deleted");
                        else if (s_selectedFriendInfo.blocked && s_selectedFriendInfo.blocked_by) status = tsm::ui::i18n::Tr("Mutually Blocked");
                        else if (s_selectedFriendInfo.blocked) status = tsm::ui::i18n::Tr("Blocked");
                        else if (s_selectedFriendInfo.blocked_by) status = tsm::ui::i18n::Tr("Blocked You");
                        else status = isOn ? tsm::ui::i18n::Tr("Online") : tsm::ui::i18n::Tr("Offline");

                        ImGui::BeginDisabled();

                        ButtonCard("Friend ID", s_selectedFriendId.c_str(), "UiMenuCrossPlatformPlayer", "");

                        std::string friendSince = tsm::utils::FormatDateDMY(s_selectedFriendInfo.when_created);
                        ButtonCard("Friend Since", friendSince.c_str(), "UiMiscHeart", "");

                        if (isOn && lvlIt != sshot.onlineLevels.end()) {
                            std::string levelName = tsm::network::SocialManager::Get().GetLevelName(lvlIt->second);
                            ButtonCard("Current Level", levelName.c_str(), "UiMenuMap", "");
                        }

                        const char* statusIcon = isOn ? "UiMiscCheck" : "UiMiscX";
                        if (s_selectedFriendInfo.deleted || s_selectedFriendInfo.blocked || s_selectedFriendInfo.blocked_by) {
                            statusIcon = "UiMiscX";
                        }
                        ButtonCard("Status", status.c_str(), statusIcon, "");

                        if (s_selectedFriendInfo.outfit.voice < kVoiceCount) {
                            const char* voiceName = kVoiceNames[s_selectedFriendInfo.outfit.voice].c_str();
                            const char* voiceIcon = GetVoiceIcon(s_selectedFriendInfo.outfit.voice);
                            ButtonCard("Voice", voiceName, voiceIcon, "");
                        }

                        if (s_selectedFriendInfo.outfit.stance < kStanceCount) {
                            const char* stanceName = kStanceNames[s_selectedFriendInfo.outfit.stance];
                            const char* stanceIcon = GetStanceIcon(s_selectedFriendInfo.outfit.stance);
                            ButtonCard("Stance", stanceName, stanceIcon, "");
                        }

                        if (s_selectedFriendInfo.outfit.height != 0.0f) {
                            char buf[64];
                            std::snprintf(buf, sizeof(buf), "%.3f", s_selectedFriendInfo.outfit.height);
                            ButtonCard("Height", buf, "UiMenuBuffRerollHeight", "");
                        }

                        if (s_selectedFriendInfo.outfit.scale != 0.0f) {
                            char buf[64];
                            std::snprintf(buf, sizeof(buf), "%.3f", s_selectedFriendInfo.outfit.scale);
                            ButtonCard("Scale", buf, "UiMenuBuffRerollHeight", "");
                        }

                        ImGui::EndDisabled();

                        bool hasCosmetics = !s_selectedFriendInfo.outfit.parts.empty();

                        if (hasCosmetics) {
                            ImGui::Dummy(ImVec2(0, 4));
                            CenterSeparator("Cosmetics");
                        }

                        auto it_hair = s_selectedFriendInfo.outfit.parts.find("hair");
                        if (it_hair != s_selectedFriendInfo.outfit.parts.end()) {
                            std::string displayName = tsm::utils::FormatOutfitName(it_hair->second.name, "Hair");
                            if (displayName != "None") {
                                std::string icon;
                                ImVec4 iconColor(1,1,1,1);
                                bool hasIcon = GetCosmeticIcon(it_hair->second.name.c_str(), icon);
                                const char* iconName = hasIcon ? icon.c_str() : "UiOutfitHairBraidSideSmall";
                                if (ButtonCard("Hair", displayName.c_str(), iconName, "WEAR")) {
                                    WearOutfit(it_hair->second.name.c_str(), "Hair");
                                }
                            }
                        }
                        auto it_hat = s_selectedFriendInfo.outfit.parts.find("hat");
                        if (it_hat != s_selectedFriendInfo.outfit.parts.end()) {
                            std::string displayName = tsm::utils::FormatOutfitName(it_hat->second.name, "Hat");
                            if (displayName != "None") {
                                std::string icon;
                                ImVec4 iconColor(1,1,1,1);
                                bool hasIcon = GetCosmeticIcon(it_hat->second.name.c_str(), icon);
                                const char* iconName = hasIcon ? icon.c_str() : "UiOutfitHatBasic";
                                if (ButtonCard("Hat", displayName.c_str(), iconName, "WEAR")) {
                                    WearOutfit(it_hat->second.name.c_str(), "Hat");
                                }
                            }
                        }
                        auto it_mask = s_selectedFriendInfo.outfit.parts.find("mask");
                        if (it_mask != s_selectedFriendInfo.outfit.parts.end()) {
                            std::string displayName = tsm::utils::FormatOutfitName(it_mask->second.name, "Mask");
                            if (displayName != "None") {
                                std::string icon;
                                ImVec4 iconColor(1,1,1,1);
                                bool hasIcon = GetCosmeticIcon(it_mask->second.name.c_str(), icon);
                                const char* iconName = hasIcon ? icon.c_str() : "UiOutfitMaskBasic";
                                if (ButtonCard("Mask", displayName.c_str(), iconName, "WEAR")) {
                                    WearOutfit(it_mask->second.name.c_str(), "Mask");
                                }
                            }
                        }
                        auto it_face = s_selectedFriendInfo.outfit.parts.find("face");
                        if (it_face != s_selectedFriendInfo.outfit.parts.end()) {
                            std::string displayName = tsm::utils::FormatOutfitName(it_face->second.name, "Face");
                            if (displayName != "None") {
                                std::string icon;
                                ImVec4 iconColor(1,1,1,1);
                                bool hasIcon = GetCosmeticIcon(it_face->second.name.c_str(), icon);
                                const char* iconName = hasIcon ? icon.c_str() : "UiMenuWardrobe";
                                if (ButtonCard("Face", displayName.c_str(), iconName, "WEAR")) {
                                    WearOutfit(it_face->second.name.c_str(), "Face");
                                }
                            }
                        }
                        auto it_neck = s_selectedFriendInfo.outfit.parts.find("neck");
                        if (it_neck != s_selectedFriendInfo.outfit.parts.end()) {
                            std::string displayName = tsm::utils::FormatOutfitName(it_neck->second.name, "Neck");
                            if (displayName != "None") {
                                std::string icon;
                                ImVec4 iconColor(1,1,1,1);
                                bool hasIcon = GetCosmeticIcon(it_neck->second.name.c_str(), icon);
                                const char* iconName = hasIcon ? icon.c_str() : "UiOutfitNeckAP17Scarf";
                                if (ButtonCard("Neck", displayName.c_str(), iconName, "WEAR")) {
                                    WearOutfit(it_neck->second.name.c_str(), "Neck");
                                }
                            }
                        }
                        auto it_body = s_selectedFriendInfo.outfit.parts.find("body");
                        if (it_body != s_selectedFriendInfo.outfit.parts.end()) {
                            std::string displayName = tsm::utils::FormatOutfitName(it_body->second.name, "Body");
                            if (displayName != "None") {
                                std::string icon;
                                ImVec4 iconColor(1,1,1,1);
                                bool hasIcon = GetCosmeticIcon(it_body->second.name.c_str(), icon);
                                const char* iconName = hasIcon ? icon.c_str() : "UiOutfitBodyClassicPants";
                                if (ButtonCard("Body", displayName.c_str(), iconName, "WEAR")) {
                                    WearOutfit(it_body->second.name.c_str(), "Body");
                                }
                            }
                        }
                        auto it_arms = s_selectedFriendInfo.outfit.parts.find("arms");
                        if (it_arms != s_selectedFriendInfo.outfit.parts.end()) {
                            std::string displayName = tsm::utils::FormatOutfitName(it_arms->second.name, "Arms");
                            if (displayName != "None") {
                                std::string icon;
                                ImVec4 iconColor(1,1,1,1);
                                bool hasIcon = GetCosmeticIcon(it_arms->second.name.c_str(), icon);
                                const char* iconName = hasIcon ? icon.c_str() : "UiMenuWardrobe";
                                if (ButtonCard("Arms", displayName.c_str(), iconName, "WEAR")) {
                                    WearOutfit(it_arms->second.name.c_str(), "Arms");
                                }
                            }
                        }
                        auto it_feet = s_selectedFriendInfo.outfit.parts.find("feet");
                        if (it_feet != s_selectedFriendInfo.outfit.parts.end()) {
                            std::string displayName = tsm::utils::FormatOutfitName(it_feet->second.name, "Feet");
                            if (displayName != "None") {
                                std::string icon;
                                ImVec4 iconColor(1,1,1,1);
                                bool hasIcon = GetCosmeticIcon(it_feet->second.name.c_str(), icon);
                                const char* iconName = hasIcon ? icon.c_str() : "UiOutfitFeetSocks";
                                if (ButtonCard("Feet", displayName.c_str(), iconName, "WEAR")) {
                                    WearOutfit(it_feet->second.name.c_str(), "Feet");
                                }
                            }
                        }
                        auto it_wing = s_selectedFriendInfo.outfit.parts.find("wing");
                        if (it_wing != s_selectedFriendInfo.outfit.parts.end()) {
                            std::string displayName = tsm::utils::FormatOutfitName(it_wing->second.name, "Wing");
                            if (displayName != "None") {
                                std::string icon;
                                ImVec4 iconColor(1,1,1,1);
                                bool hasIcon = GetCosmeticIcon(it_wing->second.name.c_str(), icon);
                                const char* iconName = hasIcon ? icon.c_str() : "UiOutfitCape";
                                if (ButtonCard("Wing", displayName.c_str(), iconName, "WEAR")) {
                                    WearOutfit(it_wing->second.name.c_str(), "Wing");
                                }
                            }
                        }
                        auto it_horn = s_selectedFriendInfo.outfit.parts.find("horn");
                        if (it_horn != s_selectedFriendInfo.outfit.parts.end()) {
                            std::string displayName = tsm::utils::FormatOutfitName(it_horn->second.name, "Horn");
                            if (displayName != "None") {
                                std::string icon;
                                ImVec4 iconColor(1,1,1,1);
                                bool hasIcon = GetCosmeticIcon(it_horn->second.name.c_str(), icon);
                                const char* iconName = hasIcon ? icon.c_str() : "UiMenuWardrobe";
                                if (ButtonCard("Horn", displayName.c_str(), iconName, "WEAR")) {
                                    WearOutfit(it_horn->second.name.c_str(), "Horn");
                                }
                            }
                        }
                        auto it_prop = s_selectedFriendInfo.outfit.parts.find("prop");
                        if (it_prop != s_selectedFriendInfo.outfit.parts.end()) {
                            std::string displayName = tsm::utils::FormatOutfitName(it_prop->second.name, "Prop");
                            if (displayName != "None") {
                                std::string icon;
                                ImVec4 iconColor(1,1,1,1);
                                bool hasIcon = GetCosmeticIcon(it_prop->second.name.c_str(), icon);
                                const char* iconName = hasIcon ? icon.c_str() : tsm::utils::cosmetics::GetPropIconFallback(it_prop->second.name.c_str());
                                if (ButtonCard("Prop", displayName.c_str(), iconName, "WEAR")) {
                                    WearOutfit(it_prop->second.name.c_str(), "Prop");
                                }
                            }
                        }

                        CenterSeparator("Actions");

                        const bool isBlocked   = s_selectedFriendInfo.blocked;
                        const bool isBlockedBy = s_selectedFriendInfo.blocked_by;
                        const bool isDeleted   = s_selectedFriendInfo.deleted;

                        const std::string friendLabel = s_selectedFriendInfo.nickname.empty() ? s_selectedFriendId : s_selectedFriendInfo.nickname;
                        auto toastGift = [&](const tsm::network::GiftResult& gr, const char* giftLabel){
                            const std::string& code = gr.code;
                            char buf[256];
                            const char* giftLabelTr = tsm::ui::i18n::Tr(giftLabel);

                            if (code == "success" || (gr.success && code.empty())) {
                                const char* fmt = tsm::ui::i18n::Tr("%s gifted to %s");
                                std::snprintf(buf, sizeof(buf), fmt, giftLabelTr, friendLabel.c_str());
                                tsm::ui::helpers::ShowToastSuccess(buf);
                                return;
                            }
                            if (code == "already_today") {
                                const char* fmt = tsm::ui::i18n::Tr("%s already gifted to %s today");
                                std::snprintf(buf, sizeof(buf), fmt, giftLabelTr, friendLabel.c_str());
                                tsm::ui::helpers::ShowToastInfo(buf);
                                return;
                            }
                            if (code == "already_pending") {
                                const char* fmt = tsm::ui::i18n::Tr("%s already gifted to %s");
                                std::snprintf(buf, sizeof(buf), fmt, giftLabelTr, friendLabel.c_str());
                                tsm::ui::helpers::ShowToastInfo(buf);
                                return;
                            }
                            if (code == "missing_prereq") {
                                const char* fmt = tsm::ui::i18n::Tr("Insufficient relationship level with %s for gifting %s");
                                std::snprintf(buf, sizeof(buf), fmt, friendLabel.c_str(), giftLabelTr);
                                tsm::ui::helpers::ShowToastError(buf);
                                return;
                            }
                            if (code == "missing_gift_reqs") {
                                tsm::ui::helpers::ShowToastError(tsm::ui::i18n::Tr("Missing gift requirements"));
                                return;
                            }
                            if (code == "insuf") {
                                tsm::ui::helpers::ShowToastError(tsm::ui::i18n::Tr("Insufficient Funds"));
                                return;
                            }
                            if (gr.success) {
                                const char* fmt = tsm::ui::i18n::Tr("%s gifted to %s");
                                std::snprintf(buf, sizeof(buf), fmt, giftLabelTr, friendLabel.c_str());
                                tsm::ui::helpers::ShowToastSuccess(buf);
                            } else {
                                const char* fmt = tsm::ui::i18n::Tr("Failed to send %s");
                                std::snprintf(buf, sizeof(buf), fmt, giftLabelTr);
                                tsm::ui::helpers::ShowToastError(buf);
                            }
                        };

                        if (!isBlockedBy && !isDeleted && !isBlocked) {
                            if (s_sendGiftBusy.load()) ImGui::BeginDisabled();
                            const char* lightDesc = s_sendGiftBusy.load() ? "Sending..." : "Send Light to this friend";
                            if (ButtonCard("Gift Light", lightDesc, "UiMenuBuffCandle", "GIFT")) {
                                s_sendGiftBusy.store(true);
                                const std::string idCopy = s_selectedFriendId;
                                const std::string friendLabelCopy = friendLabel;
                                std::thread([idCopy, friendLabelCopy](){
                                    auto gr = tsm::network::SocialManager::Get().GiftLightToEx(idCopy);
                                    const std::string& code = gr.code;
                                    char buf[256];

                                    if (code == "success" || code == "ok" || code == "true" || code == "sent" || (gr.success && code.empty())) {
                                        const char* fmt = tsm::ui::i18n::Tr("Light gifted to %s");
                                        std::snprintf(buf, sizeof(buf), fmt, friendLabelCopy.c_str());
                                        tsm::ui::helpers::ShowToastSuccess(buf);
                                    } else if (code == "already_today") {
                                        const char* fmt = tsm::ui::i18n::Tr("Light already gifted to %s today");
                                        std::snprintf(buf, sizeof(buf), fmt, friendLabelCopy.c_str());
                                        tsm::ui::helpers::ShowToastInfo(buf);
                                    } else if (code == "already_pending") {
                                        const char* fmt = tsm::ui::i18n::Tr("Light already gifted to %s");
                                        std::snprintf(buf, sizeof(buf), fmt, friendLabelCopy.c_str());
                                        tsm::ui::helpers::ShowToastInfo(buf);
                                    } else if (code == "missing_prereq") {
                                        const char* fmt = tsm::ui::i18n::Tr("Insufficient relationship level with %s for gifting Light");
                                        std::snprintf(buf, sizeof(buf), fmt, friendLabelCopy.c_str());
                                        tsm::ui::helpers::ShowToastError(buf);
                                    } else if (code == "missing_gift_reqs") {
                                        tsm::ui::helpers::ShowToastError(tsm::ui::i18n::Tr("Missing gift requirements"));
                                    } else if (code == "insuf") {
                                        tsm::ui::helpers::ShowToastError(tsm::ui::i18n::Tr("Insufficient Funds"));
                                    } else if (gr.success) {
                                        const char* fmt = tsm::ui::i18n::Tr("Light gifted to %s");
                                        std::snprintf(buf, sizeof(buf), fmt, friendLabelCopy.c_str());
                                        tsm::ui::helpers::ShowToastSuccess(buf);
                                    } else {
                                        tsm::ui::helpers::ShowToastError(tsm::ui::i18n::Tr("Failed to send Light"));
                                    }
                                    s_sendGiftBusy.store(false);
                                }).detach();
                            }
                            const char* heartDesc = s_sendGiftBusy.load() ? "Sending..." : "Send a Heart message";
                            if (ButtonCard("Gift Heart", heartDesc, "UiMiscHeart", "GIFT")) {
                                s_sendGiftBusy.store(true);
                                const std::string idCopy = s_selectedFriendId;
                                const std::string friendLabelCopy = friendLabel;
                                std::thread([idCopy, friendLabelCopy](){
                                    auto gr = tsm::network::SocialManager::Get().GiftHeartToEx(idCopy);
                                    const std::string& code = gr.code;
                                    char buf[256];

                                    if (code == "success" || code == "ok" || code == "true" || code == "sent" || (gr.success && code.empty())) {
                                        const char* fmt = tsm::ui::i18n::Tr("Heart gifted to %s");
                                        std::snprintf(buf, sizeof(buf), fmt, friendLabelCopy.c_str());
                                        tsm::ui::helpers::ShowToastSuccess(buf);
                                    } else if (code == "already_today") {
                                        const char* fmt = tsm::ui::i18n::Tr("Heart already gifted to %s today");
                                        std::snprintf(buf, sizeof(buf), fmt, friendLabelCopy.c_str());
                                        tsm::ui::helpers::ShowToastInfo(buf);
                                    } else if (code == "already_pending") {
                                        const char* fmt = tsm::ui::i18n::Tr("Heart already gifted to %s");
                                        std::snprintf(buf, sizeof(buf), fmt, friendLabelCopy.c_str());
                                        tsm::ui::helpers::ShowToastInfo(buf);
                                    } else if (code == "missing_prereq") {
                                        const char* fmt = tsm::ui::i18n::Tr("Insufficient relationship level with %s for gifting Heart");
                                        std::snprintf(buf, sizeof(buf), fmt, friendLabelCopy.c_str());
                                        tsm::ui::helpers::ShowToastError(buf);
                                    } else if (code == "missing_gift_reqs") {
                                        tsm::ui::helpers::ShowToastError(tsm::ui::i18n::Tr("Missing gift requirements"));
                                    } else if (code == "insuf") {
                                        tsm::ui::helpers::ShowToastError(tsm::ui::i18n::Tr("Insufficient Funds"));
                                    } else if (gr.success) {
                                        const char* fmt = tsm::ui::i18n::Tr("Heart gifted to %s");
                                        std::snprintf(buf, sizeof(buf), fmt, friendLabelCopy.c_str());
                                        tsm::ui::helpers::ShowToastSuccess(buf);
                                    } else {
                                        tsm::ui::helpers::ShowToastError(tsm::ui::i18n::Tr("Failed to send Heart"));
                                    }
                                    s_sendGiftBusy.store(false);
                                }).detach();
                            }
                            if (s_sendGiftBusy.load()) ImGui::EndDisabled();
                            if (ButtonCard("Block", "Prevent messages and gifts from this friend", "UiMiscHide", "BLOCK")) { ImGui::OpenPopup("##confirm_block"); }
                        }
                        if (isBlocked && !isDeleted) {
                            if (ButtonCard("Unblock", "Allow messages and gifts again", "UiMiscCheck", "UNBLOCK")) { ImGui::OpenPopup("##confirm_unblock"); }
                        }
                        if (ButtonCard("Delete", "Remove this friend from your list", "UiMiscX", "DELETE")) { ImGui::OpenPopup("##confirm_delete"); }
                        if (ButtonCard("Rename", "Change the displayed nickname", "UiMiscPencil", "RENAME")) { ImGui::OpenPopup("##rename_friend"); }

                        if (ConfirmPopup("##confirm_block", "Block Friend", "Are you sure you want to block this friend?", "Block", "Cancel")) {
                            bool ok = tsm::network::SocialManager::Get().SetBlocked(s_selectedFriendId, true);
                            if (ok) { tsm::ui::helpers::ShowToastSuccess("Blocked"); tsm::network::SocialManager::Get().RefreshAsync(); }
                            else { tsm::ui::helpers::ShowToastError("Block failed"); }
                        }
                        if (ConfirmPopup("##confirm_unblock", "Unblock Friend", "Unblock this friend?", "Unblock", "Cancel")) {
                            bool ok = tsm::network::SocialManager::Get().SetBlocked(s_selectedFriendId, false);
                            if (ok) { tsm::ui::helpers::ShowToastSuccess("Unblocked"); tsm::network::SocialManager::Get().RefreshAsync(); }
                            else { tsm::ui::helpers::ShowToastError("Unblock failed"); }
                        }
                        if (ConfirmPopup("##confirm_delete", "Delete Friend", "Are you sure you want to delete this friend?", "Delete", "Cancel")) {
                            bool ok = tsm::network::SocialManager::Get().SoftDelete(s_selectedFriendId);
                            if (ok) { tsm::ui::helpers::ShowToastSuccess("Deleted"); tsm::network::SocialManager::Get().RefreshAsync(); s_friendsPage = 0; }
                            else { tsm::ui::helpers::ShowToastError("Delete failed"); }
                        }

                        if (InputPopup("##rename_friend", "Rename Friend", "Enter nickname...", s_renameBuf, sizeof(s_renameBuf), "Save", "Cancel")) {
                            bool ok = tsm::network::SocialManager::Get().Rename(s_selectedFriendId, std::string(s_renameBuf));
                            if (ok) { tsm::ui::helpers::ShowToastSuccess("Name updated"); tsm::network::SocialManager::Get().RefreshAsync(); }
                            else { tsm::ui::helpers::ShowToastError("Rename failed"); }
                        }
                    }
                    EndPannableChild();
                }

                if (s_showGiftHeartsModal) {
                    ImGui::OpenPopup("##gift_hearts_modal");
                    s_showGiftHeartsModal = false;
                }

                ImGuiViewport* vp = ImGui::GetMainViewport();
                if (vp) {
                    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                    const char* titleText = tsm::ui::i18n::Tr("Gift Hearts");
                    ImVec2 titleSize = ImGui::CalcTextSize(titleText);

                    const char* selectAllText = tsm::ui::i18n::Tr("Select All");
                    const char* unselectAllText = tsm::ui::i18n::Tr("Unselect All");
                    ImVec2 selectAllSize = ImGui::CalcTextSize(selectAllText);
                    ImVec2 unselectAllSize = ImGui::CalcTextSize(unselectAllText);

                    const char* sendHeartsText = tsm::ui::i18n::Tr("Send Hearts");
                    const char* cancelText = tsm::ui::i18n::Tr("Cancel");
                    ImVec2 sendHeartsSize = ImGui::CalcTextSize(sendHeartsText);
                    ImVec2 cancelSize = ImGui::CalcTextSize(cancelText);

                    int maxSelected = (int)sshot.friends.size();
                    int maxCandles = maxSelected * 3;
                    char statusPreview[128];
                    const char* statusFmt = tsm::ui::i18n::Tr("%d friend(s) selected (~%d candles)");
                    std::snprintf(statusPreview, sizeof(statusPreview), statusFmt, maxSelected, maxCandles);
                    ImVec2 statusSize = ImGui::CalcTextSize(statusPreview);

                    char progressPreview[128];
                    const char* progressFmt = tsm::ui::i18n::Tr("Sending hearts... %d / %d");
                    std::snprintf(progressPreview, sizeof(progressPreview), progressFmt, maxSelected, maxSelected);
                    ImVec2 progressSize = ImGui::CalcTextSize(progressPreview);

                    float contentMax = titleSize.x;
                    float gap = DP(8.0f);
                    contentMax = std::max(contentMax, selectAllSize.x + gap + unselectAllSize.x);
                    contentMax = std::max(contentMax, sendHeartsSize.x + gap + cancelSize.x);
                    contentMax = std::max(contentMax, statusSize.x);
                    contentMax = std::max(contentMax, progressSize.x);

                    float minWidth = DP(320.0f);
                    float sidePadding = DP(24.0f);
                    float targetWidth = contentMax + sidePadding * 2.0f;
                    float maxWidth = std::max(minWidth, vp->Size.x - DP(32.0f));
                    if (targetWidth < minWidth) targetWidth = minWidth;
                    if (targetWidth > maxWidth) targetWidth = maxWidth;

                    float minHeight = DP(320.0f);
                    float verticalPadding = DP(32.0f);
                    float targetHeight = std::max(minHeight, vp->Size.y - verticalPadding);

                    ImGui::SetNextWindowSize(ImVec2(targetWidth, targetHeight), ImGuiCond_Always);
                }
                ImGuiWindowFlags modalFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize;
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(DP(12.0f), DP(10.0f)));
                if (ImGui::BeginPopupModal("##gift_hearts_modal", nullptr, modalFlags)) {
                    float cont_w = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
                    ImVec2 ts = ImGui::CalcTextSize(tsm::ui::i18n::Tr("Gift Hearts"));
                    float x = ImGui::GetWindowContentRegionMin().x + std::max(0.0f, (cont_w - ts.x) * 0.5f);
                    ImGui::SetCursorPosX(x);
                    ImGui::TextUnformatted(tsm::ui::i18n::Tr("Gift Hearts"));
                    ImGui::Separator();

                    ImGui::Dummy(ImVec2(0, DP(4.0f)));

                    float gap = DP(8.0f);
                    float btn_w = (cont_w - gap) * 0.5f;

                    if (!s_sendingHearts.load()) {
                        if (SecondaryButton("Select All", ImVec2(btn_w, 0))) {
                            for (const auto& p : sshot.friends) {
                                const auto& info = p.second;
                                if (!info.deleted && !info.blocked && !info.blocked_by) {
                                    s_selectedForHearts.insert(p.first);
                                }
                            }
                        }
                        ImGui::SameLine(0, gap);
                        if (SecondaryButton("Unselect All", ImVec2(btn_w, 0))) {
                            s_selectedForHearts.clear();
                        }
                    }

                    SearchCard("Search...", s_giftHeartsSearchBuf, sizeof(s_giftHeartsSearchBuf));

                    float contentHeight = ImGui::GetContentRegionAvail().y;
                    float statusTextHeight = ImGui::GetTextLineHeight();
                    float buttonHeight = ImGui::GetFrameHeight();
                    float bottomPadding = DP(16.0f);
                    float reservedHeight = DP(4.0f) + statusTextHeight + DP(4.0f) + buttonHeight + bottomPadding;
                    float listHeight = std::max(DP(100.0f), contentHeight - reservedHeight);

                    std::string searchLower;
                    if (s_giftHeartsSearchBuf[0] != '\0') {
                        searchLower = s_giftHeartsSearchBuf;
                        std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);
                    }

                    if (BeginPannableChild("##gift_hearts_list", ImVec2(0, listHeight))) {
                        std::vector<const std::pair<std::string, tsm::network::FriendInfo>*> friendList;
                        for (const auto& p : sshot.friends) {
                            const auto& info = p.second;
                            if (!info.deleted && !info.blocked && !info.blocked_by) {
                                friendList.push_back(&p);
                            }
                        }
                        std::sort(friendList.begin(), friendList.end(),
                            [](const auto* a, const auto* b) { return a->second.nickname < b->second.nickname; });

                        std::vector<const std::pair<std::string, tsm::network::FriendInfo>*> filteredList;
                        if (!searchLower.empty()) {
                            for (auto* pFriend : friendList) {
                                std::string nickLower = pFriend->second.nickname;
                                std::string idLower = pFriend->first;
                                std::transform(nickLower.begin(), nickLower.end(), nickLower.begin(), ::tolower);
                                std::transform(idLower.begin(), idLower.end(), idLower.begin(), ::tolower);
                                if (nickLower.find(searchLower) != std::string::npos || idLower.find(searchLower) != std::string::npos) {
                                    filteredList.push_back(pFriend);
                                }
                            }
                        } else {
                            filteredList = friendList;
                        }

                        if (filteredList.empty()) {
                            PaddedTextDisabled(searchLower.empty() ? "No eligible friends found." : "No friends match search.");
                        } else {
                            for (auto* pFriend : filteredList) {
                                const std::string& id = pFriend->first;
                                const auto& info = pFriend->second;
                                std::string displayName = info.nickname.empty() ? id : info.nickname;

                                bool selected = s_selectedForHearts.find(id) != s_selectedForHearts.end();

                                if (s_sendingHearts.load()) ImGui::BeginDisabled();

                                if (ButtonCard(displayName.c_str(),
                                             id.c_str(),
                                             selected ? "UiMiscCheck" : "UiMenuFriends",
                                             selected ? "UNSELECT" : "SELECT")) {
                                    if (selected) {
                                        s_selectedForHearts.erase(id);
                                    } else {
                                        s_selectedForHearts.insert(id);
                                    }
                                }

                                if (s_sendingHearts.load()) ImGui::EndDisabled();
                            }
                        }
                    }
                    EndPannableChild();

                    ImGui::Dummy(ImVec2(0, DP(4.0f)));

                    if (s_sendingHearts.load()) {
                        char statusBuf[64];
                        const char* fmt = tsm::ui::i18n::Tr("Sending hearts... %d / %d");
                        std::snprintf(statusBuf, sizeof(statusBuf), fmt, s_heartsSent, s_heartsTotal);
                        PaddedText(statusBuf);
                    } else {
                        char countBuf[128];
                        int selectedCount = (int)s_selectedForHearts.size();
                        int candleCost = selectedCount * 3;
                        const char* fmt = tsm::ui::i18n::Tr("%d friend(s) selected (~%d candles)");
                        std::snprintf(countBuf, sizeof(countBuf), fmt, selectedCount, candleCost);
                        PaddedText(countBuf);
                    }

                    ImGui::Dummy(ImVec2(0, DP(4.0f)));

                    bool canSend = !s_selectedForHearts.empty() && !s_sendingHearts.load();

                    if (!canSend) ImGui::BeginDisabled();
                    if (AccentButton("Send Hearts", ImVec2(btn_w, 0))) {
                        s_sendingHearts.store(true);
                        s_heartsSent = 0;
                        s_heartsTotal = (int)s_selectedForHearts.size();

                        std::set<std::string> selectedCopy = s_selectedForHearts;

                        std::thread([selectedCopy]() {
                            int successCount = 0;
                            int alreadySentCount = 0;
                            int insufficientCount = 0;
                            int prereqCount = 0;
                            int rateLimitCount = 0;
                            int otherFailCount = 0;

                            for (const std::string& friendId : selectedCopy) {
                                auto result = tsm::network::SocialManager::Get().GiftHeartToEx(friendId);

                                if (result.success || result.code == "success") {
                                    successCount++;
                                } else if (result.code == "already_today" || result.code == "already_pending") {
                                    alreadySentCount++;
                                } else if (result.code == "insuf") {
                                    insufficientCount++;
                                } else if (result.code == "missing_prereq" || result.code == "missing_gift_reqs") {
                                    prereqCount++;
                                } else if (result.code == "rate_limited") {
                                    rateLimitCount++;
                                } else {
                                    otherFailCount++;
                                }

                                s_heartsSent++;
                                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                            }

                            char summaryBuf[256];
                            if (successCount > 0) {
                                const char* fmt = tsm::ui::i18n::Tr("Sent %d heart(s)");
                                std::snprintf(summaryBuf, sizeof(summaryBuf), fmt, successCount);
                                tsm::ui::helpers::ShowToastSuccess(summaryBuf);
                            }
                            if (alreadySentCount > 0) {
                                const char* fmt = tsm::ui::i18n::Tr("%d already sent today");
                                std::snprintf(summaryBuf, sizeof(summaryBuf), fmt, alreadySentCount);
                                tsm::ui::helpers::ShowToastInfo(summaryBuf);
                            }
                            if (insufficientCount > 0) {
                                const char* fmt = tsm::ui::i18n::Tr("%d failed: insufficient candles");
                                std::snprintf(summaryBuf, sizeof(summaryBuf), fmt, insufficientCount);
                                tsm::ui::helpers::ShowToastError(summaryBuf);
                            }
                            if (prereqCount > 0) {
                                const char* fmt = tsm::ui::i18n::Tr("%d failed: missing prerequisites");
                                std::snprintf(summaryBuf, sizeof(summaryBuf), fmt, prereqCount);
                                tsm::ui::helpers::ShowToastError(summaryBuf);
                            }
                            if (rateLimitCount > 0) {
                                const char* fmt = tsm::ui::i18n::Tr("%d failed: rate limited");
                                std::snprintf(summaryBuf, sizeof(summaryBuf), fmt, rateLimitCount);
                                tsm::ui::helpers::ShowToastError(summaryBuf);
                            }
                            if (otherFailCount > 0) {
                                const char* fmt = tsm::ui::i18n::Tr("%d failed: unknown error");
                                std::snprintf(summaryBuf, sizeof(summaryBuf), fmt, otherFailCount);
                                tsm::ui::helpers::ShowToastError(summaryBuf);
                            }

                            s_sendingHearts.store(false);
                        }).detach();

                        ImGui::CloseCurrentPopup();
                    }
                    if (!canSend) ImGui::EndDisabled();

                    ImGui::SameLine(0, gap);
                    if (s_sendingHearts.load()) ImGui::BeginDisabled();
                    if (SecondaryButton("Cancel", ImVec2(btn_w, 0))) {
                        ImGui::CloseCurrentPopup();
                    }
                    if (s_sendingHearts.load()) ImGui::EndDisabled();

                    ImGui::EndPopup();
                }
                ImGui::PopStyleVar();

                break; }
            case 1: {
                if (s_selectedPlayerIndex == -1) {
                    CenterSeparator("Session Players");
                    if (BeginPannableChild("##players_list")) {
                        for (int playerIdx = 0; playerIdx < tsm::game::api::kMaxPlayers; ++playerIdx) {
                            std::uintptr_t avatar = tsm::game::api::GetCachedAvatar(playerIdx);
                            if (avatar == 0) continue;

                            if (!tsm::game::api::ShouldDisplay(avatar)) continue;

                            auto info = tsm::game::api::GetAvatarInfo(avatar);
                            if (!info.avatarPosition) continue;

                            std::string displayName;
                            const char* icon;

                            if (playerIdx == 0) {
                                displayName = tsm::ui::i18n::Tr("You");
                                icon = GetAvatarStanceIcon(info, "UiEmoteStanceHero");
                            }
                            else {
                                char nameBuf[32];
                                const char* fmt = tsm::ui::i18n::Tr("Player %d");
                                std::snprintf(nameBuf, sizeof(nameBuf), fmt, playerIdx);
                                displayName = nameBuf;
                                icon = GetAvatarStanceIcon(info, "UiMenuFriends");
                            }

                            std::string desc;
                            if (info.avatarPosition) {
                                vec3& pos = *info.avatarPosition;
                                char posBuf[128];
                                const char* fmt = tsm::ui::i18n::Tr("Position: (%.1f, %.1f, %.1f)");
                                std::snprintf(posBuf, sizeof(posBuf), fmt, pos.x, pos.y, pos.z);
                                desc = posBuf;
                            } else {
                                desc = "No position data";
                            }

                            ImGui::PushID(playerIdx);
                            if (ButtonCard(displayName.c_str(), desc.c_str(), icon, "VIEW")) {
                                s_selectedPlayerIndex = playerIdx;
                            }
                            ImGui::PopID();
                        }

                    }
                    EndPannableChild();
                } else {
                    std::uintptr_t avatar = tsm::game::api::GetCachedAvatar(s_selectedPlayerIndex);

                    if (avatar == 0 || !tsm::game::api::ShouldDisplay(avatar)) {
                        PaddedTextDisabled("Player no longer available");
                        if (StandardButton("Back to List")) {
                            s_selectedPlayerIndex = -1;
                        }
                    } else {
                        auto info = tsm::game::api::GetAvatarInfo(avatar);

                        std::string displayName;
                        if (s_selectedPlayerIndex == 0) {
                            displayName = tsm::ui::i18n::Tr("You");
                        } else if (s_selectedPlayerIndex < tsm::game::api::kMaxPlayers) {
                            char nameBuf[32];
                            const char* fmt = tsm::ui::i18n::Tr("Player %d");
                            std::snprintf(nameBuf, sizeof(nameBuf), fmt, s_selectedPlayerIndex);
                            displayName = nameBuf;
                        } else {
                            displayName = "NPC " + std::to_string(s_selectedPlayerIndex);
                        }

                        if (BeginPannableChild("##player_details")) {
                            CenterSeparator(displayName.c_str());

                            ImGui::BeginDisabled();

                            if (info.avatarPosition) {
                                vec3& pos = *info.avatarPosition;
                                char buf[128];
                                const char* fmt = tsm::ui::i18n::Tr("Position: (%.1f, %.1f, %.1f)");
                                std::snprintf(buf, sizeof(buf), fmt, pos.x, pos.y, pos.z);
                                ButtonCard("Position", buf, "UiHudWaypointBg", "");
                            }

                            if (info.avatarHeight) {
                                char buf[64];
                                std::snprintf(buf, sizeof(buf), "%.3f", *info.avatarHeight);
                                ButtonCard("Height", buf, "UiMenuBuffRerollHeight", "");
                            }

                            if (info.avatarScale) {
                                char buf[64];
                                std::snprintf(buf, sizeof(buf), "%.3f", *info.avatarScale);
                                ButtonCard("Scale", buf, "UiMenuBuffRerollHeight", "");
                            }

                            if (info.avatarVoice) {
                                unsigned voiceId = (unsigned)*info.avatarVoice;
                                const char* voiceName = (voiceId < kVoiceCount) ? kVoiceNames[voiceId].c_str() : "Unknown";
                                const char* voiceIcon = GetVoiceIcon(voiceId);
                                ButtonCard("Voice", voiceName, voiceIcon, "");
                            }

                            if (info.avatarStance) {
                                unsigned stanceId = (unsigned)*reinterpret_cast<const uint8_t*>(info.avatarStance);
                                const char* stanceName = (stanceId < kStanceCount) ? kStanceNames[stanceId] : "Unknown";
                                const char* stanceIcon = GetStanceIcon(stanceId);
                                ButtonCard("Stance", stanceName, stanceIcon, "");
                            }

                            ImGui::EndDisabled();

                            bool hasCosmetics = (info.avatarHair && *info.avatarHair) ||
                                               (info.avatarHat && *info.avatarHat) ||
                                               (info.avatarMask && *info.avatarMask) ||
                                               (info.avatarFace && *info.avatarFace) ||
                                               (info.avatarNeck && *info.avatarNeck) ||
                                               (info.avatarBody && *info.avatarBody) ||
                                               (info.avatarFeet && *info.avatarFeet) ||
                                               (info.avatarWing && *info.avatarWing) ||
                                               (info.avatarProp && *info.avatarProp);

                            if (hasCosmetics) {
                                ImGui::Dummy(ImVec2(0, 4));
                                CenterSeparator("Cosmetics");
                            }

                            if (info.avatarHair && *info.avatarHair) {
                                std::string icon = tsm::utils::cosmetics::FindCosmeticIcon(info.avatarHair);
                                const char* iconName = icon.empty() ? "UiOutfitHairBraidSideSmall" : icon.c_str();
                                std::string displayName = tsm::utils::FormatOutfitName(info.avatarHair, "Hair");
                                if (ButtonCard("Hair", displayName.c_str(), iconName, "WEAR")) {
                                    WearOutfit(info.avatarHair, "Hair");
                                }
                            }
                            if (info.avatarHat && *info.avatarHat) {
                                std::string icon = tsm::utils::cosmetics::FindCosmeticIcon(info.avatarHat);
                                const char* iconName = icon.empty() ? "UiOutfitHatBasic" : icon.c_str();
                                std::string displayName = tsm::utils::FormatOutfitName(info.avatarHat, "Hat");
                                if (ButtonCard("Hat", displayName.c_str(), iconName, "WEAR")) {
                                    WearOutfit(info.avatarHat, "Hat");
                                }
                            }
                            if (info.avatarMask && *info.avatarMask) {
                                std::string icon = tsm::utils::cosmetics::FindCosmeticIcon(info.avatarMask);
                                const char* iconName = icon.empty() ? "UiOutfitMaskBasic" : icon.c_str();
                                std::string displayName = tsm::utils::FormatOutfitName(info.avatarMask, "Mask");
                                if (ButtonCard("Mask", displayName.c_str(), iconName, "WEAR")) {
                                    WearOutfit(info.avatarMask, "Mask");
                                }
                            }
                            if (info.avatarFace && *info.avatarFace) {
                                std::string icon = tsm::utils::cosmetics::FindCosmeticIcon(info.avatarFace);
                                const char* iconName = icon.empty() ? "UiOutfitMaskBasic" : icon.c_str();
                                std::string displayName = tsm::utils::FormatOutfitName(info.avatarFace, "Face");
                                if (ButtonCard("Face", displayName.c_str(), iconName, "WEAR")) {
                                    WearOutfit(info.avatarFace, "Face");
                                }
                            }
                            if (info.avatarNeck && *info.avatarNeck) {
                                std::string icon = tsm::utils::cosmetics::FindCosmeticIcon(info.avatarNeck);
                                const char* iconName = icon.empty() ? "UiOutfitNeckAP17Scarf" : icon.c_str();
                                std::string displayName = tsm::utils::FormatOutfitName(info.avatarNeck, "Neck");
                                if (ButtonCard("Neck", displayName.c_str(), iconName, "WEAR")) {
                                    WearOutfit(info.avatarNeck, "Neck");
                                }
                            }
                            if (info.avatarBody && *info.avatarBody) {
                                std::string icon = tsm::utils::cosmetics::FindCosmeticIcon(info.avatarBody);
                                const char* iconName = icon.empty() ? "UiOutfitBodyClassicPants" : icon.c_str();
                                std::string displayName = tsm::utils::FormatOutfitName(info.avatarBody, "Body");
                                if (ButtonCard("Body", displayName.c_str(), iconName, "WEAR")) {
                                    WearOutfit(info.avatarBody, "Body");
                                }
                            }
                            if (info.avatarFeet && *info.avatarFeet) {
                                std::string icon = tsm::utils::cosmetics::FindCosmeticIcon(info.avatarFeet);
                                const char* iconName = icon.empty() ? "UiOutfitFeetSocks" : icon.c_str();
                                std::string displayName = tsm::utils::FormatOutfitName(info.avatarFeet, "Feet");
                                if (ButtonCard("Feet", displayName.c_str(), iconName, "WEAR")) {
                                    WearOutfit(info.avatarFeet, "Feet");
                                }
                            }
                            if (info.avatarWing && *info.avatarWing) {
                                std::string icon = tsm::utils::cosmetics::FindCosmeticIcon(info.avatarWing);
                                const char* iconName = icon.empty() ? "UiOutfitCape" : icon.c_str();
                                std::string displayName = tsm::utils::FormatOutfitName(info.avatarWing, "Wing");
                                if (ButtonCard("Wing", displayName.c_str(), iconName, "WEAR")) {
                                    WearOutfit(info.avatarWing, "Wing");
                                }
                            }
                            if (info.avatarProp && *info.avatarProp) {
                                std::string icon = tsm::utils::cosmetics::FindCosmeticIcon(info.avatarProp);
                                const char* iconName = icon.empty() ? tsm::utils::cosmetics::GetPropIconFallback(info.avatarProp) : icon.c_str();
                                std::string displayName = tsm::utils::FormatOutfitName(info.avatarProp, "Prop");
                                if (ButtonCard("Prop", displayName.c_str(), iconName, "WEAR")) {
                                    WearOutfit(info.avatarProp, "Prop");
                                }
                            }


                            if (s_selectedPlayerIndex != 0 && info.avatarPosition) {
                                CenterSeparator("Actions");
                                if (ButtonCard("Teleport to Player", "Move to this player's position", "UiSocialTeleport", "TELEPORT")) {
                                    vec3& pos = *info.avatarPosition;
                                    tsm::lua::helpers::SetPlayerPosition(pos, true);
                                    char toastBuf[256];
                                    const char* fmt = tsm::ui::i18n::Tr("Teleported to %s");
                                    std::snprintf(toastBuf, sizeof(toastBuf), fmt, displayName.c_str());
                                    tsm::ui::helpers::ShowToastSuccess(toastBuf);
                                }
                            }
                        }
                        EndPannableChild();
                    }
                }

                break; }
            case 2: {
                if (s_selectedNPCIndex == -1) {
                    CenterSeparator("NPCs & Entities");
                    if (BeginPannableChild("##npcs_list")) {
                        for (int i = 8; i < tsm::game::api::kMaxAvatars; ++i) {
                            std::uintptr_t avatar = tsm::game::api::GetCachedAvatar(i);
                            if (avatar == 0) continue;

                            if (!tsm::game::api::ShouldDisplay(avatar)) continue;

                            auto info = tsm::game::api::GetAvatarInfo(avatar);
                            if (!info.avatarPosition) continue;

                            char title[64];
                            const char* titleFmt = tsm::ui::i18n::Tr("NPC %d");
                            std::snprintf(title, sizeof(title), titleFmt, i);

                            std::string desc;
                            if (info.avatarPosition) {
                                vec3& pos = *info.avatarPosition;
                                char posBuf[128];
                                const char* posFmt = tsm::ui::i18n::Tr("Position: (%.1f, %.1f, %.1f)");
                                std::snprintf(posBuf, sizeof(posBuf), posFmt, pos.x, pos.y, pos.z);
                                desc = posBuf;
                            } else {
                                desc = "No position data";
                            }

                            ImGui::PushID(i);
                            const char* icon = GetAvatarStanceIcon(info, "UiEmoteAP16ArmWave0");
                            if (ButtonCard(title, desc.c_str(), icon, "VIEW")) {
                                s_selectedNPCIndex = i;
                            }
                            ImGui::PopID();
                        }
                    }
                    EndPannableChild();
                } else {
                    std::uintptr_t avatar = tsm::game::api::GetCachedAvatar(s_selectedNPCIndex);

                    if (avatar == 0 || !tsm::game::api::ShouldDisplay(avatar)) {
                        PaddedTextDisabled("NPC no longer available");
                        if (StandardButton("Back to List")) {
                            s_selectedNPCIndex = -1;
                        }
                    } else {
                        auto info = tsm::game::api::GetAvatarInfo(avatar);

                        char headerBuf[64];
                        std::snprintf(headerBuf, sizeof(headerBuf), "NPC %d", s_selectedNPCIndex);

                        if (BeginPannableChild("##npc_details")) {
                            CenterSeparator(headerBuf);

                            ImGui::BeginDisabled();

                            if (info.avatarPosition) {
                                vec3& pos = *info.avatarPosition;
                                char buf[128];
                                std::snprintf(buf, sizeof(buf), "(%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
                                ButtonCard("Position", buf, "UiHudWaypointBg", "");
                            }

                            if (info.avatarHeight) {
                                char buf[64];
                                std::snprintf(buf, sizeof(buf), "%.3f", *info.avatarHeight);
                                ButtonCard("Height", buf, "UiMenuBuffRerollHeight", "");
                            }

                            if (info.avatarScale) {
                                char buf[64];
                                std::snprintf(buf, sizeof(buf), "%.3f", *info.avatarScale);
                                ButtonCard("Scale", buf, "UiMenuBuffRerollHeight", "");
                            }

                            if (info.avatarVoice) {
                                unsigned voiceId = (unsigned)*info.avatarVoice;
                                const char* voiceName = (voiceId < kVoiceCount) ? kVoiceNames[voiceId].c_str() : "Unknown";
                                const char* voiceIcon = GetVoiceIcon(voiceId);
                                ButtonCard("Voice", voiceName, voiceIcon, "");
                            }

                            if (info.avatarStance) {
                                unsigned stanceId = (unsigned)*reinterpret_cast<const uint8_t*>(info.avatarStance);
                                const char* stanceName = (stanceId < kStanceCount) ? kStanceNames[stanceId] : "Unknown";
                                const char* stanceIcon = GetStanceIcon(stanceId);
                                ButtonCard("Stance", stanceName, stanceIcon, "");
                            }

                            ImGui::EndDisabled();

                            bool hasCosmetics = (info.avatarHair && *info.avatarHair) ||
                                               (info.avatarHat && *info.avatarHat) ||
                                               (info.avatarMask && *info.avatarMask) ||
                                               (info.avatarNeck && *info.avatarNeck) ||
                                               (info.avatarBody && *info.avatarBody) ||
                                               (info.avatarFeet && *info.avatarFeet) ||
                                               (info.avatarWing && *info.avatarWing) ||
                                               (info.avatarProp && *info.avatarProp);

                            if (hasCosmetics) {
                                ImGui::Dummy(ImVec2(0, 4));
                                CenterSeparator("Cosmetics");
                            }

                            if (info.avatarHair && *info.avatarHair) {
                                std::string icon = tsm::utils::cosmetics::FindCosmeticIcon(info.avatarHair);
                                const char* iconName = icon.empty() ? "UiOutfitHairBraidSideSmall" : icon.c_str();
                                std::string displayName = tsm::utils::FormatOutfitName(info.avatarHair, "Hair");
                                if (ButtonCard("Hair", displayName.c_str(), iconName, "WEAR")) {
                                    WearOutfit(info.avatarHair, "Hair");
                                }
                            }
                            if (info.avatarHat && *info.avatarHat) {
                                std::string icon = tsm::utils::cosmetics::FindCosmeticIcon(info.avatarHat);
                                const char* iconName = icon.empty() ? "UiOutfitHatBasic" : icon.c_str();
                                std::string displayName = tsm::utils::FormatOutfitName(info.avatarHat, "Hat");
                                if (ButtonCard("Hat", displayName.c_str(), iconName, "WEAR")) {
                                    WearOutfit(info.avatarHat, "Hat");
                                }
                            }
                            if (info.avatarMask && *info.avatarMask) {
                                std::string icon = tsm::utils::cosmetics::FindCosmeticIcon(info.avatarMask);
                                const char* iconName = icon.empty() ? "UiOutfitMaskBasic" : icon.c_str();
                                std::string displayName = tsm::utils::FormatOutfitName(info.avatarMask, "Mask");
                                if (ButtonCard("Mask", displayName.c_str(), iconName, "WEAR")) {
                                    WearOutfit(info.avatarMask, "Mask");
                                }
                            }
                            if (info.avatarNeck && *info.avatarNeck) {
                                std::string icon = tsm::utils::cosmetics::FindCosmeticIcon(info.avatarNeck);
                                const char* iconName = icon.empty() ? "UiOutfitNeckAP17Scarf" : icon.c_str();
                                std::string displayName = tsm::utils::FormatOutfitName(info.avatarNeck, "Neck");
                                if (ButtonCard("Neck", displayName.c_str(), iconName, "WEAR")) {
                                    WearOutfit(info.avatarNeck, "Neck");
                                }
                            }
                            if (info.avatarBody && *info.avatarBody) {
                                std::string icon = tsm::utils::cosmetics::FindCosmeticIcon(info.avatarBody);
                                const char* iconName = icon.empty() ? "UiOutfitBodyClassicPants" : icon.c_str();
                                std::string displayName = tsm::utils::FormatOutfitName(info.avatarBody, "Body");
                                if (ButtonCard("Body", displayName.c_str(), iconName, "WEAR")) {
                                    WearOutfit(info.avatarBody, "Body");
                                }
                            }
                            if (info.avatarFeet && *info.avatarFeet) {
                                std::string icon = tsm::utils::cosmetics::FindCosmeticIcon(info.avatarFeet);
                                const char* iconName = icon.empty() ? "UiOutfitFeetSocks" : icon.c_str();
                                std::string displayName = tsm::utils::FormatOutfitName(info.avatarFeet, "Feet");
                                if (ButtonCard("Feet", displayName.c_str(), iconName, "WEAR")) {
                                    WearOutfit(info.avatarFeet, "Feet");
                                }
                            }
                            if (info.avatarWing && *info.avatarWing) {
                                std::string icon = tsm::utils::cosmetics::FindCosmeticIcon(info.avatarWing);
                                const char* iconName = icon.empty() ? "UiOutfitCape" : icon.c_str();
                                std::string displayName = tsm::utils::FormatOutfitName(info.avatarWing, "Wing");
                                if (ButtonCard("Wing", displayName.c_str(), iconName, "WEAR")) {
                                    WearOutfit(info.avatarWing, "Wing");
                                }
                            }
                            if (info.avatarProp && *info.avatarProp) {
                                std::string icon = tsm::utils::cosmetics::FindCosmeticIcon(info.avatarProp);
                                const char* iconName = icon.empty() ? tsm::utils::cosmetics::GetPropIconFallback(info.avatarProp) : icon.c_str();
                                std::string displayName = tsm::utils::FormatOutfitName(info.avatarProp, "Prop");
                                if (ButtonCard("Prop", displayName.c_str(), iconName, "WEAR")) {
                                    WearOutfit(info.avatarProp, "Prop");
                                }
                            }

                            if (hasCosmetics) {
                                ImGui::Dummy(ImVec2(0, 4));
                            }

                            CenterSeparator("Actions");

                            if (info.avatarPosition) {
                                if (ButtonCard("Teleport NPC to You", "Move this NPC to your position", "UiSocialTeleport", "TELEPORT")) {
                                    auto localAvatar = tsm::game::api::GetCachedAvatar(0);
                                    auto localInfo = tsm::game::api::GetAvatarInfo(localAvatar);
                                    if (localInfo.avatarPosition) {
                                        vec3& myPos = *localInfo.avatarPosition;
                                        if (info.avatarPosition) {
                                            *info.avatarPosition = myPos;
                                            char toastBuf[128];
                                            const char* fmt = tsm::ui::i18n::Tr("Teleported NPC %d to you");
                                            std::snprintf(toastBuf, sizeof(toastBuf), fmt, s_selectedNPCIndex);
                                            tsm::ui::helpers::ShowToastSuccess(toastBuf);
                                        }
                                    }
                                }
                            }

                            if (info.avatarPosition) {
                                if (ButtonCard("Teleport to NPC", "Move to this NPC's position", "UiSocialTeleport", "TELEPORT")) {
                                    vec3& pos = *info.avatarPosition;
                                    tsm::lua::helpers::SetPlayerPosition(pos, true);
                                    char toastBuf[128];
                                    const char* fmt = tsm::ui::i18n::Tr("Teleported to NPC %d");
                                    std::snprintf(toastBuf, sizeof(toastBuf), fmt, s_selectedNPCIndex);
                                    tsm::ui::helpers::ShowToastSuccess(toastBuf);
                                }
                            }
                        }
                        EndPannableChild();
                    }
                }

                break; }
        }
        EndCard();
    }
}

int GetSelectedPlayerIndex() { return s_selectedPlayerIndex; }
int GetSelectedNPCIndex() { return s_selectedNPCIndex; }
int GetSocialSubIndex() { return s_socialSubIndex; }

}}}
