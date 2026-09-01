#pragma once

#include <chrono>
#include <iomanip>
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
    }
}
