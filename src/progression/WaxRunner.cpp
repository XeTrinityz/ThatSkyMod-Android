#include <progression/WaxRunner.h>
#include <game/memory/api.h>
#include <game/hooks/HookManager.h>
#include <game/interop/LuaScriptQueue.h>
#include <game/interop/LuaHelpers.h>
#include <game/data/RealmData.h>
#include <data/DataManager.h>
#include <utils/common/vec3.h>
#include <nlohmann/json.hpp>

#include <ui/core/Localization.h>

#include <state/ModState.h>

#include <unordered_set>
#include <algorithm>
#include <cmath>

#include "../../resources/data/candles_json.h"

namespace tsm { namespace progression {

WaxRunner& WaxRunner::Get() {
    static WaxRunner instance;
    return instance;
}

void WaxRunner::Start(const RunConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);

    LoadCandleDatabase();

    active_ = true;
    config_ = config;
    phase_ = RunPhase::PrepareLevel;
    status_.clear();
    level_index_ = 0;
    target_level_.clear();
    current_plants_.clear();
    plant_index_ = 0;
    current_dyes_.clear();
    dye_index_ = 0;

    patch_manager_.ApplyPatches(config_.include_plants, config_.include_dyes);

    PrepareLevelList();

    if (levels_.empty()) {
        status_ = tsm::ui::i18n::Tr("No levels available to run.");
        phase_ = RunPhase::Completed;
        active_ = false;
    } else {
        char buf[128];
        const char* fmt = tsm::ui::i18n::Tr("Processing level %d/%d");
        std::snprintf(buf, sizeof(buf), fmt, 1, static_cast<int>(levels_.size()));
        UpdateStatus(buf);
    }
}

void WaxRunner::Cancel() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!active_) return;

    active_ = false;
    phase_ = RunPhase::Idle;
    levels_.clear();
    current_plants_.clear();
    current_dyes_.clear();
    status_ = tsm::ui::i18n::Tr("Run canceled");

    patch_manager_.RestoreAllPatches();
}

void WaxRunner::Tick() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!active_) return;

    switch (phase_) {
        case RunPhase::PrepareLevel:
            TickPrepareLevel();
            break;
        case RunPhase::WaitForLoad:
            TickWaitForLoad();
            break;
        case RunPhase::WaitForDyeInit:
            TickWaitForDyeInit();
            break;
        case RunPhase::AdditionalPrePlantDelay:
            TickAdditionalPrePlantDelay();
            break;
        case RunPhase::TeleportNextPlant:
            TickTeleportNextPlant();
            break;
        case RunPhase::DelayBetweenPlants:
            TickDelayBetweenPlants();
            break;
        case RunPhase::TeleportNextDye:
            TickTeleportNextDye();
            break;
        case RunPhase::DelayBetweenDyes:
            TickDelayBetweenDyes();
            break;
        case RunPhase::DelayBetweenLevels:
            TickDelayBetweenLevels();
            break;
        case RunPhase::Completed:
            TickCompleted();
            break;
        case RunPhase::Idle:
        default:
            break;
    }
}

bool WaxRunner::IsActive() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_;
}

std::string WaxRunner::GetStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}

RunProgress WaxRunner::GetProgress() const {
    std::lock_guard<std::mutex> lock(mutex_);

    RunProgress progress;
    progress.active = active_;
    progress.phase = phase_;
    progress.status = status_;
    progress.current_level = static_cast<int>(level_index_) + 1;
    progress.total_levels = static_cast<int>(levels_.size());

    if (phase_ == RunPhase::TeleportNextPlant || phase_ == RunPhase::DelayBetweenPlants) {
        progress.current_item = static_cast<int>(plant_index_);
        progress.total_items = static_cast<int>(current_plants_.size());
    } else if (phase_ == RunPhase::TeleportNextDye || phase_ == RunPhase::DelayBetweenDyes) {
        progress.current_item = static_cast<int>(dye_index_);
        progress.total_items = static_cast<int>(current_dyes_.size());
    }

    return progress;
}

void WaxRunner::LoadCandleDatabase() {
    if (!all_candles_.empty()) return;

    try {
        const char* begin = reinterpret_cast<const char*>(kCandlesData);
        const char* end = begin + (sizeof(kCandlesData) / sizeof(kCandlesData[0]));
        nlohmann::json arr = nlohmann::json::parse(begin, end);

        if (arr.is_array()) {
            all_candles_.reserve(arr.size());

            for (const auto& item : arr) {
                if (!item.is_object()) continue;

                CandlePoint cp;
                if (auto k = item.find("name"); k != item.end() && k->is_string()) {
                    cp.name = k->get<std::string>();
                }
                if (auto k = item.find("map"); k != item.end() && k->is_string()) {
                    cp.map = k->get<std::string>();
                }
                if (auto k = item.find("x"); k != item.end() && k->is_number()) {
                    cp.position.x = k->get<float>();
                }
                if (auto k = item.find("y"); k != item.end() && k->is_number()) {
                    cp.position.y = k->get<float>();
                }
                if (auto k = item.find("z"); k != item.end() && k->is_number()) {
                    cp.position.z = k->get<float>();
                }

                if (!cp.map.empty()) {
                    all_candles_.push_back(std::move(cp));
                }
            }
        }
    } catch (...) {
        all_candles_.clear();
    }
}

