#pragma once

#include <string>
#include <algorithm>
#include <cstring>
#include <data/DataManager.h>

namespace tsm { namespace utils { namespace cosmetics {

inline std::string FindCosmeticIcon(const char* cosmeticName) {
    if (!cosmeticName || *cosmeticName == '\0') return "";

    const auto& defs = tsm::data::DataManager::Get().GetOutfitDefs();
    if (!defs.is_array()) return "";

    for (const auto& item : defs) {
        if (!item.is_object()) continue;
        if (!item.contains("mesh") || !item["mesh"].is_string()) continue;

        std::string name = item["mesh"].get<std::string>();
        if (name == cosmeticName) {
            if (item.contains("icon") && item["icon"].is_string()) {
                return item["icon"].get<std::string>();
            }
            break;
        }
    }
    return "";
}

inline const char* GetPropIconFallback(const char* propName) {
    if (!propName || *propName == '\0') return "UiMiscQuestion";

    std::string name = propName;
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);

    if (name.find("mantafloat") != std::string::npos) return "UiOutfitPropMantaFloat";
    if (name.find("guitar") != std::string::npos) return "UiOutfitPropAP04Guitar";
    if (name.find("pianoxmas") != std::string::npos) return "UiOutfitPropPianoXmas";
    if (name.find("piano") != std::string::npos) return "UiOutfitPropAP05Piano";
    if (name.find("umbrella") != std::string::npos) return "UiOutfitPropAP03UmbrellaA";
    if (name.find("ukulele") != std::string::npos) return "UiOutfitPropAP04Ukulele";
    if (name.find("kalimba") != std::string::npos) return "UiOutfitPropAP12Kalimba";
    if (name.find("lute") != std::string::npos) return "UiOutfitPropAP09Lute";
    if (name.find("handpan") != std::string::npos) return "UiOutfitPropAP07HandPan";
    if (name.find("xylophone") != std::string::npos) return "UiOutfitPropAP05Xylophone";
    if (name.find("bass") != std::string::npos) return "UiOutfitPropBass";
    if (name.find("ocarina") != std::string::npos) return "UiOutfitPropOcarina";
    if (name.find("bell") != std::string::npos) return "UiOutfitPropAP02BellA";
    if (name.find("flute") != std::string::npos) return "UiOutfitPropAP03LongFlute";
    if (name.find("panflute") != std::string::npos) return "UiOutfitPropAP03PanFlute";
    if (name.find("dundun") != std::string::npos) return "UiOutfitPropAP08Dundun";
    if (name.find("cymbal") != std::string::npos) return "UiOutfitPropAP25Cymbals";
    if (name.find("harmonica") != std::string::npos) return "UiOutfitPropAP24Harmonica";
    if (name.find("bugle") != std::string::npos) return "UiOutfitPropAP10Bugle";
    if (name.find("cello") != std::string::npos) return "UiOutfitPropAP23CelloBasic";
    if (name.find("chime") != std::string::npos) return "UiOutfitPropAP17Chimes";
    if (name.find("microphone") != std::string::npos) return "UiOutfitPropAP16Microphone";
    if (name.find("musicshell") != std::string::npos) return "UiOutfitPropMusicShell";
    if (name.find("hoop") != std::string::npos) return "UiOutfitPropAP10Hoop";
    if (name.find("torch") != std::string::npos) return "UiOutfitPropAP10Torch";
    if (name.find("brazier") != std::string::npos) return "UiOutfitPropAP05Brazier";
    if (name.find("bonfire") != std::string::npos) return "UiOutfitPropAP04Bonfire";
    if (name.find("pumpkinlamp") != std::string::npos) return "UiOutfitPropPumpkinLamp";
    if (name.find("lantern") != std::string::npos || name.find("lamp") != std::string::npos) return "UiOutfitPropAP22Lamp";
    if (name.find("spotlight") != std::string::npos) return "UiOutfitPropAP10Spotlight";
    if (name.find("firework") != std::string::npos) return "UiOutfitPropFireworks";
    if (name.find("camera") != std::string::npos) return "UiOutfitPropAP19UltimateCamera";
    if (name.find("rose") != std::string::npos || name.find("flower") != std::string::npos) return "UiOutfitPropAP11Rose";
    if (name.find("bouquet") != std::string::npos) return "UiOutfitPropAP14VaseBouquet";
    if (name.find("jar") != std::string::npos) return "UiOutfitPropAP10Jar";
    if (name.find("tent") != std::string::npos) return "UiOutfitPropAP10Tent";
    if (name.find("hammock") != std::string::npos) return "UiOutfitPropAP10Hammock";
    if (name.find("pillow") != std::string::npos) return "UiOutfitPropAP10Pillow";
    if (name.find("bookshelf") != std::string::npos || name.find("book") != std::string::npos) return "UiOutfitPropAP10Bookshelf";
    if (name.find("crabfigurine") != std::string::npos) return "UiOutfitPropAP27CrabFigurine";
    if (name.find("crab") != std::string::npos) return "UiOutfitPropAP17Crab";
    if (name.find("foxfigurine") != std::string::npos) return "UiOutfitPropAP11Fox";
    if (name.find("fox") != std::string::npos) return "UiOutfitPropAP11Fox";
    if (name.find("manateefigurine") != std::string::npos) return "UiOutfitPropAP27ManateeFigurine";
    if (name.find("darkhorn") != std::string::npos || name.find("horn") != std::string::npos) return "UiOutfitPropAP15DarkHorn";
    if (name.find("kettle") != std::string::npos) return "UiOutfitPropAP17Kettle";
    if (name.find("clock") != std::string::npos) return "UiOutfitPropAP24Clock";
    if (name.find("chandelier") != std::string::npos) return "UiOutfitPropAP24Chandelier";
    if (name.find("butterfly") != std::string::npos) return "UiOutfitPropAP27ButterflyToy";
    if (name.find("plush") != std::string::npos) return "UiOutfitPropAP24ClassicPlush";
    if (name.find("staff") != std::string::npos) return "UiOutfitPropAP27ShepherdStaff";
    if (name.find("shield") != std::string::npos) return "UiOutfitPropAP27Shield";
    if (name.find("spear") != std::string::npos) return "UiOutfitPropAP27Spear";
    if (name.find("projector") != std::string::npos) return "UiOutfitPropAP27Projector";
    if (name.find("popcorn") != std::string::npos) return "UiOutfitPropAnniversaryPopcorn";
    if (name.find("carpet") != std::string::npos) return "UiOutfitPropAnniversaryCarpet";
    if (name.find("poster") != std::string::npos) return "UiOutfitPropAP27Poster";
    if (name.find("teastall") != std::string::npos) return "UiOutfitPropAP27TeaStall";
    if (name.find("teatable") != std::string::npos) return "UiOutfitPropPastryTeaTable";
    if (name.find("flag") != std::string::npos) return "UiOutfitPropAPFlagAnim";
    if (name.find("cake") != std::string::npos) return "UiOutfitPropBirthdayCakeL";
    if (name.find("vine") != std::string::npos || name.find("plant") != std::string::npos) return "UiOutfitPropAP27PlantVine";

