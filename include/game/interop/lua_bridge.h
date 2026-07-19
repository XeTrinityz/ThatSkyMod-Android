#pragma once

#include <string>

namespace tsm::lua::bridge {

int Run(const char* script, std::string& out, int* outStatus);

}
