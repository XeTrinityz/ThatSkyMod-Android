#include <game/hooks/MusicRecordHook.h>
#include <utils/hooking/dobby_wrapper.h>
#include <game/memory/offsets.h>
#include <game/memory/Address.h>
#include <game/memory/Memory.h>
#include <utils/logging/log.h>

#include <cstdint>
#include <atomic>
#include <chrono>
#include <mutex>

namespace tsm { namespace game { namespace hooks { namespace musicrecord {

namespace {
    using PlayPianoSoundFn = void(*)(std::int64_t , std::int64_t , std::int64_t* , char* );

    PlayPianoSoundFn s_origPlayPianoSound = nullptr;

    std::vector<NoteEvent> s_notes;
    std::mutex s_notesMutex;
    std::atomic<bool> s_isRecording{false};
    std::atomic<bool> s_recordingStarted{false};
    std::chrono::steady_clock::time_point s_startTime;

    extern "C" void PlayPianoSound_Hook(std::int64_t a1, std::int64_t a2, std::int64_t* a3, char* a4) {
        int keyIndex = 0;
        try {
            std::uintptr_t base = static_cast<std::uintptr_t>(a1);
            int* keyPtr = reinterpret_cast<int*>(base + 0x18);
            if (keyPtr) {
                keyIndex = *keyPtr;
            }
        } catch (...) {
        }

        auto now = std::chrono::steady_clock::now();

        if (!s_isRecording.load(std::memory_order_relaxed)) {
            if (s_origPlayPianoSound) {
                s_origPlayPianoSound(a1, a2, a3, a4);
            }
            return;
        }

        {
            std::lock_guard<std::mutex> lock(s_notesMutex);
            if (!s_recordingStarted.load(std::memory_order_relaxed)) {
                s_recordingStarted.store(true, std::memory_order_relaxed);
                s_startTime = now;
            }

            int timestamp = static_cast<int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(now - s_startTime).count());

            s_notes.push_back({timestamp, keyIndex});
        }

        if (s_origPlayPianoSound) {
            s_origPlayPianoSound(a1, a2, a3, a4);
        }
    }
}

bool Install() {
    if (tsm::game::memory::GetBase() == 0) {
        tsm::log::e("MusicRecordHook: Module base not initialized");
        return false;
    }

    if (!tsm::utils::hooking::install_rva("SharedMemoryPlayPianoSound",
                                 tsm::game::Offsets::kSharedMemoryPlayPianoSound,
                                 (void*)PlayPianoSound_Hook,
                                 (void**)&s_origPlayPianoSound)) {
        tsm::log::e("MusicRecordHook: SharedMemoryPlayPianoSound hook failed");
        return false;
    }

    tsm::log::i("MusicRecordHook: Installed successfully");
    return true;
}

const std::vector<NoteEvent>& GetRecordedNotes() {
    return s_notes;
}

void ClearRecording() {
    std::lock_guard<std::mutex> lock(s_notesMutex);
    s_notes.clear();
    s_recordingStarted.store(false, std::memory_order_relaxed);
}

void StartRecording() {
    std::lock_guard<std::mutex> lock(s_notesMutex);
    s_notes.clear();
    s_recordingStarted.store(false, std::memory_order_relaxed);
    s_isRecording.store(true, std::memory_order_relaxed);
}

void StopRecording() {
    s_isRecording.store(false, std::memory_order_relaxed);
}

bool IsRecording() {
    return s_isRecording.load(std::memory_order_relaxed);
}

}}}}
