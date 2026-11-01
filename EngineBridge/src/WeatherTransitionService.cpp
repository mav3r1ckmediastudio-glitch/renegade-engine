#include "renegade/bridge/WeatherTransitionService.h"

#include <algorithm>
#include <cmath>

namespace renegade::bridge
{
    namespace
    {
        [[nodiscard]] float Lerp(const float a, const float b, const float t) noexcept
        {
            return a + (b - a) * t;
        }
        [[nodiscard]] float Ease(const float t) noexcept
        {
            const float x = std::clamp(std::isfinite(t) ? t : 0.0f, 0.0f, 1.0f);
            return x * x * (3.0f - 2.0f * x);
        }
        [[nodiscard]] bool HasClouds(const WeatherState& s) noexcept
        {
            return s.skyMode == WeatherState::SkyMode::RealisticWithClouds;
        }
    }

    WeatherState BlendWeatherStates(
        const WeatherState& from, const WeatherState& target,
        const float progress) noexcept
    {
        if (!(progress > 0.0f)) return from;
        if (progress >= 1.0f) return target;

        const float t = Ease(progress);
        WeatherState result = from;

        // Do not toggle the whole volumetric renderer halfway through the
        // transition. Run it across the blend and fade coverage to/from zero.
        // Skybox <-> physical-sky is discrete until completion.
        if (from.skyMode == WeatherState::SkyMode::Skybox ||
            target.skyMode == WeatherState::SkyMode::Skybox)
        {
            result.skyMode = from.skyMode;
        }
        else if (HasClouds(from) || HasClouds(target))
        {
            result.skyMode = WeatherState::SkyMode::RealisticWithClouds;
        }

        result.aerialPerspective = from.aerialPerspective;
        result.skyExposure = Lerp(from.skyExposure, target.skyExposure, t);
        result.stars = std::clamp(Lerp(from.stars, target.stars, t), 0.0f, 1.0f);
        result.ambientIntensity = std::max(0.0f,
            Lerp(from.ambientIntensity, target.ambientIntensity, t));
        result.fogStart = std::max(0.0f, Lerp(from.fogStart, target.fogStart, t));
        result.fogDensity = std::max(0.0f,
            Lerp(from.fogDensity, target.fogDensity, t));
        result.fogHeightStart = Lerp(from.fogHeightStart, target.fogHeightStart, t);
        result.fogHeightEnd = Lerp(from.fogHeightEnd, target.fogHeightEnd, t);
        result.heightFog = from.heightFog || target.heightFog;
        result.cloudCoverage = std::clamp(Lerp(
            HasClouds(from) ? from.cloudCoverage : 0.0f,
            HasClouds(target) ? target.cloudCoverage : 0.0f, t), 0.0f, 1.0f);
        result.cloudMovementSpeed = std::clamp(
            Lerp(from.cloudMovementSpeed, target.cloudMovementSpeed, t),
            0.0f, 12.0f);
        result.cloudStartHeight = std::max(0.0f,
            Lerp(from.cloudStartHeight, target.cloudStartHeight, t));
        result.cloudThickness = std::max(1.0f,
            Lerp(from.cloudThickness, target.cloudThickness, t));
        result.cloudsCastShadow = from.cloudsCastShadow || target.cloudsCastShadow;
        return result;
    }

    bool WeatherTransitionController::Start(
        const WeatherState& from, const WeatherState& target,
        const float seconds) noexcept
    {
        if (!std::isfinite(seconds) || seconds < 0.0f || seconds > 300.0f)
            return false;
        from_ = from;
        target_ = target;
        duration_ = seconds;
        elapsed_ = seconds == 0.0f ? 0.0f : 0.0f;
        active_ = seconds > 0.0f;
        return true;
    }

    float WeatherTransitionController::Progress() const noexcept
    {
        return duration_ <= 0.0f ? 1.0f
            : std::clamp(elapsed_ / duration_, 0.0f, 1.0f);
    }

    WeatherState WeatherTransitionController::Current() const noexcept
    {
        return BlendWeatherStates(from_, target_, Progress());
    }

    void WeatherTransitionController::Advance(const float deltaSeconds) noexcept
    {
        if (!active_ || !std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f)
            return;
        elapsed_ = std::min(duration_, elapsed_ + deltaSeconds);
        if (elapsed_ >= duration_) active_ = false;
    }

    void WeatherTransitionController::Cancel() noexcept
    {
        const WeatherState current = Current();
        from_ = current;
        target_ = current;
        duration_ = 0.0f;
        elapsed_ = 0.0f;
        active_ = false;
    }
}
