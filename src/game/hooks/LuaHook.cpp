#include <game/hooks/LuaHook.h>
#include <utils/hooking/dobby_wrapper.h>
#include <game/memory/offsets.h>
#include <game/memory/Address.h>
#include <game/memory/api.h>
#include <game/interop/LuaScriptQueue.h>
#include <progression/WaxRunner.h>
#include <utils/logging/log.h>
#include <atomic>

namespace tsm { namespace game { namespace hooks { namespace lua {

namespace {
    using LuaPushLightUserDataFn = long long(*)(void*, void*);

    LuaPushLightUserDataFn s_origLuaPushLightUserData = nullptr;

    extern "C" long long LuaPushLightUserData_Hook(void* a1, void* a2) {
        static std::atomic<int> s_callCount{0};
        static constexpr int kPumpEvery = 50;

        int current = s_callCount.fetch_add(1, std::memory_order_relaxed) + 1;

        if (current > kPumpEvery) {
            s_callCount.store(0, std::memory_order_relaxed);

            void* L = tsm::game::api::LuaState();
            if (L) {
                tsm::lua::queue::ProcessNext(reinterpret_cast<std::uintptr_t>(L));
            } else {
                s_callCount.store(current, std::memory_order_relaxed);
            }
        }

        tsm::progression::WaxRunner::Get().Tick();

        return s_origLuaPushLightUserData ? s_origLuaPushLightUserData(a1, a2) : 0LL;
    }
}

bool Install() {
    if (tsm::game::memory::GetBase() == 0) {
        tsm::log::e("LuaHook: Module base not initialized");
        return false;
    }

    if (!tsm::utils::hooking::install_rva("LuaPushLightUserData",
                                 tsm::game::Offsets::kLuaPushLightUserData,
                                 (void*)LuaPushLightUserData_Hook,
                                 (void**)&s_origLuaPushLightUserData)) {
        tsm::log::e("LuaHook: LuaPushLightUserData hook failed");
        return false;
    }

    tsm::log::i("LuaHook: Installed successfully");
    return true;
}

}}}}
