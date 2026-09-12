#include "renegade/bridge/AnimationTimelineService.h"

#include <iostream>
#include <memory>
#include <string>

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

    bool HasParent(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity child,
        const wi::ecs::Entity parent)
    {
        const auto* hierarchy = scene.hierarchy.GetComponent(child);
        return hierarchy != nullptr && hierarchy->parentID == parent;
    }
}

int main()
{
    using namespace renegade::bridge;

    // A creator can start a native timeline without first importing an animation-bearing asset.
    {
        wi::scene::Scene createScene;
        CommandService createCommands;
        auto create = std::make_unique<CreateTimelineAnimationCommand>(createScene, "Door Open");
        auto* view = create.get();
        if (!Require(createCommands.Execute(std::move(create)),
                "native empty clip creation should succeed")) return 1;
        const auto created = view->CreatedEntity();
        if (!Require(created != wi::ecs::INVALID_ENTITY && createScene.animations.Contains(created),
                "created native clip should own AnimationComponent")) return 1;
        if (!Require(createScene.names.GetComponent(created) != nullptr &&
                     createScene.names.GetComponent(created)->name == "Door Open",
                "created native clip should keep creator name")) return 1;
        if (!Require(createCommands.Undo() && !createScene.animations.Contains(created),
                "native clip create undo should remove the clip")) return 1;
        if (!Require(createCommands.Redo() && createScene.animations.Contains(created),
                "native clip create redo should restore the same clip")) return 1;
    }

    wi::scene::Scene scene;
    CommandService commands;

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
            std::make_unique<RecordTimelineKeyCommand>(
                scene, animationEntity, target, TimelineRecordPreset::Transform, 1.25f)),
            "transform preset should record")) return 1;

    if (!Require(animation.channels.size() == 3,
            "transform preset should create three native channels")) return 1;
    if (!Require(animation.samplers.size() == 3,
            "transform preset should create three native samplers")) return 1;

    const auto translationChannel = FindChannel(animation, AnimationPath::TRANSLATION);
    const auto rotationChannel = FindChannel(animation, AnimationPath::ROTATION);
    const auto scaleChannel = FindChannel(animation, AnimationPath::SCALE);
    if (!Require(translationChannel != static_cast<std::size_t>(-1),
            "translation channel missing")) return 1;
    if (!Require(rotationChannel != static_cast<std::size_t>(-1),
            "rotation channel missing")) return 1;
    if (!Require(scaleChannel != static_cast<std::size_t>(-1),
            "scale channel missing")) return 1;

    const auto translationDataEntity =
        animation.samplers[animation.channels[translationChannel].samplerIndex].data;
    const auto rotationDataEntity =
        animation.samplers[animation.channels[rotationChannel].samplerIndex].data;
    const auto scaleDataEntity =
        animation.samplers[animation.channels[scaleChannel].samplerIndex].data;
    const auto* translationData = scene.animation_datas.GetComponent(translationDataEntity);
    const auto* rotationData = scene.animation_datas.GetComponent(rotationDataEntity);
    const auto* scaleData = scene.animation_datas.GetComponent(scaleDataEntity);
    if (!Require(translationData != nullptr && translationData->keyframe_times.size() == 1,
            "translation key time missing")) return 1;
    if (!Require(translationData->keyframe_data.size() == 3 &&
                 translationData->keyframe_data[0] == 1.0f,
            "translation key payload mismatch")) return 1;
    if (!Require(rotationData != nullptr && rotationData->keyframe_data.size() == 4,
            "rotation key payload mismatch")) return 1;
    if (!Require(scaleData != nullptr && scaleData->keyframe_data.size() == 3,
            "scale key payload mismatch")) return 1;
    if (!Require(HasParent(scene, translationDataEntity, animationEntity) &&
                 HasParent(scene, rotationDataEntity, animationEntity) &&
                 HasParent(scene, scaleDataEntity, animationEntity),
            "new native timeline data must be owned by its Animation entity")) return 1;

    if (!Require(commands.Undo(), "transform record undo should succeed")) return 1;
    if (!Require(animation.channels.empty(),
            "record undo should restore channel vector")) return 1;
    if (!Require(!scene.animation_datas.Contains(translationDataEntity) &&
                 !scene.animation_datas.Contains(rotationDataEntity) &&
                 !scene.animation_datas.Contains(scaleDataEntity) &&
                 !scene.hierarchy.Contains(translationDataEntity) &&
                 !scene.hierarchy.Contains(rotationDataEntity) &&
                 !scene.hierarchy.Contains(scaleDataEntity),
            "record undo should remove complete newly-created animation-data entities")) return 1;
    if (!Require(commands.Redo(), "transform record redo should succeed")) return 1;
    if (!Require(animation.channels.size() == 3,
            "record redo should restore native channels")) return 1;
    if (!Require(HasParent(scene, translationDataEntity, animationEntity) &&
                 HasParent(scene, rotationDataEntity, animationEntity) &&
                 HasParent(scene, scaleDataEntity, animationEntity),
            "record redo must restore native animation-data ownership")) return 1;

    transform.translation_local = XMFLOAT3(20.0f, 21.0f, 22.0f);
    if (!Require(commands.Execute(
            std::make_unique<RecordTimelineKeyCommand>(
                scene, animationEntity, target, TimelineRecordPreset::Translation, 0.5f)),
            "second translation key should record")) return 1;

    const auto translationChannelAfterRedo = FindChannel(animation, AnimationPath::TRANSLATION);
    auto* translatedData = scene.animation_datas.GetComponent(
        animation.samplers[animation.channels[translationChannelAfterRedo].samplerIndex].data);
    if (!Require(translatedData != nullptr && translatedData->keyframe_times.size() == 2,
            "translation channel should contain two keys")) return 1;
    if (!Require(translatedData->keyframe_times[0] == 0.5f &&
                 translatedData->keyframe_times[1] == 1.25f,
            "recording should insert native keys in chronological order")) return 1;
    if (!Require(translatedData->keyframe_data[0] == 20.0f &&
                 translatedData->keyframe_data[3] == 1.0f,
            "sorted recording should preserve payload/time pairing")) return 1;

    // Move the original 1.25 key before the newly-recorded 0.5 key.
    if (!Require(commands.Execute(
            std::make_unique<MoveTimelineKeyCommand>(
                scene, animationEntity, translationChannelAfterRedo, 1, 0.25f)),
            "key move should commit")) return 1;
    translatedData = scene.animation_datas.GetComponent(
        animation.samplers[animation.channels[translationChannelAfterRedo].samplerIndex].data);
    if (!Require(translatedData->keyframe_times[0] == 0.25f &&
                 translatedData->keyframe_times[1] == 0.5f,
            "key move should keep native times sorted")) return 1;
    if (!Require(translatedData->keyframe_data[0] == 1.0f &&
                 translatedData->keyframe_data[3] == 20.0f,
            "key move should keep payloads paired with their times")) return 1;
    if (!Require(commands.Undo(), "key move undo should succeed")) return 1;
    translatedData = scene.animation_datas.GetComponent(
        animation.samplers[animation.channels[translationChannelAfterRedo].samplerIndex].data);
    if (!Require(translatedData->keyframe_times[0] == 0.5f &&
                 translatedData->keyframe_times[1] == 1.25f,
            "key move undo should restore sorted original times")) return 1;
    if (!Require(commands.Redo(), "key move redo should succeed")) return 1;

    if (!Require(commands.Execute(
            std::make_unique<SetTimelineSamplerModeCommand>(
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
            std::make_unique<DeleteTimelineKeyCommand>(
                scene, animationEntity, translationChannelAfterRedo, 0)),
            "key deletion should commit")) return 1;
    translatedData = scene.animation_datas.GetComponent(
        animation.samplers[animation.channels[translationChannelAfterRedo].samplerIndex].data);
    if (!Require(translatedData->keyframe_times.size() == 1 &&
                 translatedData->keyframe_data.size() == 3,
            "key deletion should remove matching time and payload chunk")) return 1;
    if (!Require(commands.Undo(), "key deletion undo should succeed")) return 1;

    const auto soundEntity = wi::ecs::CreateEntity();
    scene.names.Create(soundEntity).name = "Sound Cue";
    scene.sounds.Create(soundEntity);
    if (!Require(commands.Execute(
            std::make_unique<RecordTimelineKeyCommand>(
                scene, animationEntity, soundEntity, TimelineRecordPreset::SoundPlay, 1.75f)),
            "sound play event should record")) return 1;
    if (!Require(commands.Execute(
            std::make_unique<RecordTimelineKeyCommand>(
                scene, animationEntity, soundEntity, TimelineRecordPreset::SoundPlay, 0.75f)),
            "earlier sound event should record")) return 1;

    const auto soundChannel = FindChannel(animation, AnimationPath::SOUND_PLAY);
    if (!Require(soundChannel != static_cast<std::size_t>(-1),
            "sound event channel missing")) return 1;
    const auto soundDataEntity =
        animation.samplers[animation.channels[soundChannel].samplerIndex].data;
    const auto* soundData = scene.animation_datas.GetComponent(soundDataEntity);
    if (!Require(soundData != nullptr && soundData->keyframe_times.size() == 2,
            "sound event times missing")) return 1;
    if (!Require(soundData->keyframe_times[0] == 0.75f &&
                 soundData->keyframe_times[1] == 1.75f,
            "event keys must remain chronological for Wicked next_event traversal")) return 1;
    if (!Require(soundData->keyframe_data.empty(),
            "event channels must not invent payload data")) return 1;
    if (!Require(HasParent(scene, soundDataEntity, animationEntity),
            "event animation data must be owned by its Animation entity")) return 1;

    // Re-recording an event at the same time must not create a duplicate event.
    if (!Require(!commands.Execute(
            std::make_unique<RecordTimelineKeyCommand>(
                scene, animationEntity, soundEntity, TimelineRecordPreset::SoundPlay, 0.75f)),
            "same-time sound event should be a no-op")) return 1;
    soundData = scene.animation_datas.GetComponent(soundDataEntity);
    if (!Require(soundData != nullptr && soundData->keyframe_times.size() == 2,
            "same-time event record must not duplicate the event")) return 1;

    // Renegade's governed creator scripts are .rscripts, not Wicked ScriptComponent.
    std::string scriptReason;
    if (!Require(!CanRecordTimelinePreset(
            scene, target, TimelineRecordPreset::ScriptPlay, &scriptReason),
            "native Wicked script timeline preset must not claim Renegade script support")) return 1;
    if (!Require(!scriptReason.empty() &&
                 TimelineRecordPresetPaths(TimelineRecordPreset::ScriptPlay).empty(),
            "script timeline rejection should be explicit")) return 1;

    const auto channelsBeforeClose = CollectTimelineChannels(scene, animationEntity);
    if (!Require(commands.Execute(
            std::make_unique<CloseTimelineLoopCommand>(scene, animationEntity, 3.0f)),
            "close loop should duplicate first value keys")) return 1;
    const auto channelsAfterClose = CollectTimelineChannels(scene, animationEntity);
    if (!Require(channelsAfterClose.size() == channelsBeforeClose.size(),
            "close loop should not create parallel channels")) return 1;
    for (std::size_t i = 0; i < channelsBeforeClose.size(); ++i)
    {
        const bool isEvent = IsTimelineEventPath(animation.channels[i].path);
        const std::size_t expected = channelsBeforeClose[i].keyframeCount + (isEvent ? 0u : 1u);
        if (!Require(channelsAfterClose[i].keyframeCount == expected,
                "close loop must add value continuity keys without manufacturing events")) return 1;
    }
    if (!Require(commands.Undo(), "close loop undo should succeed")) return 1;

    // Wicked animation-data ownership is structural: recursive deletion of the
    // Animation entity must also remove all command-created data children.
    scene.Entity_Remove(animationEntity, true);
    if (!Require(!scene.animation_datas.Contains(translationDataEntity) &&
                 !scene.animation_datas.Contains(rotationDataEntity) &&
                 !scene.animation_datas.Contains(scaleDataEntity) &&
                 !scene.animation_datas.Contains(soundDataEntity) &&
                 !scene.hierarchy.Contains(translationDataEntity) &&
                 !scene.hierarchy.Contains(rotationDataEntity) &&
                 !scene.hierarchy.Contains(scaleDataEntity) &&
                 !scene.hierarchy.Contains(soundDataEntity),
            "recursive animation deletion must remove all owned animation-data children")) return 1;

    std::cout << "Phase 7D native timeline tests passed\n";
    return 0;
}
