#include <ui/core/Localization.h>
#include <Cipher/CipherUtils.h>
#include <nlohmann/json.hpp>

#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

namespace tsm { namespace ui { namespace i18n {

namespace {
    Language g_language = Language::English;
    int g_activeLanguageIndex = 0;

    struct TranslationPackInternal {
        std::string id;
        std::string name;
        std::string author;
        std::string version;
        std::unordered_map<std::string, std::string> entries;
    };

    static std::vector<TranslationPackInternal> g_packs;
    static bool g_packsLoaded = false;

    static const char* kNonTranslatableKeys[] = {
        "THAT SKY MOD",
        "XeTrinityz",
        "Mr Gatto",
        "Vexadros & Gxost",
        "Kiojeen",
        "TheSR",
    };

    bool IsNonTranslatable(const char* key) {
        if (!key) return false;
        for (const char* s : kNonTranslatableKeys) {
            if (std::strcmp(s, key) == 0) {
                return true;
            }
        }
        return false;
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

    inline std::string GetTranslationsDir() {
        const char* cfgDir = CipherUtils::get_ConfigsPath();
        std::string baseDir = (cfgDir && *cfgDir)
            ? std::string(cfgDir)
            : std::string("/data/data/git.artdeell.skymodloader/files");
        return baseDir + "/Translations";
    }

    inline std::string FilenameFromPath(const std::string& filepath) {
        size_t lastSlash = filepath.find_last_of("/\\");
        return (lastSlash != std::string::npos)
               ? filepath.substr(lastSlash + 1)
               : filepath;
    }

    inline std::string StripExtension(const std::string& name) {
        size_t lastDot = name.find_last_of('.');
        if (lastDot == std::string::npos) return name;
        return name.substr(0, lastDot);
    }

    inline void SanitizeFilename(std::string& name) {
        for (char& c : name) {
            if (c == '/' || c == '\\' || c == ':' || c == '*' ||
                c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
                c = '_';
            }
        }
    }

    bool LoadPackFromFile(const std::string& filepath) {
        std::ifstream ifs(filepath.c_str());
        if (!ifs.is_open()) return false;

        nlohmann::json j;
        try {
            ifs >> j;
            ifs.close();
        } catch (...) {
            return false;
        }

        if (!j.is_object()) return false;

        TranslationPackInternal pack;
        std::string filename = FilenameFromPath(filepath);
        pack.id = StripExtension(filename);

        if (auto it = j.find("name"); it != j.end() && it->is_string()) {
            pack.name = it->get<std::string>();
        }
        if (auto it = j.find("author"); it != j.end() && it->is_string()) {
            pack.author = it->get<std::string>();
        }
        if (auto it = j.find("version"); it != j.end() && it->is_string()) {
            pack.version = it->get<std::string>();
        }
        if (pack.name.empty()) {
            pack.name = pack.id;
        }

        const nlohmann::json* entriesJson = nullptr;
        if (auto it = j.find("entries"); it != j.end() && it->is_object()) {
            entriesJson = &(*it);
        } else {
            entriesJson = &j;
        }

        static const char* kMetaKeys[] = { "name", "author", "version", "language", "languageCode" };

        for (auto it = entriesJson->begin(); it != entriesJson->end(); ++it) {
            const std::string& key = it.key();
            bool isMeta = false;
            for (const char* mk : kMetaKeys) {
                if (key == mk) { isMeta = true; break; }
            }
            if (entriesJson == &j && isMeta) continue;

            if (!it.value().is_string()) continue;
            std::string value = it.value().get<std::string>();
            if (!key.empty()) {
                pack.entries[key] = std::move(value);
            }
        }

        if (pack.entries.empty()) return false;
        g_packs.push_back(std::move(pack));
        return true;
    }

    void LoadAllPacksIfNeeded() {
        if (g_packsLoaded) return;
        g_packsLoaded = true;
        g_packs.clear();

        std::string dir = GetTranslationsDir();
        if (!EnsureDirectoryExists(dir)) return;

        DIR* d = opendir(dir.c_str());
        if (!d) return;

        struct dirent* entry = nullptr;
        while ((entry = readdir(d)) != nullptr) {
            std::string filename = entry->d_name;
            if (filename == "." || filename == "..") continue;
            if (filename.length() < 5) continue;
            std::string ext = filename.substr(filename.length() - 5);
            if (ext != ".json") continue;

            std::string filepath = dir + "/" + filename;
            LoadPackFromFile(filepath);
        }
        closedir(d);
    }

    void SetActiveIndexInternal(int index) {
        LoadAllPacksIfNeeded();
        if (index <= 0) {
            g_activeLanguageIndex = 0;
            g_language = Language::English;
            return;
        }

        int packIndex = index - 1;
        if (packIndex < 0 || packIndex >= static_cast<int>(g_packs.size())) {
            g_activeLanguageIndex = 0;
            g_language = Language::English;
            return;
        }

        g_activeLanguageIndex = index;
        g_language = Language::Russian;
    }

    void CopyError(const std::string& src, char* dst, int dstSize) {
        if (!dst || dstSize <= 0) return;
        if (src.empty()) {
            dst[0] = '\0';
            return;
        }
        std::strncpy(dst, src.c_str(), static_cast<size_t>(dstSize - 1));
        dst[dstSize - 1] = '\0';
    }
}


void SetLanguage(Language language) {
    if (language == Language::English) {
        SetActiveIndexInternal(0);
    } else {
        SetActiveIndexInternal(1);
    }
}

Language GetLanguage() {
    return g_language;
}

const char* Tr(const char* key) {
    if (!key || !*key) return key;
    if (IsNonTranslatable(key)) return key;

    LoadAllPacksIfNeeded();
    if (g_activeLanguageIndex <= 0) {
        return key;
    }

    int packIndex = g_activeLanguageIndex - 1;
    if (packIndex < 0 || packIndex >= static_cast<int>(g_packs.size())) {
        return key;
    }

    const auto& pack = g_packs[packIndex];
    auto it = pack.entries.find(std::string(key));
    if (it != pack.entries.end() && !it->second.empty()) {
        return it->second.c_str();
    }
    return key;
}

int GetTranslationPackCount() {
    LoadAllPacksIfNeeded();
    return static_cast<int>(g_packs.size());
}

bool GetTranslationPackInfo(int index, TranslationPackInfo& outInfo) {
    LoadAllPacksIfNeeded();
    if (index < 0 || index >= static_cast<int>(g_packs.size())) return false;
    const auto& pack = g_packs[index];
    outInfo.id      = pack.id.c_str();
    outInfo.name    = pack.name.c_str();
    outInfo.author  = pack.author.empty()  ? nullptr : pack.author.c_str();
    outInfo.version = pack.version.empty() ? nullptr : pack.version.c_str();
    return true;
}

int GetActiveLanguageIndex() {
    return g_activeLanguageIndex;
}

void SetActiveLanguageIndex(int index) {
    SetActiveIndexInternal(index);
}

bool ImportTranslationFromFd(int fd, int* outLanguageIndex, char* outErrorBuf, int errorBufSize) {
    std::string error;
    if (fd < 0) {
        CopyError("Invalid file descriptor", outErrorBuf, errorBufSize);
        return false;
    }

    std::string content;
    char buffer[8192];
    ssize_t bytesRead;
    while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0) {
        content.append(buffer, static_cast<size_t>(bytesRead));
    }
    close(fd);

