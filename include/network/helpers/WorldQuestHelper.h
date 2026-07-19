#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include <mutex>
#include <functional>

namespace tsm { namespace network { namespace helpers {

struct WQReward {
    std::string currency;
    std::string collectible;
};

class WorldQuestHelper {
public:
    static WorldQuestHelper& Get();

    bool BuildDefinitions(bool forceReload = false);

    void FetchStatusAsync();

    void StartRun(bool collectCollectibles = false,
                 std::function<void(float, const char*)> progressCallback = nullptr);

    void Cancel() { cancel_.store(true); }
    bool IsRunning() const { return running_.load(); }
    bool IsStatusLoading() const { return statusLoading_.load(); }
    float GetProgress() const { return progress_.load(); }

    const std::unordered_map<std::string, std::vector<WQReward>>& GetDefinitions() const { return defs_; }
    const std::unordered_set<std::string>& GetCompleted() const { return completed_; }
    void SetManuallyReloaded(bool value) { hasManuallyReloaded_ = value; }
    bool HasManuallyReloaded() const { return hasManuallyReloaded_; }

    void MarkCompleted(const std::string& questName);

private:
    WorldQuestHelper() = default;

    std::unordered_map<std::string, std::vector<WQReward>> defs_;
    std::mutex defsMu_;

    std::atomic<bool> statusLoading_{false};
    std::mutex statusMu_;
    std::unordered_set<std::string> completed_;
    bool hasManuallyReloaded_ = false;

    std::atomic<bool> running_{false};
    std::atomic<bool> cancel_{false};
    std::atomic<float> progress_{0.0f};
    bool collectCollectibles_ = false;
};

}}}
