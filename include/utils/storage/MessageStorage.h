#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <cstring>
#include <Cipher/CipherUtils.h>

namespace tsm { namespace utils { namespace storage {

struct SavedMessage {
    char name[128];
    char message[512];
};

inline std::string GetMessagesDir() {
    const char* cfgDir = CipherUtils::get_ConfigsPath();
    std::string baseDir = (cfgDir && *cfgDir) ? std::string(cfgDir)
                                              : std::string("/data/data/git.artdeell.skymodloader/files");
    return baseDir + "/Messages";
}

inline bool EnsureMessagesDirectoryExists(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    if (mkdir(path.c_str(), 0755) == 0) {
        return true;
    }
    return (errno == EEXIST);
}

inline bool SaveMessagesToFile(const std::vector<SavedMessage>& messages) {
    std::string msgDir = GetMessagesDir();
    if (!EnsureMessagesDirectoryExists(msgDir)) {
        return false;
    }

    nlohmann::json j;
    j["version"] = 1;
    j["messages"] = nlohmann::json::array();
    for (const auto& msg : messages) {
        nlohmann::json jMsg;
        jMsg["name"] = std::string(msg.name);
        jMsg["message"] = std::string(msg.message);
        j["messages"].push_back(jMsg);
    }

    std::string filePath = msgDir + "/saved_messages.json";
    std::ofstream ofs(filePath);
    if (!ofs.is_open()) {
        return false;
    }
    ofs << j.dump(2);
    ofs.close();
    return true;
}

inline bool LoadMessagesFromFile(std::vector<SavedMessage>& messages) {
    std::string filePath = GetMessagesDir() + "/saved_messages.json";
    std::ifstream ifs(filePath);
    if (!ifs.is_open()) {
        return false;
    }

    try {
        nlohmann::json j;
        ifs >> j;
        ifs.close();

        messages.clear();
        if (j.contains("messages") && j["messages"].is_array()) {
            for (const auto& jMsg : j["messages"]) {
                if (!jMsg.is_object()) continue;
                SavedMessage msg{};
                std::string name = jMsg.value("name", "");
                std::string text = jMsg.value("message", "");
                std::strncpy(msg.name, name.c_str(), sizeof(msg.name) - 1);
                msg.name[sizeof(msg.name) - 1] = '\0';
                std::strncpy(msg.message, text.c_str(), sizeof(msg.message) - 1);
                msg.message[sizeof(msg.message) - 1] = '\0';
                messages.push_back(msg);
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

}}}
