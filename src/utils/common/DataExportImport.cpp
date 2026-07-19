#include <utils/common/DataExportImport.h>
#include <utils/common/AccountHelper.h>
#include <state/ModState.h>
#include <ui/core/Localization.h>
#include <Cipher/CipherUtils.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <functional>

namespace tsm {
    namespace ui {
        namespace tabs {
            void InvalidateCategoryCache();
        }
    }
}

namespace tsm {
    namespace utils {

        namespace {

            std::string GetConfigDir() {
                const char* cfg = CipherUtils::get_ConfigsPath();
                return (cfg && *cfg) ? std::string(cfg) : std::string("/data/data/git.artdeell.skymodloader/files/config");
            }

            std::string GetTranslationsDir() {
                return GetConfigDir() + "/Translations";
            }

            std::string GetLocationsDir() {
                return GetConfigDir() + "/Locations";
            }

            std::string GetMusicSheetsDir() {
                return GetConfigDir() + "/Music Sheets";
            }

            bool EnsureDir(const std::string& path) {
                struct stat st;
                if (stat(path.c_str(), &st) == 0) {
                    return S_ISDIR(st.st_mode);
                }
                if (mkdir(path.c_str(), 0755) == 0) {
                    return true;
                }
                return (errno == EEXIST);
            }

            bool CopyFileContents(const std::string& src, const std::string& dst, std::string* outError) {
                std::ifstream ifs(src, std::ios::binary);
                if (!ifs.is_open()) {
                    if (outError) *outError = "Cannot open: " + src;
                    return false;
                }
                std::ofstream ofs(dst, std::ios::binary);
                if (!ofs.is_open()) {
                    if (outError) *outError = "Cannot create: " + dst;
                    return false;
                }
                ofs << ifs.rdbuf();
                ifs.close();
                ofs.close();
                if (ofs.fail()) {
                    if (outError) *outError = "Failed to write: " + dst;
                    return false;
                }
                return true;
            }

            bool CopyDirFiles(const std::string& srcDir, const std::string& dstDir,
                const std::function<bool(const std::string&)>& filter,
                std::string* outError) {
                DIR* dir = opendir(srcDir.c_str());
                if (!dir) {
                    if (outError) *outError = "Cannot open dir: " + srcDir;
                    return false;
                }
                bool ok = true;
                struct dirent* entry;
                while ((entry = readdir(dir)) != nullptr) {
                    std::string name = entry->d_name;
                    if (name == "." || name == "..") continue;
                    std::string srcPath = srcDir + "/" + name;
                    std::string dstPath = dstDir + "/" + name;
                    struct stat st;
                    if (stat(srcPath.c_str(), &st) != 0) continue;
                    if (!S_ISDIR(st.st_mode)) {
                        if (filter && !filter(name)) continue;
                        if (!CopyFileContents(srcPath, dstPath, outError)) {
                            ok = false;
                            break;
                        }
                    }
                }
                closedir(dir);
                return ok;
            }

        } // namespace

        bool ExportAllToFolder(std::string* outError) {
            std::string base = kExportBaseDir;
            std::string configDir = GetConfigDir();
            std::string accountsDir = GetAccountsDir();
            std::string locationsDir = GetLocationsDir();
            std::string translationsDir = GetTranslationsDir();
            std::string musicDir = GetMusicSheetsDir();

            std::string expConfig = base + "/config";
            std::string expLocations = base + "/Locations";
            std::string expAccounts = base + "/Accounts";
            std::string expTranslations = base + "/Translations";
            std::string expMusic = base + "/MusicSheets";

            if (!EnsureDir(base) || !EnsureDir(expConfig) || !EnsureDir(expLocations) ||
                !EnsureDir(expAccounts) || !EnsureDir(expTranslations) || !EnsureDir(expMusic)) {
                if (outError) *outError = "Failed to create export directory";
                return false;
            }

            std::string modStatePath = configDir + "/mod_state.json";
            if (access(modStatePath.c_str(), F_OK) == 0) {
                if (!CopyFileContents(modStatePath, expConfig + "/mod_state.json", outError))
                    return false;
            }

            std::string locFile = locationsDir + "/saved_locations.json";
            if (access(locFile.c_str(), F_OK) == 0) {
                if (!CopyFileContents(locFile, expLocations + "/saved_locations.json", outError))
                    return false;
            }

            DIR* accDir = opendir(accountsDir.c_str());
            if (accDir) {
                struct dirent* e;
                while ((e = readdir(accDir)) != nullptr) {
                    std::string fn = e->d_name;
                    if (fn.size() > 4 && fn.substr(fn.size() - 4) == ".bin") {
                        if (!CopyFileContents(accountsDir + "/" + fn, expAccounts + "/" + fn, outError)) {
                            closedir(accDir);
                            return false;
                        }
                    }
                }
                closedir(accDir);
            }

            if (access(translationsDir.c_str(), F_OK) == 0) {
                auto jsonFilter = [](const std::string& name) {
                    return name.size() >= 5 && name.substr(name.size() - 5) == ".json";
                    };
                if (!CopyDirFiles(translationsDir, expTranslations, jsonFilter, outError))
                    return false;
            }

            if (access(musicDir.c_str(), F_OK) == 0) {
                auto musicFilter = [](const std::string& name) {
                    if (name == "_categories.json") return false;
                    if (name.size() >= 5 && name.substr(name.size() - 5) == ".json") return true;
                    if (name.size() >= 4 && name.substr(name.size() - 4) == ".txt")  return true;
                    return false;
                    };
                if (!CopyDirFiles(musicDir, expMusic, musicFilter, outError))
                    return false;

                std::string catSrc = musicDir + "/_categories.json";
                if (access(catSrc.c_str(), F_OK) == 0) {
                    if (!CopyFileContents(catSrc, expMusic + "/_categories.json", outError))
                        return false;
                }
            }

            return true;
        }

