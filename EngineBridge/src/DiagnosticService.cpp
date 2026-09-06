#include "renegade/bridge/DiagnosticService.h"

#include <algorithm>
#include <utility>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <unistd.h>
#endif

#ifndef RENEGADE_DIAGNOSTIC_REVISION
#define RENEGADE_DIAGNOSTIC_REVISION "unknown"
#endif
#ifndef RENEGADE_DIAGNOSTIC_CONFIGURATION
#define RENEGADE_DIAGNOSTIC_CONFIGURATION "unknown"
#endif

namespace renegade::bridge
{
    namespace
    {
        // Truncate on a UTF-8 boundary; escaping below covers every JSON control byte.
        void Bound(std::string& value, std::size_t limit)
        {
            if (value.size() <= limit) return;
            while (limit > 0 && (static_cast<unsigned char>(value[limit]) & 0xc0) == 0x80) --limit;
            value.resize(limit);
        }
        std::uint64_t UnixMs()
        {
            return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        }
    }

    DiagnosticService::DiagnosticService(const std::size_t capacity)
        : capacity_(std::clamp<std::size_t>(capacity, 1, 512)), startedUnixMs_(UnixMs())
    {
        events_.reserve(capacity_);
        Identify("unknown");
    }

    DiagnosticService::~DiagnosticService() { StopLocalEndpoint(); }

    void DiagnosticService::Identify(std::string processType)
    {
#ifdef _WIN32
        const auto pid = static_cast<std::uint64_t>(GetCurrentProcessId());
#else
        const auto pid = static_cast<std::uint64_t>(getpid());
#endif
        SetState("process", {{"type", std::move(processType)}, {"pid", pid},
            {"started_unix_ms", startedUnixMs_},
            {"build_commit", std::string(RENEGADE_DIAGNOSTIC_REVISION)},
            {"configuration", std::string(RENEGADE_DIAGNOSTIC_CONFIGURATION)}});
    }

