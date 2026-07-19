#pragma once

#include <vector>
#include <string>

namespace tsm { namespace game { namespace data {

static const std::vector<std::string> kVoiceNames = {
    "Default", "Bird", "Manta", "Whale", "Ghost", "Crab", "Jellyfish",
    "Kizuna Ai", "Baby", "Night Bird", "Blue Bird", "Aurora [Star Pin]", "Manatee", "Manatee [Star Pin]", "Lighthorn", "Count"
};

constexpr int kVoiceCount = 16;

inline const char* GetVoiceIcon(unsigned voiceId) {
    switch (voiceId) {
        case 1: return "UiEmoteCallBird";
        case 2: return "UiEmoteCallManta";
        case 3: return "UiEmoteCallFish";
        case 4: return "UiEmoteCallGhostManta";
        case 5: return "UiEmoteCallCrab";
        case 6: return "UiEmoteCallJellyfish";
        case 7: return "UiOutfitCapeKizunaAi";
        case 8: return "UiEmoteCallBabyManta";
        case 9: return "UiEmoteCallNightbird";
        case 10: return "UiEmoteCallBluebird";
        case 11: return "UiEmoteCallAurora";
        case 12: return "UiEmoteCallManatee";
        case 13: return "UiMenuManateeCall";
        case 14: return "UiEmoteCallLighthorn";
        default: return "UiEmoteCallDefault";
    }
}

}}}
