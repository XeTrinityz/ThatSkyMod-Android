#include <game/interop/lua_bridge.h>

#include <string>
#include <cstdint>
#include <game/memory/Address.h>
#include <game/memory/offsets.h>
#include <game/memory/api.h>

namespace tsm::lua::bridge {

namespace {
    using DebugDoStringFn = int(*)(void* , const char* );
}

int Run(const char* script, std::string& out, int* outStatus)
{
    int rc = -1;
    out.clear();

    if (!script || !*script) {
        out = "empty script";
        if (outStatus) *outStatus = rc;
        return rc;
    }

    void* L = tsm::game::api::LuaState();
    if (!L) {
        out = "lua_state is null";
        if (outStatus) *outStatus = rc;
        return rc;
    }

    std::uintptr_t abs = tsm::game::memory::GetBase() + tsm::game::Offsets::kLuaDebugDoString;
    if (!abs) {
        out = "DoString address is null";
        if (outStatus) *outStatus = rc;
        return rc;
    }

    auto fn = reinterpret_cast<DebugDoStringFn>(abs);

    rc = fn(L, script);
    out = (rc == 0) ? "ok" : "lua error";
    if (outStatus) *outStatus = rc;
    return rc;
}

}
