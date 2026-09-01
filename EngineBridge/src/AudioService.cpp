#include "renegade/bridge/AudioService.h"

#include <algorithm>
#include <cmath>

namespace renegade::bridge
{
    namespace
    {
        constexpr float Epsilon = 0.00001f;
        constexpr const char* KeyPlayOnStart = "renegade.audio.play_on_start";
        constexpr const char* KeyMasterVolume = "renegade.audio.master_volume";
        constexpr const char* KeySoundEffectVolume = "renegade.audio.sfx_volume";
        constexpr const char* KeyMusicVolume = "renegade.audio.music_volume";
        constexpr const char* KeyAmbienceVolume = "renegade.audio.ambience_volume";
        constexpr const char* KeyVoiceVolume = "renegade.audio.voice_volume";
        constexpr const char* KeyReverbPreset = "renegade.audio.reverb_preset";

        float FiniteOr(const float value, const float fallback) noexcept
        {
            return std::isfinite(value) ? value : fallback;
        }

        bool NearlyEqual(const float left, const float right) noexcept
        {
            return std::abs(left - right) <= Epsilon;
        }

        wi::audio::SUBMIX_TYPE ToWickedBus(const AudioBus bus) noexcept
        {
            switch (bus)
            {
            case AudioBus::Music:
                return wi::audio::SUBMIX_TYPE_MUSIC;
            case AudioBus::Ambience:
                return wi::audio::SUBMIX_TYPE_USER0;
            case AudioBus::Voice:
                return wi::audio::SUBMIX_TYPE_USER1;
            case AudioBus::SoundEffect:
            default:
                return wi::audio::SUBMIX_TYPE_SOUNDEFFECT;
            }
        }

        AudioBus FromWickedBus(const wi::audio::SUBMIX_TYPE bus) noexcept
        {
            switch (bus)
            {
            case wi::audio::SUBMIX_TYPE_MUSIC:
                return AudioBus::Music;
            case wi::audio::SUBMIX_TYPE_USER0:
                return AudioBus::Ambience;
            case wi::audio::SUBMIX_TYPE_USER1:
                return AudioBus::Voice;
            case wi::audio::SUBMIX_TYPE_SOUNDEFFECT:
            default:
                return AudioBus::SoundEffect;
            }
        }

        wi::ecs::Entity FindAudioMixEntity(const wi::scene::Scene& scene) noexcept
        {
            for (std::size_t index = 0; index < scene.metadatas.GetCount(); ++index)
            {
                const auto entity = scene.metadatas.GetEntity(index);
                const auto& metadata = scene.metadatas[index];
                if (metadata.string_values.has(AudioMixMetadataKey) &&
                    metadata.string_values.get(AudioMixMetadataKey) ==
                        AudioMixMetadataVersion)
                {
                    return entity;
                }
            }
            return wi::ecs::INVALID_ENTITY;
        }

        bool EntityExists(
            const wi::scene::Scene& scene,
            const wi::ecs::Entity entity) noexcept
        {
            return entity != wi::ecs::INVALID_ENTITY &&
                (scene.transforms.Contains(entity) ||
                    scene.names.Contains(entity) ||
                    scene.metadatas.Contains(entity) ||
                    scene.sounds.Contains(entity));
        }

        float ReadFloat(
            const wi::scene::MetadataComponent& metadata,
            const char* key,
            const float fallback) noexcept
        {
            return metadata.float_values.has(key)
                ? metadata.float_values.get(key)
                : fallback;
        }

        bool ReadBool(
            const wi::scene::MetadataComponent& metadata,
            const char* key,
            const bool fallback) noexcept
        {
            return metadata.bool_values.has(key)
                ? metadata.bool_values.get(key)
                : fallback;
        }

        int ReadInt(
            const wi::scene::MetadataComponent& metadata,
            const char* key,
            const int fallback) noexcept
        {
            return metadata.int_values.has(key)
                ? metadata.int_values.get(key)
                : fallback;
        }

