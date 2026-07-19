#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <functional>

namespace tsm { namespace network {

using EdenProgressCallback = std::function<void(float, const char*)>;

class EdenAutomation {
public:
    struct Config {
        bool recollectAfterConvert = false;
        EdenProgressCallback progressCallback = nullptr;
    };

    static void Start(const Config& config);

    static void Stop();

    static bool IsRunning();

    static float GetProgress();

    static std::string GetStatus();

private:
    static std::atomic<bool> s_busy;
    static std::atomic<bool> s_stop;
    static std::mutex s_mu;
    static float s_progress;
    static std::string s_message;

    static void RunThread(const Config& config);
    static void UpdateProgress(float pct, const char* msg);
};

}}
