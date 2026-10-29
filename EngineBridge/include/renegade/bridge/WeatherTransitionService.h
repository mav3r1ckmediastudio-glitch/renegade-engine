#pragma once

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    // Deterministic, render-API-independent interpolation of the EXISTING
    // WeatherState. Does not edit a scene, own a timer, or record Undo history.
    // The editor/runtime caller decides when to commit or serialize its state.
    [[nodiscard]] WeatherState BlendWeatherStates(
        const WeatherState& from, const WeatherState& target,
        float progress) noexcept;

    class WeatherTransitionController
    {
    public:
        // Returns false and preserves the current transition for invalid input.
        // Zero seconds is an intentional instant transition.
        [[nodiscard]] bool Start(
            const WeatherState& from, const WeatherState& target,
            float seconds) noexcept;
        void Advance(float deltaSeconds) noexcept;
        void Cancel() noexcept; // Retain the interpolated value; no time reset jump.
        [[nodiscard]] bool IsActive() const noexcept { return active_; }
        [[nodiscard]] float Progress() const noexcept;
        [[nodiscard]] float Duration() const noexcept { return duration_; }
        [[nodiscard]] float Elapsed() const noexcept { return elapsed_; }
        [[nodiscard]] const WeatherState& From() const noexcept { return from_; }
        [[nodiscard]] const WeatherState& Target() const noexcept { return target_; }
        [[nodiscard]] WeatherState Current() const noexcept;

    private:
        WeatherState from_{};
        WeatherState target_{};
        float duration_ = 0.0f;
        float elapsed_ = 0.0f;
        bool active_ = false;
    };
}
