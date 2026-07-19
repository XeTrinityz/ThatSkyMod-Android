#include <network/SocialManager.h>
#include <game/hooks/HookManager.h>
#include <data/DataManager.h>
#include <ui/core/Localization.h>

#include <thread>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <set>
#include <cstdio>

namespace tsm { namespace network {

namespace {
    inline std::string JStr(const nlohmann::json& j, const char* k, const std::string& def = {}) {
        try {
            if (j.contains(k) && j[k].is_string()) return j[k].get<std::string>();
        } catch (...) {}
        return def;
    }

    inline bool JBool(const nlohmann::json& j, const char* k, bool def = false) {
        try {
            if (j.contains(k) && j[k].is_boolean()) return j[k].get<bool>();
        } catch (...) {}
        return def;
    }

    inline std::int64_t JI64(const nlohmann::json& j, const char* k, std::int64_t def = 0) {
        try {
            if (!j.contains(k)) return def;
            const auto& v = j[k];
            if (v.is_number_integer()) return v.get<std::int64_t>();
            if (v.is_number_unsigned()) return static_cast<std::int64_t>(v.get<std::uint64_t>());
            if (v.is_number_float()) return static_cast<std::int64_t>(v.get<double>());
            if (v.is_string()) {
                const auto s = v.get<std::string>();
                if (!s.empty()) {
                    try { return std::stoll(s); } catch (...) {}
                }
            }
        } catch (...) {}
        return def;
    }

    inline float JFloat(const nlohmann::json& j, const char* k, float def = 0.0f) {
        try {
            if (!j.contains(k)) return def;
            const auto& v = j[k];
            if (v.is_number()) return v.get<float>();
        } catch (...) {}
        return def;
    }

    inline std::uint8_t JUInt8(const nlohmann::json& j, const char* k, std::uint8_t def = 0) {
        try {
            if (!j.contains(k)) return def;
            const auto& v = j[k];
            if (v.is_number_unsigned()) return static_cast<std::uint8_t>(v.get<unsigned int>());
            if (v.is_number_integer()) return static_cast<std::uint8_t>(v.get<int>());
        } catch (...) {}
        return def;
    }

    inline std::uint32_t JUInt32(const nlohmann::json& j, const char* k, std::uint32_t def = 0) {
        try {
            if (!j.contains(k)) return def;
            const auto& v = j[k];
            if (v.is_number_unsigned()) return v.get<std::uint32_t>();
            if (v.is_number_integer()) return static_cast<std::uint32_t>(v.get<int>());
        } catch (...) {}
        return def;
    }

    inline std::string ToLowerCopy(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    std::uint32_t Fnv1a32(const std::string& s) {
        std::uint32_t h = 0x811C9DC5u;
        for (unsigned char c : s) {
            h ^= c;
            h *= 0x01000193u;
        }
        return h;
    }

    std::string OutfitNameFromId(std::uint32_t id) {
        static bool built = false;
        static std::map<std::uint32_t, std::string> id_to_name;

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

        return std::string();
    }
}

SocialManager& SocialManager::Get() {
    static SocialManager instance;
    return instance;
}

SocialManager::SocialManager()
    : api_client_(std::make_unique<ApiClient>()) {
}

bool SocialManager::IsLoggedIn() const {
    return !tsm::game::hooks::HookManager::Get().GetCapturedUserId().empty() &&
           !tsm::game::hooks::HookManager::Get().GetCapturedSessionId().empty();
}

FriendList SocialManager::GetFriends() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return friends_;
}

std::vector<std::string> SocialManager::GetOnlineFriends() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return online_friends_;
}

std::map<std::string, std::uint32_t> SocialManager::GetOnlineLevels() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return online_levels_;
}

std::string SocialManager::GetFriendNickname(const std::string& uuid) const {
    if (uuid.empty()) return "";

    std::lock_guard<std::mutex> lock(data_mutex_);

    for (const auto& [friend_id, info] : friends_) {
        if (friend_id == uuid && !info.deleted && !info.nickname.empty()) {
            return info.nickname;
        }
    }

    return "";
}