        void WriteSourceMetadata(
            wi::scene::MetadataComponent& metadata,
            const SoundSourceState& state)
        {
            metadata.string_values.set(
                AudioSourceMetadataKey,
                AudioSourceMetadataVersion);
            metadata.bool_values.set(KeyPlayOnStart, state.playOnStart);
        }

        void WriteMixMetadata(
            wi::scene::MetadataComponent& metadata,
            const SceneAudioMixState& state)
        {
            metadata.string_values.set(AudioMixMetadataKey, AudioMixMetadataVersion);
            metadata.float_values.set(KeyMasterVolume, state.masterVolume);
            metadata.float_values.set(KeySoundEffectVolume, state.soundEffectVolume);
            metadata.float_values.set(KeyMusicVolume, state.musicVolume);
            metadata.float_values.set(KeyAmbienceVolume, state.ambienceVolume);
            metadata.float_values.set(KeyVoiceVolume, state.voiceVolume);
            metadata.int_values.set(
                KeyReverbPreset,
                static_cast<int>(state.reverbPreset));
        }

        std::string MakeUniqueName(
            const wi::scene::Scene& scene,
            const std::string& base)
        {
            std::string candidate = base;
            int suffix = 2;
            bool collision = true;
            while (collision)
            {
                collision = false;
                for (std::size_t index = 0; index < scene.names.GetCount(); ++index)
                {
                    if (scene.names[index].name == candidate)
                    {
                        collision = true;
                        candidate = base + " " + std::to_string(suffix++);
                        break;
                    }
                }
            }
            return candidate;
        }

        bool MixIsDefault(const SceneAudioMixState& state) noexcept
        {
            return !HasSceneAudioMixStateChange(SceneAudioMixState{}, state);
        }
    }

    bool IsRenegadeSoundSource(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        const auto* metadata = scene.metadatas.GetComponent(entity);
        return scene.sounds.Contains(entity) && metadata != nullptr &&
            metadata->string_values.has(AudioSourceMetadataKey) &&
            metadata->string_values.get(AudioSourceMetadataKey) ==
                AudioSourceMetadataVersion;
    }

    SoundSourceState CaptureSoundSource(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        SoundSourceState state;
        const auto* sound = scene.sounds.GetComponent(entity);
        if (sound == nullptr)
            return state;

        state.filename = sound->filename;
        state.volume = sound->volume;
        state.looped = sound->IsLooped();
        state.spatial = !sound->IsDisable3D();
        state.reverb = sound->soundinstance.IsEnableReverb();
        state.bus = FromWickedBus(sound->soundinstance.type);

        if (const auto* metadata = scene.metadatas.GetComponent(entity))
            state.playOnStart = ReadBool(*metadata, KeyPlayOnStart, true);

        return SanitizeSoundSourceState(state);
    }

    SoundSourceState SanitizeSoundSourceState(
        const SoundSourceState& state) noexcept
    {
        SoundSourceState result = state;
        result.volume = std::clamp(FiniteOr(result.volume, 1.0f), 0.0f, 1.0f);
        const auto bus = static_cast<std::uint32_t>(result.bus);
        if (bus > static_cast<std::uint32_t>(AudioBus::Voice))
            result.bus = AudioBus::SoundEffect;
        return result;
    }

    bool HasSoundSourceStateChange(
        const SoundSourceState& before,
        const SoundSourceState& after) noexcept
    {
        const auto left = SanitizeSoundSourceState(before);
        const auto right = SanitizeSoundSourceState(after);
        return left.filename != right.filename ||
            !NearlyEqual(left.volume, right.volume) ||
            left.looped != right.looped ||
            left.spatial != right.spatial ||
            left.reverb != right.reverb ||
            left.playOnStart != right.playOnStart ||
            left.bus != right.bus;
    }

