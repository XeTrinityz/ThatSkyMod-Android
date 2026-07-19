#include <core/UpdateChecker.h>
#include <network/HttpClient.h>
#include <utils/logging/log.h>
#include <Cipher/Cipher.h>
#include <utils/common/obfuscate.h>

#include <thread>
#include <mutex>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <cstdio>
#include <memory>

namespace tsm {
    namespace core {

        namespace {
            constexpr int API_TIMEOUT_SECONDS = 10;
            constexpr int DOWNLOAD_TIMEOUT_SECONDS = 60;
            constexpr int MAX_RETRIES = 3;
            constexpr int MAX_REDIRECTS = 5;
            constexpr size_t MAX_DOWNLOAD_SIZE = 100 * 1024 * 1024;

            const char* GITHUB_API_HOST = _O("api.github.com");
            const char* GITHUB_RELEASES_ENDPOINT = _O("/repos/XeTrinityz/ThatSkyMod-Android/releases/latest");
            const char* DOWNLOAD_URL = _O("https://github.com/XeTrinityz/ThatSkyMod-Android/releases/latest/download/libTSM.so");
            const char* DEFAULT_FILES_DIR = _O("/data/data/git.artdeell.skymodloader/files");

            std::mutex g_updateMutex;

            std::string ExtractJsonValue(const std::string& json, const std::string& key) {
                if (json.empty() || key.empty()) return _OS("");

                std::string searchKey = _OS("\"") + key + _OS("\"");
                size_t pos = json.find(searchKey);
                if (pos == std::string::npos) return _OS("");

                pos = json.find(':', pos);
                if (pos == std::string::npos) return _OS("");

                ++pos;
                while (pos < json.length() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;

                if (json.substr(pos, 4) == _OS("null")) return _OS("");

                if (pos >= json.length() || json[pos] != '"') return _OS("");
                ++pos;

                size_t end = pos;
                while (end < json.length()) {
                    if (json[end] == '"' && (end == pos || json[end - 1] != '\\')) break;
                    ++end;
                }

                if (end >= json.length()) return _OS("");
                return json.substr(pos, end - pos);
            }

            struct UrlComponents {
                std::string host;
                std::string path;
                bool valid = false;
            };

            UrlComponents ParseUrl(const std::string& url) {
                UrlComponents result;
                if (url.empty()) return result;

                std::string work = url;

                auto schemePos = work.find(_OS("://"));
                if (schemePos != std::string::npos) {
                    work = work.substr(schemePos + 3);
                }

                auto slashPos = work.find('/');
                if (slashPos == std::string::npos) {
                    result.host = work;
                    result.path = _OS("/");
                }
                else {
                    result.host = work.substr(0, slashPos);
                    result.path = work.substr(slashPos);
                }

                while (result.path.length() > 1 && result.path.back() == '/') {
                    result.path.pop_back();
                }

                result.valid = !result.host.empty();
                return result;
            }

            class FileHandle {
            public:
                explicit FileHandle(const std::string& path, std::ios::openmode mode)
                    : m_file(path, mode) {
                }

                bool IsOpen() const { return m_file.is_open(); }
                bool IsGood() const { return m_file.good(); }

                std::ofstream& Get() { return m_file; }

                ~FileHandle() {
                    if (m_file.is_open()) m_file.close();
                }

            private:
                std::ofstream m_file;
            };

            std::string GetHeader(const std::map<std::string, std::string>& headers,
                const std::string& name) {
                auto it = headers.find(name);
                if (it != headers.end()) return it->second;

                std::string lower = name;
                std::transform(lower.begin(), lower.end(), lower.begin(),
                    [](unsigned char c) { return std::tolower(c); });
                it = headers.find(lower);
                return (it != headers.end()) ? it->second : _OS("");
            }
        }