bool SocialManager::IsFriend(const std::string& uuid) const {
    if (uuid.empty()) return false;

    std::lock_guard<std::mutex> lock(data_mutex_);

    for (const auto& [friend_id, info] : friends_) {
        if (friend_id == uuid && !info.deleted) {
            return true;
        }
    }

    return false;
}

FriendList SocialManager::FetchFriendStatues() {
    FriendList out;
    nlohmann::json body = {{"max", 150}, {"sort_ver", 1}};
    auto resp = api_client_->PostJson("/account/get_friend_statues", {}, body);

    if (!resp.is_object() || !resp.contains("set_friend_statues") ||
        !resp["set_friend_statues"].is_array()) {
        return out;
    }

    for (const auto& item : resp["set_friend_statues"]) {
        if (!item.is_object()) continue;

        const std::string id = JStr(item, "friend_id");
        if (id.empty()) continue;

        FriendInfo info{};
        info.nickname = JStr(item, "nickname");
        info.deleted = JBool(item, "local_soft_deleted", false);
        info.when_created = JI64(item, "when_created", 0);

        if (item.contains("outfit") && item["outfit"].is_object()) {
            const auto& outfit = item["outfit"];
            info.outfit.voice = JUInt8(outfit, "voice", 0);
            info.outfit.stance = JUInt8(outfit, "stance", 0);
            info.outfit.scale = JFloat(outfit, "scale", 0.0f);
            info.outfit.height = JFloat(outfit, "height", 0.0f);

            const std::vector<std::string> partNames = {
                "hair", "hat", "mask", "face", "body", "arms", "feet",
                "wing", "neck", "horn", "prop"
            };

            for (const auto& partName : partNames) {
                if (outfit.contains(partName) && outfit[partName].is_object()) {
                    const auto& part = outfit[partName];
                    OutfitPart partInfo;

                    std::uint32_t partId = JUInt32(part, "id", 0);
                    if (partId != 0) {
                        partInfo.name = OutfitNameFromId(partId);
                        partInfo.mask = JUInt32(part, "mask", 0);
                        partInfo.pat = JUInt32(part, "pat", 0);
                        partInfo.tex = JUInt32(part, "tex", 0);

                        if (!partInfo.name.empty()) {
                            info.outfit.parts[partName] = partInfo;
                        }
                    }
                }
            }
        }

        out.emplace_back(id, std::move(info));
    }

    return out;
}

void SocialManager::UpdateBlockedBy(FriendList& friends) {
    if (friends.empty()) return;

    std::vector<std::string> ids;
    ids.reserve(friends.size());
    for (const auto& [friend_id, _] : friends) {
        ids.push_back(friend_id);
    }

    nlohmann::json req = {{"players", ids}};
    auto resp = api_client_->PostJson("/account/get_friends", {}, req);

    for (auto& [_, info] : friends) {
        info.blocked_by = false;
    }

    if (!resp.is_object() || !resp.contains("set_friends") ||
        !resp["set_friends"].is_array()) {
        return;
    }

    for (const auto& it : resp["set_friends"]) {
        if (!it.is_object()) continue;

        const std::string id = JStr(it, "friend_id");
        if (id.empty()) continue;

        const bool is_blocked_by = JBool(it, "local_blocked", false);

        auto pos = std::find_if(friends.begin(), friends.end(),
            [&id](const auto& pair) { return pair.first == id; });

        if (pos != friends.end()) {
            pos->second.blocked_by = is_blocked_by;
        }
    }
}

