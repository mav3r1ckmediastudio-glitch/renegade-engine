#pragma once

#include <WickedEngine.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    using AnimationPath = wi::scene::AnimationComponent::AnimationChannel::Path;
    using AnimationSamplerMode = wi::scene::AnimationComponent::AnimationSampler::Mode;

    enum class TimelineRecordPreset : std::uint32_t
    {
        Transform,
        Translation,
        Rotation,
        Scale,
        MorphWeights,
        LightColor,
        LightIntensity,
        LightRange,
        LightInnerCone,
        LightOuterCone,
        SoundPlay,
        SoundStop,
        SoundVolume,
        EmitterEmitCount,
        CameraFov,
        CameraFocalLength,
        CameraApertureSize,
        CameraApertureShape,
        ScriptPlay,
        ScriptStop,
        MaterialColor,
        MaterialEmissive,
        MaterialRoughness,
        MaterialMetalness,
        MaterialReflectance,
        MaterialTexMulAdd,
    };

    struct TimelineChannelInfo
    {
        std::size_t index = 0;
        wi::ecs::Entity target = wi::ecs::INVALID_ENTITY;
        AnimationPath path = AnimationPath::UNKNOWN;
        int samplerIndex = -1;
        wi::ecs::Entity dataEntity = wi::ecs::INVALID_ENTITY;
        std::size_t keyframeCount = 0;
        AnimationSamplerMode mode = wi::scene::AnimationComponent::AnimationSampler::LINEAR;
    };

    struct TimelineKeyInfo
    {
        std::size_t index = 0;
        float time = 0.0f;
    };

    struct TimelineDataSnapshot
    {
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        wi::scene::AnimationDataComponent data;
    };

    struct TimelineGraphSnapshot
    {
        std::vector<wi::scene::AnimationComponent::AnimationChannel> channels;
        std::vector<wi::scene::AnimationComponent::AnimationSampler> samplers;
        std::vector<TimelineDataSnapshot> data;
    };

    [[nodiscard]] const char* TimelineRecordPresetLabel(TimelineRecordPreset preset) noexcept;
    [[nodiscard]] const char* TimelinePathLabel(AnimationPath path) noexcept;
    [[nodiscard]] std::vector<AnimationPath> TimelineRecordPresetPaths(TimelineRecordPreset preset);

    [[nodiscard]] std::vector<TimelineChannelInfo> CollectTimelineChannels(
        const wi::scene::Scene& scene,
        wi::ecs::Entity animationEntity);

    [[nodiscard]] std::vector<TimelineKeyInfo> CollectTimelineKeys(
        const wi::scene::Scene& scene,
        wi::ecs::Entity animationEntity,
        std::size_t channelIndex);

    [[nodiscard]] std::size_t TimelineValueWidth(
        const wi::scene::Scene& scene,
        const wi::scene::AnimationComponent::AnimationChannel& channel) noexcept;

    [[nodiscard]] bool CanRecordTimelinePreset(
        const wi::scene::Scene& scene,
        wi::ecs::Entity target,
        TimelineRecordPreset preset,
        std::string* reason = nullptr);

    [[nodiscard]] TimelineGraphSnapshot CaptureTimelineGraphSnapshot(
        const wi::scene::Scene& scene,
        wi::ecs::Entity animationEntity);

    class RecordTimelineKeyCommand final : public ICommand
    {
    public:
        RecordTimelineKeyCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity animationEntity,
            wi::ecs::Entity target,
            TimelineRecordPreset preset,
            float time);

        bool Execute() override;
        void Undo() override;

        [[nodiscard]] const std::string& Error() const noexcept { return error_; }
        [[nodiscard]] std::size_t RecordedPathCount() const noexcept { return recordedPathCount_; }

    private:
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity animationEntity_ = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity target_ = wi::ecs::INVALID_ENTITY;
        TimelineRecordPreset preset_ = TimelineRecordPreset::Transform;
        float time_ = 0.0f;
        bool prepared_ = false;
        TimelineGraphSnapshot before_;
        TimelineGraphSnapshot after_;
        std::vector<wi::ecs::Entity> createdDataEntities_;
        std::string error_;
        std::size_t recordedPathCount_ = 0;
    };

    class CloseTimelineLoopCommand final : public ICommand
    {
    public:
        CloseTimelineLoopCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity animationEntity,
            float time);

        bool Execute() override;
        void Undo() override;

        [[nodiscard]] const std::string& Error() const noexcept { return error_; }
        [[nodiscard]] std::size_t ClosedChannelCount() const noexcept { return closedChannelCount_; }

    private:
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity animationEntity_ = wi::ecs::INVALID_ENTITY;
        float time_ = 0.0f;
        bool prepared_ = false;
        TimelineGraphSnapshot before_;
        TimelineGraphSnapshot after_;
        std::string error_;
        std::size_t closedChannelCount_ = 0;
    };

    class MoveTimelineKeyCommand final : public ICommand
    {
    public:
        MoveTimelineKeyCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity animationEntity,
            std::size_t channelIndex,
            std::size_t keyIndex,
            float time);

        bool Execute() override;
        void Undo() override;

        [[nodiscard]] const std::string& Error() const noexcept { return error_; }

    private:
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity animationEntity_ = wi::ecs::INVALID_ENTITY;
        std::size_t channelIndex_ = 0;
        std::size_t keyIndex_ = 0;
        float time_ = 0.0f;
        bool prepared_ = false;
        TimelineGraphSnapshot before_;
        TimelineGraphSnapshot after_;
        std::string error_;
    };

    class DeleteTimelineKeyCommand final : public ICommand
    {
    public:
        DeleteTimelineKeyCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity animationEntity,
            std::size_t channelIndex,
            std::size_t keyIndex);

        bool Execute() override;
        void Undo() override;

        [[nodiscard]] const std::string& Error() const noexcept { return error_; }

    private:
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity animationEntity_ = wi::ecs::INVALID_ENTITY;
        std::size_t channelIndex_ = 0;
        std::size_t keyIndex_ = 0;
        bool prepared_ = false;
        TimelineGraphSnapshot before_;
        TimelineGraphSnapshot after_;
        std::string error_;
    };

    class SetTimelineSamplerModeCommand final : public ICommand
    {
    public:
        SetTimelineSamplerModeCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity animationEntity,
            std::size_t channelIndex,
            AnimationSamplerMode mode);

        bool Execute() override;
        void Undo() override;

        [[nodiscard]] const std::string& Error() const noexcept { return error_; }

    private:
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity animationEntity_ = wi::ecs::INVALID_ENTITY;
        std::size_t channelIndex_ = 0;
        AnimationSamplerMode mode_ = wi::scene::AnimationComponent::AnimationSampler::LINEAR;
        bool prepared_ = false;
        TimelineGraphSnapshot before_;
        TimelineGraphSnapshot after_;
        std::string error_;
    };
}