    bool ApplySoundSource(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const SoundSourceState& state,
        std::string& error)
    {
        auto* sound = scene.sounds.GetComponent(entity);
        if (sound == nullptr)
        {
            error = "Audio source is missing its native Wicked SoundComponent.";
            return false;
        }

        const auto safe = SanitizeSoundSourceState(state);
        const bool recreate =
            sound->filename != safe.filename ||
            sound->soundinstance.type != ToWickedBus(safe.bus) ||
            sound->soundinstance.IsEnableReverb() != safe.reverb;
        const bool wasPlaying = sound->IsPlaying();

        if (recreate && wasPlaying)
            sound->Stop();

        if (sound->filename != safe.filename)
        {
            sound->filename = safe.filename;
            sound->soundResource = safe.filename.empty()
                ? wi::Resource{}
                : wi::resourcemanager::Load(safe.filename);
            if (!safe.filename.empty() && !sound->soundResource.IsValid())
            {
                error = "Could not load audio source: " + safe.filename;
                return false;
            }
        }

        sound->soundinstance.type = ToWickedBus(safe.bus);
        sound->soundinstance.SetEnableReverb(safe.reverb);
        if (recreate && sound->soundResource.IsValid())
        {
            if (!wi::audio::CreateSoundInstance(
                    &sound->soundResource.GetSound(),
                    &sound->soundinstance))
            {
                error = "Wicked could not create the audio playback instance.";
                return false;
            }
        }

        sound->volume = safe.volume;
        sound->SetLooped(safe.looped);
        sound->SetDisable3D(!safe.spatial);

        auto* metadata = scene.metadatas.GetComponent(entity);
        if (metadata == nullptr)
            metadata = &scene.metadatas.Create(entity);
        WriteSourceMetadata(*metadata, safe);

        if (recreate && wasPlaying && sound->soundResource.IsValid())
            sound->Play();

        error.clear();
        return true;
    }

    SceneAudioMixState CaptureSceneAudioMix(
        const wi::scene::Scene& scene) noexcept
    {
        SceneAudioMixState state;
        const auto entity = FindAudioMixEntity(scene);
        const auto* metadata = scene.metadatas.GetComponent(entity);
        if (metadata == nullptr)
            return state;

        state.masterVolume = ReadFloat(*metadata, KeyMasterVolume, state.masterVolume);
        state.soundEffectVolume = ReadFloat(
            *metadata, KeySoundEffectVolume, state.soundEffectVolume);
        state.musicVolume = ReadFloat(*metadata, KeyMusicVolume, state.musicVolume);
        state.ambienceVolume = ReadFloat(
            *metadata, KeyAmbienceVolume, state.ambienceVolume);
        state.voiceVolume = ReadFloat(*metadata, KeyVoiceVolume, state.voiceVolume);
        state.reverbPreset = static_cast<wi::audio::REVERB_PRESET>(ReadInt(
            *metadata,
            KeyReverbPreset,
            static_cast<int>(state.reverbPreset)));
        return SanitizeSceneAudioMixState(state);
    }

    SceneAudioMixState SanitizeSceneAudioMixState(
        const SceneAudioMixState& state) noexcept
    {
        SceneAudioMixState result = state;
        result.masterVolume = std::clamp(FiniteOr(result.masterVolume, 1.0f), 0.0f, 1.0f);
        result.soundEffectVolume = std::clamp(
            FiniteOr(result.soundEffectVolume, 1.0f), 0.0f, 1.0f);
        result.musicVolume = std::clamp(FiniteOr(result.musicVolume, 1.0f), 0.0f, 1.0f);
        result.ambienceVolume = std::clamp(
            FiniteOr(result.ambienceVolume, 1.0f), 0.0f, 1.0f);
        result.voiceVolume = std::clamp(FiniteOr(result.voiceVolume, 1.0f), 0.0f, 1.0f);
        const int preset = std::clamp(
            static_cast<int>(result.reverbPreset),
            static_cast<int>(wi::audio::REVERB_PRESET_DEFAULT),
            static_cast<int>(wi::audio::REVERB_PRESET_PLATE));
        result.reverbPreset = static_cast<wi::audio::REVERB_PRESET>(preset);
        return result;
    }

