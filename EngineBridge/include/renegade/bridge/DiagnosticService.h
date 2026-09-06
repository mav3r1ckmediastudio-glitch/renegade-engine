#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <atomic>
#include <string>
#include <thread>
#include <vector>

namespace renegade::bridge
{
    enum class DiagnosticSeverity : std::uint8_t
    {
        Info,
        Warning,
        Error
    };

    struct DiagnosticEvent
    {
        DiagnosticSeverity severity = DiagnosticSeverity::Info;
        std::string source;
        std::string code;
        std::string message;
    };

    // Small, dependency-free evidence store shared by editor services. It is
    // intentionally read-only from the future transport/MCP adapter's point
    // of view: callers can append evidence and request a stable JSON snapshot.
    class DiagnosticService final
    {
    public:
        explicit DiagnosticService(std::size_t capacity = 256);
        ~DiagnosticService();

        bool StartLocalEndpoint(std::uint16_t port = 38741);
        void StopLocalEndpoint() noexcept;

        void Record(DiagnosticSeverity severity, std::string source,
            std::string code, std::string message);
        void Clear() noexcept;

        [[nodiscard]] std::vector<DiagnosticEvent> Snapshot() const;
        [[nodiscard]] std::string SnapshotJson() const;
        [[nodiscard]] std::string ReadPeerSnapshot() const;
        [[nodiscard]] std::size_t ErrorCount() const noexcept;
        [[nodiscard]] std::size_t WarningCount() const noexcept;

    private:
        static std::string EscapeJson(const std::string& value);

        const std::size_t capacity_;
        mutable std::mutex mutex_;
        std::vector<DiagnosticEvent> events_;
        std::atomic_bool endpointRunning_ = false;
        std::thread endpointThread_;
        std::string peerSnapshotPath_;
        bool publishPeer_ = false;
    };
}
