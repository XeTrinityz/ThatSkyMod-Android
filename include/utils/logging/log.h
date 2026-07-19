#pragma once

#include <string>

namespace tsm::log {

void init(const char* tag = "TSM");

void v(const char* fmt, ...);
void d(const char* fmt, ...);
void i(const char* fmt, ...);
void w(const char* fmt, ...);
void e(const char* fmt, ...);

void set_enabled(bool enabled);
bool is_enabled();

}
