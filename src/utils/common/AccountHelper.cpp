#include <utils/common/AccountHelper.h>
#include <Cipher/CipherUtils.h>
#include <game/memory/offsets.h>
#include <game/memory/Address.h>
#include <game/memory/mem.h>

#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <cstring>
#include <algorithm>
#include <memory>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <fcntl.h>

namespace tsm { namespace utils {

namespace {
    constexpr const char* FALLBACK_BASE_PATH = "/data/data/git.artdeell.sml/files";
    constexpr const char* ACCOUNTS_DIR_NAME = "Accounts";
    constexpr const char* EXPORT_ACCOUNTS_DIR = "/sdcard/Download/ThatSkyMod-Accounts";
    constexpr const char* AUTH_INFO_FILENAME = "AccountAuthInfo.bin";
    constexpr const char* PREV_DATA_FILENAME = "PreviousGameData.bin";
    constexpr const char* BIN_EXTENSION = ".bin";
    constexpr size_t FILE_BUFFER_SIZE = 8192;
    constexpr mode_t DIR_PERMISSIONS = 0755;

    constexpr std::uint8_t LOGIN_TYPE_LOCAL = 0;
    constexpr std::uint8_t LOGIN_TYPE_LOGOUT = 12;
    constexpr std::uint8_t LOGIN_TYPE_NINTENDO = 7;
    constexpr std::uint8_t LOADING_TYPE_INIT = 0;
    constexpr std::uint8_t SHOULD_RESTART_YES = 1;

    std::string GetParentDirectory(const std::string& path) {
        size_t lastSlash = path.find_last_of('/');
        if (lastSlash != std::string::npos) {
            return path.substr(0, lastSlash);
        }
        return path;
    }

    std::string GetBaseDirectory() {
        const char* cfgDir = CipherUtils::get_ConfigsPath();
        if (cfgDir && *cfgDir) {
            return GetParentDirectory(std::string(cfgDir));
        }
        return FALLBACK_BASE_PATH;
    }

    class DirectoryHandle {
    public:
        explicit DirectoryHandle(const std::string& path)
            : dir_(opendir(path.c_str())) {}

        ~DirectoryHandle() {
            if (dir_) closedir(dir_);
        }

        DirectoryHandle(const DirectoryHandle&) = delete;
        DirectoryHandle& operator=(const DirectoryHandle&) = delete;

        operator bool() const { return dir_ != nullptr; }
        DIR* get() const { return dir_; }

    private:
        DIR* dir_;
    };

    void SafeStringCopy(char* dest, size_t destSize, const std::string& src) {
        size_t copyLen = std::min(src.length(), destSize - 1);
        std::memcpy(dest, src.c_str(), copyLen);
        dest[copyLen] = '\0';
    }

    bool EndsWith(const std::string& str, const std::string& suffix) {
        if (suffix.length() > str.length()) return false;
        return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
    }

    void* GetAccountBarn() {
        if (tsm::game::memory::GetBase() == 0) {
            tsm::game::memory::InitializeBase();
        }

        void* audience = tsm::game::mem::read_ptr_rva(tsm::game::Offsets::AudienceBarn);
        if (!audience) return nullptr;

        std::uintptr_t ptr_addr = tsm::game::mem::add(audience, tsm::game::Offsets::kAccountBarn);
        void* accountBarn = *reinterpret_cast<void**>(ptr_addr);

        if (!accountBarn) {
            ptr_addr = tsm::game::mem::add(audience, 0x8);
            accountBarn = *reinterpret_cast<void**>(ptr_addr);
        }

        return accountBarn;
    }

