#pragma once

#include <cstdint>
#include <string>

#include <WickedEngine.h>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    struct DecalState
    {
        bool baseColorOnlyAlpha = false;
        float slopeBlendPower = 0.0f;
    };

    struct EnvironmentProbeState
    {
        bool realTime = false;
        bool msaa = false;
        std::uint32_t resolution = 128;
        float updateInterval = 0.0f;
        float viewDistance = -1.0f;
    };

    [[nodiscard]] DecalState CaptureDecal(
        const wi::scene::DecalComponent& decal) noexcept;
    [[nodiscard]] DecalState SanitizeDecalState(
        const DecalState& state) noexcept;
    [[nodiscard]] bool HasDecalStateChange(
        const DecalState& before,
        const DecalState& after) noexcept;
    void ApplyDecal(
        wi::scene::DecalComponent& decal,
        const DecalState& state) noexcept;

    [[nodiscard]] EnvironmentProbeState CaptureEnvironmentProbe(
        const wi::scene::EnvironmentProbeComponent& probe) noexcept;
    [[nodiscard]] EnvironmentProbeState SanitizeEnvironmentProbeState(
        const EnvironmentProbeState& state) noexcept;
    [[nodiscard]] bool HasEnvironmentProbeStateChange(
        const EnvironmentProbeState& before,
        const EnvironmentProbeState& after) noexcept;
    void ApplyEnvironmentProbe(
        wi::scene::EnvironmentProbeComponent& probe,
        const EnvironmentProbeState& state) noexcept;
    bool RefreshEnvironmentProbe(
        wi::scene::Scene& scene,
        wi::ecs::Entity entity) noexcept;

    class CreateDecalCommand final : public ICommand
    {
    public:
        CreateDecalCommand(
            wi::scene::Scene& scene,
            const DecalState& state,
            const TransformState& transform);

        bool Execute() override;
        void Undo() override;

        [[nodiscard]] wi::ecs::Entity CreatedEntity() const noexcept;

    private:
        [[nodiscard]] std::string MakeUniqueName() const;

        wi::scene::Scene* scene_ = nullptr;
        DecalState state_;
        TransformState transform_;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        wi::Archive snapshot_;
        bool hasSnapshot_ = false;
    };

    class SetDecalCommand final : public ICommand
    {
    public:
        SetDecalCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            const DecalState& state);
        SetDecalCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            const DecalState& before,
            const DecalState& after);

        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const DecalState& state) noexcept;

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        DecalState before_;
        DecalState after_;
    };

    class CreateEnvironmentProbeCommand final : public ICommand
    {
    public:
        CreateEnvironmentProbeCommand(
            wi::scene::Scene& scene,
            const EnvironmentProbeState& state,
            const TransformState& transform);

        bool Execute() override;
        void Undo() override;

        [[nodiscard]] wi::ecs::Entity CreatedEntity() const noexcept;

    private:
        [[nodiscard]] std::string MakeUniqueName() const;

        wi::scene::Scene* scene_ = nullptr;
        EnvironmentProbeState state_;
        TransformState transform_;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        wi::Archive snapshot_;
        bool hasSnapshot_ = false;
    };

    class SetEnvironmentProbeCommand final : public ICommand
    {
    public:
        SetEnvironmentProbeCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            const EnvironmentProbeState& state);
        SetEnvironmentProbeCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            const EnvironmentProbeState& before,
            const EnvironmentProbeState& after);

        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const EnvironmentProbeState& state) noexcept;

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        EnvironmentProbeState before_;
        EnvironmentProbeState after_;
    };
}
