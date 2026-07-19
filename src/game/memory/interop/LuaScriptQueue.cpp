#include <game/interop/LuaScriptQueue.h>
#include <game/interop/lua_bridge.h>
#include <queue>
#include <mutex>
#include <atomic>

namespace tsm { namespace lua { namespace queue {

namespace {
    std::queue<std::string> g_queue;
    std::mutex g_mutex;
    std::atomic<bool> g_processing{false};
    std::atomic<int> g_queuedCount{0};
}

void Enqueue(const std::string& script) {
    if (script.empty()) return;

    std::lock_guard<std::mutex> lock(g_mutex);
    g_queue.push(script);
    g_queuedCount.fetch_add(1, std::memory_order_relaxed);
}

void ProcessNext(std::uintptr_t ) {
    if (g_processing.load(std::memory_order_relaxed)) return;

    std::string script;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_processing.load(std::memory_order_relaxed) || g_queue.empty()) {
            return;
        }

        g_processing.store(true, std::memory_order_relaxed);
        script = std::move(g_queue.front());
        g_queue.pop();
    }

    int prev = g_queuedCount.fetch_sub(1, std::memory_order_relaxed);
    if (prev <= 0) {
        g_queuedCount.store(0, std::memory_order_relaxed);
    }

    if (!script.empty()) {
        std::string output;
        int status = 0;
        tsm::lua::bridge::Run(script.c_str(), output, &status);
    }

    g_processing.store(false, std::memory_order_relaxed);
}

void Clear() {
    std::lock_guard<std::mutex> lock(g_mutex);

    while (!g_queue.empty()) {
        g_queue.pop();
    }

    g_processing.store(false, std::memory_order_relaxed);
    g_queuedCount.store(0, std::memory_order_relaxed);
}

int GetQueuedCount() {
    return g_queuedCount.load(std::memory_order_relaxed);
}

}}}
