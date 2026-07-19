#include <game/hooks/MusicKeyHook.h>
#include <utils/hooking/dobby_wrapper.h>
#include <game/memory/offsets.h>
#include <game/memory/Address.h>
#include <game/memory/Memory.h>
#include <utils/logging/log.h>
#include <atomic>
#include <vector>
#include <algorithm>
#include <mutex>
#include <thread>
#include <chrono>

namespace tsm { namespace game { namespace hooks { namespace musickey {

namespace {
    using PlayMusicKeyFn = void(*)(std::int64_t , std::int64_t );
    using StopNoteFn = std::int64_t(*)(std::int64_t , std::int64_t );

    constexpr int kMaxKeys = 15;
    PlayMusicKeyFn s_origPlayMusicKey = nullptr;
    std::vector<std::int64_t> s_recordedKeys;
    std::mutex s_keysMutex;
    std::atomic<bool> s_recordingComplete{false};

    extern "C" void PlayMusicKey_Hook(std::int64_t pianoButton, std::int64_t arg2) {
        if (!s_recordingComplete.load(std::memory_order_relaxed)) {
            std::lock_guard<std::mutex> lock(s_keysMutex);

            if (s_recordedKeys.size() < kMaxKeys &&
                std::find(s_recordedKeys.begin(), s_recordedKeys.end(), pianoButton) == s_recordedKeys.end()) {
                s_recordedKeys.push_back(pianoButton);
                std::sort(s_recordedKeys.begin(), s_recordedKeys.end());

                tsm::log::i("MusicKeyHook: Recorded key %d/%d: 0x%llX",
                           static_cast<int>(s_recordedKeys.size()),
                           kMaxKeys,
                           static_cast<unsigned long long>(pianoButton));

                if (s_recordedKeys.size() == kMaxKeys) {
                    s_recordingComplete.store(true, std::memory_order_relaxed);
                    tsm::log::i("MusicKeyHook: All %d keys recorded, auto-recording stopped", kMaxKeys);
                }
            }
        }

        if (s_origPlayMusicKey) {
            s_origPlayMusicKey(pianoButton, arg2);
        }
    }
}

bool Install() {
    if (tsm::game::memory::GetBase() == 0) {
        tsm::log::e("MusicKeyHook: Module base not initialized");
        return false;
    }

    if (!tsm::utils::hooking::install_rva("PlayMusicKey",
                                 tsm::game::Offsets::kPlayMusicKey,
                                 (void*)PlayMusicKey_Hook,
                                 (void**)&s_origPlayMusicKey)) {
        tsm::log::e("MusicKeyHook: PlayMusicKey hook failed");
        return false;
    }

    tsm::log::i("MusicKeyHook: Installed successfully - auto-recording up to %d keys", kMaxKeys);
    return true;
}

const std::vector<std::int64_t>& GetRecordedKeys() {
    return s_recordedKeys;
}

int GetRecordedKeyCount() {
    return static_cast<int>(s_recordedKeys.size());
}

void PlayKey(std::int64_t pianoButton) {
    PlayKeyWithSustain(pianoButton, 0.0f);
}

static void StopNote(std::int64_t pianoButton) {
    if (pianoButton == 0) return;
    std::uintptr_t base = tsm::game::memory::GetBase();
    if (base == 0) return;
    std::uintptr_t gamePtr = tsm::game::memory::ReadU64(tsm::game::Offsets::Game);
    if (gamePtr == 0) return;
    auto stopNoteFn = reinterpret_cast<StopNoteFn>(base + tsm::game::Offsets::kStopNote);
    if (stopNoteFn) {
        stopNoteFn(pianoButton, static_cast<std::int64_t>(gamePtr));
    }
}

void PlayKeyWithSustain(std::int64_t pianoButton, float sustainMs) {
    if (!s_origPlayMusicKey || pianoButton == 0) return;

    std::uintptr_t gamePtr = tsm::game::memory::ReadU64(tsm::game::Offsets::Game);
    if (gamePtr == 0) return;

    if (sustainMs <= 0.0f) {
        std::uint8_t pressValue = 1;
        void* visualPressAddr = reinterpret_cast<void*>(pianoButton + 0x302);
        tsm::game::memory::WriteBytes(visualPressAddr, &pressValue, sizeof(pressValue));
        s_origPlayMusicKey(pianoButton, static_cast<std::int64_t>(gamePtr));
        StopNote(pianoButton);
        return;
    }

    std::thread([pianoButton, sustainMs, gamePtr]() {
        std::uint8_t pressValue = 1;
        void* visualPressAddr = reinterpret_cast<void*>(pianoButton + 0x302);
        tsm::game::memory::WriteBytes(visualPressAddr, &pressValue, sizeof(pressValue));
        if (s_origPlayMusicKey) {
            s_origPlayMusicKey(pianoButton, static_cast<std::int64_t>(gamePtr));
        }
        int sleepMs = static_cast<int>(sustainMs);
        if (sleepMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
        }
        StopNote(pianoButton);
    }).detach();
}

}}}}