void SocialManager::MergeBlockedFriends(FriendList& friends) {
    nlohmann::json req = {{"page_max", 100}, {"page_offset", 0}};
    auto resp = api_client_->PostJson("/account/get_blocked_friends", {}, req);

    if (!resp.is_object() || !resp.contains("set_blocked_friends") ||
        !resp["set_blocked_friends"].is_object()) {
        return;
    }

    const auto& obj = resp["set_blocked_friends"];
    if (!obj.contains("friends") || !obj["friends"].is_array()) {
        return;
    }

    for (const auto& it : obj["friends"]) {
        if (!it.is_object()) continue;

        const std::string id = JStr(it, "friend_id");
        if (id.empty()) continue;

        const std::string nick = JStr(it, "nickname");

        auto pos = std::find_if(friends.begin(), friends.end(),
            [&id](const auto& pair) { return pair.first == id; });

        if (pos != friends.end()) {
            pos->second.blocked = true;
        } else {
            FriendInfo info{};
            info.nickname = nick;
            info.blocked = true;
            friends.emplace_back(id, std::move(info));
        }
    }
}

std::vector<std::string> SocialManager::FetchOnlineFriends() {
    std::vector<std::string> out;
    auto resp = api_client_->PostJson("/account/get_online_friends");

    if (!resp.is_object() || !resp.contains("online_friends") ||
        !resp["online_friends"].is_array()) {
        return out;
    }

    for (const auto& it : resp["online_friends"]) {
        if (it.is_string()) {
            out.push_back(it.get<std::string>());
        }
    }

    return out;
}

std::map<std::string, std::uint32_t> SocialManager::FetchFriendLevels(
    const std::vector<std::string>& ids) {
    std::map<std::string, std::uint32_t> out;

    for (const auto& id : ids) {
        if (id.empty()) continue;

        auto resp = api_client_->PostJson("/account/join_friend_game", {},
            nlohmann::json{{"friend_id", id}});

        if (resp.is_object() && resp.contains("move_to_game") &&
            resp["move_to_game"].is_object()) {
            const auto& move_to_game = resp["move_to_game"];
            if (move_to_game.contains("level") && move_to_game["level"].is_number_unsigned()) {
                out[id] = move_to_game["level"].get<std::uint32_t>();
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(80));
    }

    return out;
}

void SocialManager::RefreshAsync() {
    if (loading_.exchange(true)) return;

    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        error_.clear();
    }

    std::thread([this]() {
        if (!IsLoggedIn()) {
            std::lock_guard<std::mutex> lock(data_mutex_);
            friends_.clear();
            online_friends_.clear();
            online_levels_.clear();
            data_ok_ = false;
            error_.clear();
            loading_.store(false);
            return;
        }

        try {
            FriendList friends = FetchFriendStatues();
            UpdateBlockedBy(friends);
            MergeBlockedFriends(friends);
            auto online = FetchOnlineFriends();
            auto levels = FetchFriendLevels(online);

            {
                std::lock_guard<std::mutex> lock(data_mutex_);
                friends_ = std::move(friends);
                online_friends_ = std::move(online);
                online_levels_ = std::move(levels);
                data_ok_ = true;
                error_.clear();
            }
        } catch (...) {
            std::lock_guard<std::mutex> lock(data_mutex_);
            friends_.clear();
            online_friends_.clear();
            online_levels_.clear();
            data_ok_ = false;
            error_ = "Failed to refresh";
        }

        loading_.store(false);
    }).detach();
}

bool SocialManager::SetBlocked(const std::string& friend_id, bool blocked) {
    auto resp = api_client_->PostJson("/account/set_friend_block", {},
        nlohmann::json{{"friend", friend_id}, {"blocked", blocked}});

    if (resp.is_object() && resp.contains("result")) {
        try {
            return resp["result"].get<bool>();
        } catch (...) {}
    }

    return false;
}

bool SocialManager::SoftDelete(const std::string& friend_id) {
    auto resp = api_client_->PostJson("/service/relationship/api/v1/set_friend_soft_delete", {},
        nlohmann::json{{"friend", friend_id}, {"soft_deleted", true}});

    if (resp.is_object() && resp.contains("result")) {
        try {
            return resp["result"].get<bool>();
        } catch (...) {}
    }

    return false;
}

bool SocialManager::Rename(const std::string& friend_id, const std::string& new_name) {
    auto resp = api_client_->PostJson("/account/set_friend_name", {},
        nlohmann::json{{"friend", friend_id}, {"name", new_name}});

    if (resp.is_object() && resp.contains("result")) {
        try {
            return resp["result"].get<bool>();
        } catch (...) {}
    }

    return false;
}

