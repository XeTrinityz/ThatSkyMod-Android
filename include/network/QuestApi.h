#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <utility>

namespace tsm { namespace network {

nlohmann::json GetWorldQuests();

nlohmann::json GetSeasonQuests();

void ClaimQuestReward(const std::string& questName, int bonusPercent = 0);

std::vector<std::pair<std::string, int>> GetDailyQuests();

std::string GetQuestDescription(const std::string& questName);

nlohmann::json FetchQuestSchedule();

nlohmann::json FetchEventSchedule();

}}