void WaxRunner::PrepareLevelList() {
    levels_.clear();

    if (tsm::data::DataManager::Get().GetLevels().is_null() ||
        tsm::data::DataManager::Get().GetLevels().empty()) {
        tsm::data::DataManager::Get().LoadLevels();
    }

    const nlohmann::json& lv = tsm::data::DataManager::Get().GetLevels();

    if (lv.is_object()) {
        for (const auto& realmKey : tsm::game::data::kRealmOrder) {
            if (!lv.contains(realmKey)) continue;

            const auto& arr = lv.at(realmKey);
            if (!arr.is_array()) continue;

            for (const auto& n : arr) {
                if (!n.is_string()) continue;

                std::string levelName = n.get<std::string>();
                if (tsm::game::data::kIgnoreLevels.find(levelName) != tsm::game::data::kIgnoreLevels.end()) continue;

                levels_.push_back(levelName);
            }
        }

        if (config_.start_mode == StartMode::FromCurrentLevel) {
            const char* curLv = tsm::game::api::LevelName();
            if (curLv && *curLv) {
                for (size_t i = 0; i < levels_.size(); ++i) {
                    if (levels_[i] == std::string(curLv)) {
                        level_index_ = i;
                        break;
                    }
                }
            }
        }
    }
}

void WaxRunner::TickPrepareLevel() {
    if (level_index_ >= levels_.size()) {
        phase_ = RunPhase::Completed;
        return;
    }

    target_level_ = levels_[level_index_];
    {
        char buf[128];
        const char* fmt = tsm::ui::i18n::Tr("Processing level %d/%d");
        std::snprintf(buf, sizeof(buf), fmt, static_cast<int>(level_index_ + 1), static_cast<int>(levels_.size()));
        UpdateStatus(buf);
    }

    tsm::lua::helpers::ChangeLevel(target_level_, true);
    phase_ = RunPhase::WaitForLoad;
}

void WaxRunner::TickWaitForLoad() {
    const char* cur = tsm::game::api::LevelName();
    if (!cur || target_level_ != std::string(cur)) return;

    const auto now = std::chrono::steady_clock::now();
    int extraSec = 0;

    level_timeline_enqueued_ = false;
    level_timeline_time_ = now + std::chrono::seconds(1);

    current_plants_.clear();
    if (config_.include_plants) {
        for (const auto& cp : all_candles_) {
            if (cp.map == target_level_ && cp.name == "Plant") {
                current_plants_.push_back(cp);
            }
        }
    }

    if (!current_plants_.empty()) {
        char buf[128];
        const char* fmt = tsm::ui::i18n::Tr("Processing level %d/%d");
        std::snprintf(buf, sizeof(buf), fmt, static_cast<int>(level_index_ + 1), static_cast<int>(levels_.size()));
        UpdateStatus(buf);
        const auto& ms = tsm::state::ModState::Get();
        int pre = std::max(0, ms.progression.prePlantDelaySec);
        next_time_ = now + std::chrono::seconds(pre + extraSec);
        phase_ = RunPhase::AdditionalPrePlantDelay;
    } else if (config_.include_dyes) {
        char buf[160];
        const char* fmt = tsm::ui::i18n::Tr("Processing level %d/%d (waiting for dyes)");
        std::snprintf(buf, sizeof(buf), fmt, static_cast<int>(level_index_ + 1), static_cast<int>(levels_.size()));
        UpdateStatus(buf);
        const auto& ms = tsm::state::ModState::Get();
        int waitDye = std::max(0, ms.progression.waitForDyeInitSec);
        next_time_ = now + std::chrono::seconds(waitDye + extraSec);
        phase_ = RunPhase::WaitForDyeInit;
    } else {
        char buf[128];
        const char* fmt = tsm::ui::i18n::Tr("Processing level %d/%d");
        std::snprintf(buf, sizeof(buf), fmt, static_cast<int>(level_index_ + 1), static_cast<int>(levels_.size()));
        UpdateStatus(buf);
        const auto& ms = tsm::state::ModState::Get();
        int between = std::max(0, ms.progression.betweenLevelsSec);
        int extraTimeline = level_timeline_enqueued_ ? 3 : 0;
        next_time_ = now + std::chrono::seconds(between + extraSec + extraTimeline);
        phase_ = RunPhase::DelayBetweenLevels;
    }
}