    std::uint64_t DiagnosticService::ElapsedMs() const
    {
        return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started_).count());
    }

    void DiagnosticService::Heartbeat()
    {
        std::lock_guard lock(mutex_);
        heartbeatMs_ = ElapsedMs();
    }

    void DiagnosticService::BoundState(DiagnosticState& state)
    {
        while (state.size() > 64) state.erase(std::prev(state.end()));
        for (auto it = state.begin(); it != state.end();)
        {
            if (it->first.size() > 96) { it = state.erase(it); continue; }
            if (auto* value = std::get_if<std::string>(&it->second)) Bound(*value, 1024);
            ++it;
        }
    }

    void DiagnosticService::SetState(std::string group, DiagnosticState state)
    {
        Bound(group, 96); BoundState(state);
        std::lock_guard lock(mutex_);
        if (state_.size() >= 32 && state_.count(group) == 0) return;
        state_[std::move(group)] = std::move(state);
    }

    void DiagnosticService::Observe(std::string group, DiagnosticState state, std::string source)
    {
        Bound(group, 96); BoundState(state);
        {
            std::lock_guard lock(mutex_);
            const auto it = state_.find(group);
            if (it != state_.end() && it->second == state) return;
            if (state_.size() >= 32 && it == state_.end()) return;
            state_[group] = std::move(state);
        }
        Record(DiagnosticSeverity::Info, std::move(source), group + ".changed", "Live state changed; inspect " + group);
    }

    void DiagnosticService::Record(const DiagnosticSeverity severity,
        std::string source, std::string code, std::string message)
    {
        Bound(source, 256); Bound(code, 96); Bound(message, 2048);
        std::lock_guard lock(mutex_);
        if (!events_.empty())
        {
            auto& last = events_.back();
            if (last.severity == severity && last.source == source && last.code == code && last.message == message)
            {
                ++last.repetitions; last.elapsedMs = ElapsedMs(); return;
            }
        }
        if (events_.size() == capacity_) events_.erase(events_.begin());
        events_.push_back({severity, std::move(source), std::move(code), std::move(message), ++sequence_, ElapsedMs(), 1});
    }

    std::string DiagnosticService::ReadPeerSnapshot() const
    {
        std::lock_guard lock(mutex_);
        return peerSnapshot_;
    }

    std::string DiagnosticService::ReadPeerSummary() const
    {
        std::lock_guard lock(mutex_);
        return peerSummary_;
    }

    void DiagnosticService::Clear() noexcept
    {
        std::lock_guard lock(mutex_); events_.clear();
    }

    std::vector<DiagnosticEvent> DiagnosticService::Snapshot() const
    {
        std::lock_guard lock(mutex_); return events_;
    }

    std::string DiagnosticService::EscapeJson(const std::string& value)
    {
        constexpr char hex[] = "0123456789abcdef";
        std::string result;
        for (const unsigned char c : value)
        {
            if (c == '"' || c == '\\') { result += '\\'; result += static_cast<char>(c); }
            else if (c < 0x20) { result += "\\u00"; result += hex[c >> 4]; result += hex[c & 15]; }
            else result += static_cast<char>(c);
        }
        return result;
    }

    std::string DiagnosticService::SnapshotJson() const
    {
        std::lock_guard lock(mutex_);
        const auto quote = [](const std::string& value) { return "\"" + EscapeJson(value) + "\""; };
        std::string result = "{\"schema\":\"renegade.diagnostics.v2\",\"elapsed_ms\":" + std::to_string(ElapsedMs()) +
            ",\"heartbeat_age_ms\":" + std::to_string(ElapsedMs() - heartbeatMs_) +
            ",\"endpoint_port\":" + std::to_string(endpointPort_.load()) + ",\"state\":{";
        bool firstGroup = true;
        for (const auto& [group, fields] : state_)
        {
            if (!firstGroup) result += ',';
            firstGroup = false; result += quote(group) + ":{";
            bool firstField = true;
            for (const auto& [key, value] : fields)
            {
                if (!firstField) result += ',';
                firstField = false; result += quote(key) + ':';
                if (const auto* s = std::get_if<std::string>(&value)) result += quote(*s);
                else if (const auto* b = std::get_if<bool>(&value)) result += *b ? "true" : "false";
                else if (const auto* n = std::get_if<std::uint64_t>(&value)) result += std::to_string(*n);
                else result += "null";
            }
            result += '}';
        }
        result += "},\"events\":[";
        for (std::size_t i = 0; i < events_.size(); ++i)
        {
            if (i != 0) result += ',';
            const auto& e = events_[i];
            result += "{\"severity\":" + quote(e.severity == DiagnosticSeverity::Error ? "error" :
                e.severity == DiagnosticSeverity::Warning ? "warning" : "info") +
                ",\"source\":" + quote(e.source) + ",\"code\":" + quote(e.code) + ",\"message\":" + quote(e.message) +
                ",\"sequence\":" + std::to_string(e.sequence) + ",\"elapsed_ms\":" + std::to_string(e.elapsedMs) +
                ",\"repetitions\":" + std::to_string(e.repetitions) + '}';
        }
        return result + "]}";
    }

    std::string DiagnosticService::SummaryText() const
    {
        std::lock_guard lock(mutex_);
        std::string text = endpointRunning_ ? "LIVE DIAGNOSTICS // 127.0.0.1:" + std::to_string(endpointPort_.load()) : "LIVE DIAGNOSTICS // endpoint unavailable";
        text += " // heartbeat age " + std::to_string(ElapsedMs() - heartbeatMs_) + " ms";
        const auto start = events_.size() > 8 ? events_.size() - 8 : 0;
        for (std::size_t i = events_.size(); i > start; --i)
        {
            const auto& event = events_[i - 1];
            std::string line = event.code + " // " + event.message;
            Bound(line, 180);
            text += "\n" + line;
        }
        return text;
    }

    std::size_t DiagnosticService::ErrorCount() const noexcept
    {
        std::lock_guard lock(mutex_);
        return std::count_if(events_.begin(), events_.end(), [](const auto& e) { return e.severity == DiagnosticSeverity::Error; });
    }
    std::size_t DiagnosticService::WarningCount() const noexcept
    {
        std::lock_guard lock(mutex_);
        return std::count_if(events_.begin(), events_.end(), [](const auto& e) { return e.severity == DiagnosticSeverity::Warning; });
    }
}