bool SocialManager::SetJoinable(bool joinable) {
    auto resp = api_client_->PostJson("/account/set_joinable", {},
        nlohmann::json{{"joinable", joinable}});
    return resp.is_object();
}

bool SocialManager::GiftLightTo(const std::string& friend_id) {
    auto resp = api_client_->PostJson("/service/relationship/api/v1/free_gifts/send", {},
        nlohmann::json{{"gift_type", "gift_heart_wax"}, {"target", friend_id}});
    return resp.is_object();
}

bool SocialManager::GiftHeartTo(const std::string& friend_id) {
    auto resp = api_client_->PostJson("/account/send_message", {},
        nlohmann::json{{"gift_type", "gift"}, {"target", friend_id}});
    return resp.is_object();
}

void SocialManager::ParseGiftResponse(const nlohmann::json& resp, GiftResult& result) {
    std::string code;
    bool decided = false;
    bool ok = false;

    try {
        if (resp.contains("result")) {
            const auto& v = resp["result"];
            if (v.is_boolean()) {
                ok = v.get<bool>();
                code = ok ? "success" : "failed";
                decided = true;
            } else if (v.is_string()) {
                std::string s = v.get<std::string>();
                std::string ls = ToLowerCopy(s);
                code = ls;
                if (ls == "success" || ls == "ok" || ls == "true" || ls == "sent") {
                    ok = true;
                    decided = true;
                } else if (ls == "false") {
                    ok = false;
                    decided = true;
                }
            }
        }
    } catch (...) {}

    if (!decided) {
        try {
            std::string ec = JStr(resp, "error_code");
            if (ec.empty()) ec = JStr(resp, "error");
            if (!ec.empty()) {
                code = ToLowerCopy(ec);
                ok = false;
                decided = true;
            }
        } catch (...) {}
    }

    if (!decided) {
        try {
            if (resp.contains("ok") && resp["ok"].is_boolean()) {
                ok = resp["ok"].get<bool>();
                code = ok ? "success" : (code.empty() ? "failed" : code);
                decided = true;
            }
        } catch (...) {}
    }

    if (!decided) {
        if (code.empty()) {
            code = "success";
            ok = true;
        } else {
            static const std::set<std::string> known_errors = {
                "already_today", "already_pending", "missing_prereq",
                "missing_gift_reqs", "insuf", "rate_limited",
                "not_logged_in", "failed", "failure", "error"
            };
            std::string lc = ToLowerCopy(code);
            ok = (known_errors.find(lc) == known_errors.end());
            code = lc;
        }
    } else {
        if (!code.empty()) code = ToLowerCopy(code);
    }

    result.success = ok;
    result.code = code;
}

GiftResult SocialManager::GiftLightToEx(const std::string& friend_id) {
    GiftResult result{};
    try {
        auto resp = api_client_->PostJson("/service/relationship/api/v1/free_gifts/send", {},
            nlohmann::json{{"gift_type", "gift_heart_wax"}, {"target", friend_id}});

        if (resp.is_object()) {
            ParseGiftResponse(resp, result);
        } else {
            result.success = false;
            result.code = "error";
        }
    } catch (...) {
        result.success = false;
        result.code = "error";
    }
    return result;
}

GiftResult SocialManager::GiftHeartToEx(const std::string& friend_id) {
    GiftResult result{};
    try {
        auto resp = api_client_->PostJson("/account/send_message", {},
            nlohmann::json{{"gift_type", "gift"}, {"target", friend_id}});

        if (resp.is_object()) {
            ParseGiftResponse(resp, result);
        } else {
            result.success = false;
            result.code = "error";
        }
    } catch (...) {
        result.success = false;
        result.code = "error";
    }
    return result;
}