    bool HasSceneAudioMixStateChange(
        const SceneAudioMixState& before,
        const SceneAudioMixState& after) noexcept
    {
        const auto left = SanitizeSceneAudioMixState(before);
        const auto right = SanitizeSceneAudioMixState(after);
        return !NearlyEqual(left.masterVolume, right.masterVolume) ||
            !NearlyEqual(left.soundEffectVolume, right.soundEffectVolume) ||
            !NearlyEqual(left.musicVolume, right.musicVolume) ||
            !NearlyEqual(left.ambienceVolume, right.ambienceVolume) ||
            !NearlyEqual(left.voiceVolume, right.voiceVolume) ||
            left.reverbPreset != right.reverbPreset;
    }

    void ApplySceneAudioMixToWicked(const SceneAudioMixState& state) noexcept
    {
        const auto safe = SanitizeSceneAudioMixState(state);
        wi::audio::SetVolume(safe.masterVolume);
        wi::audio::SetSubmixVolume(
            wi::audio::SUBMIX_TYPE_SOUNDEFFECT, safe.soundEffectVolume);
        wi::audio::SetSubmixVolume(
            wi::audio::SUBMIX_TYPE_MUSIC, safe.musicVolume);
        wi::audio::SetSubmixVolume(
            wi::audio::SUBMIX_TYPE_USER0, safe.ambienceVolume);
        wi::audio::SetSubmixVolume(
            wi::audio::SUBMIX_TYPE_USER1, safe.voiceVolume);
        wi::audio::SetReverb(safe.reverbPreset);
    }

    void ActivateSceneAudio(wi::scene::Scene& scene) noexcept
    {
        ApplySceneAudioMixToWicked(CaptureSceneAudioMix(scene));
        for (std::size_t index = 0; index < scene.sounds.GetCount(); ++index)
        {
            const auto entity = scene.sounds.GetEntity(index);
            if (!IsRenegadeSoundSource(scene, entity))
                continue;
            auto& sound = scene.sounds[index];
            const auto state = CaptureSoundSource(scene, entity);
            sound.Stop();
            if (state.playOnStart && sound.soundResource.IsValid())
                sound.Play();
        }
    }

    CreateSoundSourceCommand::CreateSoundSourceCommand(
        wi::scene::Scene& scene,
        const SoundSourceState& state,
        const TransformState& transform)
        : scene_(&scene)
        , state_(SanitizeSoundSourceState(state))
        , transform_(transform)
    {
    }

    bool CreateSoundSourceCommand::Execute()
    {
        if (scene_ == nullptr)
            return false;

        if (hasSnapshot_)
        {
            if (EntityExists(*scene_, entity_))
                return false;
            snapshot_.SetReadModeAndResetPos(true);
            wi::ecs::EntitySerializer serializer;
            serializer.allow_remap = false;
            return scene_->Entity_Serialize(snapshot_, serializer) == entity_;
        }

        entity_ = scene_->Entity_CreateSound(
            MakeUniqueName(),
            state_.filename,
            transform_.translation);
        auto* transform = scene_->transforms.GetComponent(entity_);
        if (entity_ == wi::ecs::INVALID_ENTITY ||
            scene_->sounds.GetComponent(entity_) == nullptr || transform == nullptr)
        {
            if (entity_ != wi::ecs::INVALID_ENTITY)
                scene_->Entity_Remove(entity_);
            entity_ = wi::ecs::INVALID_ENTITY;
            return false;
        }

        transform->translation_local = transform_.translation;
        transform->rotation_local = transform_.rotation;
        transform->scale_local = transform_.scale;
        transform->SetDirty();
        transform->UpdateTransform();

        std::string error;
        if (!ApplySoundSource(*scene_, entity_, state_, error))
        {
            scene_->Entity_Remove(entity_);
            entity_ = wi::ecs::INVALID_ENTITY;
            return false;
        }

        snapshot_.SetReadModeAndResetPos(false);
        wi::ecs::EntitySerializer serializer;
        scene_->Entity_Serialize(snapshot_, serializer, entity_);
        hasSnapshot_ = true;
        return true;
    }

