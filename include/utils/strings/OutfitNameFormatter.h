#pragma once

#include <string>
#include <cstring>
#include <utils/strings/StringUtils.h>
#include <game/data/OutfitData.h>

namespace tsm { namespace utils {

inline std::string FormatOutfitName(const std::string& outfitName, const std::string& type) {
    std::string displayName = outfitName;
    std::string typeCapitalized = Capitalize(type);

    displayName = RemovePrefix(displayName, "CharSkyKid_" + typeCapitalized + "_");
    displayName = RemovePrefix(displayName, "CharSkyKid_");
    displayName = RemovePrefix(displayName, typeCapitalized + "_");
    displayName = RemovePrefix(displayName, "Body_");
    displayName = RemovePrefix(displayName, "Char_");

    displayName = RemoveDigits(displayName);

    ReplaceAll(displayName, '_', ' ');

    displayName = InsertSpacesBeforeCaps(displayName);

    displayName = Trim(displayName);

    std::string lowerType = ToLower(typeCapitalized);
    if (displayName.size() > lowerType.size() + 1 && displayName[lowerType.size()] == ' ') {
        bool match = true;
        for (size_t i = 0; i < lowerType.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(displayName[i])) != lowerType[i]) {
                match = false;
                break;
            }
        }
        if (match) {
            displayName.erase(0, lowerType.size() + 1);
        }
    }

    for (const std::string& token : tsm::game::data::kOutfitCategories) {
        size_t len = token.length();
        if (displayName.size() > len && displayName[len] == ' ') {
            if (StartsWithIgnoreCase(displayName, token)) {
                displayName.erase(0, len + 1);
                break;
            }
        }
    }

    if (displayName.size() >= 3 &&
        (displayName[0] == 'A' || displayName[0] == 'a') &&
        (displayName[1] == 'P' || displayName[1] == 'p') &&
        displayName[2] == ' ') {
        displayName.erase(0, 3);
    }

    displayName = RemoveDuplicateSpaces(displayName);

    displayName = CapitalizeWords(displayName);

    return displayName;
}

}}
