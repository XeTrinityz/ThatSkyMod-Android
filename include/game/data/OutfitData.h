#pragma once

#include <vector>
#include <string>

namespace tsm { namespace game { namespace data {

static const std::vector<std::string> kOutfitCategories = {
    "Body", "Face", "Feet", "Hair", "Hat", "Horn",
    "Mask", "Neck", "Prop", "Wing"
};

static const std::vector<std::string> kSpecialOutfitTypes = {
    "mask", "face", "horn"
};

}}}