void SocialManager::GiftAllLightAsync() {
    if (gifting_running_.exchange(true)) return;

    gifting_current_.store(0);
    gifting_total_.store(0);

    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        gifting_result_.clear();
    }

    std::thread([this]() {
        if (!IsLoggedIn()) {
            gifting_running_.store(false);
            std::lock_guard<std::mutex> lock(data_mutex_);
            gifting_result_ = tsm::ui::i18n::Tr("Not logged in");
            return;
        }

        int ok_count = 0, fail_count = 0;

        try {
            std::set<std::string> already_sent;
            auto pending = api_client_->PostJson("/service/relationship/api/v1/free_gifts/get_pending");

            if (pending.is_object() && pending.contains("set_sent_free_gifts") &&
                pending["set_sent_free_gifts"].is_array()) {
                for (const auto& gift : pending["set_sent_free_gifts"]) {
                    try {
                        if (gift.contains("type") && gift["type"].is_string() &&
                            gift["type"].get<std::string>() == "gift_heart_wax") {
                            if (gift.contains("to_id") && gift["to_id"].is_string()) {
                                already_sent.insert(gift["to_id"].get<std::string>());
                            }
                        }
                    } catch (...) {}
                }
            }

            std::vector<std::string> targets;
            FriendList fresh = FetchFriendStatues();
            UpdateBlockedBy(fresh);
            MergeBlockedFriends(fresh);

            for (const auto& [id, info] : fresh) {
                if (!info.deleted && !id.empty() && already_sent.find(id) == already_sent.end()) {
                    targets.push_back(id);
                }
            }

            gifting_total_.store(static_cast<int>(targets.size()));

            if (targets.empty()) {
                std::lock_guard<std::mutex> lock(data_mutex_);
                gifting_result_ = tsm::ui::i18n::Tr("No friends to gift");
                gifting_running_.store(false);
                return;
            }

            for (const auto& id : targets) {
                try {
                    auto resp = api_client_->PostJson("/service/relationship/api/v1/free_gifts/send", {},
                        nlohmann::json{{"gift_type", "gift_heart_wax"}, {"target", id}});
                    if (resp.is_object()) ++ok_count;
                    else ++fail_count;
                } catch (...) {
                    ++fail_count;
                }

                gifting_current_.fetch_add(1);
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
        } catch (...) {}

        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            char buf[128];
            if (ok_count > 0 && fail_count == 0) {
                const char* fmt = tsm::ui::i18n::Tr("Sent Light to %d friends");
                std::snprintf(buf, sizeof(buf), fmt, ok_count);
                gifting_result_ = buf;
            } else if (ok_count > 0) {
                const char* fmt = tsm::ui::i18n::Tr("Sent Light to %d, failed %d");
                std::snprintf(buf, sizeof(buf), fmt, ok_count, fail_count);
                gifting_result_ = buf;
            } else {
                gifting_result_ = tsm::ui::i18n::Tr("Failed to send any Light gifts");
            }
        }

        gifting_running_.store(false);
    }).detach();
}

