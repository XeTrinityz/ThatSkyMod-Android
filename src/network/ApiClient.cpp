#include <network/ApiClient.h>
#include <game/hooks/HookManager.h>
#include <data/DataManager.h>
#include <utils/logging/log.h>

#include <algorithm>
#include <cctype>
#include <mutex>
#include <chrono>
#include <thread>

namespace tsm { namespace network {

namespace {
    constexpr const char* kApiHost = "live.radiance.thatgamecompany.com";
    constexpr int kApiPort = 443;

    std::uint32_t Fnv1a32(const std::string& s) {
        std::uint32_t h = 0x811C9DC5u;
        for (unsigned char c : s) {
            h ^= c;
            h *= 0x01000193u;
        }
        return h;
    }
}

ApiClient::ApiClient()
    : http_client_(std::make_unique<HttpClient>(kApiHost, kApiPort)) {
}

nlohmann::json ApiClient::ParseJsonSafe(const std::string& str) {
    try {
        if (str.empty()) return nlohmann::json::object();
        return nlohmann::json::parse(str);
    } catch (...) {
        return nlohmann::json::object();
    }
}

nlohmann::json ApiClient::PostJson(
    const std::string& endpoint,
    const std::map<std::string, std::string>& extra_headers,
    const nlohmann::json& extra_content,
    HttpResponse* out_raw) {

    static std::mutex rate_mutex;
    static auto last_request = std::chrono::steady_clock::now() - std::chrono::milliseconds(min_interval_ms_);

    nlohmann::json body = extra_content;
    const std::string uid = tsm::game::hooks::HookManager::Get().GetCapturedUserId();
    const std::string sid = tsm::game::hooks::HookManager::Get().GetCapturedSessionId();

    if (!body.contains("user")) body["user"] = uid;
    if (!body.contains("user_id")) body["user_id"] = uid;
    if (!body.contains("session")) body["session"] = sid;

    HttpRequest request;
    request.endpoint = endpoint;
    request.body = body.dump();

    request.headers["Host"] = kApiHost;
    request.headers["User-Agent"] = tsm::game::hooks::HookManager::Get().GetCapturedUserAgent();
    request.headers["Content-Type"] = "application/json";
    request.headers["user-id"] = uid;
    request.headers["user"] = uid;
    request.headers["session"] = sid;

    for (const auto& [key, value] : extra_headers) {
        request.headers[key] = value;
    }

    {
        std::lock_guard<std::mutex> lock(rate_mutex);
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_request);

        if (elapsed.count() < min_interval_ms_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(min_interval_ms_ - elapsed.count()));
        }

        last_request = std::chrono::steady_clock::now();
    }

    HttpResponse response = http_client_->Post(request);

    if (out_raw) {
        *out_raw = response;
    }

    if (response.IsSuccess()) {
        return ParseJsonSafe(response.body);
    }

    return nlohmann::json::object();
}

std::vector<SpellDef> ApiClient::GetSpellDefinitions() {
    auto json_response = PostJson("/account/buff/get_buff_defs");
    std::vector<SpellDef> defs;

    if (!json_response.is_object() ||
        !json_response.contains("get_buff_defs") ||
        !json_response["get_buff_defs"].is_array()) {
        return defs;
    }

    defs.reserve(json_response["get_buff_defs"].size());

    for (const auto& item : json_response["get_buff_defs"]) {
        if (!item.is_object()) continue;
        if (!item.contains("name") || !item["name"].is_string()) continue;

        SpellDef def;
        def.name = item["name"].get<std::string>();

        if (item.contains("icon") && item["icon"].is_string()) {
            def.icon = item["icon"].get<std::string>();
        }

        defs.push_back(std::move(def));
    }

    std::sort(defs.begin(), defs.end(), [this](const SpellDef& a, const SpellDef& b) {
        return FormatName(a.name) < FormatName(b.name);
    });

    return defs;
}

