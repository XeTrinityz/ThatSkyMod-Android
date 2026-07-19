#pragma once

namespace tsm { namespace game { namespace data {

static const char* kStanceNames[] = {
    "Neutral", "Cold Wind", "Hands Out", "Proud", "Sneaky", "Tough", "Solemn",
    "Sassy", "Lazy", "Wise", "Timid", "Nerdy", "Injured", "Sad", "Guard"
};

constexpr int kStanceCount = sizeof(kStanceNames) / sizeof(kStanceNames[0]);

inline const char* GetStanceIcon(unsigned stanceId) {
    switch (stanceId) {
        case 0: return "UiEmoteStanceDefault";
        case 1: return "UiEmoteCold";
        case 2: return "UiEmoteStanceWide";
        case 3: return "UiEmoteStanceHero";
        case 4: return "UiEmoteStanceSneak";
        case 5: return "UiEmoteStanceCrossArm";
        case 6: return "UiEmoteStanceSolemn";
        case 7: return "UiEmoteStanceSassy";
        case 8: return "UiEmoteStanceLazyCool";
        case 9: return "UiEmoteStanceWise";
        case 10: return "UiEmoteStanceTimid";
        case 11: return "UiEmoteStanceNerdy";
        case 12: return "UiEmoteStanceInjured";
        case 13: return "UiEmoteStanceSad";
        case 14: return "UiEmoteStanceGuard";
        default: return "UiEmoteStanceDefault";
    }
}

}}}