        Version Version::Parse(const std::string& str) {
            Version v;
            if (str.empty()) return v;

            std::string s = str;
            if (s[0] == 'v' || s[0] == 'V') s = s.substr(1);

            auto parseToken = [](const std::string& t) -> int {
                if (t.empty()) return 0;
                try {
                    size_t pos;
                    int val = std::stoi(t, &pos);
                    return (pos == t.length()) ? val : 0;
                }
                catch (...) {
                    return 0;
                }
                };

            std::istringstream iss(s);
            std::string token;

            if (std::getline(iss, token, '.')) v.major = parseToken(token);
            if (std::getline(iss, token, '.')) v.minor = parseToken(token);
            if (std::getline(iss, token, '.')) v.patch = parseToken(token);

            return v;
        }

        bool Version::operator>(const Version& o)  const { return major != o.major ? major > o.major : minor != o.minor ? minor > o.minor : patch > o.patch; }
        bool Version::operator>=(const Version& o) const { return !(*this < o); }
        bool Version::operator<(const Version& o)  const { return o > *this; }
        bool Version::operator<=(const Version& o) const { return !(*this > o); }
        bool Version::operator==(const Version& o) const { return major == o.major && minor == o.minor && patch == o.patch; }
        bool Version::operator!=(const Version& o) const { return !(*this == o); }

        std::string Version::ToString() const {
            return std::to_string(major) + _OS(".") + std::to_string(minor) + _OS(".") + std::to_string(patch);
        }

        bool Version::IsValid() const {
            return major > 0 || minor > 0 || patch > 0;
        }


        UpdateChecker& UpdateChecker::Get() {
            static UpdateChecker instance;
            return instance;
        }

        void UpdateChecker::CheckForUpdates() {
            {
                std::lock_guard<std::mutex> lock(g_updateMutex);
                if (m_checkStarted) return;
                m_checkStarted = true;
                m_checking = true;
                m_currentVersion = GetCurrentVersionFromConfig();
            }

            std::thread([this]() { CheckForUpdatesInternal(); }).detach();
        }

        bool        UpdateChecker::HasUpdate()        const { std::lock_guard<std::mutex> lock(g_updateMutex); return m_hasUpdate; }
        std::string UpdateChecker::GetLatestVersion() const { std::lock_guard<std::mutex> lock(g_updateMutex); return m_latestVersion.ToString(); }
        std::string UpdateChecker::GetCurrentVersion()const { std::lock_guard<std::mutex> lock(g_updateMutex); return m_currentVersion.ToString(); }
        std::string UpdateChecker::GetDownloadUrl()   const { return DOWNLOAD_URL; }
        bool        UpdateChecker::IsChecking()       const { std::lock_guard<std::mutex> lock(g_updateMutex); return m_checking; }

        void UpdateChecker::MarkNotificationSeen() {
            std::lock_guard<std::mutex> lock(g_updateMutex);
            m_notificationSeen = true;
        }

        bool UpdateChecker::ShouldShowNotification() const {
            std::lock_guard<std::mutex> lock(g_updateMutex);
            return m_hasUpdate && !m_notificationSeen;
        }

        bool UpdateChecker::ConsumeJustInstalledFlag() {
            std::lock_guard<std::mutex> lock(g_updateMutex);
            bool was = m_updateJustInstalled;
            m_updateJustInstalled = false;
            return was;
        }


        void UpdateChecker::CheckForUpdatesInternal() {
            Version latestVersion = FetchLatestVersion();

            {
                std::lock_guard<std::mutex> lock(g_updateMutex);
                m_checking = false;
                m_latestVersion = latestVersion;

                m_hasUpdate = latestVersion.IsValid() && (latestVersion > m_currentVersion);
            }

            InstallLatest();
        }


        Version UpdateChecker::FetchLatestVersion() {
            try {
                tsm::network::HttpClient client(GITHUB_API_HOST, 443);
                client.SetTimeout(API_TIMEOUT_SECONDS);

                tsm::network::HttpRequest request;
                request.endpoint = GITHUB_RELEASES_ENDPOINT;
                request.headers[_OS("Accept")] = _OS("application/vnd.github.v3+json");
                request.headers[_OS("User-Agent")] = _OS("ThatSkyMod-UpdateChecker");

                tsm::network::HttpResponse response = client.Get(request, MAX_RETRIES, 3);
                if (!response.IsSuccess()) return {};

                std::string tagName = ExtractJsonValue(response.body, _OS("tag_name"));
                if (tagName.empty()) return {};

                Version version = Version::Parse(tagName);
                return version.IsValid() ? version : Version{};

            }
            catch (...) {}

            return {};
        }

