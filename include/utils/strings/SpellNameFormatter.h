#pragma once

#include <string>
#include <utils/strings/StringUtils.h>

namespace tsm { namespace utils {

inline std::string FormatSpellName(const std::string& internalName) {
    std::string display = internalName;

    ReplaceAll(display, '_', ' ');

    display = CapitalizeWords(display);

    return display;
}

}}
