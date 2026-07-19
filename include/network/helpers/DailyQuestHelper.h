#pragma once

#include <string>
#include <vector>
#include <utility>
#include <atomic>
#include <mutex>
#include <functional>

namespace tsm { namespace network { namespace helpers {

class DailyQuestHelper {
public:
    static DailyQuestHelper& Get();

    void FetchAsync(bool autoRun = false);

    void CompleteAll(std::function<void(float, const char*)> progressCallback = nullptr);

    void CollectAllCompletedSinceReload(std::function<void(float, const char*)> progressCallback = nullptr);

    void Cancel() { cancel_.store(true); }
    bool IsLoading() const { return loading_.load(); }
    bool IsCompleting() const { return completing_.load(); }
    bool IsCollecting() const { return collecting_.load(); }
    bool HasReloadSnapshot() const { std::lock_guard<std::mutex> lk(snapshotMu_); return !lastReloadActive_.empty(); }

    std::vector<std::pair<std::string, int>> GetQuests() const;

    void RemoveQuest(const std::string& questName);

private:
    DailyQuestHelper() = default;

    std::atomic<bool> loading_{false};
    mutable std::mutex questsMu_;
    std::vector<std::pair<std::string, int>> quests_;
    std::atomic<bool> autoRun_{false};
    std::atomic<bool> completing_{false};
    std::atomic<bool> cancel_{false};

    mutable std::mutex snapshotMu_;
    std::vector<std::string> lastReloadActive_;
    std::atomic<bool> collecting_{false};
};

}}}
