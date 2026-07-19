#pragma once

#include <network/NetworkTypes.h>
#include <network/HttpClient.h>
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>
#include <map>

namespace tsm { namespace network {

class ApiClient {
public:
    ApiClient();
    ~ApiClient() = default;

    ApiClient(const ApiClient&) = delete;
    ApiClient& operator=(const ApiClient&) = delete;


    nlohmann::json PostJson(
        const std::string& endpoint,
        const std::map<std::string, std::string>& extra_headers = {},
        const nlohmann::json& extra_content = nlohmann::json::object(),
        HttpResponse* out_raw = nullptr
    );


    std::vector<SpellDef> GetSpellDefinitions();
    std::vector<StatDef> GetAchievementStats();
    OutfitInfo GetCurrentOutfit();


    void SetMinInterval(int ms) { min_interval_ms_ = ms; }

private:
    std::unique_ptr<HttpClient> http_client_;
    int min_interval_ms_{500};

    nlohmann::json ParseJsonSafe(const std::string& str);
    std::string FormatName(const std::string& internal_name);
    const std::string& OutfitNameFromId(std::uint32_t id);
};

}}
