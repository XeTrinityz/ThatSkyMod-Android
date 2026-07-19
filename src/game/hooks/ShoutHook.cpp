#include <game/hooks/ShoutHook.h>
#include <utils/hooking/dobby_wrapper.h>
#include <game/memory/offsets.h>
#include <game/memory/Address.h>
#include <game/memory/api.h>
#include <state/ModState.h>
#include <utils/logging/log.h>
#include <arm_neon.h>
#include <random>
#include <mutex>
#include <atomic>

namespace tsm { namespace game { namespace hooks { namespace shout {

namespace {
    using DoShoutFn = std::int64_t(*)(std::uintptr_t , unsigned int, unsigned int,
                                      char, char, int, char, char,
                                      float, float, float, float, float32x4_t , float);

    DoShoutFn s_origDoShout = nullptr;

    struct ShoutEditorState {
        std::atomic<bool> enabled{false};
        bool rainbowMode{false};
        float colorR{1.0f};
        float colorG{1.0f};
        float colorB{1.0f};
        float colorA{1.0f};
        std::uint8_t voiceType{0};
    };

    ShoutEditorState s_editorState;

    struct ShoutColor {
        union {
            float32x4_t vector;
            float rgba[4];
            struct { float r, g, b, a; };
        };
    };

    ShoutColor s_customShoutColor = {{1.0f, 1.0f, 1.0f, 1.0f}};
    std::mt19937 s_rng(std::random_device{}());
    std::mutex s_colorMutex;

    std::uintptr_t ResolvePlayerShout() {
        try {
            auto barn = tsm::game::api::AvatarBarn();
            if (!barn) return 0;
            std::uintptr_t barnAddr = reinterpret_cast<std::uintptr_t>(barn);
            std::uintptr_t fieldAddr = barnAddr + tsm::game::Offsets::kAvatarLocalSlot
                                                + tsm::game::Offsets::kAvatarShout;
            return *reinterpret_cast<std::uintptr_t*>(fieldAddr);
        } catch (...) {
            return 0;
        }
    }

    float32x4_t RandomizeAndGetShoutColor() {
        static const float rainbowHues[7][4] = {
            {1.0f, 0.0f, 0.0f, 1.0f},
            {1.0f, 0.5f, 0.0f, 1.0f},
            {1.0f, 1.0f, 0.0f, 1.0f},
            {0.0f, 1.0f, 0.0f, 1.0f},
            {0.0f, 0.0f, 1.0f, 1.0f},
            {0.3f, 0.0f, 0.5f, 1.0f},
            {0.5f, 0.0f, 0.5f, 1.0f}
        };

        std::lock_guard<std::mutex> lock(s_colorMutex);
        int hueIndex = s_rng() % 7;
        s_customShoutColor.vector = vld1q_f32(rainbowHues[hueIndex]);
        return s_customShoutColor.vector;
    }

    extern "C" std::int64_t DoShout_Hook(std::uintptr_t avatarShout, unsigned int a2, unsigned int a3,
                                          char a4, char a5, int a6, char a7, char a8,
                                          float a9, float a10, float a11, float a12,
                                          float32x4_t shoutColor, float a14) {

        if (s_editorState.enabled.load(std::memory_order_relaxed)) {
            std::uintptr_t playerShout = ResolvePlayerShout();
            bool isPlayerShout = (playerShout != 0 && avatarShout == playerShout);

            if (isPlayerShout) {
                try {
                    if (s_editorState.rainbowMode) {
                        shoutColor = RandomizeAndGetShoutColor();
                    } else {
                        float rgba[4] = {
                            s_editorState.colorR,
                            s_editorState.colorG,
                            s_editorState.colorB,
                            s_editorState.colorA
                        };
                        shoutColor = vld1q_f32(rgba);
                    }
                } catch (...) {
                }
            }
        }

        if (s_origDoShout) {
            return s_origDoShout(avatarShout, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, shoutColor, a14);
        }
        return static_cast<std::int64_t>(avatarShout);
    }
}

bool Install() {
    if (tsm::game::memory::GetBase() == 0) {
        tsm::log::e("ShoutHook: Module base not initialized");
        return false;
    }

    if (!tsm::utils::hooking::install_rva("DoShout",
                                 tsm::game::Offsets::kDoShout,
                                 (void*)DoShout_Hook,
                                 (void**)&s_origDoShout)) {
        tsm::log::e("ShoutHook: DoShout hook failed");
        return false;
    }

    tsm::log::i("ShoutHook: Installed successfully");
    return true;
}

std::uintptr_t GetPlayerShout() {
    return ResolvePlayerShout();
}

bool IsShoutEditorEnabled() {
    return s_editorState.enabled.load(std::memory_order_relaxed);
}

void SetShoutEditorEnabled(bool enabled) {
    s_editorState.enabled.store(enabled, std::memory_order_relaxed);
}

bool IsRainbowModeEnabled() {
    return s_editorState.rainbowMode;
}

void SetRainbowMode(bool enabled) {
    s_editorState.rainbowMode = enabled;
}

void GetShoutColor(float& r, float& g, float& b, float& a) {
    r = s_editorState.colorR;
    g = s_editorState.colorG;
    b = s_editorState.colorB;
    a = s_editorState.colorA;
}

void SetShoutColor(float r, float g, float b, float a) {
    s_editorState.colorR = r;
    s_editorState.colorG = g;
    s_editorState.colorB = b;
    s_editorState.colorA = a;
}

std::uint8_t GetVoiceType() {
    return s_editorState.voiceType;
}

void SetVoiceType(std::uint8_t voice) {
    s_editorState.voiceType = voice;
}

}}}}
