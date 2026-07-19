#pragma once

#include <network/NetworkTypes.h>
#include <string>
#include <map>

namespace tsm { namespace network {

class HttpClient {
public:
    HttpClient(const std::string& host, int port = 443);
    ~HttpClient() = default;

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    HttpResponse Get(const HttpRequest& request, int retries = 3, int retry_delay_sec = 2);

    HttpResponse Post(const HttpRequest& request, int retries = 3, int retry_delay_sec = 2);

    void SetTimeout(int seconds);

    const std::string& GetHost() const { return host_; }

    int GetPort() const { return port_; }

private:
    std::string host_;
    int port_;
    int timeout_seconds_{15};

    bool SetSocketTimeouts(int fd);
    int ConnectTcp();
    bool SslWriteAll(void* ssl, const char* data, size_t len);
    bool SslReadAll(void* ssl, std::string& out);
    bool ParseHttpResponse(const std::string& raw, HttpResponse& response);
    std::string BuildHttpRequest(const HttpRequest& request, const char* method);
    HttpResponse Request(const char* method, const HttpRequest& request, int retries, int retry_delay_sec);
};

}}
