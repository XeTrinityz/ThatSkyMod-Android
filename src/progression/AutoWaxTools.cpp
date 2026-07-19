#include <progression/AutoWaxTools.h>

#include <game/memory/Patch.h>
#include <game/memory/Address.h>
#include <game/memory/Memory.h>
#include <game/memory/offsets.h>

#include <vector>

namespace tsm { namespace progression {

namespace {
    using namespace tsm::game;
    using namespace tsm::game::memory;
    using namespace tsm::game::Signatures;

    bool s_autoCollectWax = false;
    bool s_autoBurnCandles = false;
    bool s_autoBurnPlants = false;

    std::vector<Patch> s_acWaxPatches;
    std::vector<Patch> s_abCandlesPatches;
    std::vector<Patch> s_abPlantsPatches;

    void ApplyAutoCollectWaxPatches()
    {
        EnsureInitialized();
        s_acWaxPatches.clear();

        if (CreateNopPatchesForPattern(kAutoCollectWaxPattern, s_acWaxPatches, 1) > 0) {
            for (auto& p : s_acWaxPatches) {
                p.Apply();
            }
            s_autoCollectWax = true;
        } else {
            s_autoCollectWax = false;
        }
    }

    void RestoreAutoCollectWaxPatches()
    {
        for (auto& p : s_acWaxPatches) {
            if (p.address) p.Restore();
        }
        s_acWaxPatches.clear();
        s_autoCollectWax = false;
    }

    void ApplyAutoBurnCandlesPatches()
    {
        EnsureInitialized();
        s_abCandlesPatches.clear();

        if (CreateNopPatchesForPattern(kAutoBurnCandlesPattern, s_abCandlesPatches, 2) > 0) {
            for (auto& p : s_abCandlesPatches) {
                p.Apply();
            }
            s_autoBurnCandles = true;
        } else {
            s_autoBurnCandles = false;
        }
    }

    void RestoreAutoBurnCandlesPatches()
    {
        for (auto& p : s_abCandlesPatches) {
            if (p.address) p.Restore();
        }
        s_abCandlesPatches.clear();
        s_autoBurnCandles = false;
    }

    void ApplyAutoBurnPlantsPatches()
    {
        EnsureInitialized();
        s_abPlantsPatches.clear();

        const std::uint8_t kToInstr[4] = { 0xE0, 0x03, 0x27, 0x1E };
        const std::uintptr_t kTargets[] = {
            Offsets::kAutoBurnPlants1,
            Offsets::kAutoBurnPlants2,
            Offsets::kAutoBurnPlants3
        };

        for (std::uintptr_t rva : kTargets) {
            void* target = RvaToPtr(rva);
            if (!target) continue;

            std::uint32_t cur = *reinterpret_cast<std::uint32_t*>(target);
            if (cur == 0xBD424940u || cur == 0xBD424920u) {
                s_abPlantsPatches.emplace_back(CreatePatch(target, kToInstr, 4));
            }
        }

        if (!s_abPlantsPatches.empty()) {
            for (auto& p : s_abPlantsPatches) {
                p.Apply();
            }
            s_autoBurnPlants = true;
        } else {
            s_autoBurnPlants = false;
        }
    }

    void RestoreAutoBurnPlantsPatches()
    {
        for (auto& p : s_abPlantsPatches) {
            if (p.address) p.Restore();
        }
        s_abPlantsPatches.clear();
        s_autoBurnPlants = false;
    }
}

bool IsAutoCollectWaxEnabled()
{
    return s_autoCollectWax;
}

void SetAutoCollectWaxEnabled(bool enabled)
{
    if (enabled == s_autoCollectWax) return;
    if (enabled) {
        ApplyAutoCollectWaxPatches();
    } else {
        RestoreAutoCollectWaxPatches();
    }
}

bool IsAutoBurnCandlesEnabled()
{
    return s_autoBurnCandles;
}

void SetAutoBurnCandlesEnabled(bool enabled)
{
    if (enabled == s_autoBurnCandles) return;
    if (enabled) {
        ApplyAutoBurnCandlesPatches();
    } else {
        RestoreAutoBurnCandlesPatches();
    }
}

bool IsAutoBurnPlantsEnabled()
{
    return s_autoBurnPlants;
}

void SetAutoBurnPlantsEnabled(bool enabled)
{
    if (enabled == s_autoBurnPlants) return;
    if (enabled) {
        ApplyAutoBurnPlantsPatches();
    } else {
        RestoreAutoBurnPlantsPatches();
    }
}

}}
