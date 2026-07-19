#pragma once

#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <array>

namespace tsm { namespace features { namespace music {

class InstrumentManager {
public:
    static constexpr int kMaxKeys = 15;

    static void CaptureKeyData(int noteId, std::uintptr_t keyPtr, std::uintptr_t playerData);

    static std::uintptr_t GetKeyPointer(int noteId);

    static std::uintptr_t GetPlayerData();

    static void UpdateKeyInstrumentData(std::uintptr_t keyPtr, std::uintptr_t playerData, int noteId);

    static bool IsKeyValid(std::uintptr_t keyPtr);

    static std::mutex* GetKeyMutex(int noteId);

    static bool IsInitialized();

private:
    static std::map<int, std::uintptr_t> s_keyPointers;
    static std::mutex s_mutex;
    static std::atomic<std::uintptr_t> s_playerData;
    static std::array<std::mutex, kMaxKeys> s_keyMutexes;
};

}}}