    static bool g_fastAccountSwitch = false;

}

std::string GetAccountsDir() {
    return GetBaseDirectory() + "/" + ACCOUNTS_DIR_NAME;
}

std::string GetAccountAuthPath(const char* filename) {
    std::string basePath = GetBaseDirectory();
    if (filename && *filename) {
        return basePath + "/" + filename;
    }
    return basePath;
}

bool DeleteAllAuthBinFiles(std::string* errorMsg) {
    std::string accountDir = GetAccountAuthPath(nullptr);
    DirectoryHandle dir(accountDir);

    if (!dir) {
        if (errorMsg) {
            *errorMsg = "Failed to open account directory: " + accountDir;
        }
        return false;
    }

    struct dirent* entry;
    bool allDeleted = true;

    while ((entry = readdir(dir.get())) != nullptr) {
        std::string filename = entry->d_name;
        if (EndsWith(filename, BIN_EXTENSION)) {
            std::string filePath = accountDir + "/" + filename;
            if (unlink(filePath.c_str()) != 0 && errno != ENOENT) {
                allDeleted = false;
                if (errorMsg && errorMsg->empty()) {
                    *errorMsg = "Failed to delete: " + filename;
                }
            }
        }
    }

    return allDeleted;
}

bool ClearCurrentAccountData(std::string* errorMsg) {
    bool success = true;

    std::string authPath = GetAccountAuthPath(AUTH_INFO_FILENAME);
    if (FileExists(authPath)) {
        if (unlink(authPath.c_str()) != 0) {
            if (errorMsg) *errorMsg = "Failed to delete " + std::string(AUTH_INFO_FILENAME);
            success = false;
        }
    }

    std::string prevDataPath = GetAccountAuthPath(PREV_DATA_FILENAME);
    if (FileExists(prevDataPath)) {
        if (unlink(prevDataPath.c_str()) != 0) {
            if (errorMsg && success) {
                *errorMsg = "Failed to delete " + std::string(PREV_DATA_FILENAME);
            }
            success = false;
        }
    }

    return success;
}

bool ImportAccountFromFd(int fd, std::string& errorMsg, std::string* outAccountName) {
    if (fd < 0) {
        errorMsg = "Invalid file descriptor";
        return false;
    }

    std::string accountsDir = GetAccountsDir();
    if (!EnsureDirectoryExists(accountsDir)) {
        errorMsg = "Failed to create accounts directory: " + accountsDir;
        close(fd);
        return false;
    }

    std::time_t now = std::time(nullptr);
    std::ostringstream oss;
    oss << "ImportedAccount_" << std::put_time(std::localtime(&now), "%Y%m%d_%H%M%S");
    std::string baseName = oss.str();

    std::string accountName = baseName;
    std::string filePath = accountsDir + "/" + accountName + BIN_EXTENSION;
    int suffix = 1;
    while (FileExists(filePath)) {
        accountName = baseName + "_" + std::to_string(suffix);
        filePath = accountsDir + "/" + accountName + BIN_EXTENSION;
        ++suffix;
    }

    int outFd = ::open(filePath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (outFd < 0) {
        errorMsg = "Failed to create account file";
        close(fd);
        return false;
    }

    char buffer[FILE_BUFFER_SIZE];
    ssize_t bytesRead;
    bool ok = true;

    while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0) {
        ssize_t written = write(outFd, buffer, bytesRead);
        if (written != bytesRead) {
            errorMsg = "Failed to write account file";
            ok = false;
            break;
        }
    }

    if (bytesRead < 0) {
        if (errorMsg.empty()) {
            errorMsg = "Failed to read from source file";
        }
        ok = false;
    }

    close(fd);
    close(outFd);

    if (!ok) {
        unlink(filePath.c_str());
        return false;
    }

    if (outAccountName) {
        *outAccountName = accountName;
    }

    return true;
}

bool EnsureDirectoryExists(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }

    if (mkdir(path.c_str(), DIR_PERMISSIONS) == 0 || errno == EEXIST) {
        return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
    }

    return false;
}

bool FileExists(const std::string& path) {
    struct stat st;
    return (stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode));
}

bool CopyFile(const std::string& src, const std::string& dst) {
    std::ifstream srcFile(src, std::ios::binary);
    if (!srcFile) return false;

    std::ofstream dstFile(dst, std::ios::binary);
    if (!dstFile) return false;

    char buffer[FILE_BUFFER_SIZE];
    while (srcFile.read(buffer, FILE_BUFFER_SIZE) || srcFile.gcount() > 0) {
        dstFile.write(buffer, srcFile.gcount());
        if (!dstFile) return false;
    }

    return srcFile.eof() && dstFile.good();
}

