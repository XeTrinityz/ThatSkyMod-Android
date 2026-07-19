#pragma once

#include <utils/storage/MusicSheetStorage.h>
#include <vector>

namespace tsm { namespace ui { namespace overlays {


void RenderMusicPlaybackOverlay();

void UpdateMusicPlaybackState(const std::vector<tsm::utils::storage::MusicSheet>& sheets,
                              int selectedIndex);

int GetMusicPlaybackOverlaySelectedIndex();

}}}