std::vector<StatDef> ApiClient::GetAchievementStats() {
    auto json_response = PostJson("/account/get_achievement_stats");
    std::vector<StatDef> stats;

    if (!json_response.is_object() ||
        !json_response.contains("achievement_stats") ||
        !json_response["achievement_stats"].is_array()) {
        return stats;
    }

    stats.reserve(json_response["achievement_stats"].size());

    for (const auto& item : json_response["achievement_stats"]) {
        if (!item.is_object()) continue;
        if (!item.contains("type") || !item["type"].is_string()) continue;
        if (!item.contains("value")) continue;

        StatDef stat;
        stat.type = item["type"].get<std::string>();
        if (item.contains("display_name") && item["display_name"].is_string()) {
            stat.displayName = item["display_name"].get<std::string>();
        } else if (item.contains("name") && item["name"].is_string()) {
            stat.displayName = item["name"].get<std::string>();
        }
        if (stat.displayName.empty()) {
            stat.displayName = stat.type;
            for (char& c : stat.displayName) if (c == '_') c = ' ';
            bool cap = true;
            for (char& c : stat.displayName) {
                if (cap && std::isalpha(static_cast<unsigned char>(c))) {
                    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                    cap = false;
                } else if (c == ' ') cap = true;
            }
        }

        if (item["value"].is_number_float()) {
            stat.value = item["value"].get<float>();
        } else if (item["value"].is_number_integer()) {
            stat.value = static_cast<float>(item["value"].get<int>());
        } else {
            continue;
        }

        stats.push_back(std::move(stat));
    }

    std::sort(stats.begin(), stats.end(), [this](const StatDef& a, const StatDef& b) {
        return FormatName(a.type) < FormatName(b.type);
    });

    return stats;
}

const std::string& ApiClient::OutfitNameFromId(std::uint32_t id) {
    static bool built = false;
    static std::map<std::uint32_t, std::string> id_to_name;
    static std::string unknown;

    if (!built) {
        const auto& outfit_defs = tsm::data::DataManager::Get().GetOutfitDefs();
        if (outfit_defs.is_array()) {
            for (const auto& def : outfit_defs) {
                if (!def.is_object()) continue;
                if (def.contains("name") && def["name"].is_string()) {
                    std::string name = def["name"].get<std::string>();
                    std::uint32_t hash = Fnv1a32(name);
                    id_to_name.try_emplace(hash, name);
                }
            }
        }
        built = true;
    }

    auto it = id_to_name.find(id);
    if (it != id_to_name.end()) {
        return it->second;
    }

    unknown = "Unknown_" + std::to_string(id);
    return unknown;
}

OutfitInfo ApiClient::GetCurrentOutfit() {
    auto json_response = PostJson("/account/get_outfit");
    OutfitInfo outfit;

    if (!json_response.contains("set_outfit") || !json_response["set_outfit"].is_object()) {
        tsm::log::e("ApiClient: Invalid response format from get_outfit");
        return outfit;
    }

    const auto& set_outfit = json_response["set_outfit"];
    const std::vector<std::string> part_names = {
        "arms", "body", "feet", "hair", "neck", "prop", "wing",
        "hat", "horn", "mask", "face"
    };

    for (const auto& part_name : part_names) {
        if (!set_outfit.contains(part_name)) continue;

        const auto& item = set_outfit[part_name];
        if (!item.is_object() || !item.contains("id")) continue;

        std::uint32_t id = item["id"].get<std::uint32_t>();

        if (id == 0) continue;

        OutfitPart part;
        part.name = OutfitNameFromId(id);

        if (item.contains("dye") && item["dye"].is_string()) {
            std::string dye_str = item["dye"].get<std::string>();
            if (!dye_str.empty()) {
                part.dye = dye_str;
            }
        }

        if (item.contains("mask") && item["mask"].is_number()) {
            int mask_val = item["mask"].get<int>();
            if (mask_val != 0) part.mask = mask_val;
        }
        if (item.contains("pat") && item["pat"].is_number()) {
            int pat_val = item["pat"].get<int>();
            if (pat_val != 0) part.pat = pat_val;
        }
        if (item.contains("tex") && item["tex"].is_number()) {
            int tex_val = item["tex"].get<int>();
            if (tex_val != 0) part.tex = tex_val;
        }

        outfit.parts[part_name] = std::move(part);
    }

    if (set_outfit.contains("height") && set_outfit["height"].is_number()) {
        outfit.height = set_outfit["height"].get<float>();
    }
    if (set_outfit.contains("scale") && set_outfit["scale"].is_number()) {
        outfit.scale = set_outfit["scale"].get<float>();
    }
    if (set_outfit.contains("voice") && set_outfit["voice"].is_number()) {
        outfit.voice = static_cast<std::uint8_t>(set_outfit["voice"].get<int>());
    }

    return outfit;
}

std::string ApiClient::FormatName(const std::string& internal_name) {
    std::string display_name = internal_name;

    size_t pos = display_name.find('_');
    while (pos != std::string::npos) {
        display_name.replace(pos, 1, " ");
        pos = display_name.find('_', pos);
    }

    bool capitalize_next = true;
    for (char& c : display_name) {
        if (capitalize_next && std::isalpha(static_cast<unsigned char>(c))) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            capitalize_next = false;
        } else if (c == ' ') {
            capitalize_next = true;
        }
    }

    return display_name;
}

}}
