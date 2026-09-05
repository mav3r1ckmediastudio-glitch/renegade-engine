#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>

#include "renegade/bridge/ConstraintService.h"

namespace
{
    bool NearlyEqual(const float left, const float right)
    {
        return std::abs(left - right) < 0.001f;
    }

    bool NearlyEqual(const XMFLOAT3& left, const XMFLOAT3& right)
    {
        return NearlyEqual(left.x, right.x) &&
            NearlyEqual(left.y, right.y) &&
            NearlyEqual(left.z, right.z);
    }

    int Fail(const char* message)
    {
        std::cerr << "FAIL: " << message << '\n';
        return 1;
    }
}

int main()
{
    using renegade::bridge::ConstraintState;
    using Constraint = wi::scene::PhysicsConstraintComponent;
    using Type = Constraint::Type;

    // JP01 must recognize every constraint type exposed by Wicked Editor.
    {
        constexpr std::array<Type, 8> types = {
            Type::Fixed,
            Type::Point,
            Type::Distance,
            Type::Hinge,
            Type::Cone,
            Type::SixDOF,
            Type::SwingTwist,
            Type::Slider,
        };
        for (const auto type : types)
        {
            ConstraintState state;
            state.type = type;
            if (renegade::bridge::SanitizeConstraintState(state).type != type)
            {
                return Fail("a Wicked constraint type was rejected");
            }
        }

        ConstraintState invalid;
        invalid.type = static_cast<Type>(999u);
        invalid.breakDistance = -2.0f;
        invalid.distanceMin = -5.0f;
        invalid.distanceMax = -1.0f;
        invalid.hingeMinAngle = -100.0f;
        invalid.hingeMaxAngle = 100.0f;
        invalid.coneHalfAngle = 100.0f;
        invalid.swingTwistNormalHalfConeAngle = 100.0f;
        invalid.swingTwistPlaneHalfConeAngle = 100.0f;
        invalid.sliderMaxForce = -50.0f;
        invalid.sliderTargetVelocity =
            std::numeric_limits<float>::quiet_NaN();
        const auto safe = renegade::bridge::SanitizeConstraintState(invalid);
        if (safe.type != Type::Fixed || safe.breakDistance != 0.0f ||
            safe.distanceMin != 0.0f || safe.distanceMax != 0.0f ||
            safe.hingeMinAngle < -XM_PI || safe.hingeMaxAngle > XM_PI ||
            safe.coneHalfAngle > XM_PIDIV2 ||
            safe.swingTwistNormalHalfConeAngle > XM_PIDIV2 ||
            safe.swingTwistPlaneHalfConeAngle > XM_PIDIV2 ||
            safe.sliderMaxForce != 0.0f ||
            safe.sliderTargetVelocity != 0.0f)
        {
            return Fail("constraint sanitization accepted invalid authoring values");
        }
    }

    wi::scene::Scene scene;

    const auto bodyA = wi::ecs::CreateEntity();
    scene.transforms.Create(bodyA);
    scene.rigidbodies.Create(bodyA);

    const auto bodyB = wi::ecs::CreateEntity();
    scene.transforms.Create(bodyB);
    scene.rigidbodies.Create(bodyB);

    // Constraints need a Transform because Wicked takes pivot/axes from the
    // constraint entity's transform when it binds the Jolt constraint.
    const auto missingTransform = wi::ecs::CreateEntity();
    renegade::bridge::CommandService invalidCreate;
    if (invalidCreate.Execute(
            std::make_unique<renegade::bridge::CreateConstraintCommand>(
                scene,
                missingTransform)) ||
        invalidCreate.UndoCount() != 0)
    {
        return Fail("constraint creation accepted an entity without Transform");
    }

    const auto constraintEntity = wi::ecs::CreateEntity();
    scene.transforms.Create(constraintEntity);

    ConstraintState initial;
    initial.type = Type::Hinge;
    initial.bodyA = bodyA;
    initial.bodyB = bodyB;
    initial.disableSelfCollision = true;
    initial.breakDistance = 8.0f;
    initial.distanceMin = 0.5f;
    initial.distanceMax = 3.5f;
    initial.hingeMinAngle = -0.5f;
    initial.hingeMaxAngle = 0.75f;
    initial.hingeTargetAngularVelocity = 2.0f;
    initial.coneHalfAngle = 0.4f;
    initial.sixDofMinTranslation = XMFLOAT3(-1.0f, -2.0f, -3.0f);
    initial.sixDofMaxTranslation = XMFLOAT3(1.0f, 2.0f, 3.0f);
    initial.sixDofMinRotation = XMFLOAT3(-0.1f, -0.2f, -0.3f);
    initial.sixDofMaxRotation = XMFLOAT3(0.1f, 0.2f, 0.3f);
    initial.swingTwistNormalHalfConeAngle = 0.25f;
    initial.swingTwistPlaneHalfConeAngle = 0.35f;
    initial.swingTwistMinAngle = -0.45f;
    initial.swingTwistMaxAngle = 0.55f;
    initial.sliderMinLimit = -4.0f;
    initial.sliderMaxLimit = 5.0f;
    initial.sliderTargetVelocity = 6.0f;
    initial.sliderMaxForce = 700.0f;

    renegade::bridge::CommandService createCommands;
    if (!createCommands.Execute(
            std::make_unique<renegade::bridge::CreateConstraintCommand>(
                scene,
                constraintEntity,
                initial)))
    {
        return Fail("CreateConstraintCommand did not execute");
    }

    auto* constraint = scene.constraints.GetComponent(constraintEntity);
    if (constraint == nullptr || constraint->type != Type::Hinge ||
        constraint->bodyA != bodyA || constraint->bodyB != bodyB ||
        !constraint->IsDisableSelfCollision() ||
        !NearlyEqual(constraint->break_distance, 8.0f) ||
        !NearlyEqual(constraint->distance_constraint.min_distance, 0.5f) ||
        !NearlyEqual(constraint->distance_constraint.max_distance, 3.5f) ||
        !NearlyEqual(constraint->hinge_constraint.min_angle, -0.5f) ||
        !NearlyEqual(constraint->hinge_constraint.max_angle, 0.75f) ||
        !NearlyEqual(
            constraint->hinge_constraint.target_angular_velocity,
            2.0f) ||
        !NearlyEqual(constraint->cone_constraint.half_cone_angle, 0.4f) ||
        !NearlyEqual(
            constraint->six_dof.minTranslationAxes,
            XMFLOAT3(-1.0f, -2.0f, -3.0f)) ||
        !NearlyEqual(
            constraint->six_dof.maxTranslationAxes,
            XMFLOAT3(1.0f, 2.0f, 3.0f)) ||
        !NearlyEqual(
            constraint->swing_twist.normal_half_cone_angle,
            0.25f) ||
        !NearlyEqual(constraint->slider_constraint.min_limit, -4.0f) ||
        !NearlyEqual(constraint->slider_constraint.max_limit, 5.0f) ||
        !NearlyEqual(constraint->slider_constraint.target_velocity, 6.0f) ||
        !NearlyEqual(constraint->slider_constraint.max_force, 700.0f) ||
        !constraint->IsRefreshParametersNeeded())
    {
        return Fail("constraint creator state did not reach Wicked component");
    }

    if (!createCommands.Undo() || scene.constraints.Contains(constraintEntity) ||
        !scene.transforms.Contains(constraintEntity) ||
        !createCommands.Redo() || !scene.constraints.Contains(constraintEntity))
    {
        return Fail("CreateConstraintCommand Undo/Redo lifecycle failed");
    }
    constraint = scene.constraints.GetComponent(constraintEntity);
    if (constraint == nullptr)
    {
        return Fail("CreateConstraintCommand Redo lost constraint component");
    }

    renegade::bridge::CommandService duplicateCommands;
    if (duplicateCommands.Execute(
            std::make_unique<renegade::bridge::CreateConstraintCommand>(
                scene,
                constraintEntity,
                initial)) ||
        duplicateCommands.UndoCount() != 0)
    {
        return Fail("CreateConstraintCommand overwrote an existing constraint");
    }

    // One state contains the full Wicked editor surface, irrespective of the
    // currently selected constraint type. This lets type switching preserve
    // specialist settings instead of destroying them.
    const auto before = renegade::bridge::CaptureConstraint(*constraint);
    auto after = before;
    after.type = Type::SixDOF;
    after.bodyB = wi::ecs::INVALID_ENTITY;
    after.disableSelfCollision = false;
    after.breakDistance = 12.0f;
    after.distanceMin = 1.25f;
    after.distanceMax = 9.0f;
    after.hingeMinAngle = -0.8f;
    after.hingeMaxAngle = 0.9f;
    after.hingeTargetAngularVelocity = -3.0f;
    after.coneHalfAngle = 0.65f;
    after.sixDofMinTranslation = XMFLOAT3(-6.0f, -7.0f, -8.0f);
    after.sixDofMaxTranslation = XMFLOAT3(6.0f, 7.0f, 8.0f);
    after.sixDofMinRotation = XMFLOAT3(-0.6f, -0.7f, -0.8f);
    after.sixDofMaxRotation = XMFLOAT3(0.6f, 0.7f, 0.8f);
    after.swingTwistNormalHalfConeAngle = 0.5f;
    after.swingTwistPlaneHalfConeAngle = 0.6f;
    after.swingTwistMinAngle = -0.7f;
    after.swingTwistMaxAngle = 0.8f;
    after.sliderMinLimit = -10.0f;
    after.sliderMaxLimit = 11.0f;
    after.sliderTargetVelocity = -12.0f;
    after.sliderMaxForce = 1300.0f;

    renegade::bridge::CommandService editCommands;
    if (!editCommands.Execute(
            std::make_unique<renegade::bridge::SetConstraintCommand>(
                scene,
                constraintEntity,
                before,
                after)))
    {
        return Fail("SetConstraintCommand did not execute");
    }
    if (constraint->type != Type::SixDOF ||
        constraint->bodyA != bodyA ||
        constraint->bodyB != wi::ecs::INVALID_ENTITY ||
        constraint->IsDisableSelfCollision() ||
        !NearlyEqual(constraint->break_distance, 12.0f) ||
        !NearlyEqual(constraint->distance_constraint.min_distance, 1.25f) ||
        !NearlyEqual(constraint->hinge_constraint.min_angle, -0.8f) ||
        !NearlyEqual(constraint->cone_constraint.half_cone_angle, 0.65f) ||
        !NearlyEqual(
            constraint->six_dof.minTranslationAxes,
            XMFLOAT3(-6.0f, -7.0f, -8.0f)) ||
        !NearlyEqual(
            constraint->six_dof.maxRotationAxes,
            XMFLOAT3(0.6f, 0.7f, 0.8f)) ||
        !NearlyEqual(
            constraint->swing_twist.plane_half_cone_angle,
            0.6f) ||
        !NearlyEqual(constraint->slider_constraint.min_limit, -10.0f) ||
        !NearlyEqual(constraint->slider_constraint.max_force, 1300.0f) ||
        !constraint->IsRefreshParametersNeeded())
    {
        return Fail("SetConstraintCommand missed Wicked editor fields");
    }

    if (!editCommands.Undo() || constraint->type != Type::Hinge ||
        constraint->bodyB != bodyB || !constraint->IsDisableSelfCollision() ||
        !NearlyEqual(constraint->hinge_constraint.min_angle, -0.5f) ||
        !NearlyEqual(constraint->slider_constraint.max_force, 700.0f) ||
        !editCommands.Redo() || constraint->type != Type::SixDOF ||
        constraint->bodyB != wi::ecs::INVALID_ENTITY)
    {
        return Fail("SetConstraintCommand Undo/Redo lifecycle failed");
    }

    const auto current = renegade::bridge::CaptureConstraint(*constraint);
    renegade::bridge::CommandService noOpCommands;
    if (noOpCommands.Execute(
            std::make_unique<renegade::bridge::SetConstraintCommand>(
                scene,
                constraintEntity,
                current,
                current)) ||
        noOpCommands.UndoCount() != 0)
    {
        return Fail("identical constraint state polluted command history");
    }

    constraint->SetRefreshParametersNeeded(false);
    if (!renegade::bridge::RebindConstraint(scene, constraintEntity) ||
        !constraint->IsRefreshParametersNeeded())
    {
        return Fail("constraint rebind did not request native recreation");
    }

    renegade::bridge::CommandService removeCommands;
    if (!removeCommands.Execute(
            std::make_unique<renegade::bridge::RemoveConstraintCommand>(
                scene,
                constraintEntity)) ||
        scene.constraints.Contains(constraintEntity) ||
        !removeCommands.Undo() || !scene.constraints.Contains(constraintEntity))
    {
        return Fail("RemoveConstraintCommand lifecycle failed");
    }
    constraint = scene.constraints.GetComponent(constraintEntity);
    if (constraint == nullptr || constraint->type != Type::SixDOF ||
        constraint->bodyA != bodyA ||
        constraint->bodyB != wi::ecs::INVALID_ENTITY ||
        !NearlyEqual(constraint->break_distance, 12.0f) ||
        !NearlyEqual(
            constraint->six_dof.minTranslationAxes,
            XMFLOAT3(-6.0f, -7.0f, -8.0f)) ||
        !NearlyEqual(constraint->slider_constraint.max_force, 1300.0f) ||
        constraint->physicsobject != nullptr ||
        !constraint->IsRefreshParametersNeeded())
    {
        return Fail("Remove constraint Undo did not restore native state safely");
    }

    const auto blankConstraintEntity = wi::ecs::CreateEntity();
    scene.transforms.Create(blankConstraintEntity);
    renegade::bridge::CommandService blankCommands;
    if (!blankCommands.Execute(
            std::make_unique<renegade::bridge::CreateConstraintCommand>(
                scene,
                blankConstraintEntity)))
    {
        return Fail("Wicked-parity blank constraint could not be authored");
    }
    const auto* blank = scene.constraints.GetComponent(blankConstraintEntity);
    if (blank == nullptr || blank->bodyA != wi::ecs::INVALID_ENTITY ||
        blank->bodyB != wi::ecs::INVALID_ENTITY)
    {
        return Fail("blank constraint did not preserve Wicked default binding state");
    }

    std::cout << "PASS: JP01 Wicked constraint authoring parity\n";
    return 0;
}
