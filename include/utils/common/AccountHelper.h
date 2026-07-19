#pragma once

#include <string>
#include <vector>

namespace tsm { namespace utils {

struct SavedAccount {
    char name[128];
};

std::string GetAccountsDir();
std::string GetAccountAuthPath(const char* filename = nullptr);

bool EnsureDirectoryExists(const std::string& path);
bool FileExists(const std::string& path);
bool CopyFile(const std::string& src, const std::string& dst);
bool FilesAreIdentical(const std::string& file1, const std::string& file2);

bool IsCurrentAccount(const std::string& accountName);
bool SaveCurrentAccount(const std::string& accountName, std::string* errorMsg = nullptr);
bool LoadSavedAccount(const std::string& accountName);
bool DeleteAccount(const std::string& accountName);
bool ClearCurrentAccountData(std::string* errorMsg = nullptr);
bool ImportAccountFromFd(int fd, std::string& errorMsg, std::string* outAccountName = nullptr);

void CreateNewLocalAccount(std::string* errorMsg = nullptr);
bool SwitchToLocalAccount(const std::string& accountName);
bool DeleteAllAuthBinFiles(std::string* errorMsg = nullptr);
bool ExportAllAccountsToDownloads(std::string* errorMsg = nullptr);

void LoadAccountsFromDirectory(std::vector<SavedAccount>& accounts);

std::uint8_t GetLoginType();
const char* GetAccountTypeName(std::uint8_t loginType);
bool IsLocalAccount();
void SwitchAccountType(std::uint8_t targetLoginType = 0);

void SetFastAccountSwitch(bool enabled);
bool GetFastAccountSwitch();

void RelogCurrentAccount();

}}
