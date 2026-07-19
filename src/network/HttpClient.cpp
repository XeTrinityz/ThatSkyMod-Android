#include <network/HttpClient.h>
#include <utils/logging/log.h>

#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include <cstring>
#include <cstdio>
#include <algorithm>
#include <cctype>
#include <thread>
#include <chrono>
#include <mutex>

namespace tsm { namespace network {

namespace {
    std::once_flag g_sslOnce;

    void InitSslOnce() {
        std::call_once(g_sslOnce, []() {
            ERR_load_crypto_strings();
            SSL_load_error_strings();
            OpenSSL_add_ssl_algorithms();
        });
    }

    std::string ToLower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    std::string TrimSpaces(std::string s) {
        while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
        return s;
    }

    std::string EnsureLeadingSlash(const std::string& path) {
        if (path.empty()) return "/";
        if (path.front() == '/') return path;
        return "/" + path;
    }
}

HttpClient::HttpClient(const std::string& host, int port)
    : host_(host), port_(port) {
    InitSslOnce();
}

void HttpClient::SetTimeout(int seconds) {
    timeout_seconds_ = seconds;
}

bool HttpClient::SetSocketTimeouts(int fd) {
    timeval tv{};
    tv.tv_sec = timeout_seconds_;
    tv.tv_usec = 0;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0) return false;
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != 0) return false;
    return true;
}

int HttpClient::ConnectTcp() {
    char portStr[16];
    std::snprintf(portStr, sizeof(portStr), "%d", port_);

    addrinfo hints{};
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;

    addrinfo* res = nullptr;
    if (getaddrinfo(host_.c_str(), portStr, &hints, &res) != 0) {
        return -1;
    }

    int fd = -1;
    for (addrinfo* p = res; p != nullptr; p = p->ai_next) {
        fd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;

        if (!SetSocketTimeouts(fd)) {
            ::close(fd);
            fd = -1;
            continue;
        }

        if (::connect(fd, p->ai_addr, p->ai_addrlen) == 0) {
            break;
        }

        ::close(fd);
        fd = -1;
    }

    freeaddrinfo(res);
    return fd;
}

bool HttpClient::SslWriteAll(void* ssl_ptr, const char* data, size_t len) {
    SSL* ssl = static_cast<SSL*>(ssl_ptr);
    size_t sent = 0;

    while (sent < len) {
        int rc = SSL_write(ssl, data + sent, static_cast<int>(len - sent));
        if (rc <= 0) return false;
        sent += static_cast<size_t>(rc);
    }

    return true;
}

bool HttpClient::SslReadAll(void* ssl_ptr, std::string& out) {
    SSL* ssl = static_cast<SSL*>(ssl_ptr);
    char buf[4096];

    for (;;) {
        int rc = SSL_read(ssl, buf, sizeof(buf));
        if (rc <= 0) break;
        out.append(buf, static_cast<size_t>(rc));
    }

    return !out.empty();
}

bool HttpClient::ParseHttpResponse(const std::string& raw, HttpResponse& response) {
    auto pos = raw.find("\r\n\r\n");
    if (pos == std::string::npos) return false;

    std::string head = raw.substr(0, pos);
    response.body = raw.substr(pos + 4);

    auto lineEnd = head.find("\r\n");
    if (lineEnd == std::string::npos) return false;

    std::string statusLine = head.substr(0, lineEnd);
    response.status = 0;

    size_t sp1 = statusLine.find(' ');
    if (sp1 != std::string::npos) {
        size_t sp2 = statusLine.find(' ', sp1 + 1);
        std::string code = statusLine.substr(sp1 + 1,
            (sp2 == std::string::npos ? std::string::npos : sp2 - sp1 - 1));
        response.status = std::atoi(code.c_str());
    }

    size_t off = lineEnd + 2;
    while (off < head.size()) {
        auto next = head.find("\r\n", off);
        std::string hline = (next == std::string::npos) ? head.substr(off) : head.substr(off, next - off);

        if (hline.empty()) break;

        auto colon = hline.find(':');
        if (colon != std::string::npos) {
            std::string key = ToLower(hline.substr(0, colon));
            std::string value = TrimSpaces(hline.substr(colon + 1));
            response.headers.emplace(std::move(key), std::move(value));
        }

        if (next == std::string::npos) break;
        off = next + 2;
    }

    auto it = response.headers.find("transfer-encoding");
    if (it != response.headers.end() && ToLower(it->second).find("chunked") != std::string::npos) {
        std::string decoded;
        size_t i = 0;

        while (i < response.body.size()) {
            size_t eol = response.body.find("\r\n", i);
            if (eol == std::string::npos) break;

            std::string sizeHex = response.body.substr(i, eol - i);
            unsigned long chunkSize = std::strtoul(sizeHex.c_str(), nullptr, 16);
            i = eol + 2;

            if (chunkSize == 0) break;

            if (i + chunkSize > response.body.size()) break;
            decoded.append(response.body.data() + i, chunkSize);
            i += chunkSize;

            if (i + 2 <= response.body.size() && response.body[i] == '\r' && response.body[i + 1] == '\n') {
                i += 2;
            }
        }

        response.body = std::move(decoded);
    }

    return true;
}

