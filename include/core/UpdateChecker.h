#pragma once

#include <string>
#include <cstdint>

namespace tsm { namespace core {

struct Version {
    int major{0};
    int minor{0};
    int patch{0};

    static Version Parse(const std::string& str);

    bool IsValid() const;

    bool operator>(const Version& other) const;
    bool operator>=(const Version& other) const;
    bool operator<(const Version& other) const;
    bool operator<=(const Version& other) const;
    bool operator==(const Version& other) const;
    bool operator!=(const Version& other) const;

    std::string ToString() const;
};

class UpdateChecker {
public:
    static UpdateChecker& Get();

    void CheckForUpdates();

    bool HasUpdate() const;

    std::string GetLatestVersion() const;

    std::string GetCurrentVersion() const;

    std::string GetDownloadUrl() const;

    bool IsChecking() const;

    void MarkNotificationSeen();

    bool ShouldShowNotification() const;

    bool ConsumeJustInstalledFlag();

    void InstallLatest();

private:
    UpdateChecker() = default;
    ~UpdateChecker() = default;
    UpdateChecker(const UpdateChecker&) = delete;
    UpdateChecker& operator=(const UpdateChecker&) = delete;

    void CheckForUpdatesInternal();

    Version FetchLatestVersion();

    Version GetCurrentVersionFromConfig();

    void InstallLatestInternal();

    bool DownloadLatestToFile(const std::string& path);

    std::string DetermineFilesDirectory();

    bool m_checkStarted{false};
    bool m_checking{false};
    bool m_hasUpdate{false};
    bool m_notificationSeen{false};
    bool m_installRunning{false};
    bool m_updateJustInstalled{false};

    Version m_currentVersion;
    Version m_latestVersion;
};

}}
