#include <game/hooks/AccountHooks.h>
#include <utils/hooking/dobby_wrapper.h>
#include <game/memory/offsets.h>
#include <game/memory/Address.h>
#include <utils/logging/log.h>
#include <mutex>

namespace tsm { namespace game { namespace hooks { namespace account {

namespace {
    using SetSessionFn = long double(*)(std::uintptr_t, const void*, const void*);
    using SetUserAgentFn = long long(*)(void*, const char*, const char*, const char*, int, const char*, const char*, const char*);

    SetSessionFn s_origSetSession = nullptr;
    SetUserAgentFn s_origSetUserAgent = nullptr;

    std::string s_userId;
    std::string s_sessionId;
    std::string s_userAgent;
    std::mutex s_dataMutex;

    std::string ToGuidHex(const unsigned char* p, bool dashed) {
        if (!p) return {};

        static const char* hex = "0123456789abcdef";
        std::string out;
        out.reserve(dashed ? 36 : 32);

        auto appendByte = [&](unsigned char b) {
            out.push_back(hex[(b >> 4) & 0xF]);
            out.push_back(hex[b & 0xF]);
        };

        for (int i = 0; i < 16; ++i) {
            appendByte(p[i]);
            if (dashed && (i == 3 || i == 5 || i == 7 || i == 9)) {
                out.push_back('-');
            }
        }

        return out;
    }

    extern "C" long double SetSession_Hook(std::uintptr_t a1, const void* a2, const void* a3) {
        if (a2) {
            std::string userId = ToGuidHex(reinterpret_cast<const unsigned char*>(a2), true);
            {
                std::lock_guard<std::mutex> lock(s_dataMutex);
                s_userId = std::move(userId);
            }
            tsm::log::i("SetSession: userId=%s", s_userId.c_str());
        }

        if (a3) {
            std::string sessionId = ToGuidHex(reinterpret_cast<const unsigned char*>(a3), false);
            {
                std::lock_guard<std::mutex> lock(s_dataMutex);
                s_sessionId = std::move(sessionId);
            }
            tsm::log::i("SetSession: session=%s", s_sessionId.c_str());
        }

        return s_origSetSession ? s_origSetSession(a1, a2, a3) : 0.0L;
    }

    extern "C" long long SetUserAgent_Hook(void* _this, const char* a2, const char* a3,
                                           const char* a4, int a5, const char* a6,
                                           const char* a7, const char* a8) {
        long long rc = s_origSetUserAgent ? s_origSetUserAgent(_this, a2, a3, a4, a5, a6, a7, a8) : 0;

        const std::uintptr_t bufAddr = reinterpret_cast<std::uintptr_t>(_this) + 20u;
        const char* buf = reinterpret_cast<const char*>(bufAddr);

        std::string ua;
        ua.reserve(128);
        for (size_t i = 0; i < 0x80; ++i) {
            char c = buf[i];
            if (c == '\0') break;
            ua.push_back(c);
        }

        if (!ua.empty()) {
            {
                std::lock_guard<std::mutex> lock(s_dataMutex);
                s_userAgent = std::move(ua);
            }
            tsm::log::i("UserAgent='%s'", s_userAgent.c_str());
        }

        return rc;
    }
}

bool Install() {
    if (tsm::game::memory::GetBase() == 0) {
        tsm::log::e("AccountHooks: Module base not initialized");
        return false;
    }

    bool allOk = true;

    if (!tsm::utils::hooking::install_rva("SetSession",
                                 tsm::game::Offsets::kAccountServerSetSession,
                                 (void*)SetSession_Hook,
                                 (void**)&s_origSetSession)) {
        tsm::log::e("AccountHooks: SetSession hook failed");
        allOk = false;
    }

    if (!tsm::utils::hooking::install_rva("SetUserAgent",
                                 tsm::game::Offsets::kHttpClientSetUserAgent,
                                 (void*)SetUserAgent_Hook,
                                 (void**)&s_origSetUserAgent)) {
        tsm::log::e("AccountHooks: SetUserAgent hook failed");
        allOk = false;
    }

    if (allOk) {
        tsm::log::i("AccountHooks: Installed successfully");
    }

    return allOk;
}

const std::string& GetCapturedUserId() {
    std::lock_guard<std::mutex> lock(s_dataMutex);
    return s_userId;
}

const std::string& GetCapturedSessionId() {
    std::lock_guard<std::mutex> lock(s_dataMutex);
    return s_sessionId;
}

const std::string& GetCapturedUserAgent() {
    std::lock_guard<std::mutex> lock(s_dataMutex);
    return s_userAgent;
}

}}}}