    return "UiMiscQuestion";
}

inline std::string MeshToOutfitName(const char* meshName, const std::string& type) {
    if (!meshName || *meshName == '\0') return "";

    const auto& defs = tsm::data::DataManager::Get().GetOutfitDefs();
    if (defs.is_array()) {
        for (const auto& item : defs) {
            if (!item.is_object()) continue;
            if (!item.contains("mesh") || !item["mesh"].is_string()) continue;

            std::string mesh = item["mesh"].get<std::string>();
            if (mesh == meshName) {
                if (item.contains("name") && item["name"].is_string()) {
                    std::string outfitName = item["name"].get<std::string>();

                    const char* kPrefix = "CharSkyKid_";
                    size_t prefixLen = std::strlen(kPrefix);
                    if (outfitName.size() > prefixLen && outfitName.compare(0, prefixLen, kPrefix) == 0) {
                        return outfitName.substr(prefixLen);
                    }
                    return outfitName;
                }
                break;
            }
        }
    }

    std::string execName = meshName;
    const char* kPrefix = "CharSkyKid_";
    size_t prefixLen = std::strlen(kPrefix);
    if (execName.size() > prefixLen && execName.compare(0, prefixLen, kPrefix) == 0) {
        return execName.substr(prefixLen);
    }

    size_t lastUnderscore = execName.find_last_of('_');
    if (lastUnderscore != std::string::npos && lastUnderscore + 1 < execName.length()) {
        std::string shortName = execName.substr(lastUnderscore + 1);
        std::string typeCapitalized = type;
        if (!typeCapitalized.empty()) {
            typeCapitalized[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(typeCapitalized[0])));
        }
        return typeCapitalized + "_" + shortName;
    }

    return execName;
}

}}}
