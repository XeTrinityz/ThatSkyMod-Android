#include <network/helpers/DailyQuestHelper.h>
#include <network/QuestApi.h>
#include <network/SocialManager.h>
#include <game/interop/LuaHelpers.h>
#include <state/ModState.h>
#include <network/ApiClient.h>
#include <ui/core/Localization.h>
#include <thread>
#include <chrono>
#include <algorithm>
#include <iterator>
#include <unordered_set>

namespace tsm { namespace network { namespace helpers {

DailyQuestHelper& DailyQuestHelper::Get() {
    static DailyQuestHelper instance;
    return instance;
}

void DailyQuestHelper::FetchAsync(bool autoRun) {
    if (loading_.exchange(true)) return;
    if (autoRun) autoRun_.store(true);

    std::thread([this]() {
        std::vector<std::pair<std::string, int>> local;
        try {
            if (tsm::network::SocialManager::Get().IsLoggedIn()) {
                std::vector<std::pair<std::string, int>> active;
                try {
                    active = tsm::network::GetDailyQuests();
                } catch (...) {}

                std::vector<std::string> activeNames;
                activeNames.reserve(active.size());
                std::unordered_set<std::string> activeSet;
                activeSet.reserve(active.size());
                for (const auto& q : active) {
                    activeNames.push_back(q.first);
                    activeSet.insert(q.first);
                }

                std::unordered_set<std::string> collectedNames;
                try {
                    static tsm::network::ApiClient client;
                    auto raw = client.PostJson("/service/quest/api/v1/get_player_quests");
                    if (raw.is_object() && raw.contains("collected") && raw["collected"].is_array()) {
                        for (const auto& cq : raw["collected"]) {
                            try {
                                if (cq.is_object() && cq.contains("quest_name") && cq["quest_name"].is_string()) {
                                    collectedNames.insert(cq["quest_name"].get<std::string>());
                                }
                            } catch (...) {}
                        }
                    }
                } catch (...) {}

                std::vector<std::pair<std::string, int>> seasonFallback;
                bool anyAutoAccepted = false;

                auto seasonResp = tsm::network::GetSeasonQuests();
                if (seasonResp.is_object() && seasonResp.contains("season_quests") && seasonResp["season_quests"].is_array()) {
                    for (const auto& sq : seasonResp["season_quests"]) {
                        try {
                            if (!sq.is_object()) continue;
                            if (!sq.contains("daily_quest_def_id") || !sq["daily_quest_def_id"].is_string()) continue;
                            std::string questName = sq["daily_quest_def_id"].get<std::string>();

                            bool activated = false;
                            if (sq.contains("activated") && sq["activated"].is_boolean()) {
                                activated = sq["activated"].get<bool>();
                            }

                            bool inActive = activeSet.find(questName) != activeSet.end();
                            bool inCollected = collectedNames.find(questName) != collectedNames.end();

                            if (!activated && !inCollected) {
                                seasonFallback.push_back({questName, 1});
                            }

                            if (!activated && !inActive && !inCollected) {
                                tsm::lua::helpers::AutoAcceptSmartQuest(questName);
                                anyAutoAccepted = true;
                            }
                        } catch (...) {}
                    }
                }

                if (anyAutoAccepted) {
                    for (int i = 0; i < 30; ++i) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                }

                try {
                    local = tsm::network::GetDailyQuests();
                } catch (...) {}

                if (local.empty()) {
                    if (!active.empty()) {
                        local = std::move(active);
                    } else if (!seasonFallback.empty()) {
                        local = std::move(seasonFallback);
                    }
                } else {
                    for (const auto& sf : seasonFallback) {
                        bool found = false;
                        for (const auto& it : local) {
                            if (it.first == sf.first) { found = true; break; }
                        }
                        if (!found) local.push_back(sf);
                    }
                }
            } else {
                try {
                    local = tsm::network::GetDailyQuests();
                } catch (...) {}
            }
        } catch (...) {}
        {
            std::lock_guard<std::mutex> lk(questsMu_);
            quests_ = std::move(local);
        }
        if (!autoRun_.load()) {
            std::vector<std::string> names;
            names.reserve(quests_.size());
            for (const auto& q : quests_) names.push_back(q.first);
            std::lock_guard<std::mutex> lk2(snapshotMu_);
            lastReloadActive_ = std::move(names);
        }
        loading_.store(false);
        if (autoRun_.load()) {
            autoRun_.store(false);
            CompleteAll();
        }
    }).detach();
}

void DailyQuestHelper::CompleteAll(std::function<void(float, const char*)> progressCallback) {
    if (completing_.exchange(true)) return;
    cancel_.store(false);

    if (progressCallback) progressCallback(0.0f, tsm::ui::i18n::Tr("Starting daily quests..."));
    std::vector<std::pair<std::string, int>> local;
    {
        std::lock_guard<std::mutex> lk(questsMu_);
        local = quests_;
    }

    std::thread([this, local, progressCallback]() {
        size_t done = 0, total = local.size();
        for (const auto& it : local) {
            const int count = std::max(1, it.second);
            for (int i = 0; i < count; ++i) {
                if (cancel_.load()) break;
                tsm::lua::helpers::TryCompleteQuestOrIncrementStat(it.first);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            if (cancel_.load()) break;
            RemoveQuest(it.first);
            ++done;
            float pct = (total > 0 ? (float)done / (float)total : 1.0f) * 100.0f;
            char op[192];
            std::string displayName = tsm::network::GetQuestDescription(it.first);
            const char* fmt = tsm::ui::i18n::Tr("Completed %s (%zu/%zu)");
            std::snprintf(op, sizeof(op), fmt, displayName.c_str(), done, total);

            if (progressCallback) progressCallback(pct, op);
        }

        bool cancelled = cancel_.load();
        completing_.store(false);
        if (progressCallback)
            progressCallback(100.0f, cancelled ? tsm::ui::i18n::Tr("Canceled") : tsm::ui::i18n::Tr("Daily quests completed"));

    }).detach();
}

std::vector<std::pair<std::string, int>> DailyQuestHelper::GetQuests() const {
    std::lock_guard<std::mutex> lk(questsMu_);
    return quests_;
}

void DailyQuestHelper::RemoveQuest(const std::string& questName) {
    std::lock_guard<std::mutex> lk(questsMu_);
    quests_.erase(
        std::remove_if(quests_.begin(), quests_.end(),
            [&questName](const std::pair<std::string, int>& q) {
                return q.first == questName;
            }),
        quests_.end()
    );
}

void DailyQuestHelper::CollectAllCompletedSinceReload(std::function<void(float, const char*)> progressCallback) {
    if (collecting_.exchange(true)) return;
    cancel_.store(false);

    std::vector<std::string> prev;
    {
        std::lock_guard<std::mutex> lk(snapshotMu_);
        prev = lastReloadActive_;
    }
    std::vector<std::string> current;
    {
        std::lock_guard<std::mutex> lk(questsMu_);
        current.reserve(quests_.size());
        for (const auto& q : quests_) current.push_back(q.first);
    }

    std::sort(prev.begin(), prev.end());
    std::sort(current.begin(), current.end());
    std::vector<std::string> toCollect;
    toCollect.reserve(prev.size());
    std::set_difference(prev.begin(), prev.end(), current.begin(), current.end(), std::back_inserter(toCollect));

    std::thread([this, toCollect, progressCallback]() {
        if (progressCallback) progressCallback(0.0f, tsm::ui::i18n::Tr("Collecting completed quests..."));
        const size_t total = toCollect.size();
        size_t idx = 0;

        for (const auto& name : toCollect) {
            if (cancel_.load()) break;
            tsm::lua::helpers::AutoCollectCompletedQuest(name);
            ++idx;
            float pct = (total > 0 ? (float)idx / (float)total : 1.0f) * 100.0f;
            char msg[192];
            std::string displayName = tsm::network::GetQuestDescription(name);
            const char* fmt = tsm::ui::i18n::Tr("Collected %s (%zu/%zu)");
            std::snprintf(msg, sizeof(msg), fmt, displayName.c_str(), idx, total);
            if (progressCallback) progressCallback(pct, msg);
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }

        bool cancelled = cancel_.load();
        collecting_.store(false);
        if (progressCallback) progressCallback(100.0f, cancelled ? tsm::ui::i18n::Tr("Canceled") : tsm::ui::i18n::Tr("Daily quest collection complete"));
    }).detach();
}

}}}
