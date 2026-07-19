#pragma once

#include <string>
#include <ctime>
#include <cstdint>

namespace tsm { namespace utils {

inline std::string FormatDateDMY(std::int64_t epochSeconds) {
    if (epochSeconds <= 0) return "N/A";

    time_t tt = static_cast<time_t>(epochSeconds);
    std::tm tmBuf{};

#ifdef _WIN32
    localtime_s(&tmBuf, &tt);
#else
    localtime_r(&tt, &tmBuf);
#endif

    char buf[64];
    std::strftime(buf, sizeof(buf), "%d/%m/%Y", &tmBuf);
    return std::string(buf);
}

}}
