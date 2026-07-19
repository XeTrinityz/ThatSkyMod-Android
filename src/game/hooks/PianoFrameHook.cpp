#include <game/hooks/PianoFrameHook.h>
#include <features/music/InstrumentManager.h>
#include <utils/hooking/dobby_wrapper.h>
#include <game/memory/offsets.h>
#include <game/memory/Address.h>
#include <utils/logging/log.h>

namespace tsm { namespace game { namespace hooks { namespace pianoframe {

namespace {
    using PianoFrameFn = int (*)(std::uintptr_t a1, std::uintptr_t a2);
    PianoFrameFn s_origPianoFrame = nullptr;

    int PianoFrame_Hook(std::uintptr_t a1, std::uintptr_t a2) {
        std::uint8_t pressFlag = *reinterpret_cast<std::uint8_t*>(a2 + 40);
        if (pressFlag & 4) {
            std::uintptr_t keyPtr = *reinterpret_cast<std::uintptr_t*>(a1 + 8);
            std::uintptr_t playerData = *reinterpret_cast<std::uintptr_t*>(a1 + 0x28);
            int noteId = *reinterpret_cast<int*>(a1 + 0x10);
            tsm::features::music::InstrumentManager::CaptureKeyData(noteId, keyPtr, playerData);
        }
        if (s_origPianoFrame) {
            return s_origPianoFrame(a1, a2);
        }
        return 0;
    }
}

bool Install() {
    if (tsm::game::memory::GetBase() == 0) {
        tsm::log::e("PianoFrameHook: Module base not initialized");
        return false;
    }
    if (!tsm::utils::hooking::install_rva("PianoFrame",
                                          tsm::game::Offsets::kPianoFrame,
                                          reinterpret_cast<void*>(PianoFrame_Hook),
                                          reinterpret_cast<void**>(&s_origPianoFrame))) {
        tsm::log::e("PianoFrameHook: Install failed");
        return false;
    }
    tsm::log::i("PianoFrameHook: Installed - capturing key data for InstrumentManager");
    return true;
}

}}}}