        Version UpdateChecker::GetCurrentVersionFromConfig() {
            return Version{ 0, 25, 2 };
        }


        void UpdateChecker::InstallLatest() {
            {
                std::lock_guard<std::mutex> lock(g_updateMutex);
                if (m_installRunning) return;
                m_installRunning = true;
            }

            std::thread([this]() { InstallLatestInternal(); }).detach();
        }

        void UpdateChecker::InstallLatestInternal() {
            bool        success = false;
            bool        hadUpdate = false;

            {
                std::lock_guard<std::mutex> lock(g_updateMutex);
                hadUpdate = m_hasUpdate;
            }

            try {
                std::string filesDir = DetermineFilesDirectory();
                std::string targetPath = filesDir + _OS("/mods/libTSM.so");
                std::string tempPath = targetPath + _OS(".tmp");

                if (!DownloadLatestToFile(tempPath)) {
                    throw std::runtime_error(_OS("Download failed"));
                }

                std::remove(targetPath.c_str());

                if (std::rename(tempPath.c_str(), targetPath.c_str()) != 0) {
                    std::remove(tempPath.c_str());
                    throw std::runtime_error(_OS("Failed to move updated binary into place"));
                }

                success = true;

            }
            catch (const std::exception&) {
            }
            catch (...) {}

            {
                std::lock_guard<std::mutex> lock(g_updateMutex);
                m_installRunning = false;
                if (success) {
                    m_updateJustInstalled = hadUpdate;
                    m_hasUpdate = false;
                    m_notificationSeen = true;
                }
            }
        }


        std::string UpdateChecker::DetermineFilesDirectory() {
            const char* cfgPath = Cipher::getConfigPath();
            if (!cfgPath || !*cfgPath) return DEFAULT_FILES_DIR;

            std::string filesDir(cfgPath);

            auto configPos = filesDir.rfind(_OS("/config"));
            if (configPos != std::string::npos) {
                filesDir.erase(configPos);
            }
            else {
                auto pos = filesDir.find_last_of("/\\");
                if (pos != std::string::npos && pos > 0) filesDir.erase(pos);
            }

            while (filesDir.length() > 1 && filesDir.back() == '/') filesDir.pop_back();

            return filesDir;
        }

        bool UpdateChecker::DownloadLatestToFile(const std::string& path) {
            try {
                std::string currentUrl = GetDownloadUrl();
                if (currentUrl.empty()) return false;

                for (int redirectCount = 0; redirectCount < MAX_REDIRECTS; ++redirectCount) {
                    UrlComponents components = ParseUrl(currentUrl);
                    if (!components.valid) return false;

                    tsm::network::HttpClient client(components.host, 443);
                    client.SetTimeout(DOWNLOAD_TIMEOUT_SECONDS);

                    tsm::network::HttpRequest request;
                    request.endpoint = components.path;
                    request.headers[_OS("User-Agent")] = _OS("ThatSkyMod-UpdateChecker");

                    tsm::network::HttpResponse response = client.Get(request, MAX_RETRIES, 3);

                    if (response.status >= 301 && response.status <= 308) {
                        std::string location = GetHeader(response.headers, _OS("Location"));
                        if (location.empty()) return false;
                        currentUrl = location;
                        continue;
                    }

                    if (!response.IsSuccess())       return false;
                    if (response.body.empty())        return false;
                    if (response.body.size() > MAX_DOWNLOAD_SIZE) return false;

                    FileHandle file(path, std::ios::binary | std::ios::trunc);
                    if (!file.IsOpen()) return false;

                    file.Get().write(response.body.data(),
                        static_cast<std::streamsize>(response.body.size()));

                    return file.IsGood();
                }

                return false;

            }
            catch (...) {}

            return false;
        }

    }
}