bool FilesAreIdentical(const std::string& file1, const std::string& file2) {
    std::ifstream f1(file1, std::ios::binary | std::ios::ate);
    std::ifstream f2(file2, std::ios::binary | std::ios::ate);

    if (!f1 || !f2) return false;

    auto size1 = f1.tellg();
    auto size2 = f2.tellg();
    if (size1 != size2 || size1 < 0) return false;

    f1.seekg(0);
    f2.seekg(0);

    char buffer1[FILE_BUFFER_SIZE];
    char buffer2[FILE_BUFFER_SIZE];

    while (f1 && f2) {
        f1.read(buffer1, FILE_BUFFER_SIZE);
        f2.read(buffer2, FILE_BUFFER_SIZE);

        auto count1 = f1.gcount();
        auto count2 = f2.gcount();

        if (count1 != count2) return false;
        if (count1 > 0 && std::memcmp(buffer1, buffer2, count1) != 0) return false;

        if (count1 < FILE_BUFFER_SIZE) break;
    }

    return true;
}

bool IsCurrentAccount(const std::string& accountName) {
    std::string currentPath = GetAccountAuthPath(AUTH_INFO_FILENAME);
    std::string savedPath = GetAccountsDir() + "/" + accountName + BIN_EXTENSION;

    if (!FileExists(currentPath) || !FileExists(savedPath)) {
        return false;
    }

    return FilesAreIdentical(currentPath, savedPath);
}

bool SaveCurrentAccount(const std::string& accountName, std::string* errorMsg) {
    if (accountName.empty()) {
        if (errorMsg) *errorMsg = "Account name cannot be empty";
        return false;
    }

    std::string accountsDir = GetAccountsDir();
    if (!EnsureDirectoryExists(accountsDir)) {
        if (errorMsg) *errorMsg = "Failed to create accounts directory: " + accountsDir;
        return false;
    }

    std::string srcPath = GetAccountAuthPath(AUTH_INFO_FILENAME);
    if (!FileExists(srcPath)) {
        if (errorMsg) *errorMsg = "Account file not found: " + srcPath;
        return false;
    }

    std::string dstPath = accountsDir + "/" + accountName + BIN_EXTENSION;
    if (!CopyFile(srcPath, dstPath)) {
        if (errorMsg) *errorMsg = "Failed to copy file to: " + dstPath;
        return false;
    }

    return true;
}

bool LoadSavedAccount(const std::string& accountName) {
    if (accountName.empty()) return false;

    std::string accountsDir = GetAccountsDir();
    std::string srcPath = accountsDir + "/" + accountName + BIN_EXTENSION;

    if (!FileExists(srcPath)) return false;

    std::string dstPath = GetAccountAuthPath(AUTH_INFO_FILENAME);
    return CopyFile(srcPath, dstPath);
}

bool DeleteAccount(const std::string& accountName) {
    if (accountName.empty()) return false;

    std::string accountsDir = GetAccountsDir();
    std::string filePath = accountsDir + "/" + accountName + BIN_EXTENSION;

    if (!FileExists(filePath)) return false;

    return (unlink(filePath.c_str()) == 0);
}

void LoadAccountsFromDirectory(std::vector<SavedAccount>& accounts) {
    accounts.clear();

    std::string accountsDir = GetAccountsDir();
    if (!EnsureDirectoryExists(accountsDir)) {
        return;
    }

    DirectoryHandle dir(accountsDir);
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir.get())) != nullptr) {
        std::string filename = entry->d_name;

        if (EndsWith(filename, BIN_EXTENSION)) {
            SavedAccount account;
            std::string accountName = filename.substr(0, filename.length() - 4);
            SafeStringCopy(account.name, sizeof(account.name), accountName);
            accounts.push_back(account);
        }
    }

    std::sort(accounts.begin(), accounts.end(),
        [](const SavedAccount& a, const SavedAccount& b) {
            return std::strcmp(a.name, b.name) < 0;
        });
}

bool ExportAllAccountsToDownloads(std::string* errorMsg) {
    std::string accountsDir = GetAccountsDir();
    if (!EnsureDirectoryExists(accountsDir)) {
        if (errorMsg) {
            *errorMsg = "Failed to access accounts directory: " + accountsDir;
        }
        return false;
    }

    std::string exportDir = EXPORT_ACCOUNTS_DIR;
    std::string parentDir = GetParentDirectory(exportDir);
    if (!EnsureDirectoryExists(parentDir) || !EnsureDirectoryExists(exportDir)) {
        if (errorMsg) {
            *errorMsg = "Failed to create export directory: " + exportDir;
        }
        return false;
    }

    DirectoryHandle dir(accountsDir);
    if (!dir) {
        if (errorMsg) {
            *errorMsg = "Failed to open accounts directory: " + accountsDir;
        }
        return false;
    }

    struct dirent* entry;
    bool anyExported = false;

    while ((entry = readdir(dir.get())) != nullptr) {
        std::string filename = entry->d_name;

        if (EndsWith(filename, BIN_EXTENSION)) {
            std::string srcPath = accountsDir + "/" + filename;
            std::string dstPath = exportDir + "/" + filename;

            if (!CopyFile(srcPath, dstPath)) {
                if (errorMsg && errorMsg->empty()) {
                    *errorMsg = "Failed to export: " + filename;
                }
                return false;
            }

            anyExported = true;
        }
    }

    if (!anyExported) {
        if (errorMsg) {
            *errorMsg = "No saved accounts to export";
        }
        return false;
    }

    return true;
}

