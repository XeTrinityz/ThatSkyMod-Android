#pragma once

#include <progression/RunConfig.h>
#include <progression/PatchManager.h>
#include <string>
#include <vector>
#include <mutex>
#include <chrono>

namespace tsm { namespace progression {

class WaxRunner {
public:
    static WaxRunner& Get();

    WaxRunner(const WaxRunner&) = delete;
    WaxRunner& operator=(const WaxRunner&) = delete;

    void Start(const RunConfig& config);

    void Cancel();

    void Tick();

    bool IsActive() const;

    std::string GetStatus() const;

    RunProgress GetProgress() const;

private:
    WaxRunner() = default;
    ~WaxRunner() = default;

    mutable std::mutex mutex_;
    bool active_{false};
    RunPhase phase_{RunPhase::Idle};
    RunConfig config_;
    std::string status_;
    std::chrono::steady_clock::time_point next_time_;
    std::chrono::steady_clock::time_point level_timeline_time_;
    bool level_timeline_enqueued_{false};

    std::vector<std::string> levels_;
    size_t level_index_{0};
    std::string target_level_;

    std::vector<CandlePoint> all_candles_;
    std::vector<CandlePoint> current_plants_;
    size_t plant_index_{0};

    std::vector<Position3D> current_dyes_;
    size_t dye_index_{0};

    PatchManager patch_manager_;

    void LoadCandleDatabase();
    void PrepareLevelList();

    void TickPrepareLevel();
    void TickWaitForLoad();
    void TickWaitForDyeInit();
    void TickAdditionalPrePlantDelay();
    void TickTeleportNextPlant();
    void TickDelayBetweenPlants();
    void TickTeleportNextDye();
    void TickDelayBetweenDyes();
    void TickDelayBetweenLevels();
    void TickCompleted();

    void UpdateStatus(const std::string& msg);
    void MaybeEnqueueLevelTimeline(const std::chrono::steady_clock::time_point& now);
    std::vector<Position3D> ClusterDyes(const std::vector<Position3D>& dyes, float radius);
    float Distance3D(const Position3D& a, const Position3D& b) const;
};

}}
