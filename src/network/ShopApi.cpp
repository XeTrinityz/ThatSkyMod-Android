#include <network/ShopApi.h>
#include <network/ApiClient.h>
#include <unordered_set>
#include <algorithm>

namespace tsm { namespace network {

nlohmann::json GetSpiritShops() {
    static ApiClient client;
    return client.PostJson("/account/get_spirit_shops");
}

std::vector<std::string> GetSpiritShopIds() {
    std::vector<std::string> result;
    std::unordered_set<std::string> uniqueShops;

    auto resp = GetSpiritShops();
    if (resp.is_object() && resp.contains("spirit_shops") && resp["spirit_shops"].is_array()) {
        for (const auto& item : resp["spirit_shops"]) {
            try {
                if (item.is_object() && item.contains("shop") && item["shop"].is_string()) {
                    std::string shopId = item["shop"].get<std::string>();
                    if (uniqueShops.insert(shopId).second) {
                        result.push_back(shopId);
                    }
                }
            } catch (...) {
            }
        }
    }

    return result;
}

std::vector<GenericShopDef> GetGenericShops() {
    std::vector<GenericShopDef> result;
    static ApiClient client;
    auto resp = client.PostJson("/account/get_generic_shops");
    if (!resp.is_object()) return result;

    const nlohmann::json* arr = nullptr;
    if (resp.contains("generic_shops") && resp["generic_shops"].is_array()) {
        arr = &resp["generic_shops"];
    } else if (resp.contains("generic_shop_defs") && resp["generic_shop_defs"].is_array()) {
        arr = &resp["generic_shop_defs"];
    }
    if (!arr) return result;

    std::unordered_map<std::string, std::string> shopIcons;
    for (const auto& item : *arr) {
        if (!item.is_object()) continue;
        std::string name;
        if (item.contains("shop_name") && item["shop_name"].is_string()) {
            name = item["shop_name"].get<std::string>();
        } else if (item.contains("name") && item["name"].is_string()) {
            name = item["name"].get<std::string>();
        }
        if (name.empty() || name == "NoShop") continue;
        std::string icon;
        if (item.contains("icon") && item["icon"].is_string()) {
            icon = item["icon"].get<std::string>();
        } else if (item.contains("shop_icon") && item["shop_icon"].is_string()) {
            icon = item["shop_icon"].get<std::string>();
        }
        if (shopIcons[name].empty() && !icon.empty()) shopIcons[name] = icon;
    }
    for (const auto& kv : shopIcons) {
        result.push_back({kv.first, kv.second});
    }
    std::sort(result.begin(), result.end(), [](const GenericShopDef& a, const GenericShopDef& b) {
        return a.name < b.name;
    });
    return result;
}

}}
