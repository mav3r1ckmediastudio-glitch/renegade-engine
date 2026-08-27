#include <cmath>
#include <iostream>
#include <memory>

#include "renegade/bridge/RagdollPhysicsService.h"

namespace
{
    bool NearlyEqual(const float left, const float right)
    {
        return std::abs(left - right) < 0.001f;
    }

    int Fail(const char* message)
    {
        std::cerr << "FAIL: " << message << '\n';
        return 1;
    }
}

int main()
{
    using renegade::bridge::RagdollPhysicsState;

    {
        RagdollPhysicsState dirty;
        dirty.fatness = -5.0f;
        dirty.headSize = 10.0f;
        const auto safe = renegade::bridge::SanitizeRagdollPhysicsState(dirty);
        if (!NearlyEqual(safe.fatness, 0.5f) ||
            !NearlyEqual(safe.headSize, 2.0f))
        {
            return Fail("ragdoll sanitization diverged from Wicked Editor ranges");
        }
    }

    wi::scene::Scene scene;
    const auto entity = wi::ecs::CreateEntity();
    scene.transforms.Create(entity);
    auto& humanoid = scene.humanoids.Create(entity);

    RagdollPhysicsState configured;
    configured.disabled = false;
    configured.physicsEnabled = true;
    configured.locked2D = true;
    configured.fatness = 1.4f;
    configured.headSize = 0.8f;

    renegade::bridge::CommandService commands;
    if (!commands.Execute(
            std::make_unique<renegade::bridge::SetRagdollPhysicsCommand>(
                scene,
                entity,
                configured)))
    {
        return Fail("SetRagdollPhysicsCommand did not execute");
    }

    if (humanoid.IsRagdollDisabled() ||
        !humanoid.IsRagdollPhysicsEnabled() ||
        !humanoid.IsRagdoll2D() ||
        !NearlyEqual(humanoid.ragdoll_fatness, 1.4f) ||
        !NearlyEqual(humanoid.ragdoll_headsize, 0.8f) ||
        humanoid.ragdoll != nullptr)
    {
        return Fail("ragdoll authoring missed Wicked component state");
    }

    if (!commands.Undo() || humanoid.IsRagdollPhysicsEnabled() ||
        humanoid.IsRagdoll2D() ||
        !NearlyEqual(humanoid.ragdoll_fatness, 1.0f) ||
        !NearlyEqual(humanoid.ragdoll_headsize, 1.0f) ||
        !commands.Redo() || !humanoid.IsRagdollPhysicsEnabled() ||
        !humanoid.IsRagdoll2D())
    {
        return Fail("ragdoll authoring Undo/Redo lifecycle failed");
    }

    const auto current = renegade::bridge::CaptureRagdollPhysics(humanoid);
    renegade::bridge::CommandService noOpCommands;
    if (noOpCommands.Execute(
            std::make_unique<renegade::bridge::SetRagdollPhysicsCommand>(
                scene,
                entity,
                current,
                current)) ||
        noOpCommands.UndoCount() != 0)
    {
        return Fail("identical ragdoll state polluted command history");
    }

    // A HumanoidComponent does not imply that Wicked has created the native
    // ragdoll yet. Runtime helpers must report that distinction safely.
    if (renegade::bridge::HasLiveRagdollPhysics(scene, entity) ||
        renegade::bridge::ApplyRagdollImpulse(
            scene,
            entity,
            wi::scene::HumanoidComponent::HumanoidBone::Hips,
            XMFLOAT3(0, 10, 0)) ||
        renegade::bridge::ApplyRagdollImpulseAt(
            scene,
            entity,
            wi::scene::HumanoidComponent::HumanoidBone::Head,
            XMFLOAT3(0, 10, 0),
            XMFLOAT3(0, 1, 0)) ||
        renegade::bridge::SetRagdollGhostMode(scene, entity, true))
    {
        return Fail("ragdoll runtime API treated an uncreated ragdoll as live");
    }

    RagdollPhysicsState disabled = current;
    disabled.disabled = true;
    renegade::bridge::CommandService disabledCommands;
    if (!disabledCommands.Execute(
            std::make_unique<renegade::bridge::SetRagdollPhysicsCommand>(
                scene,
                entity,
                disabled)) ||
        !humanoid.IsRagdollDisabled() || humanoid.ragdoll != nullptr)
    {
        return Fail("ragdoll disable did not follow Wicked destruction semantics");
    }

    const auto secondEntity = wi::ecs::CreateEntity();
    scene.transforms.Create(secondEntity);
    auto& second = scene.humanoids.Create(secondEntity);
    second.SetRagdollPhysicsEnabled(false);
    renegade::bridge::ActivateAllRagdolls(scene);

    // Wicked Editor's "Activate all ragdolls" action only requests dynamic
    // ragdoll physics on each HumanoidComponent. It deliberately does not
    // clear Ragdoll Disabled, and Wicked's setter refuses to activate a
    // disabled humanoid. Mirror that exact behavior: eligible humanoids are
    // activated, while an explicitly disabled humanoid remains disabled.
    if (!humanoid.IsRagdollDisabled() ||
        humanoid.IsRagdollPhysicsEnabled() ||
        !second.IsRagdollPhysicsEnabled())
    {
        return Fail("ActivateAllRagdolls diverged from Wicked editor disabled-state semantics");
    }

    const auto noHumanoidEntity = wi::ecs::CreateEntity();
    scene.transforms.Create(noHumanoidEntity);
    renegade::bridge::CommandService missingCommands;
    if (missingCommands.Execute(
            std::make_unique<renegade::bridge::SetRagdollPhysicsCommand>(
                scene,
                noHumanoidEntity,
                configured)) ||
        missingCommands.UndoCount() != 0)
    {
        return Fail("ragdoll authoring silently created a HumanoidComponent");
    }

    std::cout << "PASS: JP01 Wicked ragdoll physics parity\n";
    return 0;
}
