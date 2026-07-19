#pragma once

#include <string>
#include <cstdint>

namespace tsm { namespace progression {

enum class StartMode {
    FromBeginning = 0,
    FromCurrentLevel = 1
};

struct RunConfig {
    StartMode start_mode{StartMode::FromBeginning};
    bool include_plants{true};
    bool include_dyes{false};
};

enum class RunPhase {
    Idle,
    PrepareLevel,
    WaitForLoad,
    WaitForDyeInit,
    AdditionalPrePlantDelay,
    TeleportNextPlant,
    DelayBetweenPlants,
    TeleportNextDye,
    DelayBetweenDyes,
    DelayBetweenLevels,
    Completed
};

struct RunProgress {
    bool active{false};
    RunPhase phase{RunPhase::Idle};
    std::string status;
    int current_level{0};
    int total_levels{0};
    int current_item{0};
    int total_items{0};
};

struct Position3D {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
};

struct CandlePoint {
    std::string name;
    std::string map;
    Position3D position;
};

}}
