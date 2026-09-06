#include "renegade/bridge/DiagnosticService.h"

#include <chrono>
#include <algorithm>
#include <cctype>
#include <utility>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>

namespace
{
    using Clock = std::chrono::steady_clock;
    struct Socket
    {
        SOCKET value = INVALID_SOCKET;
        ~Socket() { if (value != INVALID_SOCKET) closesocket(value); }
    };
    bool NonBlocking(SOCKET socket)
    {
        u_long mode = 1;
        return ioctlsocket(socket, FIONBIO, &mode) == 0;
    }
    bool Wait(SOCKET socket, bool writing, Clock::time_point deadline, const std::atomic_bool& running)
    {
        while (running && Clock::now() < deadline)
        {
            fd_set fds; FD_ZERO(&fds); FD_SET(socket, &fds);
            timeval timeout{0, 20000};
            const int ready = select(0, writing ? nullptr : &fds, writing ? &fds : nullptr, nullptr, &timeout);
            if (ready > 0) return true;
            if (ready < 0) return false;
        }
        return false;
    }
    bool Send(SOCKET socket, const std::string& data, Clock::time_point deadline, const std::atomic_bool& running)
    {
        std::size_t offset = 0;
        while (offset < data.size() && Wait(socket, true, deadline, running))
        {
            const int count = send(socket, data.data() + offset, static_cast<int>(data.size() - offset), 0);
            if (count > 0) offset += static_cast<std::size_t>(count);
            else if (count == 0 || WSAGetLastError() != WSAEWOULDBLOCK) return false;
        }
        return offset == data.size();
    }
    std::string Peer(const std::atomic_bool& running, bool summary = false)
    {
        Socket socket{::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)};
        if (socket.value == INVALID_SOCKET || !NonBlocking(socket.value)) return {};
        sockaddr_in address{}; address.sin_family = AF_INET; address.sin_port = htons(38742);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        const auto deadline = Clock::now() + std::chrono::milliseconds(250);
        const int connected = connect(socket.value, reinterpret_cast<sockaddr*>(&address), sizeof(address));
        if (connected != 0 && WSAGetLastError() != WSAEWOULDBLOCK) return {};
        if (!Wait(socket.value, true, deadline, running)) return {};
        int error = 0; int size = sizeof(error);
        if (getsockopt(socket.value, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &size) != 0 || error != 0) return {};
        if (!Send(socket.value, std::string("GET ") + (summary ? "/summary" : "/snapshot") + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n", deadline, running)) return {};
        std::string response;
        char buffer[4096];
        bool closed = false;
        while (response.size() <= 4 * 1024 * 1024 && Wait(socket.value, false, deadline, running))
        {
            const int count = recv(socket.value, buffer, sizeof(buffer), 0);
            if (count == 0) { closed = true; break; }
            if (count < 0) { if (WSAGetLastError() == WSAEWOULDBLOCK) continue; return {}; }
            response.append(buffer, static_cast<std::size_t>(count));
        }
        const auto start = response.find("\r\n\r\n");
        if (!closed || response.size() > 4 * 1024 * 1024 || response.rfind("HTTP/1.1 200 OK\r\n", 0) != 0 || start == std::string::npos) return {};
        const auto body = response.substr(start + 4);
        if (summary) return body.substr(0, 4096);
        if (body.rfind("{\"schema\":\"renegade.diagnostics.v2\",", 0) != 0 || body.back() != '}') return {};
        return body;
    }
}
#endif

namespace renegade::bridge
{
    bool DiagnosticService::StartLocalEndpoint(const std::uint16_t port)
    {
        if (endpointRunning_) return false;
        StopLocalEndpoint();
#ifdef _WIN32
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
        {
            Record(DiagnosticSeverity::Error, "DiagnosticEndpoint.cpp", "endpoint.start.failed", "WSAStartup failed");
            return false;
        }
        const SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in address{}; address.sin_family = AF_INET;
        address.sin_port = htons(port); address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        BOOL exclusive = TRUE;
        const bool configured = listener != INVALID_SOCKET &&
            setsockopt(listener, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&exclusive), sizeof(exclusive)) == 0 &&
            NonBlocking(listener);
        if (!configured || bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 || listen(listener, 4) != 0)
        {
            const int error = WSAGetLastError();
            if (listener != INVALID_SOCKET) closesocket(listener);
            WSACleanup();
            Record(DiagnosticSeverity::Error, "DiagnosticEndpoint.cpp", "endpoint.start.failed", "Loopback bind/listen failed: " + std::to_string(error));
            return false;
        }
        int size = sizeof(address);
        if (getsockname(listener, reinterpret_cast<sockaddr*>(&address), &size) != 0)
        {
            closesocket(listener); WSACleanup(); return false;
        }
        endpointPort_ = ntohs(address.sin_port);
        endpointRunning_ = true;
        try
        {
            endpointThread_ = std::thread([this, listener, port]()
            {
                Socket ownedListener{listener};
                auto nextPeer = Clock::now();
                while (endpointRunning_)
                {
                    if (port == 38741 && Clock::now() >= nextPeer)
                    {
                        auto peer = Peer(endpointRunning_);
                        auto summary = peer.empty() ? std::string() : Peer(endpointRunning_, true);
                        { std::lock_guard lock(mutex_); peerSnapshot_ = std::move(peer); peerSummary_ = std::move(summary); }
                        nextPeer = Clock::now() + std::chrono::seconds(1);
                    }
                    if (!Wait(listener, false, Clock::now() + std::chrono::milliseconds(50), endpointRunning_)) continue;
                    Socket client{accept(listener, nullptr, nullptr)};
                    if (client.value == INVALID_SOCKET || !NonBlocking(client.value)) continue;
                    const auto deadline = Clock::now() + std::chrono::milliseconds(250);
                    std::string request;
                    char buffer[1024];
                    while (request.size() < 4096 && request.find("\r\n\r\n") == std::string::npos &&
                        Wait(client.value, false, deadline, endpointRunning_))
                    {
                        const int count = recv(client.value, buffer, sizeof(buffer), 0);
                        if (count <= 0) break;
                        request.append(buffer, static_cast<std::size_t>(count));
                    }
                    if (request.size() >= 4096 || request.find("\r\n\r\n") == std::string::npos) continue;
                    std::string status = "200 OK";
                    std::string body;
                    std::string contentType = "application/json";
                    const auto end = request.find("\r\n");
                    const auto line = request.substr(0, end);
                    // Explicit read-only routes; reject browser origins and
                    // non-loopback Host values (including DNS rebinding names).
                    auto headers = request.substr(end);
                    std::transform(headers.begin(), headers.end(), headers.begin(),
                        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                    const auto hostStart = headers.find("\r\nhost:");
                    std::string host;
                    if (hostStart != std::string::npos)
                    {
                        const auto valueStart = hostStart + 7;
                        host = headers.substr(valueStart, headers.find("\r\n", valueStart) - valueStart);
                        const auto first = host.find_first_not_of(" \t");
                        if (first != std::string::npos) host.erase(0, first);
                        const auto last = host.find_last_not_of(" \t");
                        if (last != std::string::npos) host.erase(last + 1);
                    }
                    const auto expectedHost = "127.0.0.1:" + std::to_string(endpointPort_.load());
                    if (headers.find("\r\norigin:") != std::string::npos ||
                        (host != "127.0.0.1" && host != expectedHost))
                    { status = "403 Forbidden"; body = "{\"error\":\"browser access is disabled\"}"; }
                    else if (line == "GET /snapshot HTTP/1.1" || line == "GET / HTTP/1.1") body = SnapshotJson();
                    else if (line == "GET /summary HTTP/1.1") { body = SummaryText(); contentType = "text/plain; charset=utf-8"; }
                    else { status = "404 Not Found"; body = "{\"error\":\"only GET /snapshot is supported\"}"; }
                    const auto response = "HTTP/1.1 " + status + "\r\nContent-Type: " + contentType + "\r\nCache-Control: no-store\r\nContent-Length: " +
                        std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
                    Send(client.value, response, deadline, endpointRunning_);
                }
                // Socket must close before the matching Winsock cleanup.
                closesocket(ownedListener.value); ownedListener.value = INVALID_SOCKET;
                WSACleanup();
            });
        }
        catch (...)
        {
            endpointRunning_ = false; endpointPort_ = 0;
            closesocket(listener); WSACleanup();
            Record(DiagnosticSeverity::Error, "DiagnosticEndpoint.cpp", "endpoint.start.failed", "Could not start endpoint thread");
            return false;
        }
        Record(DiagnosticSeverity::Info, "DiagnosticEndpoint.cpp", "endpoint.listening", "127.0.0.1:" + std::to_string(endpointPort_.load()));
        return true;
#else
        (void)port;
        Record(DiagnosticSeverity::Warning, "DiagnosticEndpoint.cpp", "endpoint.unsupported", "Live transport requires Windows");
        return false;
#endif
    }

    void DiagnosticService::StopLocalEndpoint() noexcept
    {
        endpointRunning_ = false;
        if (endpointThread_.joinable()) endpointThread_.join();
        endpointPort_ = 0;
        std::lock_guard lock(mutex_); peerSnapshot_.clear(); peerSummary_.clear();
    }
}
