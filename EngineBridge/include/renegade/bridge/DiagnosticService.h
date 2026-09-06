#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <variant>
#include <vector>

namespace renegade::bridge
{
    enum class DiagnosticSeverity : std::uint8_t { Info, Warning, Error };
    using DiagnosticValue = std::variant<std::nullptr_t, bool, std::uint64_t, std::string>;
    using DiagnosticState = std::map<std::string, DiagnosticValue>;

    struct DiagnosticEvent
    {
        DiagnosticSeverity severity = DiagnosticSeverity::Info;
        std::string source;
        std::string code;
        std::string message;
        std::uint64_t sequence = 0;
        std::uint64_t elapsedMs = 0;
        std::uint64_t repetitions = 1;
    };

    // Writers publish copied values on the application thread. The transport
    // never reads ECS/widgets and never writes project data or runs commands.
    class DiagnosticService final
    {
    public:
        explicit DiagnosticService(std::size_t capacity = 256);
        ~DiagnosticService();
        void Identify(std::string processType);
        bool StartLocalEndpoint(std::uint16_t port = 38741);
        void StopLocalEndpoint() noexcept;
        [[nodiscard]] bool EndpointRunning() const noexcept { return endpointRunning_; }
        [[nodiscard]] std::uint16_t EndpointPort() const noexcept { return endpointPort_; }
        void Record(DiagnosticSeverity severity, std::string source,
            std::string code, std::string message);
        // A complete group replacement prevents stale keys after scene changes.
        void SetState(std::string group, DiagnosticState state);
        void Observe(std::string group, DiagnosticState state, std::string source);
        void Heartbeat();
        void Clear() noexcept;
        [[nodiscard]] std::vector<DiagnosticEvent> Snapshot() const;
        [[nodiscard]] std::string SnapshotJson() const;
        [[nodiscard]] std::string SummaryText() const;
        // Cached live HTTP response, refreshed off the render thread. Empty
        // means unavailable, never evidence that Runtime successfully stopped.
        [[nodiscard]] std::string ReadPeerSnapshot() const;
        [[nodiscard]] std::string ReadPeerSummary() const;
        [[nodiscard]] std::size_t ErrorCount() const noexcept;
        [[nodiscard]] std::size_t WarningCount() const noexcept;
        [[nodiscard]] std::uint64_t ElapsedMs() const;

    private:
        static std::string EscapeJson(const std::string& value);
        static void BoundState(DiagnosticState& state);
        const std::size_t capacity_;
        const std::chrono::steady_clock::time_point started_ = std::chrono::steady_clock::now();
        const std::uint64_t startedUnixMs_;
        mutable std::mutex mutex_;
        std::vector<DiagnosticEvent> events_;
        std::map<std::string, DiagnosticState> state_;
        std::uint64_t sequence_ = 0;
        std::uint64_t heartbeatMs_ = 0;
        std::string peerSnapshot_;
        std::string peerSummary_;
        std::atomic_bool endpointRunning_ = false;
        std::atomic<std::uint16_t> endpointPort_ = 0;
        std::thread endpointThread_;
    };
}
