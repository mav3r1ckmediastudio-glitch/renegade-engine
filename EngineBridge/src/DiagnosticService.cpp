#include "renegade/bridge/DiagnosticService.h"

#include <algorithm>
#include <chrono>
#include <thread>
#include <filesystem>
#include <fstream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace renegade::bridge
{
    DiagnosticService::DiagnosticService(const std::size_t capacity)
        : capacity_(std::max<std::size_t>(1, capacity))
    {
        events_.reserve(capacity_);
        peerSnapshotPath_ =
            (std::filesystem::temp_directory_path() / "renegade-runtime-diagnostics.json").string();
    }

    DiagnosticService::~DiagnosticService()
    {
        StopLocalEndpoint();
    }

    bool DiagnosticService::StartLocalEndpoint(const std::uint16_t port)
    {
        publishPeer_ = (port == 38742);
#ifdef _WIN32
        if (endpointRunning_.exchange(true))
            return false;
        endpointThread_ = std::thread([this, port]()
        {
            WSADATA data{};
            if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
            {
                endpointRunning_ = false;
                return;
            }
            const SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (listener == INVALID_SOCKET)
            {
                WSACleanup(); endpointRunning_ = false; return;
            }
            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_port = htons(port);
            inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
            if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR ||
                listen(listener, 2) == SOCKET_ERROR)
            {
                closesocket(listener); WSACleanup(); endpointRunning_ = false; return;
            }
            u_long nonBlocking = 1;
            ioctlsocket(listener, FIONBIO, &nonBlocking);
            DWORD timeout = 250;
            setsockopt(listener, SOL_SOCKET, SO_RCVTIMEO,
                reinterpret_cast<const char*>(&timeout), sizeof(timeout));
            while (endpointRunning_)
            {
                const SOCKET client = accept(listener, nullptr, nullptr);
                if (client == INVALID_SOCKET) continue;
                char request[1024]{};
                recv(client, request, sizeof(request) - 1, 0);
                const std::string body = SnapshotJson();
                const std::string response =
                    "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                    "Content-Length: " + std::to_string(body.size()) +
                    "\r\nConnection: close\r\n\r\n" + body;
                send(client, response.data(), static_cast<int>(response.size()), 0);
                closesocket(client);
            }
            closesocket(listener); WSACleanup();
        });
        return true;
#else
        (void)port;
        return false;
#endif
    }

    void DiagnosticService::StopLocalEndpoint() noexcept
    {
        endpointRunning_ = false;
        if (endpointThread_.joinable()) endpointThread_.join();
    }

    void DiagnosticService::Record(const DiagnosticSeverity severity,
        std::string source, std::string code, std::string message)
    {
        std::lock_guard lock(mutex_);
        if (events_.size() == capacity_)
            events_.erase(events_.begin());
        events_.push_back({severity, std::move(source), std::move(code),
            std::move(message)});
        if (publishPeer_)
        {
            const auto temporary = peerSnapshotPath_ + ".tmp";
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (output) { output << SnapshotJson(); output.close();
                std::error_code ec; std::filesystem::rename(temporary, peerSnapshotPath_, ec);
                if (ec) std::filesystem::remove(temporary, ec); }
        }
    }

    std::string DiagnosticService::ReadPeerSnapshot() const
    {
        std::ifstream input(peerSnapshotPath_, std::ios::binary);
        if (!input) return {};
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }

    void DiagnosticService::Clear() noexcept
    {
        std::lock_guard lock(mutex_);
        events_.clear();
    }

    std::vector<DiagnosticEvent> DiagnosticService::Snapshot() const
    {
        std::lock_guard lock(mutex_);
        return events_;
    }

    std::string DiagnosticService::EscapeJson(const std::string& value)
    {
        std::string result;
        result.reserve(value.size());
        for (const char character : value)
        {
            switch (character)
            {
            case '\\': result += "\\\\"; break;
            case '"': result += "\\\""; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += character; break;
            }
        }
        return result;
    }

    std::string DiagnosticService::SnapshotJson() const
    {
        const auto snapshot = Snapshot();
        std::string result = "{\"schema\":\"renegade.diagnostics.v1\",\"events\":[";
        for (std::size_t i = 0; i < snapshot.size(); ++i)
        {
            if (i != 0) result += ',';
            const auto& event = snapshot[i];
            const char* severity = event.severity == DiagnosticSeverity::Error
                ? "error" : event.severity == DiagnosticSeverity::Warning
                ? "warning" : "info";
            result += "{\"severity\":\"" + std::string(severity) +
                "\",\"source\":\"" + EscapeJson(event.source) +
                "\",\"code\":\"" + EscapeJson(event.code) +
                "\",\"message\":\"" + EscapeJson(event.message) + "\"}";
        }
        return result + "]}";
    }

    std::size_t DiagnosticService::ErrorCount() const noexcept
    {
        std::lock_guard lock(mutex_);
        return std::count_if(events_.begin(), events_.end(),
            [](const auto& event) { return event.severity == DiagnosticSeverity::Error; });
    }

    std::size_t DiagnosticService::WarningCount() const noexcept
    {
        std::lock_guard lock(mutex_);
        return std::count_if(events_.begin(), events_.end(),
            [](const auto& event) { return event.severity == DiagnosticSeverity::Warning; });
    }
}
