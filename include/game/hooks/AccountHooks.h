#pragma once

#include <string>
#include <cstdint>

namespace tsm { namespace game { namespace hooks { namespace account {

bool Install();

const std::string& GetCapturedUserId();
const std::string& GetCapturedSessionId();
const std::string& GetCapturedUserAgent();

}}}}
