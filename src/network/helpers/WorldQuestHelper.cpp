#include <network/helpers/WorldQuestHelper.h>
#include <network/QuestApi.h>
#include <data/DataManager.h>
#include <game/interop/LuaHelpers.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>
#include <ui/core/Localization.h>

namespace tsm { namespace network { namespace helpers {

WorldQuestHelper& WorldQuestHelper::Get() {
    static WorldQuestHelper instance;
    return instance;
}

bool WorldQuestHelper::BuildDefinitions(bool forceReload) {
    std::lock_guard<std::mutex> lk(defsMu_);
    if (forceReload) defs_.clear();
    if (!defs_.empty()) return true;

    nlohmann::json worldQuests = tsm::data::DataManager::Get().GetWorldQuestDefs();
    if (!worldQuests.is_array() || worldQuests.empty()) {
        (void)tsm::data::DataManager::Get().LoadWorldQuestDefs();
        worldQuests = tsm::data::DataManager::Get().GetWorldQuestDefs();
    }

    const nlohmann::json& arr = worldQuests;
    if (!arr.is_array()) return false;

    try {
        for (const auto& entry : arr) {
            if (!entry.is_object()) continue;
            if (entry.contains("type") && entry["type"].is_string() &&
                entry["type"].get<std::string>() == "storm_spawner") continue;

            std::string id;
            if (entry.contains("id") && entry["id"].is_string())
                id = entry["id"].get<std::string>();
            else if (entry.contains("name") && entry["name"].is_string())
                id = entry["name"].get<std::string>();
            if (id.empty()) continue;

            std::string currency = (entry.contains("reward_currency_type_1") &&
                                   entry["reward_currency_type_1"].is_string())
                                   ? entry["reward_currency_type_1"].get<std::string>() : std::string();
            std::string collectible = (entry.contains("collectible_name") &&
                                      entry["collectible_name"].is_string())
                                      ? entry["collectible_name"].get<std::string>() : std::string();
            defs_[id].push_back(WQReward{currency, collectible});
        }
    } catch (...) {
        defs_.clear();
        return false;
    }
    return !defs_.empty();
}

void WorldQuestHelper::MarkCompleted(const std::string& questName) {
    std::lock_guard<std::mutex> lk(statusMu_);
    completed_.insert(questName);
}

void WorldQuestHelper::FetchStatusAsync() {
    if (statusLoading_.exchange(true)) return;
    std::thread([this]() {
        std::unordered_set<std::string> completed;
        try {
            auto resp = tsm::network::GetWorldQuests();
            if (resp.is_object() && resp.contains("set_world_quests") &&
                resp["set_world_quests"].is_array()) {
                for (const auto& it : resp["set_world_quests"]) {
                    try {
                        if (it.is_object() && it.contains("world_quest_def_id") &&
                            it["world_quest_def_id"].is_string())
                            completed.insert(it["world_quest_def_id"].get<std::string>());
                    } catch (...) {}
                }
            }
        } catch (...) {}
        {
            std::lock_guard<std::mutex> lk(statusMu_);
            completed_ = std::move(completed);
        }
        statusLoading_.store(false);
    }).detach();
}

void WorldQuestHelper::StartRun(bool collectCollectibles,
                               std::function<void(float, const char*)> progressCallback) {
    if (running_.exchange(true)) return;
    cancel_.store(false);
    progress_.store(0.0f);
    collectCollectibles_ = collectCollectibles;

    if (progressCallback) progressCallback(0.0f, tsm::ui::i18n::Tr("Preparing world quests..."));

    std::thread([this, progressCallback]() {
        if (!hasManuallyReloaded_) {
            if (progressCallback) progressCallback(0.0f, tsm::ui::i18n::Tr("Fetching completed quests..."));
            try {
                auto resp = tsm::network::GetWorldQuests();
                if (resp.is_object() && resp.contains("set_world_quests") &&
                    resp["set_world_quests"].is_array()) {
                    std::unordered_set<std::string> completed;
                    for (const auto& it : resp["set_world_quests"]) {
                        try {
                            if (it.is_object() && it.contains("world_quest_def_id") &&
                                it["world_quest_def_id"].is_string())
                                completed.insert(it["world_quest_def_id"].get<std::string>());
                        } catch (...) {}
                    }
                    {
                        std::lock_guard<std::mutex> lk(statusMu_);
                        completed_ = std::move(completed);
                    }
                    hasManuallyReloaded_ = true;
                }
            } catch (...) {}
        }

        if (!BuildDefinitions(false)) {
            running_.store(false);
            return;
        }

        std::unordered_set<std::string> completed;
        {
            std::lock_guard<std::mutex> lk(statusMu_);
            completed = completed_;
        }

        auto isQuestFlag = [this](const std::string& qname) {
            auto it = defs_.find(qname);
            if (it == defs_.end()) return false;
            for (const auto& rw : it->second) {
                if (!rw.collectible.empty() && rw.collectible.find("_quest_done") != std::string::npos)
                    return true;
            }
            return false;
        };

        auto hasCollectible = [this](const std::string& qname) {
            auto it = defs_.find(qname);
            if (it == defs_.end()) return false;
            for (const auto& rw : it->second) {
                if (!rw.collectible.empty() && rw.collectible.find("_quest_done") == std::string::npos)
                    return true;
            }
            return false;
        };

        std::vector<std::string> toProcess;
        toProcess.reserve(defs_.size());
        for (const auto& kv : defs_) {
            const std::string& q = kv.first;
            if (completed.count(q) > 0) continue;
            if (isQuestFlag(q)) continue;
            if (!collectCollectibles_ && hasCollectible(q)) continue;
            toProcess.push_back(q);
        }

        size_t done = 0, total = toProcess.size();
        if (progressCallback) progressCallback(5.0f, tsm::ui::i18n::Tr("Claiming quest rewards..."));

        for (const auto& q : toProcess) {
            if (cancel_.load()) break;
            tsm::network::ClaimQuestReward(q);
            if (collectCollectibles_) {
                auto it = defs_.find(q);
                if (it != defs_.end()) {
                    for (const auto& rw : it->second) {
                        if (cancel_.load()) break;
                        if (!rw.collectible.empty() &&
                            rw.collectible.find("_quest_done") == std::string::npos) {
                            tsm::lua::helpers::CollectCollectible(rw.collectible, "emote");
                            std::this_thread::sleep_for(std::chrono::seconds(5));
                        }
                    }
                }
            }
            ++done;
            progress_.store(total > 0 ? (float)done / (float)total : 1.0f);
            float pct = (total > 0 ? (float)done / (float)total : 1.0f) * 100.0f;
            char op[256];
            std::string displayName = tsm::network::GetQuestDescription(q);
            const char* fmt = tsm::ui::i18n::Tr("Completed %s (%zu/%zu)");
            std::snprintf(op, sizeof(op), fmt, displayName.c_str(), done, total);
            if (progressCallback) progressCallback(pct, op);
        }

        bool cancelled = cancel_.load();
        running_.store(false);
        if (progressCallback)
            progressCallback(100.0f, cancelled ? tsm::ui::i18n::Tr("Canceled") : tsm::ui::i18n::Tr("World quests completed"));
        FetchStatusAsync();
    }).detach();
}

}}}
