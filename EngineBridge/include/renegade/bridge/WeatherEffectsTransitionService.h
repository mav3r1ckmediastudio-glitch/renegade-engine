#pragma once

#include "renegade/bridge/PrecipitationService.h"

namespace renegade::bridge
{
    // G4 native precipitation/wind counterpart to G3 WeatherTransitionController.
    // This is a pure data blend: the Studio or Runtime caller owns updates,
    // scene authority, persistence and any authoring Undo/Redo transaction.
    [[nodiscard]] PrecipitationState BlendWeatherEffects(
        const PrecipitationState& from,
        const PrecipitationState& target,
        float progress) noexcept;

    class WeatherEffectsTransitionController
    {
    public:
        [[nodiscard]] bool Start(
            const PrecipitationState& from,
            const PrecipitationState& target,
            float seconds) noexcept;
        void Advance(float deltaSeconds) noexcept;
        void Cancel() noexcept;
        [[nodiscard]] bool IsActive() const noexcept { return active_; }
        [[nodiscard]] float Progress() const noexcept;
        [[nodiscard]] float Duration() const noexcept { return duration_; }
        [[nodiscard]] const PrecipitationState& From() const noexcept { return from_; }
        [[nodiscard]] const PrecipitationState& Target() const noexcept { return target_; }
        [[nodiscard]] PrecipitationState Current() const noexcept;

    private:
        PrecipitationState from_{};
        PrecipitationState target_{};
        float duration_ = 0.0f;
        float elapsed_ = 0.0f;
        bool active_ = false;
    };
}
