#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace tsm { namespace network {


struct HttpResponse {
    int status{0};
    std::string body;
    std::map<std::string, std::string> headers;
    std::string error;

    bool IsSuccess() const { return status == 200 || status == 418; }
    bool IsRetryable() const { return status == 429 || status == 500 || status == 502 || status == 503; }
};

struct HttpRequest {
    std::string endpoint;
    std::map<std::string, std::string> headers;
    std::string body;
};


struct SpellDef {
    std::string name;
    std::string icon;
};

struct StatDef {
    std::string type;
    std::string displayName;
    float value{0.0f};
};

struct OutfitPart {
    std::string name;
    std::string dye;
    int mask{0};
    int pat{0};
    int tex{0};
};

struct OutfitInfo {
    std::map<std::string, OutfitPart> parts;
    float height{0.0f};
    float scale{0.0f};
    std::uint8_t voice{0};
    std::uint8_t stance{0};
};


struct FriendInfo {
    std::string nickname;
    bool blocked{false};
    bool blocked_by{false};
    bool deleted{false};
    std::int64_t when_created{0};
    OutfitInfo outfit;
};

using FriendList = std::vector<std::pair<std::string, FriendInfo>>;

struct OnlineInfo {
    std::vector<std::string> friend_ids;
    std::map<std::string, std::uint32_t> levels;
};

struct GiftResult {
    bool success{false};
    std::string code;
};


struct OperationProgress {
    bool running{false};
    int current{0};
    int total{0};
    std::string result_message;
};

}}
