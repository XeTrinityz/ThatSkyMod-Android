#include <features/music/InstrumentManager.h>
#include <game/memory/Memory.h>
#include <game/memory/Address.h>
#include <cstring>

namespace tsm { namespace features { namespace music {

std::map<int, std::uintptr_t> InstrumentManager::s_keyPointers;
std::mutex InstrumentManager::s_mutex;
std::atomic<std::uintptr_t> InstrumentManager::s_playerData{0};
std::array<std::mutex, InstrumentManager::kMaxKeys> InstrumentManager::s_keyMutexes;

void InstrumentManager::CaptureKeyData(int noteId, std::uintptr_t keyPtr, std::uintptr_t playerData) {
    if (noteId < 0 || noteId >= kMaxKeys) return;
    std::lock_guard<std::mutex> lock(s_mutex);
    s_keyPointers[noteId] = keyPtr;
    s_playerData.store(playerData, std::memory_order_release);
}

std::uintptr_t InstrumentManager::GetKeyPointer(int noteId) {
    if (noteId < 0 || noteId >= kMaxKeys) return 0;
    std::lock_guard<std::mutex> lock(s_mutex);
    auto it = s_keyPointers.find(noteId);
    return (it != s_keyPointers.end()) ? it->second : 0;
}

std::uintptr_t InstrumentManager::GetPlayerData() {
    return s_playerData.load(std::memory_order_acquire);
}

void InstrumentManager::UpdateKeyInstrumentData(std::uintptr_t keyPtr, std::uintptr_t playerData, int noteId) {
    if (!keyPtr || !playerData) return;
    if (!IsKeyValid(keyPtr)) return;

    constexpr std::uintptr_t kKeyNoteId = 648;
    constexpr std::uintptr_t kKeyInstWord = 656;
    constexpr std::uintptr_t kKeyInstDword = 676;
    constexpr std::uintptr_t kKeyInstByte = 696;
    constexpr std::uintptr_t kKeyInstZero = 752;
    constexpr std::uintptr_t kPlayerInst1 = 84;
    constexpr std::uintptr_t kPlayerInst2 = 85;
    constexpr std::uintptr_t kPlayerInst3 = 86;
    constexpr std::uintptr_t kPlayerInst4 = 87;

    std::uint32_t noteIdVal = static_cast<std::uint32_t>(noteId);
    tsm::game::memory::WriteBytes(reinterpret_cast<void*>(keyPtr + kKeyNoteId), &noteIdVal, sizeof(noteIdVal));

    std::uint16_t instWord = *reinterpret_cast<std::uint16_t*>(playerData + kPlayerInst1);
    tsm::game::memory::WriteBytes(reinterpret_cast<void*>(keyPtr + kKeyInstWord), &instWord, sizeof(instWord));

    std::uint8_t b3 = *reinterpret_cast<std::uint8_t*>(playerData + kPlayerInst3);
    std::uint32_t u32 = static_cast<std::uint32_t>(b3);
    tsm::game::memory::WriteBytes(reinterpret_cast<void*>(keyPtr + kKeyInstDword), &u32, sizeof(u32));

    std::uint8_t b4 = *reinterpret_cast<std::uint8_t*>(playerData + kPlayerInst4);
    tsm::game::memory::WriteBytes(reinterpret_cast<void*>(keyPtr + kKeyInstByte), &b4, 1);

    std::uint8_t zero = 0;
    tsm::game::memory::WriteBytes(reinterpret_cast<void*>(keyPtr + kKeyInstZero), &zero, 1);
}

bool InstrumentManager::IsKeyValid(std::uintptr_t keyPtr) {
    if (!keyPtr) return false;
    std::uintptr_t vtable = *reinterpret_cast<std::uintptr_t*>(keyPtr);
    if (!vtable) return false;
    std::uintptr_t owner = *reinterpret_cast<std::uintptr_t*>(keyPtr + 8);
    if (!owner) return false;
    return true;
}

std::mutex* InstrumentManager::GetKeyMutex(int noteId) {
    if (noteId < 0 || noteId >= kMaxKeys) return nullptr;
    return &s_keyMutexes[static_cast<size_t>(noteId)];
}

bool InstrumentManager::IsInitialized() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_playerData.load(std::memory_order_acquire) != 0 && !s_keyPointers.empty();
}

}}}
