#pragma once

#include <nlohmann/json.hpp>

namespace tsm { namespace data { namespace loaders {

bool LoadLevelsFromLua(nlohmann::json& outLevels);

}}}
