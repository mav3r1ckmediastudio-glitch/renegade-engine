#include "RuntimePlayerViewRig.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
    bool Near(
        const float left,
        const float right,
        const float epsilon = 0.002f)
    {
        return std::abs(left - right) <= epsilon;
    }

    bool Near3(
        const XMFLOAT3& left,
        const XMFLOAT3& right,
        const float epsilon = 0.002f)
    {
        return Near(left.x, right.x, epsilon) &&
            Near(left.y, right.y, epsilon) &&
            Near(left.z, right.z, epsilon);
    }

    [[noreturn]] void Fail(const std::string& message)
    {
        std::cerr << "PLAYER VIEW RIG FAIL // " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

int main()
{
    using namespace renegade::runtime;

    wi::scene::Scene scene;

    const auto player =
        scene.Entity_CreateTransform("__p1_authoritative_player");
    auto* playerTransform = scene.transforms.GetComponent(player);
    if (playerTransform == nullptr)
        Fail("could not create authoritative Player transform");

    playerTransform->translation_local = XMFLOAT3(8.0f, 0.75f, -3.0f);
    XMStoreFloat4(
        &playerTransform->rotation_local,
        XMQuaternionRotationRollPitchYaw(
            wi::math::DegreesToRadians(9.0f),
            wi::math::DegreesToRadians(35.0f),
            wi::math::DegreesToRadians(-6.0f)));
    playerTransform->SetDirty();
    playerTransform->UpdateTransform();

    RuntimePlayerViewRigState rig;
    RuntimePlayerViewRigSettings settings;
    settings.createProofGeometry = false;

    constexpr float eyeHeight = 1.72f;
    std::string error;
    if (!SpawnRuntimePlayerViewRig(
            scene, rig, player, eyeHeight, error, settings))
    {
        Fail("socket-only View Rig spawn failed: " + error);
    }

    if (!rig.IsSpawned() ||
        rig.playerEntity != player ||
        rig.presentationRoot == wi::ecs::INVALID_ENTITY ||
        rig.primaryHandSocket == wi::ecs::INVALID_ENTITY ||
        rig.offHandSocket == wi::ecs::INVALID_ENTITY ||
        rig.twoHandSupportSocket == wi::ecs::INVALID_ENTITY)
    {
        Fail("View Rig did not create all first-class hand sockets");
    }
    const auto* rootHierarchy = scene.hierarchy.GetComponent(rig.root);
    const auto* presentationHierarchy =
        scene.hierarchy.GetComponent(rig.presentationRoot);
    const auto* primaryHierarchy =
        scene.hierarchy.GetComponent(rig.primaryHandSocket);
    const auto* offHierarchy =
        scene.hierarchy.GetComponent(rig.offHandSocket);
    const auto* supportHierarchy =
        scene.hierarchy.GetComponent(rig.twoHandSupportSocket);
    if (rootHierarchy == nullptr ||
        rootHierarchy->parentID != player ||
        presentationHierarchy == nullptr ||
        presentationHierarchy->parentID != rig.root ||
        primaryHierarchy == nullptr ||
        offHierarchy == nullptr ||
        supportHierarchy == nullptr ||
        primaryHierarchy->parentID != rig.presentationRoot ||
        offHierarchy->parentID != rig.presentationRoot ||
        supportHierarchy->parentID != rig.presentationRoot)
    {
        Fail("View Rig is not parented through the authoritative Player");
    }

    const auto* primary =
        scene.transforms.GetComponent(rig.primaryHandSocket);
    const auto* off =
        scene.transforms.GetComponent(rig.offHandSocket);
    if (primary == nullptr || off == nullptr ||
        !Near(primary->translation_local.x, settings.primaryHandOffset.x) ||
        !Near(off->translation_local.x, settings.offHandOffset.x) ||
        !(primary->translation_local.x > 0.0f) ||
        !(off->translation_local.x < 0.0f))
    {
        Fail("primary/off-hand socket offsets are not independent");
    }

    // Prove the generic policy separately from GPU primitive creation.
    const auto modelRoot = scene.Entity_CreateTransform("View Model Root");
    const auto modelChild = scene.Entity_CreateTransform("View Model Child");
    scene.objects.Create(modelRoot);
    scene.objects.Create(modelChild);
    scene.Component_Attach(modelChild, modelRoot, true);

    const std::size_t configured =
        ConfigureRuntimeViewModelHierarchy(scene, modelRoot);
    const auto* rootObject = scene.objects.GetComponent(modelRoot);
    const auto* childObject = scene.objects.GetComponent(modelChild);
    if (configured != 2 ||
        rootObject == nullptr ||
        childObject == nullptr ||
        !rootObject->IsForeground() ||
        !childObject->IsForeground() ||
        rootObject->IsCastingShadow() ||
        childObject->IsCastingShadow() ||
        !rootObject->IsNotVisibleInReflections() ||
        !childObject->IsNotVisibleInReflections() ||
        rootObject->IsNotVisibleInMainCamera() ||
        childObject->IsNotVisibleInMainCamera())
    {
        Fail("Wicked foreground/reflection/shadow view-model policy is wrong");
    }
    const auto importedViewModel =
        scene.Entity_CreateTransform("Imported First Person Arms");
    const auto importedMesh =
        scene.Entity_CreateTransform("Imported First Person Arms Mesh");
    scene.objects.Create(importedMesh);
    scene.Component_Attach(importedMesh, importedViewModel, true);
    if (!AttachRuntimeViewModelHierarchy(
            scene,
            rig,
            importedViewModel,
            PlayerViewRigSocket::PresentationRoot,
            error))
    {
        Fail("real view-model hierarchy attachment seam failed: " + error);
    }
    const auto* importedHierarchy =
        scene.hierarchy.GetComponent(importedViewModel);
    const auto* importedObject = scene.objects.GetComponent(importedMesh);
    if (importedHierarchy == nullptr ||
        importedHierarchy->parentID != rig.presentationRoot ||
        importedObject == nullptr ||
        !importedObject->IsForeground() ||
        importedObject->IsCastingShadow() ||
        !importedObject->IsNotVisibleInReflections())
    {
        Fail("attached view-model hierarchy did not inherit P1 foreground policy");
    }

    renegade::bridge::PlayerInputFrame presentationInput;
    UpdateRuntimePlayerViewRigPresentation(
        scene, rig, presentationInput, 1.0f / 60.0f);
    if (rig.action != PlayerViewAction::Idle)
        Fail("stationary Player did not resolve Idle view action");

    presentationInput.moveForward = 1.0f;
    UpdateRuntimePlayerViewRigPresentation(
        scene, rig, presentationInput, 1.0f / 60.0f);
    if (rig.action != PlayerViewAction::Walk)
        Fail("moving Player did not resolve Walk view action");

    presentationInput.sprintDown = true;
    UpdateRuntimePlayerViewRigPresentation(
        scene, rig, presentationInput, 1.0f / 60.0f);
    if (rig.action != PlayerViewAction::Sprint)
        Fail("sprinting Player did not resolve Sprint view action");

    const auto* presentation =
        scene.transforms.GetComponent(rig.presentationRoot);
    if (presentation == nullptr ||
        (Near(presentation->translation_local.x, 0.0f) &&
         Near(presentation->translation_local.y, 0.0f)))
    {
        Fail("semantic movement presentation did not affect the View Rig");
    }

    const float desiredYaw = wi::math::DegreesToRadians(65.0f);
    const float desiredPitch = wi::math::DegreesToRadians(-20.0f);
    if (!PoseRuntimePlayerViewRig(
            scene, rig, desiredYaw, desiredPitch))
    {
        Fail("View Rig did not accept Player look state");
    }

    auto* rigRoot = scene.transforms.GetComponent(rig.root);
    const auto* playerCurrent = scene.transforms.GetComponent(player);
    if (rigRoot == nullptr || playerCurrent == nullptr)
        Fail("View Rig or authoritative Player transform disappeared");

    const XMFLOAT3 playerPosition = playerCurrent->GetPosition();
    const XMFLOAT3 expectedEye(
        playerPosition.x,
        playerPosition.y + eyeHeight,
        playerPosition.z);
    if (!Near3(rigRoot->GetPosition(), expectedEye))
        Fail("parented View Rig did not preserve world-space eye height");

    wi::scene::TransformComponent expectedCameraTransform;
    expectedCameraTransform.Translate(expectedEye);
    expectedCameraTransform.RotateRollPitchYaw(
        XMFLOAT3(desiredPitch, desiredYaw, 0.0f));
    expectedCameraTransform.UpdateTransform();

    if (!Near3(
            rigRoot->GetForward(),
            expectedCameraTransform.GetForward()) ||
        !Near3(
            rigRoot->GetRight(),
            expectedCameraTransform.GetRight()) ||
        !Near3(
            rigRoot->GetUp(),
            expectedCameraTransform.GetUp()))
    {
        Fail("View Rig world orientation double-applied the Player Start rotation");
    }

    // Simulate the Jolt parent moving before Wicked hierarchy propagation.
    auto* playerMoved = scene.transforms.GetComponent(player);
    rigRoot = scene.transforms.GetComponent(rig.root);
    if (playerMoved == nullptr || rigRoot == nullptr)
        Fail("View Rig or Player transform disappeared before movement proof");

    playerMoved->translation_local = XMFLOAT3(10.0f, 1.25f, -1.0f);
    playerMoved->SetDirty();
    playerMoved->UpdateTransform();
    rigRoot->UpdateTransform_Parented(*playerMoved);

    const XMFLOAT3 movedEye(10.0f, 1.25f + eyeHeight, -1.0f);
    if (!Near3(rigRoot->GetPosition(), movedEye))
        Fail("View Rig did not inherit authoritative Player movement");

    RuntimePlayerViewRigState duplicateState = rig;
    if (SpawnRuntimePlayerViewRig(
            scene,
            duplicateState,
            player,
            eyeHeight,
            error,
            settings))
    {
        Fail("a second spawn over an active View Rig was not rejected");
    }

    const auto rootEntity = rig.root;
    const auto primaryEntity = rig.primaryHandSocket;
    const auto offEntity = rig.offHandSocket;
    DespawnRuntimePlayerViewRig(scene, rig);
    if (rig.IsSpawned() ||
        scene.transforms.Contains(rootEntity) ||
        scene.transforms.Contains(primaryEntity) ||
        scene.transforms.Contains(offEntity) ||
        !scene.transforms.Contains(player))
    {
        Fail("View Rig despawn damaged Player ownership or left transient entities");
    }

    std::cout
        << "PASS: parented dual-hand Player View Rig and Wicked foreground policy\n";
    return EXIT_SUCCESS;
}
