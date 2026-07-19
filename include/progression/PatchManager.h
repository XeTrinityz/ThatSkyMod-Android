#pragma once

#include <game/memory/Patch.h>
#include <vector>

namespace tsm { namespace progression {

class PatchManager {
public:
    PatchManager() = default;
    ~PatchManager();

    PatchManager(const PatchManager&) = delete;
    PatchManager& operator=(const PatchManager&) = delete;

    void ApplyPatches(bool include_plants, bool include_dyes);

    void RestoreAllPatches();

    bool ArePatchesApplied() const;

private:
    std::vector<tsm::game::memory::Patch> wax_patches_;
    std::vector<tsm::game::memory::Patch> candle_patches_;
    std::vector<tsm::game::memory::Patch> plant_patches_;
    std::vector<tsm::game::memory::Patch> dye_patches_;

    bool wax_applied_{false};
    bool candles_applied_{false};
    bool plants_applied_{false};
    bool dyes_applied_{false};

    void ApplyWaxPatches();
    void ApplyCandlePatches();
    void ApplyPlantPatches();
    void ApplyDyePatches();

    void RestoreWaxPatches();
    void RestoreCandlePatches();
    void RestorePlantPatches();
    void RestoreDyePatches();
};

}}
