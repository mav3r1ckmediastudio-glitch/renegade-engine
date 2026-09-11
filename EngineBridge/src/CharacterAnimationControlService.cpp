#include "renegade/bridge/CharacterAnimationControlService.h"

#include <algorithm>

namespace
{
    bool Same(const XMFLOAT2& a, const XMFLOAT2& b) noexcept
    {
        return a.x == b.x && a.y == b.y;
    }

    bool Same(
        const renegade::bridge::InverseKinematicsState& a,
        const renegade::bridge::InverseKinematicsState& b) noexcept
    {
        return a.componentExists == b.componentExists &&
            a.target == b.target &&
            a.disabled == b.disabled &&
            a.chainLength == b.chainLength &&
            a.iterationCount == b.iterationCount;
    }

    bool Same(
        const renegade::bridge::HumanoidLookAtState& a,
        const renegade::bridge::HumanoidLookAtState& b) noexcept
    {
        return a.componentExists == b.componentExists &&
            a.enabled == b.enabled &&
            a.target == b.target &&
            Same(a.headRotationMax, b.headRotationMax) &&
            a.headRotationSpeed == b.headRotationSpeed &&
            Same(a.eyeRotationMax, b.eyeRotationMax) &&
            a.eyeRotationSpeed == b.eyeRotationSpeed;
    }

    bool Same(
        const renegade::bridge::ExpressionMasterState& a,
        const renegade::bridge::ExpressionMasterState& b) noexcept
    {
        return a.componentExists == b.componentExists &&
            a.forceTalking == b.forceTalking &&
            a.blinkFrequency == b.blinkFrequency &&
            a.blinkLength == b.blinkLength &&
            a.blinkCount == b.blinkCount &&
            a.lookFrequency == b.lookFrequency &&
            a.lookLength == b.lookLength;
    }

    bool Same(
        const renegade::bridge::ExpressionEntryState& a,
        const renegade::bridge::ExpressionEntryState& b) noexcept
    {
        return a.valid == b.valid &&
            a.binary == b.binary &&
            a.weight == b.weight &&
            a.overrideMouth == b.overrideMouth &&
            a.overrideBlink == b.overrideBlink &&
            a.overrideLook == b.overrideLook;
    }
}

namespace renegade::bridge
{
    InverseKinematicsState CaptureInverseKinematicsState(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        InverseKinematicsState state;
        const auto* ik = scene.inverse_kinematics.GetComponent(entity);
        if (ik == nullptr)
            return state;

        state.componentExists = true;
        state.target = ik->target;
        state.disabled = ik->IsDisabled();
        state.chainLength = ik->chain_length;
        state.iterationCount = ik->iteration_count;
        return state;
    }