void WaxRunner::TickWaitForDyeInit() {
    const auto now = std::chrono::steady_clock::now();
    MaybeEnqueueLevelTimeline(now);
    if (now < next_time_) return;

    current_dyes_.clear();

    int dyeCount = 0;
    if (int* p = tsm::game::hooks::HookManager::Get().GetDyeCountPtr()) {
        dyeCount = *p;
    }

    std::uintptr_t firstDye = tsm::game::hooks::HookManager::Get().GetFirstDye();
    constexpr std::uintptr_t kDyeStride = 0x40;

    if (firstDye != 0 && dyeCount > 0) {
        std::vector<Position3D> allDyes;
        allDyes.reserve(dyeCount);

        for (int i = 0; i < dyeCount; ++i) {
            std::uintptr_t addr = firstDye + i * kDyeStride;
            const float* v = reinterpret_cast<const float*>(addr);
            allDyes.push_back(Position3D{v[0], v[1], v[2]});
        }

        current_dyes_ = ClusterDyes(allDyes, 10.0f);
    }

    if (!current_dyes_.empty()) {
        dye_index_ = 0;
        phase_ = RunPhase::TeleportNextDye;
    } else {
        char buf[128];
        const char* fmt = tsm::ui::i18n::Tr("Processing level %d/%d");
        std::snprintf(buf, sizeof(buf), fmt, static_cast<int>(level_index_ + 1), static_cast<int>(levels_.size()));
        UpdateStatus(buf);
        const auto& ms = tsm::state::ModState::Get();
        int between = std::max(0, ms.progression.betweenLevelsSec);
        next_time_ = now + std::chrono::seconds(between + (level_timeline_enqueued_ ? 3 : 0));
        phase_ = RunPhase::DelayBetweenLevels;
    }
}

void WaxRunner::TickAdditionalPrePlantDelay() {
    const auto now = std::chrono::steady_clock::now();
    MaybeEnqueueLevelTimeline(now);
    if (now < next_time_) return;

    plant_index_ = 0;
    phase_ = RunPhase::TeleportNextPlant;
}

void WaxRunner::TickTeleportNextPlant() {
    MaybeEnqueueLevelTimeline(std::chrono::steady_clock::now());
    if (plant_index_ < current_plants_.size()) {
        const auto& cp = current_plants_[plant_index_];
        char buf[128];
        const char* fmt = tsm::ui::i18n::Tr("Processing level %d/%d");
        std::snprintf(buf, sizeof(buf), fmt, static_cast<int>(level_index_ + 1), static_cast<int>(levels_.size()));
        UpdateStatus(buf);

        vec3 pos{cp.position.x, cp.position.y, cp.position.z};
        tsm::lua::helpers::SetPlayerPosition(pos, false);

        plant_index_++;
        const auto& ms = tsm::state::ModState::Get();
        int plantItv = std::max(0, ms.progression.plantIntervalSec);
        next_time_ = std::chrono::steady_clock::now() + std::chrono::seconds(plantItv);
        phase_ = RunPhase::DelayBetweenPlants;
    } else if (config_.include_dyes) {
        char buf[160];
        const char* fmt = tsm::ui::i18n::Tr("Processing level %d/%d (waiting for dyes)");
        std::snprintf(buf, sizeof(buf), fmt, static_cast<int>(level_index_ + 1), static_cast<int>(levels_.size()));
        UpdateStatus(buf);
        const auto& ms = tsm::state::ModState::Get();
        int waitDye = std::max(0, ms.progression.waitForDyeInitSec);
        next_time_ = std::chrono::steady_clock::now() + std::chrono::seconds(waitDye);
        phase_ = RunPhase::WaitForDyeInit;
    } else {
        UpdateStatus("Processing level " + std::to_string(level_index_ + 1) + "/" + std::to_string(levels_.size()));
        const auto& ms = tsm::state::ModState::Get();
        int between = std::max(0, ms.progression.betweenLevelsSec);
        next_time_ = std::chrono::steady_clock::now() + std::chrono::seconds(between + (level_timeline_enqueued_ ? 3 : 0));
        phase_ = RunPhase::DelayBetweenLevels;
    }
}

void WaxRunner::TickDelayBetweenPlants() {
    const auto now = std::chrono::steady_clock::now();
    MaybeEnqueueLevelTimeline(now);
    if (now < next_time_) return;

    phase_ = RunPhase::TeleportNextPlant;
}

