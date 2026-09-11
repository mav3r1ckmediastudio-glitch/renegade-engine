#pragma once

#include <cstddef>
#include <cstdint>

#include <WickedEngine.h>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    struct InverseKinematicsState
    {
        bool componentExists = false;
        wi::ecs::Entity target = wi::ecs::INVALID_ENTITY;
        bool disabled = false;
        std::uint32_t chainLength = 0;
        std::uint32_t iterationCount = 1;
    };

    [[nodiscard]] InverseKinematicsState CaptureInverseKinematicsState(
        const wi::scene::Scene& scene,
        wi::ecs::Entity entity) noexcept;

    class SetInverseKinematicsStateCommand final : public ICommand
    {
    public:
        SetInverseKinematicsStateCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            const InverseKinematicsState& after);

        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const InverseKinematicsState& state) noexcept;

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        InverseKinematicsState before_;
        InverseKinematicsState after_;
    };

    struct HumanoidLookAtState
    {
        bool componentExists = false;
        bool enabled = false;
        wi::ecs::Entity target = wi::ecs::INVALID_ENTITY;
        XMFLOAT2 headRotationMax = XMFLOAT2(XM_PI / 3.0f, XM_PI / 6.0f);
        float headRotationSpeed = 0.1f;
        XMFLOAT2 eyeRotationMax = XMFLOAT2(XM_PI / 20.0f, XM_PI / 20.0f);
        float eyeRotationSpeed = 0.1f;
    };

    [[nodiscard]] HumanoidLookAtState CaptureHumanoidLookAtState(
        const wi::scene::Scene& scene,
        wi::ecs::Entity humanoidEntity) noexcept;

    class SetHumanoidLookAtStateCommand final : public ICommand
    {
    public:
        SetHumanoidLookAtStateCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity humanoidEntity,
            const HumanoidLookAtState& after);

        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const HumanoidLookAtState& state) noexcept;

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity humanoidEntity_ = wi::ecs::INVALID_ENTITY;
        HumanoidLookAtState before_;
        HumanoidLookAtState after_;
    };

    using ExpressionOverride = wi::scene::ExpressionComponent::Override;

    struct ExpressionMasterState
    {
        bool componentExists = false;
        bool forceTalking = false;
        float blinkFrequency = 0.3f;
        float blinkLength = 0.1f;
        int blinkCount = 2;
        float lookFrequency = 0.0f;
        float lookLength = 0.6f;
    };

    [[nodiscard]] ExpressionMasterState CaptureExpressionMasterState(
        const wi::scene::Scene& scene,
        wi::ecs::Entity expressionEntity) noexcept;

    class SetExpressionMasterStateCommand final : public ICommand
    {
    public:
        SetExpressionMasterStateCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity expressionEntity,
            const ExpressionMasterState& after);

        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const ExpressionMasterState& state) noexcept;

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity expressionEntity_ = wi::ecs::INVALID_ENTITY;
        ExpressionMasterState before_;
        ExpressionMasterState after_;
    };

    struct ExpressionEntryState
    {
        bool valid = false;
        bool binary = false;
        float weight = 0.0f;
        ExpressionOverride overrideMouth = ExpressionOverride::None;
        ExpressionOverride overrideBlink = ExpressionOverride::None;
        ExpressionOverride overrideLook = ExpressionOverride::None;
    };

    [[nodiscard]] ExpressionEntryState CaptureExpressionEntryState(
        const wi::scene::Scene& scene,
        wi::ecs::Entity expressionEntity,
        std::size_t expressionIndex) noexcept;

    class SetExpressionEntryStateCommand final : public ICommand
    {
    public:
        SetExpressionEntryStateCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity expressionEntity,
            std::size_t expressionIndex,
            const ExpressionEntryState& after);

        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const ExpressionEntryState& state) noexcept;

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity expressionEntity_ = wi::ecs::INVALID_ENTITY;
        std::size_t expressionIndex_ = 0;
        ExpressionEntryState before_;
        ExpressionEntryState after_;
    };

    [[nodiscard]] wi::ecs::Entity FindExpressionEntity(
        const wi::scene::Scene& scene,
        wi::ecs::Entity humanoidEntity) noexcept;
}
