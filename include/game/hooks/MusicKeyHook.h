#pragma once

#include <cstdint>
#include <vector>

namespace tsm { namespace game { namespace hooks { namespace musickey {

bool Install();

const std::vector<std::int64_t>& GetRecordedKeys();

int GetRecordedKeyCount();

void PlayKey(std::int64_t pianoButton);

void PlayKeyWithSustain(std::int64_t pianoButton, float sustainMs);

}}}}
