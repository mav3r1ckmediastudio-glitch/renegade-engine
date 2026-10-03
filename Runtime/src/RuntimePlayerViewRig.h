#pragma once

#include <WickedEngine.h>

#include <algorithm>
#include <cmath>
#include <string>

#include "renegade/bridge/PlayerService.h"

namespace renegade::runtime
{
    inline constexpr const char* RuntimePlayerViewRigRootName =
        "__renegade_player_view_rig";
    inline constexpr const char* RuntimePlayerViewRigPresentationName =
        "__renegade_player_view_presentation";
    inline constexpr const char* RuntimePlayerPrimaryHandSocketName =
        "__renegade_player_primary_hand";
    inline constexpr const char* RuntimePlayerOffHandSocketName =
        "__renegade_player_off_hand";
    inline constexpr const char* RuntimePlayerTwoHandSupportSocketName =
        "__renegade_player_two_hand_support";

    enum class PlayerViewAction
    {
        Idle,
        Walk,
        Sprint,
    };

    enum class PlayerViewRigSocket
    {
        PrimaryHand,
        OffHand,
        TwoHandSupport,
        PresentationRoot,
    };

    struct RuntimePlayerViewRigSettings
    {
        XMFLOAT3 primaryHandOffset = XMFLOAT3(0.22f, -0.27f, 0.58f);
        XMFLOAT3 offHandOffset = XMFLOAT3(-0.22f, -0.27f, 0.58f);
        XMFLOAT3 twoHandSupportOffset = XMFLOAT3(-0.10f, -0.22f, 0.74f);
        bool createProofGeometry = true;
    };

    struct RuntimePlayerViewRigState
    {
        wi::ecs::Entity playerEntity = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity root = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity presentationRoot = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity primaryHandSocket = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity offHandSocket = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity twoHandSupportSocket = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity primaryArmProof = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity offHandArmProof = wi::ecs::INVALID_ENTITY;
        PlayerViewAction action = PlayerViewAction::Idle;
        float presentationPhase = 0.0f;
        float eyeHeight = 1.65f;

        [[nodiscard]] constexpr bool IsSpawned() const noexcept
        {
            return playerEntity != wi::ecs::INVALID_ENTITY &&
                root != wi::ecs::INVALID_ENTITY;
        }
    };
    namespace player_view_rig_detail
    {
        inline void SetLocalTransform(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            const XMFLOAT3& translation,
            const XMFLOAT3& scale,
            const XMFLOAT3& rotation = XMFLOAT3(0.0f, 0.0f, 0.0f))
        {
            auto* transform = scene.transforms.GetComponent(entity);
            if (transform == nullptr)
                return;

            transform->translation_local = translation;
            transform->scale_local = scale;
            XMStoreFloat4(
                &transform->rotation_local,
                XMQuaternionRotationRollPitchYaw(
                    rotation.x, rotation.y, rotation.z));
            transform->SetDirty();
            transform->UpdateTransform();
        }

        inline void ConfigureForegroundObject(
            wi::scene::ObjectComponent& object) noexcept
        {
            object.SetForeground(true);
            object.SetCastShadow(false);
            object.SetNotVisibleInReflections(true);
            object.SetDynamic(true);
        }

        inline bool HasTransform(
            const wi::scene::Scene& scene,
            wi::ecs::Entity entity) noexcept
        {
            return entity != wi::ecs::INVALID_ENTITY &&
                scene.transforms.Contains(entity);
        }
    }

    inline std::size_t ConfigureRuntimeViewModelHierarchy(
        wi::scene::Scene& scene,
        wi::ecs::Entity root) noexcept
    {
        if (root == wi::ecs::INVALID_ENTITY)
            return 0;

        std::size_t configured = 0;
        for (std::size_t i = 0; i < scene.objects.GetCount(); ++i)
        {
            const wi::ecs::Entity entity = scene.objects.GetEntity(i);
            if (entity != root && !scene.Entity_IsDescendant(entity, root))
                continue;

            auto& object = scene.objects[i];
            player_view_rig_detail::ConfigureForegroundObject(object);
            ++configured;
        }
        return configured;
    }
    [[nodiscard]] inline PlayerViewAction ResolvePlayerViewAction(
        const bridge::PlayerInputFrame& input) noexcept
    {
        const float movement =
            std::abs(input.moveRight) + std::abs(input.moveForward);
        if (movement <= 0.001f)
            return PlayerViewAction::Idle;
        return input.sprintDown
            ? PlayerViewAction::Sprint
            : PlayerViewAction::Walk;
    }

    [[nodiscard]] inline wi::ecs::Entity ResolvePlayerViewRigSocket(
        const RuntimePlayerViewRigState& state,
        const PlayerViewRigSocket socket) noexcept
    {
        switch (socket)
        {
        case PlayerViewRigSocket::PrimaryHand:
            return state.primaryHandSocket;
        case PlayerViewRigSocket::OffHand:
            return state.offHandSocket;
        case PlayerViewRigSocket::TwoHandSupport:
            return state.twoHandSupportSocket;
        case PlayerViewRigSocket::PresentationRoot:
        default:
            return state.presentationRoot;
        }
    }

