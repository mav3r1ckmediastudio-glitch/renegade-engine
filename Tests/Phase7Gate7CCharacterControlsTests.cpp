#include "renegade/bridge/CharacterAnimationControlService.h"

#include <iostream>
#include <memory>

namespace
{
    bool Require(const bool condition, const char* message)
    {
        if (!condition)
            std::cerr << "Phase7Gate7C test failure: " << message << '\n';
        return condition;
    }
}

int main()
{
    wi::scene::Scene scene;
    renegade::bridge::CommandService commands;

    const auto rig = wi::ecs::CreateEntity();
    scene.names.Create(rig).name = "Character";
    scene.transforms.Create(rig);
    scene.armatures.Create(rig);
    scene.humanoids.Create(rig);

    const auto effector = wi::ecs::CreateEntity();
    scene.names.Create(effector).name = "RightHand";
    scene.transforms.Create(effector);
    scene.Component_Attach(effector, rig);

    const auto target = wi::ecs::CreateEntity();
    scene.names.Create(target).name = "IK Target";
    scene.transforms.Create(target);

    auto ikAfter = renegade::bridge::CaptureInverseKinematicsState(scene, effector);
    ikAfter.componentExists = true;
    ikAfter.target = target;
    ikAfter.chainLength = 3;
    ikAfter.iterationCount = 4;
    if (!Require(commands.Execute(
            std::make_unique<renegade::bridge::SetInverseKinematicsStateCommand>(
                scene, effector, ikAfter)),
            "IK command should create native component")) return 1;

    const auto* ik = scene.inverse_kinematics.GetComponent(effector);
    if (!Require(ik != nullptr, "IK component missing after command")) return 1;
    if (!Require(ik->target == target, "IK target mismatch")) return 1;
    if (!Require(ik->chain_length == 3, "IK chain length mismatch")) return 1;
    if (!Require(ik->iteration_count == 4, "IK iteration count mismatch")) return 1;
    if (!Require(commands.Undo(), "IK undo should succeed")) return 1;
    if (!Require(!scene.inverse_kinematics.Contains(effector),
            "IK undo should remove newly-created component")) return 1;
    if (!Require(commands.Redo(), "IK redo should succeed")) return 1;
    if (!Require(scene.inverse_kinematics.Contains(effector),
            "IK redo should restore component")) return 1;

    auto lookAfter = renegade::bridge::CaptureHumanoidLookAtState(scene, rig);
    lookAfter.enabled = true;
    lookAfter.target = target;
    lookAfter.headRotationMax = XMFLOAT2(0.7f, 0.4f);
    lookAfter.headRotationSpeed = 0.35f;
    lookAfter.eyeRotationMax = XMFLOAT2(0.2f, 0.15f);
    lookAfter.eyeRotationSpeed = 0.45f;
    if (!Require(commands.Execute(
            std::make_unique<renegade::bridge::SetHumanoidLookAtStateCommand>(
                scene, rig, lookAfter)),
            "look-at command should commit")) return 1;

    const auto* humanoid = scene.humanoids.GetComponent(rig);
    if (!Require(humanoid != nullptr && humanoid->IsLookAtEnabled(),
            "look-at should be enabled")) return 1;
    if (!Require(humanoid->lookAtEntity == target, "look-at target mismatch")) return 1;
    if (!Require(humanoid->head_rotation_speed == 0.35f,
            "head speed mismatch")) return 1;
    if (!Require(commands.Undo(), "look-at undo should succeed")) return 1;
    if (!Require(!scene.humanoids.GetComponent(rig)->IsLookAtEnabled(),
            "look-at undo should restore disabled state")) return 1;

    const auto expressionEntity = wi::ecs::CreateEntity();
    scene.names.Create(expressionEntity).name = "Expressions";
    scene.transforms.Create(expressionEntity);
    scene.Component_Attach(expressionEntity, rig);
    auto& expressionMaster = scene.expressions.Create(expressionEntity);
    expressionMaster.expressions.emplace_back();
    expressionMaster.expressions.back().name = "Smile";

    if (!Require(
            renegade::bridge::FindExpressionEntity(scene, rig) == expressionEntity,
            "expression owner should resolve through character hierarchy")) return 1;

    auto masterAfter = renegade::bridge::CaptureExpressionMasterState(scene, expressionEntity);
    masterAfter.forceTalking = true;
    masterAfter.blinkFrequency = 0.55f;
    masterAfter.blinkLength = 0.2f;
    masterAfter.blinkCount = 3;
    masterAfter.lookFrequency = 0.4f;
    masterAfter.lookLength = 0.7f;
    if (!Require(commands.Execute(
            std::make_unique<renegade::bridge::SetExpressionMasterStateCommand>(
                scene, expressionEntity, masterAfter)),
            "expression master command should commit")) return 1;

    const auto* expressionMasterView = scene.expressions.GetComponent(expressionEntity);
    if (!Require(expressionMasterView->IsForceTalkingEnabled(),
            "force talking should be enabled")) return 1;
    if (!Require(expressionMasterView->blink_count == 3,
            "blink count mismatch")) return 1;
    if (!Require(commands.Undo(), "expression master undo should succeed")) return 1;
    if (!Require(!scene.expressions.GetComponent(expressionEntity)->IsForceTalkingEnabled(),
            "expression master undo should restore force talking")) return 1;

    auto entryAfter = renegade::bridge::CaptureExpressionEntryState(scene, expressionEntity, 0);
    entryAfter.binary = true;
    entryAfter.weight = 0.75f;
    entryAfter.overrideMouth = renegade::bridge::ExpressionOverride::Blend;
    entryAfter.overrideBlink = renegade::bridge::ExpressionOverride::Block;
    entryAfter.overrideLook = renegade::bridge::ExpressionOverride::Blend;
    if (!Require(commands.Execute(
            std::make_unique<renegade::bridge::SetExpressionEntryStateCommand>(
                scene, expressionEntity, 0, entryAfter)),
            "expression entry command should commit")) return 1;

    const auto& expression = scene.expressions.GetComponent(expressionEntity)->expressions[0];
    if (!Require(expression.IsBinary(), "expression binary flag mismatch")) return 1;
    if (!Require(expression.weight == 0.75f, "expression weight mismatch")) return 1;
    if (!Require(expression.override_mouth == renegade::bridge::ExpressionOverride::Blend,
            "expression mouth override mismatch")) return 1;
    if (!Require(commands.Undo(), "expression entry undo should succeed")) return 1;
    if (!Require(!scene.expressions.GetComponent(expressionEntity)->expressions[0].IsBinary(),
            "expression entry undo should restore binary flag")) return 1;

    std::cout << "Phase 7C character controls tests passed\n";
    return 0;
}
