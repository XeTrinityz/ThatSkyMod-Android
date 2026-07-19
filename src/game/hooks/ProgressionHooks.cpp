#include <game/hooks/ProgressionHooks.h>
#include <utils/hooking/dobby_wrapper.h>
#include <game/memory/offsets.h>
#include <game/memory/Address.h>
#include <utils/logging/log.h>
#include <atomic>

namespace tsm { namespace game { namespace hooks { namespace progression {

namespace {
    using WingBuffUpdateFn = void(*)(std::uintptr_t, std::uintptr_t, std::uintptr_t);
    using RadianceBarnFn = void(*)(std::uintptr_t, std::uintptr_t);

    WingBuffUpdateFn s_origWingBuffUpdate = nullptr;
    RadianceBarnFn s_origRadianceBarn = nullptr;

    std::atomic<bool> s_wbuUnhooked{false};
    std::atomic<bool> s_rbUnhooked{false};

    std::uintptr_t s_wingBuffBarn = 0;
    std::uintptr_t s_firstWing = 0;
    int* s_wingCountPtr = nullptr;

    std::uintptr_t s_radianceBarn = 0;
    std::uintptr_t s_firstDye = 0;
    int* s_dyeCountPtr = nullptr;

    extern "C" void WingBuffUpdate_Hook(std::uintptr_t wingBuffBarn, std::uintptr_t game, std::uintptr_t param) {
        bool captured = false;

        if (wingBuffBarn != 0) {
            s_wingBuffBarn = wingBuffBarn;
            s_firstWing = wingBuffBarn + 0x50;
            s_wingCountPtr = reinterpret_cast<int*>(wingBuffBarn + 0x5730);
            captured = true;
        }

        if (s_origWingBuffUpdate) {
            s_origWingBuffUpdate(wingBuffBarn, game, param);
        }

        if (captured) {
            bool expected = false;
            if (s_wbuUnhooked.compare_exchange_strong(expected, true)) {
                void* target = reinterpret_cast<void*>(tsm::game::memory::GetBase() + tsm::game::Offsets::kWingBuffUpdate);
                if (tsm::utils::hooking::uninstall(target)) {
                    tsm::log::i("ProgressionHooks: WingBuffUpdate unhooked after capture");
                    s_origWingBuffUpdate = nullptr;
                } else {
                    tsm::log::w("ProgressionHooks: WingBuffUpdate unhook failed");
                }
            }
        }
    }

    extern "C" void RadianceBarn_Hook(std::uintptr_t radianceBarn, std::uintptr_t game) {
        bool captured = false;

        if (radianceBarn != 0) {
            s_radianceBarn = radianceBarn;
            s_firstDye = radianceBarn + 0x15EFF0;
            s_dyeCountPtr = reinterpret_cast<int*>(radianceBarn + 0x163310);
            captured = true;
        }

        if (s_origRadianceBarn) {
            s_origRadianceBarn(radianceBarn, game);
        }

        if (captured) {
            bool expected = false;
            if (s_rbUnhooked.compare_exchange_strong(expected, true)) {
                void* target = reinterpret_cast<void*>(tsm::game::memory::GetBase() + tsm::game::Offsets::kRadianceBarn);
                if (tsm::utils::hooking::uninstall(target)) {
                    tsm::log::i("ProgressionHooks: RadianceBarn unhooked after capture");
                    s_origRadianceBarn = nullptr;
                } else {
                    tsm::log::w("ProgressionHooks: RadianceBarn unhook failed");
                }
            }
        }
    }
}

bool Install() {
    if (tsm::game::memory::GetBase() == 0) {
        tsm::log::e("ProgressionHooks: Module base not initialized");
        return false;
    }

    bool allOk = true;

    if (!tsm::utils::hooking::install_rva("WingBuffUpdate",
                                 tsm::game::Offsets::kWingBuffUpdate,
                                 (void*)WingBuffUpdate_Hook,
                                 (void**)&s_origWingBuffUpdate)) {
        tsm::log::e("ProgressionHooks: WingBuffUpdate hook failed");
        allOk = false;
    }

    if (!tsm::utils::hooking::install_rva("RadianceBarn",
                                 tsm::game::Offsets::kRadianceBarn,
                                 (void*)RadianceBarn_Hook,
                                 (void**)&s_origRadianceBarn)) {
        tsm::log::e("ProgressionHooks: RadianceBarn hook failed");
        allOk = false;
    }

    if (allOk) {
        tsm::log::i("ProgressionHooks: Installed successfully (will self-unhook after capture)");
    }

    return allOk;
}

std::uintptr_t GetWingBuffBarn() { return s_wingBuffBarn; }
std::uintptr_t GetFirstWing() { return s_firstWing; }
int* GetWingCountPtr() { return s_wingCountPtr; }

std::uintptr_t GetRadianceBarn() { return s_radianceBarn; }
std::uintptr_t GetFirstDye() { return s_firstDye; }
int* GetDyeCountPtr() { return s_dyeCountPtr; }

}}}}