    if (bytesRead < 0) {
        CopyError("Failed to read file", outErrorBuf, errorBufSize);
        return false;
    }
    if (content.empty()) {
        CopyError("File is empty", outErrorBuf, errorBufSize);
        return false;
    }

    nlohmann::json j;
    try {
        j = nlohmann::json::parse(content);
    } catch (const nlohmann::json::exception& e) {
        CopyError(std::string("JSON parse error: ") + e.what(), outErrorBuf, errorBufSize);
        return false;
    }
    if (!j.is_object()) {
        CopyError("Invalid JSON structure", outErrorBuf, errorBufSize);
        return false;
    }

    std::string name;
    std::string author;
    std::string version;
    if (auto it = j.find("name"); it != j.end() && it->is_string()) {
        name = it->get<std::string>();
    }
    if (auto it = j.find("author"); it != j.end() && it->is_string()) {
        author = it->get<std::string>();
    }
    if (auto it = j.find("version"); it != j.end() && it->is_string()) {
        version = it->get<std::string>();
    }

    const nlohmann::json* entriesJson = nullptr;
    if (auto it = j.find("entries"); it != j.end() && it->is_object()) {
        entriesJson = &(*it);
    } else {
        CopyError("Missing or invalid 'entries' object", outErrorBuf, errorBufSize);
        return false;
    }

