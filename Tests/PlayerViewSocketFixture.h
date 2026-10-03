#pragma once

#include "RuntimePlayerViewAsset.h"
#include "RuntimePlayerViewAnimation.h"

namespace renegade::tests
{
    // Test skeleton: +X primary, -X off hand, +Z forward, metres, unit root.
    // Deliberately generic bone names; binding uses semantic metadata only.
    inline wi::ecs::Entity CreatePlayerViewSocketAsset(
        wi::scene::Scene& scene, bool geometry = false)
    {
        using namespace runtime;
        const auto root = scene.Entity_CreateTransform("Generated socket proof rig");
        const auto primary = scene.Entity_CreateTransform("Joint A");
        const auto off = scene.Entity_CreateTransform("Joint B");
        scene.Component_Attach(primary, root, true);
        scene.Component_Attach(off, root, true);
        const std::array<wi::ecs::Entity, 2> bones = {primary, off};
        for (std::size_t i = 0; i < bones.size(); ++i)
        {
            const float x = i == 0 ? 0.22f : -0.22f;
            player_view_rig_detail::SetLocalTransform(
                scene, bones[i], XMFLOAT3(x, -0.27f, 0.58f), XMFLOAT3(1,1,1));
            auto& armature = i == 0 ? scene.armatures.Create(root) : *scene.armatures.GetComponent(root);
            armature.boneCollection.push_back(bones[i]);
            XMFLOAT4X4 inverse;
            XMStoreFloat4x4(&inverse, XMMatrixTranslation(-x, 0.27f, -0.58f));
            armature.inverseBindMatrices.push_back(inverse);
            scene.metadatas.Create(bones[i]).bool_values.set(PlayerViewSocketMetadataKeys[i], true);

            if (geometry)
            {
                const auto arm = scene.Entity_CreateCube(i == 0 ? "Blue proof forearm" : "Orange proof forearm");
                scene.Component_Attach(arm, bones[i], true);
                player_view_rig_detail::SetLocalTransform(
                    scene, arm, XMFLOAT3(0,-0.11f,-0.25f),
                    XMFLOAT3(0.075f,0.085f,0.30f), XMFLOAT3(-0.18f,0,0));
                scene.materials.GetComponent(arm)->baseColor =
                    i == 0 ? XMFLOAT4(0.18f,0.42f,0.68f,1) : XMFLOAT4(0.68f,0.30f,0.16f,1);
            }
        }
        const auto support = scene.Entity_CreateTransform("Authored support grip");
        scene.Component_Attach(support, off, true);
        player_view_rig_detail::SetLocalTransform(
            scene, support, XMFLOAT3(0.04f,0.02f,0.10f), XMFLOAT3(1,1,1),
            XMFLOAT3(0,0.2f,0));
        scene.metadatas.Create(support).bool_values.set(PlayerViewSocketMetadataKeys[2], true);

        for (std::size_t action = 0; action < 3; ++action)
        {
            const auto clip = scene.Entity_CreateTransform(action == 0 ? "Proof Idle" : action == 1 ? "Proof Walk" : "Proof Sprint");
            scene.Component_Attach(clip, root, true);
            auto& animation = scene.animations.Create(clip);
            animation.start = 0;
            animation.end = 1;
            animation.amount = 1;
            animation.SetLooped(true);
            scene.metadatas.Create(clip).string_values.set(
                bridge::CreatorCharacterAnimationActionMetadataKey,
                action == 0 ? "Idle" : action == 1 ? "Walk" : "Sprint");
            const float amplitude = 0.01f + 0.025f * float(action);
            for (std::size_t i = 0; i < bones.size(); ++i)
            {
                const auto dataEntity = scene.Entity_CreateTransform("Native socket animation data");
                scene.Component_Attach(dataEntity, clip, true);
                auto& data = scene.animation_datas.Create(dataEntity);
                data.keyframe_times = {0,0.5f,1};
                const float x = i == 0 ? 0.22f : -0.22f;
                const float sign = i == 0 ? 1.0f : -1.0f;
                data.keyframe_data = {
                    x,-0.27f,0.58f,
                    x,-0.27f + sign * amplitude,0.58f + amplitude,
                    x,-0.27f,0.58f,
                };
                auto* clipComponent = scene.animations.GetComponent(clip);
                wi::scene::AnimationComponent::AnimationSampler sampler;
                sampler.data = dataEntity;
                clipComponent->samplers.push_back(sampler);
                wi::scene::AnimationComponent::AnimationChannel channel;
                channel.target = bones[i];
                channel.samplerIndex = int(clipComponent->samplers.size()) - 1;
                channel.path = wi::scene::AnimationComponent::AnimationChannel::Path::TRANSLATION;
                clipComponent->channels.push_back(channel);

                const auto rotationDataEntity = scene.Entity_CreateTransform("Native socket rotation data");
                scene.Component_Attach(rotationDataEntity, clip, true);
                auto& rotationData = scene.animation_datas.Create(rotationDataEntity);
                rotationData.keyframe_times = {0,0.5f,1};
                XMFLOAT4 turn;
                XMStoreFloat4(&turn, XMQuaternionRotationRollPitchYaw(
                    sign * (0.10f + 0.08f * float(action)), 0.1f, sign * 0.08f));
                rotationData.keyframe_data = {
                    0,0,0,1, turn.x,turn.y,turn.z,turn.w, 0,0,0,1,
                };
                clipComponent = scene.animations.GetComponent(clip);
                sampler.data = rotationDataEntity;
                channel.samplerIndex = int(clipComponent->samplers.size());
                clipComponent->samplers.push_back(sampler);
                channel.path = wi::scene::AnimationComponent::AnimationChannel::Path::ROTATION;
                clipComponent->channels.push_back(channel);
            }
        }
        return root;
    }

    inline float MatrixDifference(const XMFLOAT4X4& first, const XMFLOAT4X4& second)
    {
        float maximum = 0;
        for (int row = 0; row < 4; ++row)
            for (int column = 0; column < 4; ++column)
                maximum = std::max(maximum, std::abs(first.m[row][column] - second.m[row][column]));
        return maximum;
    }
}
