#pragma once

#include <imgui/imgui.h>

namespace tsm { namespace ui { namespace widgets {


enum class PlaybackAction {
    None,
    Play,
    Pause,
    Stop,
    Previous,
    Next
};

PlaybackAction DrawPlaybackControls(bool isPlaying, bool isPaused, float progress,
                                     const char* songName = nullptr,
                                     int currentTimeMs = 0, int totalTimeMs = 0,
                                     bool canNavigate = true,
                                     bool* autoPlayNext = nullptr, bool* shuffle = nullptr);

}}}
