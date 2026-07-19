#include <utils/hooking/dobby_wrapper.h>
#include <game/memory/Address.h>
#include <utils/logging/log.h>
#include <vector>
#include <mutex>
#include <algorithm>
#include <dlfcn.h>

extern "C" int DobbyHook(void*, void*, void**) __attribute__((weak));
extern "C" int DobbyDestroy(void*) __attribute__((weak));

namespace tsm { namespace utils { namespace hooking {

struct DobbyAPI {
    using HookFn = int(*)(void* function, void* replace, void** origin);
    using DestroyFn = int(*)(void* function);

    HookFn hook{nullptr};
    DestroyFn destroy{nullptr};

    bool ensure_loaded() {
        if (hook && destroy) return true;
#ifdef TSM_HAVE_DOBBY
        if (DobbyHook && DobbyDestroy) {
            tsm::log::d("Dobby: using linked symbols");
            hook = DobbyHook;
            destroy = DobbyDestroy;
            return true;
        }
        tsm::log::e("Dobby: TSM_HAVE_DOBBY defined but symbols not found (link issue)");
        return false;
#else
        if (DobbyHook && DobbyDestroy) {
            tsm::log::d("Dobby: using weakly-linked symbols");
            hook = DobbyHook;
            destroy = DobbyDestroy;
            return true;
        }
        hook = reinterpret_cast<HookFn>(dlsym(RTLD_DEFAULT, "DobbyHook"));
        destroy = reinterpret_cast<DestroyFn>(dlsym(RTLD_DEFAULT, "DobbyDestroy"));
        if (hook && destroy) {
            tsm::log::d("Dobby: resolved via dlsym(RTLD_DEFAULT)");
            return true;
        }
        tsm::log::e("Dobby: not available (no linked symbols; dynamic loading disabled)");
        return false;
#endif
    }
};

static DobbyAPI& dobby() { static DobbyAPI api; return api; }

static std::mutex g_mutex;
static std::vector<HookInfo> g_hooks;

static HookInfo* find_by_target(void* target) {
    auto it = std::find_if(g_hooks.begin(), g_hooks.end(), [&](const HookInfo& h){ return h.target == target; });
    return (it == g_hooks.end()) ? nullptr : &(*it);
}

static HookInfo* find_by_name(const std::string& name) {
    auto it = std::find_if(g_hooks.begin(), g_hooks.end(), [&](const HookInfo& h){ return h.name == name; });
    return (it == g_hooks.end()) ? nullptr : &(*it);
}

bool install(void* target, void* replacement, void** original_out, const char* name)
{
    if (!target || !replacement || !original_out) return false;
    if (!dobby().ensure_loaded()) {
        tsm::log::e("hook::install: Dobby not loaded (name=%s)", name ? name : "");
        return false;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (find_by_target(target)) return false;

    void* origin = nullptr;
    int rc = dobby().hook(target, replacement, &origin);
    if (rc != 0) {
        tsm::log::e("DobbyHook rc=%d target=%p name=%s", rc, target, name ? name : "");
        return false;
    }

    HookInfo h{};
    h.name = name ? name : std::string();
    h.target = target;
    h.replacement = replacement;
    h.original = origin;
    h.rva = 0;
    h.installed = true;
    g_hooks.push_back(h);

    *original_out = origin;
    return true;
}

bool install_rva(const char* name, std::uintptr_t rva, void* replacement, void** original_out)
{
    if (!rva) return false;
    void* target = reinterpret_cast<void*>(tsm::game::memory::GetBase() + rva);
    if (!target) return false;
    tsm::log::d("hook::install_rva name=%s rva=0x%llx target=%p base=0x%llx",
                name ? name : "",
                (unsigned long long)rva, target,
                (unsigned long long)tsm::game::memory::GetBase());
    bool ok = install(target, replacement, original_out, name);
    if (ok) {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (auto* hi = find_by_target(target)) hi->rva = rva;
    }
    return ok;
}

bool uninstall(void* target)
{
    if (!target) return false;
    if (!dobby().ensure_loaded()) return false;

    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = std::find_if(g_hooks.begin(), g_hooks.end(), [&](const HookInfo& h){ return h.target == target; });
    if (it == g_hooks.end()) return false;

    bool ok = (dobby().destroy(target) == 0);
    if (ok) g_hooks.erase(it);
    return ok;
}

bool uninstall_name(const std::string& name)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = std::find_if(g_hooks.begin(), g_hooks.end(), [&](const HookInfo& h){ return h.name == name; });
    if (it == g_hooks.end()) return false;
    void* target = it->target;
    return uninstall(target);
}

void uninstall_all()
{
    if (!dobby().ensure_loaded()) return;
    std::lock_guard<std::mutex> lock(g_mutex);
    for (auto it = g_hooks.begin(); it != g_hooks.end(); ) {
        void* target = it->target;
        if (dobby().destroy(target) == 0) it = g_hooks.erase(it);
        else ++it;
    }
}

bool is_installed(void* target)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return find_by_target(target) != nullptr;
}

bool is_installed_name(const std::string& name)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return find_by_name(name) != nullptr;
}

}}}
