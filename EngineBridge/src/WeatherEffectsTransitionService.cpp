#include "renegade/bridge/WeatherEffectsTransitionService.h"

#include <algorithm>
#include <cmath>

namespace renegade::bridge
{
    namespace
    {
        [[nodiscard]] float Lerp(float a, float b, float t) noexcept
        {
            return a + (b - a) * t;
        }

        [[nodiscard]] float Ease(float progress) noexcept
        {
            const float t = std::clamp(
                std::isfinite(progress) ? progress : 0.0f, 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }

        [[nodiscard]] float WindAzimuth(float a, float b, float t) noexcept
        {
            // 170 to -170 must travel 20 degrees, not spin backwards 340.
            const float delta = std::remainder(b - a, 360.0f);
            return std::remainder(a + delta * t, 360.0f);
        }

        [[nodiscard]] float Amount(const PrecipitationState& state) noexcept
        {
            return state.mode == PrecipitationMode::None
                ? 0.0f : std::clamp(state.intensity, 0.0f, 1.0f);
        }
    }

    PrecipitationState BlendWeatherEffects(
        const PrecipitationState& from,
        const PrecipitationState& target,
        float progress) noexcept
    {
        if (!(progress > 0.0f)) return from;
        if (progress >= 1.0f) return target;

        const float t = Ease(progress);
        PrecipitationState result = from;

        // Native Wicked precipitation has one emitter. Rain and Snow cannot
        // render concurrently; fade the first mode down before switching
        // and fading the second up. The existing Snow profile is preserved,
        // but no Snow V2/deposition features are introduced here.
        const bool incompatibleModes =
            from.mode != PrecipitationMode::None &&
            target.mode != PrecipitationMode::None &&
            from.mode != target.mode;
        if (incompatibleModes)
        {
            if (t < 0.5f)
            {
                result.mode = from.mode;
                result.intensity = Lerp(Amount(from), 0.0f, t * 2.0f);
            }
            else
            {
                result.mode = target.mode;
                result.intensity = Lerp(
                    0.0f, Amount(target), (t - 0.5f) * 2.0f);
            }
        }
        else
        {
            result.mode = from.mode == PrecipitationMode::None
                ? target.mode : from.mode;
            result.intensity = Lerp(Amount(from), Amount(target), t);
        }

        result.fallSpeed = std::clamp(
            Lerp(from.fallSpeed, target.fallSpeed, t), 0.01f, 2.0f);
        result.particleScale = std::clamp(
            Lerp(from.particleScale, target.particleScale, t),
            0.005f, 0.1f);
        result.windAzimuthDegrees = WindAzimuth(
            from.windAzimuthDegrees, target.windAzimuthDegrees, t);
        result.windSpeed = std::clamp(
            Lerp(from.windSpeed, target.windSpeed, t), 0.0f, 50.0f);
        result.turbulence = std::clamp(
            Lerp(from.turbulence, target.turbulence, t), 0.0f, 20.0f);
        result.streakLength = std::clamp(
            Lerp(from.streakLength, target.streakLength, t), 0.0f, 0.1f);
        result.splashScale = std::clamp(
            Lerp(from.splashScale, target.splashScale, t), 0.0f, 1.0f);
        result.color.x = Lerp(from.color.x, target.color.x, t);
        result.color.y = Lerp(from.color.y, target.color.y, t);
        result.color.z = Lerp(from.color.z, target.color.z, t);
        result.color.w = Lerp(from.color.w, target.color.w, t);
        return result;
    }

    bool WeatherEffectsTransitionController::Start(
        const PrecipitationState& from,
        const PrecipitationState& target,
        float seconds) noexcept
    {
        if (!std::isfinite(seconds) || seconds < 0.0f || seconds > 300.0f)
            return false;
        from_ = from;
        target_ = target;
        elapsed_ = 0.0f;
        duration_ = seconds;
        active_ = seconds > 0.0f;
        return true;
    }

    float WeatherEffectsTransitionController::Progress() const noexcept
    {
        return duration_ <= 0.0f ? 1.0f
            : std::clamp(elapsed_ / duration_, 0.0f, 1.0f);
    }

    PrecipitationState WeatherEffectsTransitionController::Current() const noexcept
    {
        return BlendWeatherEffects(from_, target_, Progress());
    }

    void WeatherEffectsTransitionController::Advance(float deltaSeconds) noexcept
    {
        if (!active_ || !std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f)
            return;
        elapsed_ = std::min(duration_, elapsed_ + deltaSeconds);
        if (elapsed_ >= duration_) active_ = false;
    }

    void WeatherEffectsTransitionController::Cancel() noexcept
    {
        const PrecipitationState current = Current();
        from_ = current;
        target_ = current;
        duration_ = 0.0f;
        elapsed_ = 0.0f;
        active_ = false;
    }
}
