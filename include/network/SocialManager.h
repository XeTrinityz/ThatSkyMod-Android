#pragma once

#include <network/NetworkTypes.h>
#include <network/ApiClient.h>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <atomic>
#include <mutex>

namespace tsm { namespace network {

class SocialManager {
public:
    static SocialManager& Get();

    SocialManager(const SocialManager&) = delete;
    SocialManager& operator=(const SocialManager&) = delete;


    void RefreshAsync();

    bool IsLoggedIn() const;

    bool IsLoading() const { return loading_.load(); }

    FriendList GetFriends() const;

    std::vector<std::string> GetOnlineFriends() const;

    std::map<std::string, std::uint32_t> GetOnlineLevels() const;

    std::string GetFriendNickname(const std::string& uuid) const;

    bool IsFriend(const std::string& uuid) const;


    bool SetBlocked(const std::string& friend_id, bool blocked);
    bool SoftDelete(const std::string& friend_id);
    bool Rename(const std::string& friend_id, const std::string& new_name);


    bool SetJoinable(bool joinable);


    bool GiftLightTo(const std::string& friend_id);
    bool GiftHeartTo(const std::string& friend_id);

    GiftResult GiftLightToEx(const std::string& friend_id);
    GiftResult GiftHeartToEx(const std::string& friend_id);

    void GiftAllLightAsync();

    void CollectAllGiftsAsync();

    OperationProgress GetGiftingProgress() const;

    OperationProgress GetCollectingProgress() const;


    std::string GetLevelName(std::uint32_t level_id) const;

private:
    SocialManager();
    ~SocialManager() = default;

    std::unique_ptr<ApiClient> api_client_;

    mutable std::mutex data_mutex_;
    std::atomic<bool> loading_{false};
    bool data_ok_{false};
    std::string error_;
    FriendList friends_;
    std::vector<std::string> online_friends_;
    std::map<std::string, std::uint32_t> online_levels_;

    std::atomic<bool> gifting_running_{false};
    std::atomic<int> gifting_current_{0};
    std::atomic<int> gifting_total_{0};
    std::string gifting_result_;

    std::atomic<bool> collecting_running_{false};
    std::atomic<int> collecting_current_{0};
    std::atomic<int> collecting_total_{0};
    std::string collecting_result_;

    FriendList FetchFriendStatues();
    void UpdateBlockedBy(FriendList& friends);
    void MergeBlockedFriends(FriendList& friends);
    std::vector<std::string> FetchOnlineFriends();
    std::map<std::string, std::uint32_t> FetchFriendLevels(const std::vector<std::string>& ids);
    void ParseGiftResponse(const nlohmann::json& resp, GiftResult& result);
};

}}
