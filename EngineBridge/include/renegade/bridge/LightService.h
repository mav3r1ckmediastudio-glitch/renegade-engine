#pragma once

#include <string>

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
        float radius = 0.025f;
        float length = 0.0f;
        float height = 0.0f;
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

    // Applies only the curated fields in LightState. Cascades, shadow
    // resolution, masks, camera source, lens flares and all unexposed flags
    // remain untouched.
    void ApplyLight(
        wi::scene::LightComponent& light,
        const LightState& state) noexcept;

    // Creator-facing defaults for a newly authored native Wicked light.
    // These follow Wicked Editor's own creation values while giving each
    // shape useful dimensions before the Inspector edits it.
    [[nodiscard]] LightState MakeNewLightState(
        wi::scene::LightComponent::LightType type) noexcept;

    class CreateLightCommand final : public ICommand
    {
    public:
        CreateLightCommand(
            wi::scene::Scene& scene,
            wi::scene::LightComponent::LightType type,
            const XMFLOAT3& position,
            const XMFLOAT4& rotation = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));

        bool Execute() override;
        void Undo() override;

        [[nodiscard]] wi::ecs::Entity CreatedEntity() const noexcept;

    private:
        [[nodiscard]] std::string MakeUniqueName() const;

        wi::scene::Scene* scene_ = nullptr;
        wi::scene::LightComponent::LightType type_ =
            wi::scene::LightComponent::POINT;
        XMFLOAT3 position_ = {};
        XMFLOAT4 rotation_ = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        wi::Archive snapshot_;
        bool hasSnapshot_ = false;
    };

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
