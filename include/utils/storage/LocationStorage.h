#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <Cipher/CipherUtils.h>

namespace tsm { namespace utils { namespace storage {

struct SavedLocation {
    char name[128];
    float x, y, z;
    std::string category;
    std::string level;
    bool builtin;
};

inline std::string GetLocationsDir() {
    const char* cfgDir = CipherUtils::get_ConfigsPath();
    std::string baseDir = (cfgDir && *cfgDir) ? std::string(cfgDir)
                                               : std::string("/data/data/git.artdeell.skymodloader/files");
    return baseDir + "/Locations";
}

inline bool EnsureDirectoryExists(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    if (mkdir(path.c_str(), 0755) == 0) {
        return true;
    }
    return (errno == EEXIST);
}

inline bool SaveLocationsToFile(const std::vector<SavedLocation>& locations,
                                const std::vector<std::string>& categories) {
    std::string locDir = GetLocationsDir();
    if (!EnsureDirectoryExists(locDir)) {
        return false;
    }

    nlohmann::json j;
    j["version"] = 1;

    j["categories"] = nlohmann::json::array();
    for (const auto& cat : categories) {
        j["categories"].push_back(cat);
    }

    j["locations"] = nlohmann::json::array();
    for (const auto& loc : locations) {
        if (loc.builtin) {
            continue;
        }
        nlohmann::json jLoc;
        jLoc["name"] = std::string(loc.name);
        jLoc["x"] = loc.x;
        jLoc["y"] = loc.y;
        jLoc["z"] = loc.z;
        jLoc["category"] = loc.category;
        jLoc["level"] = loc.level;
        j["locations"].push_back(jLoc);
    }

    std::string filePath = locDir + "/saved_locations.json";
    std::ofstream ofs(filePath);
    if (!ofs.is_open()) {
        return false;
    }

    ofs << j.dump(2);
    ofs.close();
    return true;
}

inline bool LoadLocationsFromFile(std::vector<SavedLocation>& locations,
                                  std::vector<std::string>& categories) {
    std::string locDir = GetLocationsDir();
    std::string filePath = locDir + "/saved_locations.json";

    std::ifstream ifs(filePath);
    if (!ifs.is_open()) {
        return false;
    }

    try {
        nlohmann::json j;
        ifs >> j;
        ifs.close();

        categories.clear();
        if (j.contains("categories") && j["categories"].is_array()) {
            for (const auto& cat : j["categories"]) {
                if (cat.is_string()) {
                    categories.push_back(cat.get<std::string>());
                }
            }
        }

        locations.clear();
        if (j.contains("locations") && j["locations"].is_array()) {
            for (const auto& jLoc : j["locations"]) {
                if (!jLoc.is_object()) continue;

                SavedLocation loc{};
                loc.builtin = false;
                if (jLoc.contains("name") && jLoc["name"].is_string()) {
                    std::string name = jLoc["name"].get<std::string>();
                    std::strncpy(loc.name, name.c_str(), sizeof(loc.name) - 1);
                    loc.name[sizeof(loc.name) - 1] = '\0';
                }
                if (jLoc.contains("x") && jLoc["x"].is_number()) loc.x = jLoc["x"].get<float>();
                if (jLoc.contains("y") && jLoc["y"].is_number()) loc.y = jLoc["y"].get<float>();
                if (jLoc.contains("z") && jLoc["z"].is_number()) loc.z = jLoc["z"].get<float>();
                if (jLoc.contains("category") && jLoc["category"].is_string()) {
                    loc.category = jLoc["category"].get<std::string>();
                }
                if (jLoc.contains("level") && jLoc["level"].is_string()) {
                    loc.level = jLoc["level"].get<std::string>();
                }

                locations.push_back(loc);
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

}}}