void WaxRunner::TickTeleportNextDye() {
    MaybeEnqueueLevelTimeline(std::chrono::steady_clock::now());
    if (dye_index_ < current_dyes_.size()) {
        const auto& dp = current_dyes_[dye_index_];
        char buf[160];
        const char* fmt = tsm::ui::i18n::Tr("Processing level %d/%d (Dyes)");
        std::snprintf(buf, sizeof(buf), fmt, static_cast<int>(level_index_ + 1), static_cast<int>(levels_.size()));
        UpdateStatus(buf);

        vec3 pos{dp.x, dp.y, dp.z};
        tsm::lua::helpers::SetPlayerPosition(pos, false);
        tsm::lua::helpers::PlayEmote("butterfly", 1.0f, 10.0f, 1);

        dye_index_++;
        const auto& ms = tsm::state::ModState::Get();
        int dyeItv = std::max(0, ms.progression.dyeIntervalSec);
        next_time_ = std::chrono::steady_clock::now() + std::chrono::seconds(dyeItv);
        phase_ = RunPhase::DelayBetweenDyes;
    } else {
        char buf[128];
        const char* fmt = tsm::ui::i18n::Tr("Processing level %d/%d");
        std::snprintf(buf, sizeof(buf), fmt, static_cast<int>(level_index_ + 1), static_cast<int>(levels_.size()));
        UpdateStatus(buf);
        const auto& ms = tsm::state::ModState::Get();
        int between = std::max(0, ms.progression.betweenLevelsSec);
        next_time_ = std::chrono::steady_clock::now() + std::chrono::seconds(between + (level_timeline_enqueued_ ? 3 : 0));
        phase_ = RunPhase::DelayBetweenLevels;
    }
}

void WaxRunner::TickDelayBetweenDyes() {
    const auto now = std::chrono::steady_clock::now();
    MaybeEnqueueLevelTimeline(now);
    if (now < next_time_) return;

    phase_ = RunPhase::TeleportNextDye;
}

void WaxRunner::TickDelayBetweenLevels() {
    const auto now = std::chrono::steady_clock::now();
    MaybeEnqueueLevelTimeline(now);
    if (now < next_time_) return;

    level_index_++;
    phase_ = RunPhase::PrepareLevel;
}

void WaxRunner::TickCompleted() {
    status_ = tsm::ui::i18n::Tr("Run completed");
    patch_manager_.RestoreAllPatches();
    active_ = false;
    phase_ = RunPhase::Idle;
}

void WaxRunner::UpdateStatus(const std::string& msg) {
    status_ = msg;
}

void WaxRunner::MaybeEnqueueLevelTimeline(const std::chrono::steady_clock::time_point& now) {
    if (level_timeline_enqueued_) return;
    if (now < level_timeline_time_) return;

    const char* cur = tsm::game::api::LevelName();
    if (!cur || target_level_ != std::string(cur)) return;

    if (target_level_ == "Dawn_TrialsEarth") {
        tsm::lua::queue::Enqueue("PlayTimeline(game, \"Revelation\")");
    } else if (target_level_ == "DayHubCave") {
        tsm::lua::queue::Enqueue("PlayTimeline(game, \"PlatformUp\")");
    }

    level_timeline_enqueued_ = true;
    if (phase_ == RunPhase::DelayBetweenLevels) {
        next_time_ = std::max(next_time_, now + std::chrono::seconds(3));
    }
}

std::vector<Position3D> WaxRunner::ClusterDyes(const std::vector<Position3D>& dyes, float radius) {
    if (dyes.empty()) return {};

    std::vector<bool> visited(dyes.size(), false);
    std::vector<Position3D> clusters;

    for (size_t i = 0; i < dyes.size(); ++i) {
        if (visited[i]) continue;

        std::vector<size_t> clusterIndices;
        clusterIndices.push_back(i);
        visited[i] = true;

        for (size_t j = i + 1; j < dyes.size(); ++j) {
            if (visited[j]) continue;

            float dist = Distance3D(dyes[i], dyes[j]);
            if (dist <= radius) {
                clusterIndices.push_back(j);
                visited[j] = true;
            }
        }

        float sumX = 0, sumY = 0, sumZ = 0;
        for (size_t idx : clusterIndices) {
            sumX += dyes[idx].x;
            sumY += dyes[idx].y;
            sumZ += dyes[idx].z;
        }
        float count = static_cast<float>(clusterIndices.size());
        clusters.push_back(Position3D{sumX / count, sumY / count, sumZ / count});
    }

    return clusters;
}

float WaxRunner::Distance3D(const Position3D& a, const Position3D& b) const {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float dz = b.z - a.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

}}