        bool ImportAllFromFolder(const std::string& folderPath, std::string* outError) {
            std::string configDir = GetConfigDir();
            std::string accountsDir = GetAccountsDir();
            std::string locationsDir = GetLocationsDir();
            std::string translationsDir = GetTranslationsDir();
            std::string musicDir = GetMusicSheetsDir();

            std::string expConfig = folderPath + "/config";
            std::string expLocations = folderPath + "/Locations";
            std::string expAccounts = folderPath + "/Accounts";
            std::string expTranslations = folderPath + "/Translations";
            std::string expMusic = folderPath + "/MusicSheets";

            if (!EnsureDirectoryExists(configDir) || !EnsureDirectoryExists(accountsDir) ||
                !EnsureDirectoryExists(locationsDir) || !EnsureDirectoryExists(translationsDir) ||
                !EnsureDirectoryExists(musicDir)) {
                if (outError) *outError = "Failed to create target directories";
                return false;
            }

            std::string modStateSrc = expConfig + "/mod_state.json";
            if (access(modStateSrc.c_str(), F_OK) == 0) {
                if (!CopyFileContents(modStateSrc, configDir + "/mod_state.json", outError))
                    return false;
                auto& ms = tsm::state::ModState::Get();
                if (!ms.LoadFromFile()) {
                    if (outError) *outError = "Config imported but failed to reload";
                    return false;
                }
            }

            std::string locSrc = expLocations + "/saved_locations.json";
            if (access(locSrc.c_str(), F_OK) == 0) {
                if (!CopyFileContents(locSrc, locationsDir + "/saved_locations.json", outError))
                    return false;
            }

            if (access(expAccounts.c_str(), F_OK) == 0) {
                DIR* dir = opendir(expAccounts.c_str());
                if (dir) {
                    struct dirent* e;
                    while ((e = readdir(dir)) != nullptr) {
                        std::string fn = e->d_name;
                        if (fn.size() > 4 && fn.substr(fn.size() - 4) == ".bin") {
                            if (!CopyFileContents(expAccounts + "/" + fn, accountsDir + "/" + fn, outError)) {
                                closedir(dir);
                                return false;
                            }
                        }
                    }
                    closedir(dir);
                }
            }

            if (access(expTranslations.c_str(), F_OK) == 0) {
                auto jsonFilter = [](const std::string& name) {
                    return name.size() >= 5 && name.substr(name.size() - 5) == ".json";
                    };
                if (!CopyDirFiles(expTranslations, translationsDir, jsonFilter, outError))
                    return false;
                tsm::ui::i18n::ReloadTranslations();
            }

            if (access(expMusic.c_str(), F_OK) == 0) {
                auto musicFilter = [](const std::string& name) {
                    if (name == "_categories.json") return false;
                    if (name.size() >= 5 && name.substr(name.size() - 5) == ".json") return true;
                    if (name.size() >= 4 && name.substr(name.size() - 4) == ".txt")  return true;
                    return false;
                    };
                if (!CopyDirFiles(expMusic, musicDir, musicFilter, outError))
                    return false;

                std::string catSrc = expMusic + "/_categories.json";
                if (access(catSrc.c_str(), F_OK) == 0) {
                    if (!CopyFileContents(catSrc, musicDir + "/_categories.json", outError))
                        return false;
                }

                tsm::ui::tabs::InvalidateCategoryCache();
            }

            return true;
        }

    }
}