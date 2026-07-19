#include <progression/PatchManager.h>
#include <game/memory/Address.h>
#include <game/memory/offsets.h>

namespace tsm { namespace progression {

PatchManager::~PatchManager() {
    RestoreAllPatches();
}

void PatchManager::ApplyPatches(bool include_plants, bool include_dyes) {
    ApplyWaxPatches();
    ApplyCandlePatches();

    if (include_plants) {
        ApplyPlantPatches();
    }

    if (include_dyes) {
        ApplyDyePatches();
    }
}

void PatchManager::RestoreAllPatches() {
    RestoreWaxPatches();
    RestoreCandlePatches();
    RestorePlantPatches();
    RestoreDyePatches();
}

bool PatchManager::ArePatchesApplied() const {
    return wax_applied_ || candles_applied_ || plants_applied_ || dyes_applied_;
}

void PatchManager::ApplyWaxPatches() {
    using namespace tsm::game::Signatures;
    if (wax_applied_) return;

    if (tsm::game::memory::GetBase() == 0) {
        tsm::game::memory::InitializeBase();
    }

    wax_patches_.clear();
    if (tsm::game::memory::CreateNopPatchesForPattern(kAutoCollectWaxPattern, wax_patches_, 1) > 0) {
        for (auto& patch : wax_patches_) {
            if (!patch.applied) {
                patch.Apply();
            }
        }
        wax_applied_ = true;
    }
}

void PatchManager::ApplyCandlePatches() {
    using namespace tsm::game::Signatures;
    if (candles_applied_) return;

    if (tsm::game::memory::GetBase() == 0) {
        tsm::game::memory::InitializeBase();
    }

    candle_patches_.clear();
    if (tsm::game::memory::CreateNopPatchesForPattern(kAutoBurnCandlesPattern, candle_patches_, 2) > 0) {
        for (auto& patch : candle_patches_) {
            if (!patch.applied) {
                patch.Apply();
            }
        }
        candles_applied_ = true;
    }
}

void PatchManager::ApplyPlantPatches() {
    if (plants_applied_) return;

    if (tsm::game::memory::GetBase() == 0) {
        tsm::game::memory::InitializeBase();
    }

    plant_patches_.clear();

    const std::uint8_t kToInstr[4] = { 0xE0, 0x03, 0x27, 0x1E };
    const std::uintptr_t kTargets[] = {
        tsm::game::Offsets::kAutoBurnPlants1,
        tsm::game::Offsets::kAutoBurnPlants2,
        tsm::game::Offsets::kAutoBurnPlants3
    };

    for (std::uintptr_t rva : kTargets) {
        void* target = tsm::game::memory::RvaToPtr(rva);
        if (!target) continue;

        std::uint32_t cur = *reinterpret_cast<std::uint32_t*>(target);
        if (cur == 0xBD424940u || cur == 0xBD424920u) {
            plant_patches_.emplace_back(tsm::game::memory::CreatePatch(target, kToInstr, 4));
        }
    }

    for (auto& patch : plant_patches_) {
        if (!patch.applied) {
            patch.Apply();
        }
    }

    if (!plant_patches_.empty()) {
        plants_applied_ = true;
    }
}

void PatchManager::ApplyDyePatches() {
    if (dyes_applied_) return;

    if (tsm::game::memory::GetBase() == 0) {
        tsm::game::memory::InitializeBase();
    }

    dye_patches_.clear();

    const std::uint8_t kToInstr[4] = { 0xE0, 0x03, 0x27, 0x1E };
    const std::uintptr_t kTargets[] = {
        tsm::game::Offsets::kAutoBurnPlants1,
        tsm::game::Offsets::kAutoBurnPlants2,
        tsm::game::Offsets::kAutoBurnPlants3
    };

    for (std::uintptr_t rva : kTargets) {
        void* target = tsm::game::memory::RvaToPtr(rva);
        if (!target) continue;

        std::uint32_t cur = *reinterpret_cast<std::uint32_t*>(target);
        if (cur == 0xBD424940u || cur == 0xBD424920u) {
            dye_patches_.emplace_back(tsm::game::memory::CreatePatch(target, kToInstr, 4));
        }
    }

    for (auto& patch : dye_patches_) {
        if (!patch.applied) {
            patch.Apply();
        }
    }

    if (!dye_patches_.empty()) {
        dyes_applied_ = true;
    }
}

void PatchManager::RestoreWaxPatches() {
    if (!wax_applied_) return;

    for (auto& patch : wax_patches_) {
        if (patch.address && patch.applied) {
            patch.Restore();
        }
    }
    wax_applied_ = false;
}

void PatchManager::RestoreCandlePatches() {
    if (!candles_applied_) return;

    for (auto& patch : candle_patches_) {
        if (patch.address && patch.applied) {
            patch.Restore();
        }
    }
    candles_applied_ = false;
}

void PatchManager::RestorePlantPatches() {
    if (!plants_applied_) return;

    for (auto& patch : plant_patches_) {
        if (patch.address && patch.applied) {
            patch.Restore();
        }
    }
    plants_applied_ = false;
}

void PatchManager::RestoreDyePatches() {
    if (!dyes_applied_) return;

    for (auto& patch : dye_patches_) {
        if (patch.address && patch.applied) {
            patch.Restore();
        }
    }
    dyes_applied_ = false;
}

}}
