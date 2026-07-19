#include <network/WebhookLogger.h>
#include <network/HttpClient.h>
#include <utils/common/obfuscate.h>

#include <thread>
#include <atomic>
#include <string>
#include <sstream>
#include <iomanip>
#include <cstdint>

namespace tsm { namespace network {

namespace {

std::string FormatUserIdShort(const std::string& userId)
{
    return userId.substr(0, 20) + _OS("...");
}

std::atomic<bool> g_sent{false};

void SendImpl(const std::string& userId)
{
    try {
        const char* host = _O("discord.com");
        const char* endpoint = _O("/api/webhooks/1449249378672967873/AAaLJ-k3pvfg8Ghcrffw8Ldg1odWy4Z0S5upiUaXwz5tz4xyRCgaAJLIIVO75J1T9Trr");

        tsm::network::HttpClient client(host, 443);
        client.SetTimeout(5);

        std::string shortId = FormatUserIdShort(userId);

        std::string body = _OS("{\"embeds\":[{\"title\":\"That Sky Mod\",\"description\":\"User logged in\",\"color\":10798649,\"thumbnail\":{\"url\":\"https://i.ibb.co/prJ0CVdY/android-logo-png-transparent.png\"},\"footer\":{\"text\":\"")
            + shortId
            + _OS("\"}}]}");

        tsm::network::HttpRequest request;
        request.endpoint = endpoint;
        request.headers[_OS("Content-Type")] = _OS("application/json");
        request.body = body;

        (void)client.Post(request, 1, 1);
    } catch (...) {
    }
}

}

void SendDiscordLoginWebhookOnce(const std::string& userId)
{
    if (userId.empty()) {
        return;
    }

    bool already = g_sent.exchange(true);
    if (already) {
        return;
    }

    std::string uidCopy = userId;

    std::thread([uidCopy]() {
        SendImpl(uidCopy);
    }).detach();
}

}}
