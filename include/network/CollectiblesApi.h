#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace tsm { namespace network {

nlohmann::json GetCollectibles();

void AcknowledgeCollectible(const std::string& collectibleId, const std::string& ackType = "used");

}}
