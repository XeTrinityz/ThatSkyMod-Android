#include <network/QuestApi.h>
#include <network/ApiClient.h>
#include <resources/data/DailyQuests_Desc.h>
#include <unordered_map>

namespace tsm { namespace network {

static const std::unordered_map<std::string, std::string>& GetQuestDescriptions() {
    static std::unordered_map<std::string, std::string> s_descriptions;
    static bool s_initialized = false;

    if (!s_initialized) {
        s_initialized = true;
        try {
            std::string jsonStr(reinterpret_cast<const char*>(kDailyQuests_DescData), kDailyQuests_DescSize);
            auto parsed = nlohmann::json::parse(jsonStr);

            if (parsed.is_object()) {
                for (auto it = parsed.begin(); it != parsed.end(); ++it) {
                    if (it.value().is_string()) {
                        s_descriptions[it.key()] = it.value().get<std::string>();
                    }
                }
            }
        } catch (...) {
        }
    }

    return s_descriptions;
}

nlohmann::json GetWorldQuests() {
    static ApiClient client;
    return client.PostJson("/account/get_account_world_quests");
}

nlohmann::json GetSeasonQuests() {
    static ApiClient client;
    return client.PostJson("/account/get_season_quests");
}

void ClaimQuestReward(const std::string& questName, int bonusPercent) {
    static ApiClient client;
    nlohmann::json body;
    body["name"] = questName;
    body["bonus_percent"] = bonusPercent;
    client.PostJson("/account/claim_quest_reward", {}, body);
}

std::vector<std::pair<std::string, int>> GetDailyQuests() {
    static ApiClient client;
    std::vector<std::pair<std::string, int>> result;

    auto resp = client.PostJson("/service/quest/api/v1/get_player_quests");
    if (resp.is_object() && resp.contains("active") && resp["active"].is_array()) {
        for (const auto& q : resp["active"]) {
            try {
                if (q.is_object() && q.contains("quest_name") && q["quest_name"].is_string()) {
                    std::string qname = q["quest_name"].get<std::string>();
                    int reqCalls = 1;
                    if (q.contains("stats") && q["stats"].is_object()) {
                        const auto& stats = q["stats"];
                        if (stats.contains("completion_requirements") && stats["completion_requirements"].is_array() &&
                            !stats["completion_requirements"].empty()) {
                            const auto& req = stats["completion_requirements"][0];
                            if (req.is_object() && req.contains("value") && req["value"].is_number_integer()) {
                                reqCalls = req["value"].get<int>();
                            }
                        }
                    }
                    result.push_back({qname, reqCalls});
                }
            } catch (...) {
            }
        }
    }

    return result;
}

std::string GetQuestDescription(const std::string& questName) {
    const auto& descriptions = GetQuestDescriptions();
    auto it = descriptions.find(questName);
    if (it != descriptions.end()) {
        return it->second;
    }
    return questName;
}

nlohmann::json FetchQuestSchedule() {
    static ApiClient client;
    return client.PostJson("/service/quest/api/v1/get_quest_schedule");
}

nlohmann::json FetchEventSchedule() {
    static ApiClient client;
    return client.PostJson("/account/get_event_schedule");
}

}}
