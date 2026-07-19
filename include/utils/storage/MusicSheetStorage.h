#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <Cipher/CipherUtils.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <ctime>
#include <sstream>
#include <iomanip>

namespace tsm { namespace utils { namespace storage {

struct MusicNote {
    int time;
    int keyIndex;
};

struct MusicSheet {
    std::string name;
    int bpm;
    std::vector<MusicNote> notes;
    std::string filename;
};

inline std::string GetMusicSheetsDir() {
    const char* cfgDir = CipherUtils::get_ConfigsPath();
    std::string baseDir = (cfgDir && *cfgDir) ? std::string(cfgDir)
                                               : std::string("/data/data/git.artdeell.skymodloader/files");
    return baseDir + "/Music Sheets";
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

inline bool HasSupportedExtension(const std::string& filename) {
    if (filename.length() < 5) {
        return false;
    }

    std::string ext = filename.substr(filename.length() - 5);
    if (ext == ".json") {
        return true;
    }

    if (filename.length() >= 4) {
        ext = filename.substr(filename.length() - 4);
        if (ext == ".txt") {
            return true;
        }
    }

    return false;
}

inline int ParseKeyIndex(const std::string& keyStr) {
    size_t keyPos = keyStr.find("Key");
    if (keyPos == std::string::npos) {
        return -1;
    }

    std::string numStr = keyStr.substr(keyPos + 3);
    if (numStr.empty()) {
        return -1;
    }

    try {
        return std::stoi(numStr);
    } catch (...) {
        return -1;
    }
}

inline bool LoadMusicSheet(const std::string& filepath, MusicSheet& sheet) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) {
        return false;
    }

    try {
        nlohmann::json j;
        ifs >> j;
        ifs.close();

        if (j.is_array() && !j.empty()) {
            j = j[0];
        }

        if (!j.is_object()) {
            return false;
        }

        if (j.contains("bpm") && j["bpm"].is_number()) {
            sheet.bpm = j["bpm"].get<int>();
        }

        sheet.notes.clear();
        if (j.contains("songNotes") && j["songNotes"].is_array()) {
            for (const auto& jNote : j["songNotes"]) {
                if (!jNote.is_object()) continue;

                MusicNote note{};
                bool hasTime = false;
                bool hasValidKey = false;

                if (jNote.contains("time") && jNote["time"].is_number()) {
                    note.time = jNote["time"].get<int>();
                    hasTime = true;
                }

                if (jNote.contains("key") && jNote["key"].is_string()) {
                    std::string keyStr = jNote["key"].get<std::string>();
                    note.keyIndex = ParseKeyIndex(keyStr);

                    if (note.keyIndex >= 0 && note.keyIndex <= 14) {
                        hasValidKey = true;
                    }
                }

                if (hasTime && hasValidKey) {
                    sheet.notes.push_back(note);
                }
            }
        }

        size_t lastSlash = filepath.find_last_of("/\\");
        sheet.filename = (lastSlash != std::string::npos)
                        ? filepath.substr(lastSlash + 1)
                        : filepath;

        sheet.name = sheet.filename;
        size_t lastDot = sheet.name.find_last_of(".");
        if (lastDot != std::string::npos) {
            sheet.name = sheet.name.substr(0, lastDot);
        }

        return true;
    } catch (...) {
        return false;
    }
}

inline bool ImportMusicSheetFromFd(int fd, std::string& outError, const std::string& originalFilename = "") {
    if (fd < 0) {
        outError = "Invalid file descriptor";
        return false;
    }

    if (!originalFilename.empty()) {
        bool validExtension = false;

        if (originalFilename.length() >= 5) {
            std::string ext = originalFilename.substr(originalFilename.length() - 5);
            if (ext == ".json") {
                validExtension = true;
            }
        }

        if (!validExtension && originalFilename.length() >= 4) {
            std::string ext = originalFilename.substr(originalFilename.length() - 4);
            if (ext == ".txt") {
                validExtension = true;
            }
        }

        if (!validExtension) {
            outError = "Invalid file type. Only .json and .txt files are supported";
            close(fd);
            return false;
        }
    }

    std::string musicDir = GetMusicSheetsDir();
    if (!EnsureDirectoryExists(musicDir)) {
        outError = "Failed to create Music Sheets directory";
        close(fd);
        return false;
    }

    off_t fileSize = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    std::string content;
    if (fileSize > 0 && fileSize < 10 * 1024 * 1024) {
        content.reserve(fileSize);
    }

    char buffer[8192];
    ssize_t bytesRead;

    while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0) {
        content.append(buffer, bytesRead);
    }
    close(fd);

    if (bytesRead < 0) {
        outError = "Failed to read file";
        return false;
    }

    if (content.empty()) {
        outError = "File is empty";
        return false;
    }

    size_t start = 0;
    size_t end = content.length();

    while (start < end && (content[start] == ' ' || content[start] == '\t' ||
                          content[start] == '\n' || content[start] == '\r' ||
                          content[start] == '\0')) {
        start++;
    }

    while (end > start && (content[end-1] == ' ' || content[end-1] == '\t' ||
                          content[end-1] == '\n' || content[end-1] == '\r' ||
                          content[end-1] == '\0')) {
        end--;
    }