std::uint8_t GetLoginType() {
    void* accountBarn = GetAccountBarn();
    if (!accountBarn) return LOGIN_TYPE_LOGOUT;

    std::uintptr_t loginType_abs = tsm::game::mem::add(accountBarn, tsm::game::Offsets::kLoginType);
    return tsm::game::mem::read_abs<std::uint8_t>(loginType_abs);
}

const char* GetAccountTypeName(std::uint8_t loginType) {
    static constexpr const char* LOGIN_TYPE_NAMES[] = {
        "Local Account",
        "Apple Game Center",
        "Apple Game Center VN",
        "Google",
        "Facebook",
        "Apple",
        "Apple VN",
        "Nintendo",
        "Huawei",
        "Sony",
        "Steam",
        "Twitch",
        "Not Logged In"
    };

    constexpr size_t NUM_TYPES = sizeof(LOGIN_TYPE_NAMES) / sizeof(LOGIN_TYPE_NAMES[0]);

    if (loginType < NUM_TYPES) {
        return LOGIN_TYPE_NAMES[loginType];
    }
    return "Unknown";
}

bool IsLocalAccount() {
    return (GetLoginType() == LOGIN_TYPE_LOCAL);
}

void SwitchAccountType(std::uint8_t targetLoginType) {
    void* accountBarn = GetAccountBarn();
    if (!accountBarn) return;

    if (targetLoginType != LOGIN_TYPE_LOCAL) {
        DeleteAllAuthBinFiles();
    }

    std::uintptr_t loadingType_abs = tsm::game::mem::add(accountBarn, tsm::game::Offsets::kLoadingType);
    std::uintptr_t loginType_abs = tsm::game::mem::add(accountBarn, tsm::game::Offsets::kLoginType);
    std::uintptr_t shouldRestart_abs = tsm::game::mem::add(accountBarn, tsm::game::Offsets::kShouldRestart);

    tsm::game::mem::write_abs<std::uint8_t>(loadingType_abs, LOADING_TYPE_INIT);
    tsm::game::mem::write_abs<std::uint8_t>(loginType_abs, LOGIN_TYPE_LOGOUT);
    tsm::game::mem::write_abs<std::uint8_t>(shouldRestart_abs, g_fastAccountSwitch ? 0 : SHOULD_RESTART_YES);

    tsm::game::mem::write_abs<std::uint8_t>(loadingType_abs, LOADING_TYPE_INIT);
    tsm::game::mem::write_abs<std::uint8_t>(loginType_abs, targetLoginType);
    tsm::game::mem::write_abs<std::uint8_t>(shouldRestart_abs, g_fastAccountSwitch ? 0 : SHOULD_RESTART_YES);
}

void CreateNewLocalAccount(std::string* errorMsg) {
    DeleteAllAuthBinFiles(errorMsg);
    SwitchAccountType(LOGIN_TYPE_LOCAL);
}

bool SwitchToLocalAccount(const std::string& accountName) {
    if (!DeleteAllAuthBinFiles()) return false;
    if (!LoadSavedAccount(accountName)) return false;

    SwitchAccountType(LOGIN_TYPE_LOCAL);
    return true;
}

void SetFastAccountSwitch(bool enabled) {
    g_fastAccountSwitch = enabled;
}

void RelogCurrentAccount() {
    void* accountBarn = GetAccountBarn();
    if (!accountBarn) return;
    std::uintptr_t loadingType_abs = tsm::game::mem::add(accountBarn, tsm::game::Offsets::kLoadingType);
    tsm::game::mem::write_abs<std::uint8_t>(loadingType_abs, LOADING_TYPE_INIT);
}

bool GetFastAccountSwitch() {
    return g_fastAccountSwitch;
}

}}
