#pragma once

#include <string>
#include <cstdint>

namespace tsm { namespace lua { namespace queue {

void Enqueue(const std::string& script);

void ProcessNext(std::uintptr_t lua_state);

void Clear();

int GetQueuedCount();

}}}
