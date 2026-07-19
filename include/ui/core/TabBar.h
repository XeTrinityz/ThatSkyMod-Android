#pragma once

#include <imgui/imgui.h>

namespace tsm { namespace ui {

class TabBar {
public:
    static int Render(int current_tab, const ImVec4& accent_color);

private:
    static constexpr const char* kTabNames[] = {
        "Player", "Social", "Teleport", "Outfits",
        "Unlocks", "Progression", "Music", "World", "Settings"
    };

    static constexpr const char* kTabIcons[] = {
        "UiEmoteStanceHero",
        "UiMenuFriends",
        "UiSocialTeleport",
        "UiOutfitPropHangingMask",
        "UiMenuUnlock",
        "UiMenuBuffDoubleWax",
        "UiMenuBroadcastMusicEnabled",
        "UiMenuCrossPlatformPlayer",
        "UiMenuCog"
    };

    static constexpr int kTabCount = sizeof(kTabNames) / sizeof(kTabNames[0]);
};

}}