    SetInverseKinematicsStateCommand::SetInverseKinematicsStateCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const InverseKinematicsState& after)
        : scene_(&scene),
          entity_(entity),
          before_(CaptureInverseKinematicsState(scene, entity)),
          after_(after)
    {
    }

    bool SetInverseKinematicsStateCommand::Apply(
        const InverseKinematicsState& state) noexcept
    {
        if (scene_ == nullptr || entity_ == wi::ecs::INVALID_ENTITY)
            return false;

        if (!state.componentExists)
        {
            scene_->inverse_kinematics.Remove(entity_);
            return true;
        }

        auto* ik = scene_->inverse_kinematics.GetComponent(entity_);
        if (ik == nullptr)
            ik = &scene_->inverse_kinematics.Create(entity_);

        ik->target = state.target;
        ik->SetDisabled(state.disabled);
        ik->chain_length = state.chainLength;
        ik->iteration_count = state.iterationCount;
        return true;
    }

    bool SetInverseKinematicsStateCommand::Execute()
    {
        if (Same(before_, after_))
            return false;
        return Apply(after_);
    }

    void SetInverseKinematicsStateCommand::Undo()
    {
        (void)Apply(before_);
    }

    HumanoidLookAtState CaptureHumanoidLookAtState(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity humanoidEntity) noexcept
    {
        HumanoidLookAtState state;
        const auto* humanoid = scene.humanoids.GetComponent(humanoidEntity);
        if (humanoid == nullptr)
            return state;

        state.componentExists = true;
        state.enabled = humanoid->IsLookAtEnabled();
        state.target = humanoid->lookAtEntity;
        state.headRotationMax = humanoid->head_rotation_max;
        state.headRotationSpeed = humanoid->head_rotation_speed;
        state.eyeRotationMax = humanoid->eye_rotation_max;
        state.eyeRotationSpeed = humanoid->eye_rotation_speed;
        return state;
    }

    SetHumanoidLookAtStateCommand::SetHumanoidLookAtStateCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity humanoidEntity,
        const HumanoidLookAtState& after)
        : scene_(&scene),
          humanoidEntity_(humanoidEntity),
          before_(CaptureHumanoidLookAtState(scene, humanoidEntity)),
          after_(after)
    {
    }

    bool SetHumanoidLookAtStateCommand::Apply(
        const HumanoidLookAtState& state) noexcept
    {
        if (scene_ == nullptr || humanoidEntity_ == wi::ecs::INVALID_ENTITY ||
            !state.componentExists)
        {
            return false;
        }

        auto* humanoid = scene_->humanoids.GetComponent(humanoidEntity_);
        if (humanoid == nullptr)
            return false;

        humanoid->SetLookAtEnabled(state.enabled);
        humanoid->lookAtEntity = state.target;
        humanoid->head_rotation_max = state.headRotationMax;
        humanoid->head_rotation_speed = state.headRotationSpeed;
        humanoid->eye_rotation_max = state.eyeRotationMax;
        humanoid->eye_rotation_speed = state.eyeRotationSpeed;
        return true;
    }

    bool SetHumanoidLookAtStateCommand::Execute()
    {
        if (Same(before_, after_))
            return false;
        return Apply(after_);
    }

    void SetHumanoidLookAtStateCommand::Undo()
    {
        (void)Apply(before_);
    }

    ExpressionMasterState CaptureExpressionMasterState(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity expressionEntity) noexcept
    {
        ExpressionMasterState state;
        const auto* expressions = scene.expressions.GetComponent(expressionEntity);
        if (expressions == nullptr)
            return state;

        state.componentExists = true;
        state.forceTalking = expressions->IsForceTalkingEnabled();
        state.blinkFrequency = expressions->blink_frequency;
        state.blinkLength = expressions->blink_length;
        state.blinkCount = expressions->blink_count;
        state.lookFrequency = expressions->look_frequency;
        state.lookLength = expressions->look_length;
        return state;
    }

    SetExpressionMasterStateCommand::SetExpressionMasterStateCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity expressionEntity,
        const ExpressionMasterState& after)
        : scene_(&scene),
          expressionEntity_(expressionEntity),
          before_(CaptureExpressionMasterState(scene, expressionEntity)),
          after_(after)
    {
    }

    bool SetExpressionMasterStateCommand::Apply(
        const ExpressionMasterState& state) noexcept
    {
        if (scene_ == nullptr || expressionEntity_ == wi::ecs::INVALID_ENTITY ||
            !state.componentExists)
        {
            return false;
        }

        auto* expressions = scene_->expressions.GetComponent(expressionEntity_);
        if (expressions == nullptr)
            return false;

        expressions->SetForceTalkingEnabled(state.forceTalking);
        expressions->blink_frequency = state.blinkFrequency;
        expressions->blink_length = state.blinkLength;
        expressions->blink_count = state.blinkCount;
        expressions->look_frequency = state.lookFrequency;
        expressions->look_length = state.lookLength;
        return true;
    }

    bool SetExpressionMasterStateCommand::Execute()
    {
        if (Same(before_, after_))
            return false;
        return Apply(after_);
    }

    void SetExpressionMasterStateCommand::Undo()
    {
        (void)Apply(before_);
    }

    ExpressionEntryState CaptureExpressionEntryState(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity expressionEntity,
        const std::size_t expressionIndex) noexcept
    {
        ExpressionEntryState state;
        const auto* expressions = scene.expressions.GetComponent(expressionEntity);
        if (expressions == nullptr || expressionIndex >= expressions->expressions.size())
            return state;

        const auto& expression = expressions->expressions[expressionIndex];
        state.valid = true;
        state.binary = expression.IsBinary();
        state.weight = expression.weight;
        state.overrideMouth = expression.override_mouth;
        state.overrideBlink = expression.override_blink;
        state.overrideLook = expression.override_look;
        return state;
    }

    SetExpressionEntryStateCommand::SetExpressionEntryStateCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity expressionEntity,
        const std::size_t expressionIndex,
        const ExpressionEntryState& after)
        : scene_(&scene),
          expressionEntity_(expressionEntity),
          expressionIndex_(expressionIndex),
          before_(CaptureExpressionEntryState(scene, expressionEntity, expressionIndex)),
          after_(after)
    {
    }

    bool SetExpressionEntryStateCommand::Apply(
        const ExpressionEntryState& state) noexcept
    {
        if (scene_ == nullptr || expressionEntity_ == wi::ecs::INVALID_ENTITY || !state.valid)
            return false;

        auto* expressions = scene_->expressions.GetComponent(expressionEntity_);
        if (expressions == nullptr || expressionIndex_ >= expressions->expressions.size())
            return false;

        auto& expression = expressions->expressions[expressionIndex_];
        expression.SetBinary(state.binary);
        expression.weight = std::clamp(state.weight, 0.0f, 1.0f);
        expression.override_mouth = state.overrideMouth;
        expression.override_blink = state.overrideBlink;
        expression.override_look = state.overrideLook;
        expression.SetDirty();
        return true;
    }

    bool SetExpressionEntryStateCommand::Execute()
    {
        if (!before_.valid || !after_.valid || Same(before_, after_))
            return false;
        return Apply(after_);
    }

    void SetExpressionEntryStateCommand::Undo()
    {
        (void)Apply(before_);
    }

    wi::ecs::Entity FindExpressionEntity(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity humanoidEntity) noexcept
    {
        if (humanoidEntity == wi::ecs::INVALID_ENTITY)
            return wi::ecs::INVALID_ENTITY;

        if (scene.expressions.Contains(humanoidEntity))
            return humanoidEntity;

        for (std::size_t i = 0; i < scene.expressions.GetCount(); ++i)
        {
            const auto entity = scene.expressions.GetEntity(i);
            if (entity == humanoidEntity ||
                scene.Entity_IsDescendant(entity, humanoidEntity) ||
                scene.Entity_IsDescendant(humanoidEntity, entity))
            {
                return entity;
            }
        }

        return wi::ecs::INVALID_ENTITY;
    }
}