void SocialManager::CollectAllGiftsAsync() {
    if (collecting_running_.exchange(true)) return;

    collecting_current_.store(0);
    collecting_total_.store(0);

    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        collecting_result_.clear();
    }

    std::thread([this]() {
        if (!IsLoggedIn()) {
            collecting_running_.store(false);
            std::lock_guard<std::mutex> lock(data_mutex_);
            collecting_result_ = tsm::ui::i18n::Tr("Not logged in");
            return;
        }

        int ok_count = 0, fail_count = 0;

        try {
            std::vector<std::pair<std::string, std::int64_t>> free_gifts;
            std::vector<std::int64_t> message_gifts;

            auto gift_resp = api_client_->PostJson("/service/relationship/api/v1/free_gifts/get_pending");
            if (gift_resp.is_object() && gift_resp.contains("set_recvd_free_gifts") &&
                gift_resp["set_recvd_free_gifts"].is_array()) {
                for (const auto& it : gift_resp["set_recvd_free_gifts"]) {
                    try {
                        if (it.contains("from_id") && it.contains("gift_id") &&
                            it["from_id"].is_string() && it["gift_id"].is_number_integer()) {
                            free_gifts.emplace_back(
                                it["from_id"].get<std::string>(),
                                it["gift_id"].get<std::int64_t>()
                            );
                        }
                    } catch (...) {}
                }
            }

            auto msg_resp = api_client_->PostJson("/account/get_pending_messages", {},
                nlohmann::json{{"exclude_free_gifts", false}});
            if (msg_resp.is_object() && msg_resp.contains("set_recvd_messages") &&
                msg_resp["set_recvd_messages"].is_array()) {
                for (const auto& it : msg_resp["set_recvd_messages"]) {
                    try {
                        if (it.contains("type") && it["type"].is_string() &&
                            it["type"].get<std::string>() == "gift" &&
                            it.contains("msg_id") && it["msg_id"].is_number_integer()) {
                            message_gifts.push_back(it["msg_id"].get<std::int64_t>());
                        }
                    } catch (...) {}
                }
            }

            collecting_total_.store(static_cast<int>(free_gifts.size() + message_gifts.size()));

            if (collecting_total_.load() == 0) {
                std::lock_guard<std::mutex> lock(data_mutex_);
                collecting_result_ = tsm::ui::i18n::Tr("No gifts to collect");
                collecting_running_.store(false);
                return;
            }

            for (const auto& [from_id, gift_id] : free_gifts) {
                try {
                    auto resp = api_client_->PostJson("/service/relationship/api/v1/free_gifts/claim", {},
                        nlohmann::json{{"from_id", from_id}, {"gift_id", gift_id}});
                    if (resp.is_object()) ++ok_count;
                    else ++fail_count;
                } catch (...) {
                    ++fail_count;
                }

                collecting_current_.fetch_add(1);
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }

            for (const auto& msg_id : message_gifts) {
                try {
                    auto resp = api_client_->PostJson("/account/claim_message_gift", {},
                        nlohmann::json{{"msg_id", msg_id}});
                    if (resp.is_object()) ++ok_count;
                    else ++fail_count;
                } catch (...) {
                    ++fail_count;
                }

                collecting_current_.fetch_add(1);
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
        } catch (...) {}

        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            char buf[128];
            if (ok_count > 0 && fail_count == 0) {
                const char* fmt = tsm::ui::i18n::Tr("Collected %d gifts");
                std::snprintf(buf, sizeof(buf), fmt, ok_count);
                collecting_result_ = buf;
            } else if (ok_count > 0) {
                const char* fmt = tsm::ui::i18n::Tr("Collected %d, failed %d");
                std::snprintf(buf, sizeof(buf), fmt, ok_count, fail_count);
                collecting_result_ = buf;
            } else {
                collecting_result_ = tsm::ui::i18n::Tr("Failed to collect gifts");
            }
        }

        collecting_running_.store(false);
    }).detach();
}

OperationProgress SocialManager::GetGiftingProgress() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return {
        gifting_running_.load(),
        gifting_current_.load(),
        gifting_total_.load(),
        gifting_result_
    };
}

OperationProgress SocialManager::GetCollectingProgress() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return {
        collecting_running_.load(),
        collecting_current_.load(),
        collecting_total_.load(),
        collecting_result_
    };
}

std::string SocialManager::GetLevelName(std::uint32_t level_id) const {
    static bool built = false;
    static std::map<std::uint32_t, std::string> id_to_name;

    if (!built) {
        if (!tsm::data::DataManager::Get().GetLevels().is_object()) {
            (void)tsm::data::DataManager::Get().LoadLevels();
        }

        const auto& levels = tsm::data::DataManager::Get().GetLevels();
        for (auto it = levels.begin(); it != levels.end(); ++it) {
            const auto& arr = it.value();
            if (!arr.is_array()) continue;

            for (const auto& v : arr) {
                if (!v.is_string()) continue;
                std::string name = v.get<std::string>();
                id_to_name.try_emplace(Fnv1a32(name), name);
            }
        }

        built = true;
    }

    auto it = id_to_name.find(level_id);
    if (it != id_to_name.end()) {
        return it->second;
    }

    return "Level " + std::to_string(level_id);
}

}}
