#pragma once
#include "../Runtime/src/RuntimeCharacterAnimation.h"
#include <cmath>
#include <iostream>

inline bool VerifyNativeCharacterCrossfades()
{
    using namespace renegade::runtime;
    wi::scene::Scene scene;
    const auto actor = scene.Entity_CreateTransform("Blend proof");
    auto add = [&](const char* name, float x)
    {
        const auto entity = scene.Entity_CreateTransform(name);
        auto& animation = scene.animations.Create(entity);
        animation.end = 2.0f;
        const auto dataEntity = wi::ecs::CreateEntity();
        auto& data = scene.animation_datas.Create(dataEntity);
        data.keyframe_times = {0.0f, 2.0f};
        data.keyframe_data = {x, 0, 0, x, 0, 0};
        wi::scene::AnimationComponent::AnimationSampler sampler;
        sampler.data = dataEntity;
        animation.samplers.push_back(sampler);
        wi::scene::AnimationComponent::AnimationChannel channel;
        channel.target = actor;
        channel.samplerIndex = 0;
        channel.path = wi::scene::AnimationComponent::AnimationChannel::Path::TRANSLATION;
        animation.channels.push_back(channel);
        const auto rotationData = wi::ecs::CreateEntity();
        auto& rotation = scene.animation_datas.Create(rotationData);
        rotation.keyframe_times = {0.0f, 2.0f};
        const float angle = x * 3.14159265f / 200.0f;
        rotation.keyframe_data = {0, std::sin(angle), 0, std::cos(angle),
            0, std::sin(angle), 0, std::cos(angle)};
        sampler.data = rotationData;
        animation.samplers.push_back(sampler);
        channel.samplerIndex = 1;
        channel.path = wi::scene::AnimationComponent::AnimationChannel::Path::ROTATION;
        animation.channels.push_back(channel);
        const auto scaleData = wi::ecs::CreateEntity();
        auto& scale = scene.animation_datas.Create(scaleData);
        scale.keyframe_times = {0.0f, 2.0f};
        scale.keyframe_data = {1 + x * 0.01f, 1, 1, 1 + x * 0.01f, 1, 1};
        sampler.data = scaleData;
        animation.samplers.push_back(sampler);
        channel.samplerIndex = 2;
        channel.path = wi::scene::AnimationComponent::AnimationChannel::Path::SCALE;
        animation.channels.push_back(channel);
        return entity;
    };
    const auto idle = add("Idle", 0);
    const auto walk = add("Walk", 10);
    const auto run = add("Run", 20);
    const auto attack = add("Attack", 30);
    const auto hit = add("Hit", 40);
    const auto death = add("Death", 50);
    RuntimeCharacterAnimationState state;
    state.characters.emplace_back();
    auto& record = state.characters.front();
    record.entity = actor;
    const std::pair<CharacterAnimationSemantic, wi::ecs::Entity> assignments[] = {
        {CharacterAnimationSemantic::Idle, idle},
        {CharacterAnimationSemantic::Locomotion, walk},
        {CharacterAnimationSemantic::Run, run},
        {CharacterAnimationSemantic::Attack, attack},
        {CharacterAnimationSemantic::Hit, hit},
        {CharacterAnimationSemantic::Death, death}};
    for (const auto& pair : assignments)
        record.clips[CharacterAnimationIndex(pair.first)].push_back({pair.second, ""});
    auto request = [&](CharacterAnimationSemantic semantic)
    { return RequestCharacterAnimation(scene, state, record, semantic); };
    auto sample = [&]()
    {
        scene.Update(0.0f);
        return scene.transforms.GetComponent(actor)->translation_local.x;
    };
    auto expectPose = [&](float expected)
    {
        const float actual = sample();
        const auto* transform = scene.transforms.GetComponent(actor);
        if (std::abs(actual - expected) < 0.001f &&
            std::abs(transform->scale_local.x - (1 + expected * 0.01f)) < 0.001f &&
            std::abs(transform->rotation_local.y - std::sin(expected * 3.14159265f / 200.0f)) < 0.001f)
            return true;
        std::cerr << "CROSSFADE FAIL expected " << expected << " actual " << actual << '\n';
        return false;
    };
    if (!request(CharacterAnimationSemantic::Idle) || !expectPose(0)) return false;
    if (!request(CharacterAnimationSemantic::Locomotion) || !expectPose(0)) return false;
    AdvanceCharacterAnimationBlend(scene, record, 0.10f);
    if (!expectPose(5)) return false;
    // Repeat samples must not accumulate towards the new pose.
    if (!expectPose(5) || !expectPose(5)) return false;
    const auto requests = state.playbackRequests;
    scene.animations.GetComponent(walk)->timer = 0.4f;
    if (!request(CharacterAnimationSemantic::Locomotion) ||
        state.playbackRequests != requests ||
        scene.animations.GetComponent(walk)->timer != 0.4f) return false;
    // Interrupt halfway through a fade: all current weights become the baseline.
    if (!request(CharacterAnimationSemantic::Run) || !expectPose(5)) return false;
    AdvanceCharacterAnimationBlend(scene, record, 0.10f);
    if (!expectPose(12.5f)) return false;
    AdvanceCharacterAnimationBlend(scene, record, 0.10f);
    if (!expectPose(20) || scene.animations.GetComponent(idle)->IsPlaying() ||
        scene.animations.GetComponent(walk)->IsPlaying()) return false;
    // Incoming earlier in native component order must give the same midpoint.
    if (!request(CharacterAnimationSemantic::Idle) || !expectPose(20)) return false;
    AdvanceCharacterAnimationBlend(scene, record, 0.10f);
    if (!expectPose(10)) return false;
    // Returning to a still-contributing loop preserves its playback phase.
    scene.animations.GetComponent(run)->timer = 0.7f;
    if (!request(CharacterAnimationSemantic::Run) || !expectPose(10) ||
        scene.animations.GetComponent(run)->timer != 0.7f) return false;
    AdvanceCharacterAnimationBlend(scene, record, 0.20f);
    if (!expectPose(20)) return false;
    if (!request(CharacterAnimationSemantic::Attack)) return false;
    AdvanceCharacterAnimationBlend(scene, record, 0.04f);
    if (!expectPose(25) || !scene.animations.GetComponent(attack)->IsPlayingOnce()) return false;
    if (!request(CharacterAnimationSemantic::Attack) || !expectPose(25)) return false;
    if (!request(CharacterAnimationSemantic::Hit) || !expectPose(25)) return false;
    AdvanceCharacterAnimationBlend(scene, record, 0.04f);
    if (!expectPose(32.5f)) return false;
    if (!request(CharacterAnimationSemantic::Death) || !expectPose(32.5f)) return false;
    AdvanceCharacterAnimationBlend(scene, record, 0.05f);
    if (!expectPose(50) || request(CharacterAnimationSemantic::Idle)) return false;
    scene.animations.GetComponent(death)->Pause();
    if (!request(CharacterAnimationSemantic::Death)) return false;
    AdvanceCharacterAnimationBlend(scene, record, 0.10f);
    if (!expectPose(50) || scene.animations.GetComponent(death)->IsPlaying()) return false;
    StopCharacterAnimationBlend(scene, record);
    if (!record.blendClips.empty()) return false;
    // Missing tracks preserve immediate-switch behaviour, with no old pose leak.
    record.activeClip = wi::ecs::INVALID_ENTITY;
    record.activeSemantic = CharacterAnimationSemantic::Idle;
    if (!request(CharacterAnimationSemantic::Idle)) return false;
    scene.animations.GetComponent(walk)->channels.clear();
    if (!request(CharacterAnimationSemantic::Locomotion) ||
        record.blendDuration != 0.0f || scene.animations.GetComponent(idle)->IsPlaying())
        return false;
    StopCharacterAnimationBlend(scene, record);
    std::cout << "NATIVE CROSSFADE POSE/ORDER/INTERRUPTION/PHASE/DEATH PASS\n";
    return true;
}
