#pragma once

#include <WickedEngine.h>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    enum class PrecipitationMode
    {
        None,
        Rain,
        Snow,
    };

    // Renegade's authored view of Wicked's native precipitation emitter.
    // Snow is a deliberate particle profile over that emitter: slower,
    // larger, unstretched particles with no rain splash. Surface accumulation
    // is not implied by this state.
    struct PrecipitationState
    {
        PrecipitationMode mode = PrecipitationMode::None;
        float intensity = 0.0f;
        float fallSpeed = 1.0f;
        float particleScale = 0.005f;
        float windAzimuthDegrees = 0.0f;
        float windSpeed = 1.0f;
        float turbulence = 5.0f;
        float streakLength = 0.04f;
        float splashScale = 0.1f;
        XMFLOAT4 color = XMFLOAT4(0.6f, 0.8f, 1.0f, 0.5f);
    };

    [[nodiscard]] PrecipitationState CapturePrecipitation(
        const wi::scene::WeatherComponent& weather) noexcept;
    void ApplyPrecipitation(
        wi::scene::WeatherComponent& weather,
        const PrecipitationState& state) noexcept;
    // Keeps Wicked's native precipitation simulation and swaps only its
    // transient billboard material. Call before the Scene update so a loaded
    // Snow profile receives Renegade's static flake instead of Wicked's
    // procedural circular rain mask.
    void RefreshPrecipitationVisual(wi::scene::Scene& scene);
    [[nodiscard]] PrecipitationState MakePrecipitationProfile(
        const PrecipitationState& current,
        PrecipitationMode mode) noexcept;

    class SetPrecipitationCommand final : public ICommand
    {
    public:
        SetPrecipitationCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            const PrecipitationState& precipitation);
        SetPrecipitationCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            const PrecipitationState& before,
            const PrecipitationState& after);

        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const PrecipitationState& state);

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        PrecipitationState before_;
        PrecipitationState after_;
    };
}
