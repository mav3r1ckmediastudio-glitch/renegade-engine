#pragma once

#include <WickedEngine.h>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    enum class SunPreset
    {
        Dawn,
        Midday,
        GoldenHour,
        Dusk,
        Midnight,
    };

    // One authored sun state drives both Wicked's serialized weather direction
    // and the primary directional-light transform. Time is an editor-friendly
    // view over the same direction, ready for the later Lua day/night driver.
    struct SunState
    {
        float timeHours = 12.0f;
        float azimuthDegrees = 0.0f;
        float elevationDegrees = 75.0f;
        XMFLOAT3 direction = XMFLOAT3(0.0f, 1.0f, 0.0f);
        wi::ecs::Entity lightEntity = wi::ecs::INVALID_ENTITY;
        TransformState lightTransform;
    };

    [[nodiscard]] wi::ecs::Entity FindPrimarySunLight(
        const wi::scene::Scene& scene) noexcept;
    // Creates the canonical serialized Sun only when the scene has no
    // directional light. Existing authored lighting is never replaced.
    [[nodiscard]] wi::ecs::Entity EnsurePrimarySunLight(
        wi::scene::Scene& scene,
        wi::ecs::Entity weatherEntity,
        bool* created = nullptr);

    // Repairs a serialized Weather-only Level without replacing an existing
    // directional light. The Sun and Weather runtime state share one Undo step.
    class CreateSunCommand final : public ICommand
    {
    public:
        CreateSunCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity weatherEntity);

        bool Execute() override;
        void Undo() override;

        [[nodiscard]] wi::ecs::Entity CreatedEntity() const noexcept;

    private:
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity weatherEntity_ = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        wi::scene::WeatherComponent weatherBefore_;
        wi::scene::WeatherComponent weatherAfter_;
        wi::scene::WeatherComponent resolvedBefore_;
        wi::scene::WeatherComponent resolvedAfter_;
        wi::Archive snapshot_;
        bool hasSnapshot_ = false;
    };
    [[nodiscard]] SunState CaptureSun(
        const wi::scene::Scene& scene,
        wi::ecs::Entity weatherEntity) noexcept;
    void SetSunTime(SunState& state, float timeHours) noexcept;
    void SetSunAzimuth(SunState& state, float azimuthDegrees) noexcept;
    void SetSunElevation(SunState& state, float elevationDegrees) noexcept;
    [[nodiscard]] SunState MakeSunPreset(
        const SunState& current,
        SunPreset preset) noexcept;
    bool ApplySun(
        wi::scene::Scene& scene,
        wi::ecs::Entity weatherEntity,
        const SunState& state) noexcept;

    class SetSunCommand final : public ICommand
    {
    public:
        SetSunCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity weatherEntity,
            const SunState& sun);
        SetSunCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity weatherEntity,
            const SunState& before,
            const SunState& after);

        bool Execute() override;
        void Undo() override;

    private:
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity weatherEntity_ = wi::ecs::INVALID_ENTITY;
        SunState before_;
        SunState after_;
    };
}