    [[nodiscard]] inline bool AttachRuntimeViewModelHierarchy(
        wi::scene::Scene& scene,
        const RuntimePlayerViewRigState& state,
        const wi::ecs::Entity viewModelRoot,
        const PlayerViewRigSocket socket,
        std::string& error)
    {
        error.clear();
        if (!state.IsSpawned())
        {
            error = "Runtime Player View Rig is not spawned.";
            return false;
        }
        if (!player_view_rig_detail::HasTransform(scene, viewModelRoot))
        {
            error = "View-model hierarchy root has no native Wicked transform.";
            return false;
        }
        const wi::ecs::Entity parent =
            ResolvePlayerViewRigSocket(state, socket);
        if (!player_view_rig_detail::HasTransform(scene, parent))
        {
            error = "Requested Player View Rig attachment socket is unavailable.";
            return false;
        }

        scene.Component_Attach(viewModelRoot, parent, true);
        ConfigureRuntimeViewModelHierarchy(scene, viewModelRoot);
        return true;
    }

    inline void UpdateRuntimePlayerViewRigPresentation(
        wi::scene::Scene& scene,
        RuntimePlayerViewRigState& state,
        const bridge::PlayerInputFrame& input,
        const float dt) noexcept
    {
        if (!state.IsSpawned())
            return;

        auto* presentation =
            scene.transforms.GetComponent(state.presentationRoot);
        if (presentation == nullptr)
            return;

        state.action = ResolvePlayerViewAction(input);
        const float safeDt =
            std::isfinite(dt) ? std::clamp(dt, 0.0f, 0.1f) : 0.0f;

        float frequency = 1.4f;
        float horizontalAmplitude = 0.0015f;
        float verticalAmplitude = 0.0020f;
        switch (state.action)
        {
        case PlayerViewAction::Walk:
            frequency = 7.0f;
            horizontalAmplitude = 0.006f;
            verticalAmplitude = 0.010f;
            break;
        case PlayerViewAction::Sprint:
            frequency = 10.0f;
            horizontalAmplitude = 0.010f;
            verticalAmplitude = 0.017f;
            break;
        case PlayerViewAction::Idle:
        default:
            break;
        }

        state.presentationPhase = std::fmod(
            state.presentationPhase + safeDt * frequency,
            XM_2PI);

        presentation->translation_local = XMFLOAT3(
            std::sin(state.presentationPhase) * horizontalAmplitude,
            std::sin(state.presentationPhase * 2.0f) * verticalAmplitude,
            0.0f);
        presentation->rotation_local =
            XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        presentation->scale_local = XMFLOAT3(1.0f, 1.0f, 1.0f);
        presentation->SetDirty();
        presentation->UpdateTransform();
    }

