#pragma once

#include <utils/storage/MusicSheetStorage.h>
#include <game/hooks/MusicKeyHook.h>
#include <features/music/InstrumentManager.h>
#include <state/ModState.h>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>

namespace tsm { namespace features { namespace music {

class MusicSheetPlayer {
public:
    static MusicSheetPlayer& Get() {
        static MusicSheetPlayer instance;
        return instance;
    }

    void Play(const tsm::utils::storage::MusicSheet& sheet) {
        Stop();

        std::lock_guard<std::mutex> lock(m_mutex);
        m_currentSheet = sheet;
        m_isPlaying.store(true, std::memory_order_relaxed);
        m_isPaused.store(false, std::memory_order_relaxed);
        m_accumulatedTime.store(0.0f, std::memory_order_release);
        {
            std::lock_guard<std::mutex> tlock(m_timingMutex);
            m_startTime = std::chrono::steady_clock::now();
            m_pausedDuration = std::chrono::steady_clock::duration::zero();
        }

        m_playbackThread = std::thread(&MusicSheetPlayer::PlaybackLoop, this);
    }

    void Stop() {
        m_isPlaying.store(false, std::memory_order_relaxed);
        m_isPaused.store(false, std::memory_order_relaxed);

        if (m_playbackThread.joinable()) {
            m_playbackThread.join();
        }
    }

    void Pause() {
        if (!m_isPaused.load(std::memory_order_relaxed) && m_isPlaying.load(std::memory_order_relaxed)) {
            std::lock_guard<std::mutex> lock(m_timingMutex);
            m_isPaused.store(true, std::memory_order_release);
            m_pauseTime = std::chrono::steady_clock::now();
        }
    }

    void Resume() {
        if (m_isPaused.load(std::memory_order_relaxed)) {
            std::lock_guard<std::mutex> lock(m_timingMutex);
            auto now = std::chrono::steady_clock::now();
            m_pausedDuration += now - m_pauseTime;
            m_isPaused.store(false, std::memory_order_release);
        }
    }

    bool IsPlaying() const {
        return m_isPlaying.load(std::memory_order_relaxed);
    }

    bool IsPaused() const {
        return m_isPaused.load(std::memory_order_relaxed);
    }

    float GetProgress() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_currentSheet.notes.empty()) return 0.0f;

        int totalTime = 0;
        for (const auto& note : m_currentSheet.notes) {
            if (note.time > totalTime) {
                totalTime = note.time;
            }
        }

        if (totalTime == 0) return 0.0f;
        return static_cast<float>(GetCurrentTime()) / static_cast<float>(totalTime);
    }

    std::string GetCurrentSheetName() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_currentSheet.name;
    }

    int GetCurrentTime() const {
        return static_cast<int>(GetElapsedTimeMs());
    }

    int GetTotalTime() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_currentSheet.notes.empty()) return 0;

        int maxTime = 0;
        for (const auto& note : m_currentSheet.notes) {
            if (note.time > maxTime) {
                maxTime = note.time;
            }
        }
        float mult = m_bpmMultiplier.load(std::memory_order_acquire);
        return (mult > 0.001f) ? static_cast<int>(maxTime / mult) : maxTime;
    }

    void SetBPMMultiplier(float multiplier) {
        if (multiplier < 0.5f || multiplier > 2.0f) return;

        float currentElapsed = GetElapsedTimeMs();
        {
            std::lock_guard<std::mutex> lock(m_timingMutex);
            auto now = std::chrono::steady_clock::now();
            m_startTime = now;
            m_pausedDuration = std::chrono::steady_clock::duration::zero();
            if (m_isPaused.load(std::memory_order_relaxed)) {
                m_pauseTime = now;
            }
        }
        m_accumulatedTime.store(currentElapsed, std::memory_order_release);
        m_bpmMultiplier.store(multiplier, std::memory_order_release);
    }

    float GetBPMMultiplier() const {
        return m_bpmMultiplier.load(std::memory_order_acquire);
    }

