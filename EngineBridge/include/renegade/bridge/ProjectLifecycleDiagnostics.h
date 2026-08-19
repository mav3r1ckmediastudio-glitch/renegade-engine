#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>

#include <wiBacklog.h>

namespace renegade::bridge::diagnostics
{
    using LifecycleClock = std::chrono::steady_clock;
    using LifecycleTimePoint = LifecycleClock::time_point;

    inline double MillisecondsSince(const LifecycleTimePoint& started) noexcept
    {
        return std::chrono::duration<double, std::milli>(
            LifecycleClock::now() - started).count();
    }

    inline void LogProjectLifecycleTiming(
        const std::string& phase,
        const double elapsedMilliseconds,
        const std::string& detail = {})
    {
        std::ostringstream line;
        line << "[PR58-GATE1] " << phase << " // "
             << std::fixed << std::setprecision(2)
             << elapsedMilliseconds << " ms";
        if (!detail.empty())
            line << " // " << detail;

        const std::string text = line.str();
        wi::backlog::post(text);

        // Gate 1 diagnostics are deliberately fail-open: timing evidence must
        // never influence project/scene behavior. Persist a simple owner-test
        // log alongside Studio's Saved state so Release evidence can be handed
        // back without requiring the Wicked backlog UI to remain visible.
        static std::mutex fileMutex;
        const std::lock_guard<std::mutex> lock(fileMutex);
        std::error_code ec;
        const std::filesystem::path directory =
            std::filesystem::path("Saved") / "Diagnostics";
        std::filesystem::create_directories(directory, ec);
        if (ec)
            return;

        std::ofstream stream(
            directory / "PR58Gate1Lifecycle.log",
            std::ios::out | std::ios::app);
        if (stream)
            stream << text << '\n';
    }
}