std::string HttpClient::BuildHttpRequest(const HttpRequest& request, const char* method) {
    const std::string path = EnsureLeadingSlash(request.endpoint);

    std::string req;
    req.reserve(512 + request.headers.size() * 32 + request.body.size());

    req += method;
    req += " ";
    req += path;
    req += " HTTP/1.1\r\n";

    req += "Host: ";
    req += host_;
    req += "\r\n";

    for (const auto& [key, value] : request.headers) {
        req += key;
        req += ": ";
        req += value;
        req += "\r\n";
    }

    req += "Accept: application/json\r\n";
    req += "Accept-Encoding: identity\r\n";
    req += "Connection: close\r\n";
    req += "User-Agent: ThatSkyMod/Android\r\n";

    if (request.body.size() > 0) {
        req += "Content-Length: ";
        req += std::to_string(request.body.size());
        req += "\r\n\r\n";
        req += request.body;
    } else {
        req += "\r\n";
    }

    return req;
}

HttpResponse HttpClient::Request(const char* method, const HttpRequest& request, int retries, int retry_delay_sec) {
    HttpResponse response;

    for (int attempt = 0; attempt < retries; ++attempt) {
        int fd = ConnectTcp();
        if (fd < 0) {
            response.status = -1;
            response.error = "Connection failed";
            tsm::log::e("HttpClient: Connection to {}:{} failed", host_.c_str(), port_);
            std::this_thread::sleep_for(std::chrono::seconds(retry_delay_sec));
            continue;
        }

        SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
        if (!ctx) {
            ::close(fd);
            response.status = -1;
            response.error = "SSL_CTX_new failed";
            std::this_thread::sleep_for(std::chrono::seconds(retry_delay_sec));
            continue;
        }

        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);

        SSL* ssl = SSL_new(ctx);
        if (!ssl) {
            SSL_CTX_free(ctx);
            ::close(fd);
            response.status = -1;
            response.error = "SSL_new failed";
            std::this_thread::sleep_for(std::chrono::seconds(retry_delay_sec));
            continue;
        }

        SSL_set_tlsext_host_name(ssl, host_.c_str());
        SSL_set_fd(ssl, fd);

        if (SSL_connect(ssl) != 1) {
            SSL_free(ssl);
            SSL_CTX_free(ctx);
            ::close(fd);
            response.status = -1;
            response.error = "SSL_connect failed";
            std::this_thread::sleep_for(std::chrono::seconds(retry_delay_sec));
            continue;
        }

        std::string reqStr = BuildHttpRequest(request, method);
        bool ok = SslWriteAll(ssl, reqStr.data(), reqStr.size());

        std::string rawResp;
        if (ok) {
            ok = SslReadAll(ssl, rawResp);
        }

        SSL_shutdown(ssl);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        ::close(fd);

        if (!ok) {
            response.status = -1;
            response.error = "I/O error";
            tsm::log::e("HttpClient: I/O error during request");
            std::this_thread::sleep_for(std::chrono::seconds(retry_delay_sec));
            continue;
        }

        if (!ParseHttpResponse(rawResp, response)) {
            response.status = -1;
            response.error = "Malformed HTTP response";
            response.body = rawResp;
            tsm::log::e("HttpClient: Malformed HTTP response");
            std::this_thread::sleep_for(std::chrono::seconds(retry_delay_sec));
            continue;
        }

        if (response.IsSuccess() || !response.IsRetryable()) {
            response.error.clear();
            return response;
        }

        std::this_thread::sleep_for(std::chrono::seconds(retry_delay_sec));
    }

    if (response.error.empty()) {
        response.error = "Retries exhausted";
    }

    return response;
}

HttpResponse HttpClient::Get(const HttpRequest& request, int retries, int retry_delay_sec) {
    return Request("GET", request, retries, retry_delay_sec);
}

HttpResponse HttpClient::Post(const HttpRequest& request, int retries, int retry_delay_sec) {
    return Request("POST", request, retries, retry_delay_sec);
}

}}
