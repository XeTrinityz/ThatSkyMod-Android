#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace tsm { namespace network {

nlohmann::json GetSpiritShops();

std::vector<std::string> GetSpiritShopIds();

struct GenericShopDef {
    std::string name;
    std::string icon;
};

std::vector<GenericShopDef> GetGenericShops();

}}
