#pragma once

#include "PlayerViewSocketFixture.h"

#include <iostream>
#include <string>

// Runs only in the manual GPU fixture, after Application initialization.
inline int RunPlayerViewSocketGraphicsProof(
    wi::Application& app, wi::scene::Scene& world, const std::string& outputPath)
{
    using namespace renegade::runtime;
    wi::scene::Scene generated;
    renegade::tests::CreatePlayerViewSocketAsset(generated, true);
    {
        wi::Archive output(outputPath, false, false);
        if (!output.IsOpen()) return 20;
        generated.Serialize(output);
    }
    wi::scene::Scene reopened;
    {
        wi::Archive input(outputPath, true, false);
        if (!input.IsOpen()) return 21;
        reopened.Serialize(input);
    }

    const auto start = renegade::bridge::ResolvePlayerStart(world);
    renegade::bridge::RuntimePlayerState player;
    RuntimePlayerViewRigState rig;
    RuntimePlayerViewRigSettings settings;
    settings.createProofGeometry = false;
    std::string error;
    if (!renegade::bridge::SpawnRuntimePlayer(world, start.start, player, error) ||
        !SpawnRuntimePlayerViewRig(world, rig, player.entity, start.start.settings.eyeHeight, error, settings) ||
        !CommitRuntimePlayerViewAsset(world, rig, reopened,
            "34343434-3434-4434-8434-343434343434", error))
    {
        std::cerr << error << "\n";
        return 22;
    }
    RuntimePlayerViewAnimationState animation;
    if (!InitializeRuntimePlayerViewAnimations(world, rig, animation, error))
    {
        std::cerr << error << "\n";
        return 23;
    }

    const std::array<wi::ecs::Entity, 3> sockets = {
        rig.primaryHandSocket, rig.offHandSocket, rig.twoHandSupportSocket,
    };
    for (std::size_t role = 0; role < sockets.size(); ++role)
    {
        const auto marker = world.Entity_CreateCube("Socket-bound proof marker");
        world.Component_Attach(marker, sockets[role], true);
        player_view_rig_detail::SetLocalTransform(world, marker,
            XMFLOAT3(0,0,0.02f), XMFLOAT3(0.035f,0.035f,0.035f));
        world.materials.GetComponent(marker)->baseColor =
            role == 0 ? XMFLOAT4(1,0.85f,0.05f,1) :
            role == 1 ? XMFLOAT4(0.15f,1,0.1f,1) : XMFLOAT4(0.8f,0.1f,1,1);
    }
    ConfigureRuntimeViewModelHierarchy(world, rig.root);

    wi::scene::CameraComponent camera;
    wi::RenderPath3D path;
    path.scene = &world;
    path.camera = &camera;
    path.setSceneUpdateEnabled(false);
    path.setDepthOfFieldEnabled(false);
    path.Load();
    app.ActivatePath(&path);

    float maximumSocketError = 0;
    float maximumCameraError = 0;
    float minimumAnimatedZ = 100;
    float maximumAnimatedZ = -100;
    int captures = 0;
    for (int frame = 0; frame < 300; ++frame)
    {
        renegade::bridge::PlayerInputFrame input;
        if (frame >= 60)
        {
            const int direction = ((frame - 60) / 60) % 4;
            input.moveForward = direction == 0 ? 1.0f : direction == 1 ? -1.0f : 0;
            input.moveRight = direction == 2 ? 1.0f : direction == 3 ? -1.0f : 0;
            input.sprintDown = frame >= 180;
        }
        player.yaw = 0.25f * std::sin(float(frame) * 0.015f);
        player.pitch = 0.12f * std::sin(float(frame) * 0.02f);
        (void)renegade::bridge::UpdateRuntimePlayer(world, player, input, start.start.settings);
        (void)PoseRuntimePlayerViewRig(world, rig, player.yaw, player.pitch);
        UpdateRuntimePlayerViewRigPresentation(world, rig, input, 1.0f / 75);
        UpdateRuntimePlayerViewAnimations(world, animation, rig.action, 1.0f / 75);
        world.Update(1.0f / 75);
        renegade::bridge::ApplyRuntimePlayerCamera(world, player, camera, start.start.settings);
        for (std::size_t role = 0; role < sockets.size(); ++role)
        {
            const auto* target = world.transforms.GetComponent(rig.socketTargets[role]);
            const auto* socket = world.transforms.GetComponent(sockets[role]);
            if (target == nullptr || socket == nullptr) return 24;
            maximumSocketError = std::max(maximumSocketError,
                renegade::tests::MatrixDifference(target->world, socket->world));
        }
        const auto eye = world.transforms.GetComponent(rig.root)->GetPosition();
        maximumCameraError = std::max(maximumCameraError,
            std::max({std::abs(eye.x-camera.Eye.x), std::abs(eye.y-camera.Eye.y), std::abs(eye.z-camera.Eye.z)}));
        const float animatedZ = world.transforms.GetComponent(rig.socketTargets[0])->translation_local.z;
        minimumAnimatedZ = std::min(minimumAnimatedZ, animatedZ);
        maximumAnimatedZ = std::max(maximumAnimatedZ, animatedZ);
        app.Run();
        if (frame == 50 || frame == 130 || frame == 230)
        {
            const std::string capture = outputPath + (frame == 50 ? ".idle.png" : frame == 130 ? ".walk.png" : ".sprint.png");
            wi::graphics::GetDevice()->WaitForGPU();
            if (wi::helper::saveTextureToFile(path.GetRenderResult3D(), capture)) ++captures;
        }
    }

    // Freeze both animation and physics with the same zero-dt pause contract.
    world.Update(0);
    const auto pausedWorld = world.transforms.GetComponent(rig.primaryHandSocket)->world;
    const float pausedTimer = world.animations.GetComponent(animation.activeClip)->timer;
    for (int frame = 0; frame < 12; ++frame)
    {
        UpdateRuntimePlayerViewAnimations(world, animation, rig.action, 0);
        world.Update(0);
    }
    const float pauseError = renegade::tests::MatrixDifference(
        pausedWorld, world.transforms.GetComponent(rig.primaryHandSocket)->world);
    const float timerError = std::abs(pausedTimer - world.animations.GetComponent(animation.activeClip)->timer);
    app.ActivatePath(nullptr);
    while (wi::renderer::IsPipelineCreationActive() > 0) Sleep(10);
    wi::graphics::GetDevice()->WaitForGPU();

    std::cout << "SOCKET_MAX_MATRIX_ERROR=" << maximumSocketError
        << " CAMERA_MAX_ERROR=" << maximumCameraError
        << " ANIMATED_Z_RANGE=" << maximumAnimatedZ-minimumAnimatedZ
        << " PAUSE_MATRIX_ERROR=" << pauseError
        << " PAUSE_TIMER_ERROR=" << timerError
        << " CAPTURES=" << captures << "\n";
    const bool passed = maximumSocketError < 0.0001f && maximumCameraError < 0.0001f &&
        maximumAnimatedZ-minimumAnimatedZ > 0.02f &&
        pauseError < 0.0001f && timerError < 0.0001f && captures == 3;
    DespawnRuntimePlayerViewRig(world, rig);
    if (world.transforms.Contains(sockets[0]) || world.transforms.Contains(sockets[1]) ||
        world.transforms.Contains(sockets[2]) || !world.transforms.Contains(player.entity))
        return 25;
    return passed ? 0 : 26;
}