    [[nodiscard]] inline bool PoseRuntimePlayerViewRig(
        wi::scene::Scene& scene,
        RuntimePlayerViewRigState& state,
        const float yaw,
        const float pitch) noexcept
    {
        if (!state.IsSpawned())
            return false;

        auto* player = scene.transforms.GetComponent(state.playerEntity);
        auto* root = scene.transforms.GetComponent(state.root);
        if (player == nullptr || root == nullptr ||
            !std::isfinite(yaw) || !std::isfinite(pitch))
        {
            return false;
        }

        const XMFLOAT3 playerPosition = player->GetPosition();
        wi::scene::TransformComponent desiredWorld;
        desiredWorld.Translate(XMFLOAT3(
            playerPosition.x,
            playerPosition.y + state.eyeHeight,
            playerPosition.z));
        desiredWorld.RotateRollPitchYaw(XMFLOAT3(pitch, yaw, 0.0f));
        desiredWorld.UpdateTransform();

        const XMMATRIX parentWorld = XMLoadFloat4x4(&player->world);
        const XMMATRIX desired = XMLoadFloat4x4(&desiredWorld.world);
        const XMMATRIX local =
            desired * XMMatrixInverse(nullptr, parentWorld);

        root->translation_local = XMFLOAT3(0.0f, 0.0f, 0.0f);
        root->rotation_local = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        root->scale_local = XMFLOAT3(1.0f, 1.0f, 1.0f);
        root->SetDirty();
        root->MatrixTransform(local);
        root->UpdateTransform_Parented(*player);
        return true;
    }
    [[nodiscard]] inline bool SpawnRuntimePlayerViewRig(
        wi::scene::Scene& scene,
        RuntimePlayerViewRigState& state,
        wi::ecs::Entity playerEntity,
        float eyeHeight,
        std::string& error,
        const RuntimePlayerViewRigSettings& settings = {})
    {
        error.clear();
        if (state.IsSpawned())
        {
            error = "Runtime Player View Rig is already spawned.";
            return false;
        }
        if (!player_view_rig_detail::HasTransform(scene, playerEntity))
        {
            error = "Runtime Player View Rig requires the authoritative Player transform.";
            return false;
        }
        if (!std::isfinite(eyeHeight) || eyeHeight <= 0.0f)
        {
            error = "Runtime Player View Rig requires a finite positive eye height.";
            return false;
        }

        RuntimePlayerViewRigState created;
        created.playerEntity = playerEntity;
        created.eyeHeight = eyeHeight;
        created.root = scene.Entity_CreateTransform(RuntimePlayerViewRigRootName);
        created.presentationRoot =
            scene.Entity_CreateTransform(RuntimePlayerViewRigPresentationName);
        created.primaryHandSocket =
            scene.Entity_CreateTransform(RuntimePlayerPrimaryHandSocketName);
        created.offHandSocket =
            scene.Entity_CreateTransform(RuntimePlayerOffHandSocketName);
        created.twoHandSupportSocket =
            scene.Entity_CreateTransform(RuntimePlayerTwoHandSupportSocketName);

        if (!player_view_rig_detail::HasTransform(scene, created.root) ||
            !player_view_rig_detail::HasTransform(scene, created.presentationRoot) ||
            !player_view_rig_detail::HasTransform(scene, created.primaryHandSocket) ||
            !player_view_rig_detail::HasTransform(scene, created.offHandSocket) ||
            !player_view_rig_detail::HasTransform(scene, created.twoHandSupportSocket))
        {
            error = "Wicked could not create the Runtime Player View Rig sockets.";
            if (created.root != wi::ecs::INVALID_ENTITY)
                scene.Entity_Remove(created.root);
            return false;
        }

        scene.Component_Attach(created.root, created.playerEntity, true);
        scene.Component_Attach(
            created.presentationRoot, created.root, true);
        scene.Component_Attach(
            created.primaryHandSocket, created.presentationRoot, true);
        scene.Component_Attach(
            created.offHandSocket, created.presentationRoot, true);
        scene.Component_Attach(
            created.twoHandSupportSocket, created.presentationRoot, true);

        player_view_rig_detail::SetLocalTransform(
            scene, created.primaryHandSocket,
            settings.primaryHandOffset, XMFLOAT3(1.0f, 1.0f, 1.0f));
        player_view_rig_detail::SetLocalTransform(
            scene, created.offHandSocket,
            settings.offHandOffset, XMFLOAT3(1.0f, 1.0f, 1.0f));
        player_view_rig_detail::SetLocalTransform(
            scene, created.twoHandSupportSocket,
            settings.twoHandSupportOffset, XMFLOAT3(1.0f, 1.0f, 1.0f));
        if (settings.createProofGeometry)
        {
            created.primaryArmProof =
                scene.Entity_CreateCube("__renegade_player_primary_arm_proxy");
            created.offHandArmProof =
                scene.Entity_CreateCube("__renegade_player_off_arm_proxy");
            if (!player_view_rig_detail::HasTransform(
                    scene, created.primaryArmProof) ||
                !player_view_rig_detail::HasTransform(
                    scene, created.offHandArmProof))
            {
                error = "Wicked could not create the P1 View Rig proof geometry.";
                scene.Entity_Remove(created.root);
                return false;
            }

            player_view_rig_detail::SetLocalTransform(
                scene, created.primaryArmProof,
                XMFLOAT3(0.0f, -0.11f, -0.25f),
                XMFLOAT3(0.075f, 0.085f, 0.30f),
                XMFLOAT3(-0.18f, 0.0f, -0.045f));
            player_view_rig_detail::SetLocalTransform(
                scene, created.offHandArmProof,
                XMFLOAT3(0.0f, -0.11f, -0.25f),
                XMFLOAT3(0.075f, 0.085f, 0.30f),
                XMFLOAT3(-0.18f, 0.0f, 0.045f));

            scene.Component_Attach(
                created.primaryArmProof, created.primaryHandSocket, true);
            scene.Component_Attach(
                created.offHandArmProof, created.offHandSocket, true);

            ConfigureRuntimeViewModelHierarchy(scene, created.root);

            if (auto* material =
                    scene.materials.GetComponent(created.primaryArmProof))
            {
                material->baseColor = XMFLOAT4(0.18f, 0.42f, 0.68f, 1.0f);
                material->roughness = 0.72f;
            }
            if (auto* material =
                    scene.materials.GetComponent(created.offHandArmProof))
            {
                material->baseColor = XMFLOAT4(0.68f, 0.30f, 0.16f, 1.0f);
                material->roughness = 0.72f;
            }
        }

        state = created;
        return PoseRuntimePlayerViewRig(scene, state, 0.0f, 0.0f);
    }
    inline void DespawnRuntimePlayerViewRig(
        wi::scene::Scene& scene,
        RuntimePlayerViewRigState& state) noexcept
    {
        if (state.root != wi::ecs::INVALID_ENTITY &&
            scene.transforms.Contains(state.root))
        {
            scene.Entity_Remove(state.root);
        }
        state = {};
    }
}