    if (start >= end) {
        outError = "File contains only whitespace";
        return false;
    }

    content = content.substr(start, end - start);

    std::string cleanContent;
    cleanContent.reserve(content.length());
    for (size_t i = 0; i < content.length(); i++) {
        unsigned char c = content[i];
        if ((c >= 32 && c < 127) || c == '\n' || c == '\r' || c == '\t') {
            cleanContent += c;
        }
    }

    if (cleanContent.empty()) {
        outError = "File contains no valid characters";
        return false;
    }

    std::string sheetName;
    try {
        nlohmann::json j = nlohmann::json::parse(cleanContent);

        if (j.is_array() && !j.empty()) {
            j = j[0];
        }

        if (!j.is_object()) {
            outError = "Invalid JSON structure";
            return false;
        }

        if (!j.contains("songNotes") || !j["songNotes"].is_array()) {
            outError = "Missing or invalid 'songNotes' field";
            return false;
        }

        if (j.contains("name") && j["name"].is_string()) {
            sheetName = j["name"].get<std::string>();

            for (char& c : sheetName) {
                if (c == '/' || c == '\\' || c == ':' || c == '*' ||
                    c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
                    c = '_';
                }
            }
        }
    } catch (const nlohmann::json::exception& e) {
        outError = std::string("JSON parse error: ") + e.what();
        return false;
    } catch (...) {
        outError = "Unknown error parsing JSON";
        return false;
    }

    content = cleanContent;

    std::string filename;
    if (!sheetName.empty()) {
        filename = sheetName;
    } else {
        std::time_t now = std::time(nullptr);
        std::ostringstream oss;
        oss << "imported_" << std::put_time(std::localtime(&now), "%Y%m%d_%H%M%S");
        filename = oss.str();
    }

    if (filename.length() < 5 || filename.substr(filename.length() - 5) != ".json") {
        filename += ".json";
    }

    std::string filepath = musicDir + "/" + filename;
    int suffix = 1;
    while (access(filepath.c_str(), F_OK) == 0) {
        std::string baseName = filename.substr(0, filename.length() - 5);
        filepath = musicDir + "/" + baseName + "_" + std::to_string(suffix) + ".json";
        suffix++;
    }

    std::ofstream ofs(filepath);
    if (!ofs.is_open()) {
        outError = "Failed to create output file";
        return false;
    }

    ofs << content;
    ofs.close();

    if (ofs.fail()) {
        outError = "Failed to write file content";
        return false;
    }

    return true;
}

inline bool DeleteMusicSheet(const std::string& filename) {
    std::string musicDir = GetMusicSheetsDir();
    std::string filepath = musicDir + "/" + filename;

    if (unlink(filepath.c_str()) == 0) {
        return true;
    }
    return false;
}

inline bool RenameMusicSheet(const std::string& oldFilename, const std::string& newName, std::string& outError) {
    std::string musicDir = GetMusicSheetsDir();
    std::string oldPath = musicDir + "/" + oldFilename;

    std::string cleanName = newName;
    for (char& c : cleanName) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
    }

    if (cleanName.empty()) {
        outError = "Name cannot be empty";
        return false;
    }

    std::string extension = ".json";
    if (oldFilename.length() >= 4) {
        std::string oldExt = oldFilename.substr(oldFilename.length() - 4);
        if (oldExt == ".txt") {
            extension = ".txt";
        } else if (oldFilename.length() >= 5) {
            oldExt = oldFilename.substr(oldFilename.length() - 5);
            if (oldExt == ".json") {
                extension = ".json";
            }
        }
    }

    std::string newFilename = cleanName;
    if (extension == ".json") {
        if (newFilename.length() < 5 || newFilename.substr(newFilename.length() - 5) != ".json") {
            newFilename += ".json";
        }
    } else {
        if (newFilename.length() < 4 || newFilename.substr(newFilename.length() - 4) != ".txt") {
            newFilename += ".txt";
        }
    }

    std::string newPath = musicDir + "/" + newFilename;

    if (oldFilename != newFilename && access(newPath.c_str(), F_OK) == 0) {
        outError = "A sheet with this name already exists";
        return false;
    }

    if (rename(oldPath.c_str(), newPath.c_str()) == 0) {
        return true;
    }

    outError = "Failed to rename file";
    return false;
}

inline std::vector<MusicSheet> LoadAllMusicSheets() {
    std::vector<MusicSheet> sheets;
    std::string musicDir = GetMusicSheetsDir();

    if (!EnsureDirectoryExists(musicDir)) {
        return sheets;
    }

    DIR* dir = opendir(musicDir.c_str());
    if (!dir) {
        return sheets;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string filename = entry->d_name;

        if (filename == "." || filename == "..") {
            continue;
        }

        if (HasSupportedExtension(filename)) {
            std::string filepath = musicDir + "/" + filename;
            MusicSheet sheet;

            if (LoadMusicSheet(filepath, sheet)) {
                sheets.push_back(sheet);
            }
        }
    }

    closedir(dir);
    return sheets;
}

}}}