    void CreateSoundSourceCommand::Undo()
    {
        if (scene_ != nullptr && EntityExists(*scene_, entity_))
            scene_->Entity_Remove(entity_);
    }

    wi::ecs::Entity CreateSoundSourceCommand::CreatedEntity() const noexcept
    {
        return entity_;
    }

    std::string CreateSoundSourceCommand::MakeUniqueName() const
    {
        return ::renegade::bridge::MakeUniqueName(*scene_, "Sound Source");
    }

    SetSoundSourceCommand::SetSoundSourceCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const SoundSourceState& state)
        : scene_(&scene)
        , entity_(entity)
        , before_(CaptureSoundSource(scene, entity))
        , after_(SanitizeSoundSourceState(state))
    {
    }

    SetSoundSourceCommand::SetSoundSourceCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const SoundSourceState& before,
        const SoundSourceState& after)
        : scene_(&scene)
        , entity_(entity)
        , before_(SanitizeSoundSourceState(before))
        , after_(SanitizeSoundSourceState(after))
    {
    }

    bool SetSoundSourceCommand::Execute()
    {
        return HasSoundSourceStateChange(before_, after_) && Apply(after_);
    }

    void SetSoundSourceCommand::Undo()
    {
        Apply(before_);
    }

    bool SetSoundSourceCommand::Apply(const SoundSourceState& state) noexcept
    {
        if (scene_ == nullptr)
            return false;
        std::string ignored;
        return ApplySoundSource(*scene_, entity_, state, ignored);
    }

    SetSceneAudioMixCommand::SetSceneAudioMixCommand(
        wi::scene::Scene& scene,
        const SceneAudioMixState& state)
        : scene_(&scene)
        , originalMixEntity_(FindAudioMixEntity(scene))
        , before_(CaptureSceneAudioMix(scene))
        , after_(SanitizeSceneAudioMixState(state))
    {
    }

    SetSceneAudioMixCommand::SetSceneAudioMixCommand(
        wi::scene::Scene& scene,
        const SceneAudioMixState& before,
        const SceneAudioMixState& after)
        : scene_(&scene)
        , originalMixEntity_(FindAudioMixEntity(scene))
        , before_(SanitizeSceneAudioMixState(before))
        , after_(SanitizeSceneAudioMixState(after))
    {
    }

    bool SetSceneAudioMixCommand::Execute()
    {
        return HasSceneAudioMixStateChange(before_, after_) && Apply(after_, true);
    }

    void SetSceneAudioMixCommand::Undo()
    {
        if (scene_ == nullptr)
            return;
        if (originalMixEntity_ == wi::ecs::INVALID_ENTITY &&
            createdMixEntity_ != wi::ecs::INVALID_ENTITY &&
            EntityExists(*scene_, createdMixEntity_))
        {
            scene_->Entity_Remove(createdMixEntity_);
            createdMixEntity_ = wi::ecs::INVALID_ENTITY;
            ApplySceneAudioMixToWicked(before_);
            return;
        }
        Apply(before_, false);
    }

    bool SetSceneAudioMixCommand::Apply(
        const SceneAudioMixState& state,
        const bool permitCreate) noexcept
    {
        if (scene_ == nullptr)
            return false;

        const auto safe = SanitizeSceneAudioMixState(state);
        auto entity = FindAudioMixEntity(*scene_);
        if (entity == wi::ecs::INVALID_ENTITY)
        {
            if (!permitCreate || MixIsDefault(safe))
            {
                ApplySceneAudioMixToWicked(safe);
                return true;
            }
            entity = scene_->Entity_CreateTransform("Audio Mix");
            if (entity == wi::ecs::INVALID_ENTITY)
                return false;
            createdMixEntity_ = entity;
        }

        auto* metadata = scene_->metadatas.GetComponent(entity);
        if (metadata == nullptr)
            metadata = &scene_->metadatas.Create(entity);
        WriteMixMetadata(*metadata, safe);
        ApplySceneAudioMixToWicked(safe);
        return true;
    }
}
