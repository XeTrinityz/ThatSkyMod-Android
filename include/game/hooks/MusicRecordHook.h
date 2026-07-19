#pragma once

#include <vector>

namespace tsm { namespace game { namespace hooks { namespace musicrecord {

struct NoteEvent {
    int time;
    int keyIndex;
};

bool Install();

const std::vector<NoteEvent>& GetRecordedNotes();

void ClearRecording();

void StartRecording();
void StopRecording();
bool IsRecording();

}}}}