private:
    MusicSheetPlayer() = default;
    ~MusicSheetPlayer() {
        Stop();
    }

    MusicSheetPlayer(const MusicSheetPlayer&) = delete;
    MusicSheetPlayer& operator=(const MusicSheetPlayer&) = delete;

    float GetElapsedTimeMs() const {
        if (!m_isPlaying.load(std::memory_order_relaxed)) return 0.0f;

        float accumulated = m_accumulatedTime.load(std::memory_order_acquire);
        float mult = m_bpmMultiplier.load(std::memory_order_acquire);
        mult = (mult >= 0.5f && mult <= 2.0f) ? mult : 1.0f;

        std::lock_guard<std::mutex> lock(m_timingMutex);
        auto now = std::chrono::steady_clock::now();
        auto refTime = m_isPaused.load(std::memory_order_relaxed) ? m_pauseTime : now;
        auto elapsed = std::chrono::duration<float, std::milli>(refTime - m_startTime - m_pausedDuration).count();
        return accumulated + (elapsed * mult);
    }

    void WaitForNoteTime(float targetMs) {
        constexpr float kSleepGranularityMs = 5.0f;

        while (m_isPlaying.load(std::memory_order_relaxed)) {
            while (m_isPaused.load(std::memory_order_relaxed) && m_isPlaying.load(std::memory_order_relaxed)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if (!m_isPlaying.load(std::memory_order_relaxed)) return;

            float currentMs = GetElapsedTimeMs();
            float timeUntilNote = targetMs - currentMs;
            if (timeUntilNote <= 0.0f) return;

            float sleepMs = std::min(timeUntilNote, kSleepGranularityMs);
            if (sleepMs > 0.0f) {
                std::this_thread::sleep_for(std::chrono::duration<float, std::milli>(sleepMs));
            }
        }
    }

    void PlaybackLoop() {
        const auto& recordedKeys = tsm::game::hooks::musickey::GetRecordedKeys();
        int keyCount = tsm::game::hooks::musickey::GetRecordedKeyCount();

        if (keyCount == 0) {
            m_isPlaying.store(false, std::memory_order_relaxed);
            return;
        }

        size_t currentNoteIndex = 0;

        while (m_isPlaying.load(std::memory_order_relaxed) &&
               currentNoteIndex < m_currentSheet.notes.size()) {

            const auto& note = m_currentSheet.notes[currentNoteIndex];
            float targetMs = static_cast<float>(note.time);

            WaitForNoteTime(targetMs);

            if (!m_isPlaying.load(std::memory_order_relaxed) || m_isPaused.load(std::memory_order_relaxed)) {
                continue;
            }

            float maxSustainMs = -1.0f;
            float mult = m_bpmMultiplier.load(std::memory_order_acquire);
            if (mult <= 0.0f) mult = 1.0f;
            for (size_t i = currentNoteIndex + 1; i < m_currentSheet.notes.size(); ++i) {
                const auto& nextNote = m_currentSheet.notes[i];
                if (nextNote.time > note.time) {
                    float gapMs = static_cast<float>(nextNote.time - note.time);
                    maxSustainMs = gapMs / mult;
                    if (maxSustainMs < 0.0f) maxSustainMs = 0.0f;
                    break;
                }
            }

            if (note.keyIndex >= 0 && note.keyIndex < InstrumentManager::kMaxKeys) {
                std::int64_t pianoButton = 0;
                if (InstrumentManager::IsInitialized()) {
                    std::uintptr_t keyPtr = InstrumentManager::GetKeyPointer(note.keyIndex);
                    std::uintptr_t playerData = InstrumentManager::GetPlayerData();
                    if (keyPtr && InstrumentManager::IsKeyValid(keyPtr)) {
                        InstrumentManager::UpdateKeyInstrumentData(keyPtr, playerData, note.keyIndex);
                        pianoButton = static_cast<std::int64_t>(keyPtr);
                    }
                }
                if (pianoButton == 0 && note.keyIndex < keyCount) {
                    pianoButton = recordedKeys[note.keyIndex];
                }
                if (pianoButton != 0) {
                    float sustain = tsm::state::ModState::Get().ui.musicSustainDuration;
                    if (maxSustainMs >= 0.0f && maxSustainMs < sustain) {
                        sustain = maxSustainMs;
                    }
                    tsm::game::hooks::musickey::PlayKeyWithSustain(pianoButton, sustain);
                }
            }
            currentNoteIndex++;
        }

        m_isPlaying.store(false, std::memory_order_relaxed);
    }

    std::atomic<bool> m_isPlaying{false};
    std::atomic<bool> m_isPaused{false};
    std::atomic<float> m_bpmMultiplier{1.0f};
    std::atomic<float> m_accumulatedTime{0.0f};
    std::thread m_playbackThread;
    mutable std::mutex m_mutex;
    mutable std::mutex m_timingMutex;
    std::chrono::steady_clock::time_point m_startTime;
    std::chrono::steady_clock::time_point m_pauseTime;
    std::chrono::steady_clock::duration m_pausedDuration{0};
    tsm::utils::storage::MusicSheet m_currentSheet;
};

}}}
