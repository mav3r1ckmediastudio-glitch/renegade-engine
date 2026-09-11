#include "renegade/bridge/AnimationTimelineService.h"

#include <iostream>
#include <memory>

namespace
{
    bool Require(const bool condition, const char* message)
    {
        if (!condition)
            std::cerr << "Phase7Gate7D test failure: " << message << '\n';
        return condition;
    }

    std::size_t FindChannel(
        const wi::scene::AnimationComponent& animation,
        const renegade::bridge::AnimationPath path)
    {
        for (std::size_t i = 0; i < animation.channels.size(); ++i)
        {
            if (animation.channels[i].path == path)
                return i;
        }
        return static_cast<std::size_t>(-1);
    }
}

int main()
{
    wi::scene::Scene scene;
    renegade::bridge::CommandService commands;

    const auto animationEntity = wi::ecs::CreateEntity();
    scene.names.Create(animationEntity).name = "Authoring Clip";
    auto& animation = scene.animations.Create(animationEntity);
    animation.start = 0.0f;
    animation.end = 3.0f;
    animation.timer = 1.25f;

    const auto target = wi::ecs::CreateEntity();
    scene.names.Create(target).name = "Animated Target";
    auto& transform = scene.transforms.Create(target);
    transform.translation_local = XMFLOAT3(1.0f, 2.0f, 3.0f);
    transform.rotation_local = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    transform.scale_local = XMFLOAT3(2.0f, 2.0f, 2.0f);

    if (!Require(commands.Execute(
            std::make_unique<renegade::bridge::RecordTimelineKeyCommand>(
                scene,
                animationEntity,
                target,
                renegade::bridge::TimelineRecordPreset::Transform,
                1.25f)),
            "transform preset should record")) return 1;

    if (!Require(animation.channels.size() == 3, "transform preset should create three native channels")) return 1;
    if (!Require(animation.samplers.size() == 3, "transform preset should create three native samplers")) return 1;

    const auto translationChannel = FindChannel(animation, renegade::bridge::AnimationPath::TRANSLATION);
    const auto rotationChannel = FindChannel(animation, renegade::bridge::AnimationPath::ROTATION);
    const auto scaleChannel = FindChannel(animation, renegade::bridge::AnimationPath::SCALE);
    if (!Require(translationChannel != static_cast<std::size_t>(-1), "translation channel missing")) return 1;
    if (!Require(rotationChannel != static_cast<std::size_t>(-1), "rotation channel missing")) return 1;
    if (!Require(scaleChannel != static_cast<std::size_t>(-1), "scale channel missing")) return 1;

    const auto translationDataEntity = animation.samplers[animation.channels[translationChannel].samplerIndex].data;
    const auto rotationDataEntity = animation.samplers[animation.channels[rotationChannel].samplerIndex].data;
    const auto scaleDataEntity = animation.samplers[animation.channels[scaleChannel].samplerIndex].data;
    const auto* translationData = scene.animation_datas.GetComponent(translationDataEntity);
    const auto* rotationData = scene.animation_datas.GetComponent(rotationDataEntity);
    const auto* scaleData = scene.animation_datas.GetComponent(scaleDataEntity);
    if (!Require(translationData != nullptr && translationData->keyframe_times.size() == 1,
            "translation key time missing")) return 1;
    if (!Require(translationData->keyframe_data.size() == 3 && translationData->keyframe_data[0] == 1.0f,
            "translation key payload mismatch")) return 1;
    if (!Require(rotationData != nullptr && rotationData->keyframe_data.size() == 4,
            "rotation key payload mismatch")) return 1;
    if (!Require(scaleData != nullptr && scaleData->keyframe_data.size() == 3,
            "scale key payload mismatch")) return 1;

    if (!Require(commands.Undo(), "transform record undo should succeed")) return 1;
    if (!Require(animation.channels.empty(), "record undo should restore channel vector")) return 1;
    if (!Require(!scene.animation_datas.Contains(translationDataEntity) &&
                 !scene.animation_datas.Contains(rotationDataEntity) &&
                 !scene.animation_datas.Contains(scaleDataEntity),
            "record undo should remove newly-created AnimationDataComponent entities")) return 1;
    if (!Require(commands.Redo(), "transform record redo should succeed")) return 1;
    if (!Require(animation.channels.size() == 3, "record redo should restore native channels")) return 1;

    transform.translation_local = XMFLOAT3(20.0f, 21.0f, 22.0f);
    if (!Require(commands.Execute(
            std::make_unique<renegade::bridge::RecordTimelineKeyCommand>(
                scene,
                animationEntity,
                target,
                renegade::bridge::TimelineRecordPreset::Translation,
                0.5f)),
            "second translation key should record")) return 1;

    const auto translationChannelAfterRedo = FindChannel(animation, renegade::bridge::AnimationPath::TRANSLATION);
    auto* translatedData = scene.animation_datas.GetComponent(
        animation.samplers[animation.channels[translationChannelAfterRedo].samplerIndex].data);
    if (!Require(translatedData != nullptr && translatedData->keyframe_times.size() == 2,
            "translation channel should contain two keys")) return 1;
    if (!Require(translatedData->keyframe_times[0] == 1.25f && translatedData->keyframe_times[1] == 0.5f,
            "recording should preserve Wicked append semantics")) return 1;

    if (!Require(commands.Execute(
            std::make_unique<renegade::bridge::MoveTimelineKeyCommand>(
                scene,
                animationEntity,
                translationChannelAfterRedo,
                0,
                0.25f)),
            "key move should commit")) return 1;
    translatedData = scene.animation_datas.GetComponent(
        animation.samplers[animation.channels[translationChannelAfterRedo].samplerIndex].data);
    if (!Require(translatedData->keyframe_times[0] == 0.25f && translatedData->keyframe_times[1] == 0.5f,
            "key move should keep native times sorted")) return 1;
    if (!Require(translatedData->keyframe_data[0] == 1.0f && translatedData->keyframe_data[3] == 20.0f,
            "key move should keep payloads paired with their times")) return 1;
    if (!Require(commands.Undo(), "key move undo should succeed")) return 1;
    translatedData = scene.animation_datas.GetComponent(
        animation.samplers[animation.channels[translationChannelAfterRedo].samplerIndex].data);
    if (!Require(translatedData->keyframe_times[0] == 1.25f && translatedData->keyframe_times[1] == 0.5f,
            "key move undo should restore original native ordering")) return 1;
    if (!Require(commands.Redo(), "key move redo should succeed")) return 1;

    if (!Require(commands.Execute(
            std::make_unique<renegade::bridge::SetTimelineSamplerModeCommand>(
                scene,
                animationEntity,
                translationChannelAfterRedo,
                wi::scene::AnimationComponent::AnimationSampler::STEP)),
            "sampler mode should change to step")) return 1;
    if (!Require(animation.samplers[animation.channels[translationChannelAfterRedo].samplerIndex].mode ==
            wi::scene::AnimationComponent::AnimationSampler::STEP,
            "native sampler mode mismatch")) return 1;
    if (!Require(commands.Undo(), "sampler mode undo should succeed")) return 1;
    if (!Require(animation.samplers[animation.channels[translationChannelAfterRedo].samplerIndex].mode ==
            wi::scene::AnimationComponent::AnimationSampler::LINEAR,
            "sampler mode undo should restore linear")) return 1;

    if (!Require(commands.Execute(
            std::make_unique<renegade::bridge::DeleteTimelineKeyCommand>(
                scene,
                animationEntity,
                translationChannelAfterRedo,
                0)),
            "key deletion should commit")) return 1;
    translatedData = scene.animation_datas.GetComponent(
        animation.samplers[animation.channels[translationChannelAfterRedo].samplerIndex].data);
    if (!Require(translatedData->keyframe_times.size() == 1 && translatedData->keyframe_data.size() == 3,
            "key deletion should remove matching time and payload chunk")) return 1;
    if (!Require(commands.Undo(), "key deletion undo should succeed")) return 1;

    const auto soundEntity = wi::ecs::CreateEntity();
    scene.names.Create(soundEntity).name = "Sound Cue";
    scene.sounds.Create(soundEntity);
    if (!Require(commands.Execute(
            std::make_unique<renegade::bridge::RecordTimelineKeyCommand>(
                scene,
                animationEntity,
                soundEntity,
                renegade::bridge::TimelineRecordPreset::SoundPlay,
                1.75f)),
            "sound play event should record")) return 1;
    const auto soundChannel = FindChannel(animation, renegade::bridge::AnimationPath::SOUND_PLAY);
    if (!Require(soundChannel != static_cast<std::size_t>(-1), "sound event channel missing")) return 1;
    const auto* soundData = scene.animation_datas.GetComponent(
        animation.samplers[animation.channels[soundChannel].samplerIndex].data);
    if (!Require(soundData != nullptr && soundData->keyframe_times.size() == 1,
            "sound event time missing")) return 1;
    if (!Require(soundData->keyframe_data.empty(), "event channels must not invent payload data")) return 1;

    const auto channelsBeforeClose = renegade::bridge::CollectTimelineChannels(scene, animationEntity);
    if (!Require(commands.Execute(
            std::make_unique<renegade::bridge::CloseTimelineLoopCommand>(
                scene,
                animationEntity,
                3.0f)),
            "close loop should duplicate first native keys")) return 1;
    const auto channelsAfterClose = renegade::bridge::CollectTimelineChannels(scene, animationEntity);
    if (!Require(channelsAfterClose.size() == channelsBeforeClose.size(),
            "close loop should not create parallel channels")) return 1;
    for (std::size_t i = 0; i < channelsBeforeClose.size(); ++i)
    {
        if (!Require(channelsAfterClose[i].keyframeCount == channelsBeforeClose[i].keyframeCount + 1,
                "close loop should append one key to every populated native channel")) return 1;
    }
    if (!Require(commands.Undo(), "close loop undo should succeed")) return 1;

    std::cout << "Phase 7D native timeline tests passed\n";
    return 0;
}
