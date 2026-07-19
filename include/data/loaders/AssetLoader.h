#pragma once

#include <nlohmann/json.hpp>

namespace tsm { namespace data { namespace loaders {

bool LoadJsonAsset(const char* assetPath, nlohmann::json& outJson);

}}}
