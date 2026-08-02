#pragma once

#include <WickedEngine.h>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    struct LightState
    {
        wi::scene::LightComponent::LightType type =
            wi::scene::LightComponent::POINT;
        XMFLOAT3 color = XMFLOAT3(1.0f, 1.0f, 1.0f);
        float intensity = 1.0f;
        float range = 10.0f;
        float outerConeDegrees = 45.0f;
        float innerConeDegrees = 0.0f;
        bool castShadow = false;
        bool volumetrics = false;
        float volumetricBoost = 0.0f;
    };

    [[nodiscard]] LightState CaptureLight(
        const wi::scene::LightComponent& light) noexcept;
    [[nodiscard]] LightState SanitizeLightState(
        const LightState& state) noexcept;
    [[nodiscard]] bool HasLightStateChange(
        const LightState& before,
        const LightState& after) noexcept;

    // Applies only the curated fields in LightState. Radius, length, height,
    // cascades, shadow resolution, masks, camera source, lens flares and all
    // unexposed flags remain untouched.
    void ApplyLight(
        wi::scene::LightComponent& light,
        const LightState& state) noexcept;

    class SetLightCommand final : public ICommand
    {
    public:
        SetLightCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            const LightState& light);
        SetLightCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            const LightState& before,
            const LightState& after);

        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const LightState& state) noexcept;

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        LightState before_;
        LightState after_;
    };
}
