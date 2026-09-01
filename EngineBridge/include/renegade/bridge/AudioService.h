#pragma once

#include <WickedEngine.h>

#include <cstdint>
#include <string>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    inline constexpr const char* AudioSourceMetadataKey =
        "renegade.audio.source";
    inline constexpr const char* AudioSourceMetadataVersion = "1";
    inline constexpr const char* AudioMixMetadataKey =
        "renegade.audio.mix";
    inline constexpr const char* AudioMixMetadataVersion = "1";

    // These names deliberately map onto Wicked's four serialized SUBMIX_TYPE
    // values without introducing a competing mixer.
    enum class AudioBus : std::uint32_t
    {
        SoundEffect = 0,
        Music = 1,
        Ambience = 2, // Wicked USER0
        Voice = 3,   // Wicked USER1
    };

    struct SoundSourceState
    {
        std::string filename;
        float volume = 1.0f;
        bool looped = false;
        bool spatial = true;
        bool reverb = false;
        bool playOnStart = true;
        AudioBus bus = AudioBus::SoundEffect;
    };

    struct SceneAudioMixState
    {
        float masterVolume = 1.0f;
        float soundEffectVolume = 1.0f;
        float musicVolume = 1.0f;
        float ambienceVolume = 1.0f;
        float voiceVolume = 1.0f;
        wi::audio::REVERB_PRESET reverbPreset =
            wi::audio::REVERB_PRESET_DEFAULT;
    };

    [[nodiscard]] bool IsRenegadeSoundSource(
        const wi::scene::Scene& scene,
        wi::ecs::Entity entity) noexcept;
    [[nodiscard]] SoundSourceState CaptureSoundSource(
        const wi::scene::Scene& scene,
        wi::ecs::Entity entity) noexcept;
    [[nodiscard]] SoundSourceState SanitizeSoundSourceState(
        const SoundSourceState& state) noexcept;
    [[nodiscard]] bool HasSoundSourceStateChange(
        const SoundSourceState& before,
        const SoundSourceState& after) noexcept;
    bool ApplySoundSource(
        wi::scene::Scene& scene,
        wi::ecs::Entity entity,
        const SoundSourceState& state,
        std::string& error);

    [[nodiscard]] SceneAudioMixState CaptureSceneAudioMix(
        const wi::scene::Scene& scene) noexcept;
    [[nodiscard]] SceneAudioMixState SanitizeSceneAudioMixState(
        const SceneAudioMixState& state) noexcept;
    [[nodiscard]] bool HasSceneAudioMixStateChange(
        const SceneAudioMixState& before,
        const SceneAudioMixState& after) noexcept;
    void ApplySceneAudioMixToWicked(
        const SceneAudioMixState& state) noexcept;

    // Runtime calls this after every accepted scene replacement. It applies the
    // authored scene mix and starts only sources explicitly marked Play On Start.
    void ActivateSceneAudio(
        wi::scene::Scene& scene) noexcept;

    class CreateSoundSourceCommand final : public ICommand
    {
    public:
        CreateSoundSourceCommand(
            wi::scene::Scene& scene,
            const SoundSourceState& state,
            const TransformState& transform);

        bool Execute() override;
        void Undo() override;

        [[nodiscard]] wi::ecs::Entity CreatedEntity() const noexcept;

    private:
        [[nodiscard]] std::string MakeUniqueName() const;

        wi::scene::Scene* scene_ = nullptr;
        SoundSourceState state_;
        TransformState transform_;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        wi::Archive snapshot_;
        bool hasSnapshot_ = false;
    };

    class SetSoundSourceCommand final : public ICommand
    {
    public:
        SetSoundSourceCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            const SoundSourceState& state);
        SetSoundSourceCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            const SoundSourceState& before,
            const SoundSourceState& after);

        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const SoundSourceState& state) noexcept;

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        SoundSourceState before_;
        SoundSourceState after_;
    };

    class SetSceneAudioMixCommand final : public ICommand
    {
    public:
        SetSceneAudioMixCommand(
            wi::scene::Scene& scene,
            const SceneAudioMixState& state);
        SetSceneAudioMixCommand(
            wi::scene::Scene& scene,
            const SceneAudioMixState& before,
            const SceneAudioMixState& after);

        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const SceneAudioMixState& state, bool permitCreate) noexcept;

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity originalMixEntity_ = wi::ecs::INVALID_ENTITY;
        SceneAudioMixState before_;
        SceneAudioMixState after_;
        wi::ecs::Entity createdMixEntity_ = wi::ecs::INVALID_ENTITY;
    };
}