    TranslationPackInternal pack;
    std::unordered_map<std::string, std::string> entries;
    for (auto it = entriesJson->begin(); it != entriesJson->end(); ++it) {
        if (!it.value().is_string()) continue;
        std::string key = it.key();
        std::string value = it.value().get<std::string>();
        if (!key.empty()) {
            entries.emplace(std::move(key), std::move(value));
        }
    }
    if (entries.empty()) {
        CopyError("No translation entries found", outErrorBuf, errorBufSize);
        return false;
    }

    if (name.empty()) {
        name = "translation";
    }
    SanitizeFilename(name);
    if (name.empty()) {
        name = "translation";
    }

    std::string dir = GetTranslationsDir();
    if (!EnsureDirectoryExists(dir)) {
        CopyError("Failed to create Translations directory", outErrorBuf, errorBufSize);
        return false;
    }

    std::string filename = name;
    if (filename.length() < 5 || filename.substr(filename.length() - 5) != ".json") {
        filename += ".json";
    }

    std::string filepath = dir + "/" + filename;
    int suffix = 1;
    while (access(filepath.c_str(), F_OK) == 0) {
        std::string base = StripExtension(filename);
        filepath = dir + "/" + base + "_" + std::to_string(suffix) + ".json";
        ++suffix;
    }

    std::ofstream ofs(filepath.c_str());
    if (!ofs.is_open()) {
        CopyError("Failed to create output file", outErrorBuf, errorBufSize);
        return false;
    }
    ofs << content;
    ofs.close();
    if (ofs.fail()) {
        CopyError("Failed to write file content", outErrorBuf, errorBufSize);
        return false;
    }

    LoadAllPacksIfNeeded();
    pack.id = StripExtension(FilenameFromPath(filepath));
    pack.name = name;
    pack.author = author;
    pack.version = version;
    pack.entries = std::move(entries);
    g_packs.push_back(std::move(pack));

    int packIndex = static_cast<int>(g_packs.size()) - 1;
    int langIndex = packIndex + 1;
    SetActiveIndexInternal(langIndex);
    if (outLanguageIndex) {
        *outLanguageIndex = langIndex;
    }

    CopyError("", outErrorBuf, errorBufSize);
    return true;
}

bool DeleteTranslationPack(int packIndex) {
    LoadAllPacksIfNeeded();
    if (packIndex < 0 || packIndex >= static_cast<int>(g_packs.size())) {
        return false;
    }

    std::string dir = GetTranslationsDir();
    std::string filepath = dir + "/" + g_packs[packIndex].id + ".json";
    if (unlink(filepath.c_str()) != 0) {
        return false;
    }

    int deletedLangIndex = packIndex + 1;

    if (g_activeLanguageIndex == deletedLangIndex) {
        if (static_cast<int>(g_packs.size()) == 1) {
            g_activeLanguageIndex = 0;
        } else if (deletedLangIndex == static_cast<int>(g_packs.size())) {
            g_activeLanguageIndex = deletedLangIndex - 1;
        } else {
        }
    } else if (g_activeLanguageIndex > deletedLangIndex) {
        --g_activeLanguageIndex;
    }

    g_packsLoaded = false;
    g_packs.clear();
    LoadAllPacksIfNeeded();
    SetActiveIndexInternal(g_activeLanguageIndex);
    return true;
}

void ReloadTranslations() {
    g_packsLoaded = false;
    g_packs.clear();
    LoadAllPacksIfNeeded();
    SetActiveIndexInternal(g_activeLanguageIndex);
}

}}}
