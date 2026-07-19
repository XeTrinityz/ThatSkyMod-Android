#pragma once

#include <cstdint>

namespace tsm { namespace game { namespace hooks { namespace progression {

bool Install();

std::uintptr_t GetWingBuffBarn();
std::uintptr_t GetFirstWing();
int* GetWingCountPtr();

std::uintptr_t GetRadianceBarn();
std::uintptr_t GetFirstDye();
int* GetDyeCountPtr();

}}}}
