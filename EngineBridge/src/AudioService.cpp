#include "renegade/bridge/AudioService.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>

namespace renegade::bridge
{
    namespace
    {
        constexpr float Epsilon = 0.00001f;
        constexpr const char* KeyPlayOnStart = "renegade.audio.play_on_start";
        constexpr const char* KeyZoneEnabled = "renegade.audio.zone_enabled";
        constexpr const char* KeyZoneRadius = "renegade.audio.zone_radius";
        constexpr const char* KeyZoneDuration = "renegade.audio.zone_duration";
        constexpr const char* KeyZoneRepeatable = "renegade.audio.zone_repeatable";
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
            metadata.bool_values.set(KeyZoneEnabled, state.zoneEnabled);
            metadata.float_values.set(KeyZoneRadius, state.zoneRadius);
            metadata.float_values.set(KeyZoneDuration, state.durationSeconds);
            metadata.bool_values.set(KeyZoneRepeatable, state.repeatable);
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

        std::uint16_t ReadLe16(const std::array<unsigned char, 40>& bytes, const std::size_t offset)
        {
            return static_cast<std::uint16_t>(bytes[offset]) |
                (static_cast<std::uint16_t>(bytes[offset + 1]) << 8u);
        }

        std::uint32_t ReadLe32(const unsigned char* bytes)
        {
            return static_cast<std::uint32_t>(bytes[0]) |
                (static_cast<std::uint32_t>(bytes[1]) << 8u) |
                (static_cast<std::uint32_t>(bytes[2]) << 16u) |
                (static_cast<std::uint32_t>(bytes[3]) << 24u);
        }

        bool ReadExact(std::ifstream& stream, void* destination, const std::size_t size)
        {
            return static_cast<bool>(stream.read(
                static_cast<char*>(destination),
                static_cast<std::streamsize>(size)));
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
        {
            state.playOnStart = ReadBool(*metadata, KeyPlayOnStart, true);
            state.zoneEnabled = ReadBool(*metadata, KeyZoneEnabled, false);
            state.zoneRadius = ReadFloat(*metadata, KeyZoneRadius, 5.0f);
            state.durationSeconds = ReadFloat(*metadata, KeyZoneDuration, 0.0f);
            state.repeatable = ReadBool(*metadata, KeyZoneRepeatable, true);
        }

        return SanitizeSoundSourceState(state);
    }

    SoundSourceState SanitizeSoundSourceState(
        const SoundSourceState& state) noexcept
    {
        SoundSourceState result = state;
        result.volume = std::clamp(FiniteOr(result.volume, 1.0f), 0.0f, 1.0f);
        result.zoneRadius = std::clamp(
            FiniteOr(result.zoneRadius, 5.0f), 0.5f, 500.0f);
        result.durationSeconds = std::clamp(
            FiniteOr(result.durationSeconds, 0.0f), 0.0f, 3600.0f);
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
            left.bus != right.bus ||
            left.zoneEnabled != right.zoneEnabled ||
            !NearlyEqual(left.zoneRadius, right.zoneRadius) ||
            !NearlyEqual(left.durationSeconds, right.durationSeconds) ||
            left.repeatable != right.repeatable;
    }

    bool ValidateAudioAssetForWicked(
        const std::string& filename,
        std::string& error)
    {
        namespace fs = std::filesystem;
        error.clear();
        if (filename.empty())
            return true;

        std::ifstream stream(fs::u8path(filename), std::ios::binary | std::ios::ate);
        if (!stream)
        {
            error = "Audio asset is not readable: " + filename;
            return false;
        }
        const auto end = stream.tellg();
        if (end <= 0)
        {
            error = "Audio asset is empty.";
            return false;
        }
        const auto fileSize = static_cast<std::uint64_t>(end);
        stream.seekg(0, std::ios::beg);

        std::string extension = fs::u8path(filename).extension().generic_u8string();
        std::transform(
            extension.begin(), extension.end(), extension.begin(),
            [](const unsigned char value)
            { return static_cast<char>(std::tolower(value)); });
        if (extension == ".ogg")
        {
            if (fileSize > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
            {
                error = "OGG asset exceeds the pinned Wicked decoder limit.";
                return false;
            }
            std::array<unsigned char, 4> signature = {};
            if (!ReadExact(stream, signature.data(), signature.size()) ||
                signature != std::array<unsigned char, 4>{'O', 'g', 'g', 'S'})
            {
                error = "Audio asset is not a valid OGG stream.";
                return false;
            }
            return true;
        }

        if (extension != ".wav")
        {
            error = "Gate 3 supports WAV and OGG audio assets.";
            return false;
        }
        if (fileSize < 12u)
        {
            error = "WAV asset is truncated.";
            return false;
        }

        std::array<unsigned char, 12> riff = {};
        if (!ReadExact(stream, riff.data(), riff.size()) ||
            std::string(reinterpret_cast<const char*>(riff.data()), 4) != "RIFF" ||
            std::string(reinterpret_cast<const char*>(riff.data() + 8), 4) != "WAVE")
        {
            error = "Audio asset is not a RIFF/WAVE file.";
            return false;
        }
        const std::uint64_t declaredEnd = 8u + ReadLe32(riff.data() + 4);
        if (declaredEnd > fileSize)
        {
            error = "WAV RIFF length exceeds the file size.";
            return false;
        }

        bool foundFormat = false;
        bool foundData = false;
        std::uint64_t offset = 12u;
        while (offset + 8u <= fileSize)
        {
            stream.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
            std::array<unsigned char, 8> chunk = {};
            if (!ReadExact(stream, chunk.data(), chunk.size()))
                break;
            const std::string id(reinterpret_cast<const char*>(chunk.data()), 4);
            const std::uint32_t chunkSize = ReadLe32(chunk.data() + 4);
            const std::uint64_t payload = offset + 8u;
            const std::uint64_t payloadEnd = payload + chunkSize;
            if (payloadEnd > fileSize)
            {
                error = "WAV chunk exceeds the file size.";
                return false;
            }

            if (id == "fmt ")
            {
                if (chunkSize < 16u || chunkSize > 18u)
                {
                    error = "Extended WAV headers are not safe in the pinned Wicked audio loader; export standard PCM WAV or OGG.";
                    return false;
                }
                std::array<unsigned char, 40> format = {};
                stream.seekg(static_cast<std::streamoff>(payload), std::ios::beg);
                if (!ReadExact(stream, format.data(), chunkSize))
                {
                    error = "WAV format chunk is truncated.";
                    return false;
                }
                const std::uint16_t formatTag = ReadLe16(format, 0);
                const std::uint16_t channels = ReadLe16(format, 2);
                const std::uint32_t sampleRate = ReadLe32(format.data() + 4);
                const std::uint32_t byteRate = ReadLe32(format.data() + 8);
                const std::uint16_t blockAlign = ReadLe16(format, 12);
                const std::uint16_t bitsPerSample = ReadLe16(format, 14);
                const bool supportedBits = bitsPerSample == 8u ||
                    bitsPerSample == 16u || bitsPerSample == 24u ||
                    bitsPerSample == 32u;
                const std::uint32_t expectedBlockAlign =
                    static_cast<std::uint32_t>(channels) * bitsPerSample / 8u;
                if (formatTag != 1u || channels == 0u || channels > 8u ||
                    sampleRate < 1000u || sampleRate > 200000u ||
                    !supportedBits || expectedBlockAlign == 0u ||
                    blockAlign != expectedBlockAlign ||
                    byteRate != sampleRate * expectedBlockAlign)
                {
                    error = "WAV format is not compatible with the pinned Wicked/XAudio2 PCM path.";
                    return false;
                }
                foundFormat = true;
            }
            else if (id == "data")
            {
                if (chunkSize == 0u || chunkSize > 0x7fffffffu)
                {
                    error = "WAV sample data exceeds the pinned XAudio2 buffer limit.";
                    return false;
                }
                foundData = true;
            }

            offset = payloadEnd + (chunkSize & 1u);
        }

        if (!foundFormat || !foundData)
        {
            error = "WAV asset is missing a valid fmt or data chunk.";
            return false;
        }
        return true;
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
        const bool filenameChanged = sound->filename != safe.filename;
        const bool recreate =
            filenameChanged ||
            sound->soundinstance.type != ToWickedBus(safe.bus) ||
            sound->soundinstance.IsEnableReverb() != safe.reverb ||
            sound->IsLooped() != safe.looped ||
            sound->IsDisable3D() == safe.spatial;
        const bool wasPlaying = sound->IsPlaying();

        wi::Resource nextResource = sound->soundResource;
        if (filenameChanged)
        {
            if (!ValidateAudioAssetForWicked(safe.filename, error))
                return false;
            nextResource = safe.filename.empty()
                ? wi::Resource{}
                : wi::resourcemanager::Load(safe.filename);
            if (!safe.filename.empty() && !nextResource.IsValid())
            {
                error = "Could not load audio source: " + safe.filename;
                return false;
            }
        }

        wi::audio::SoundInstance nextInstance;
        if (recreate)
        {
            // Preserve authored routing and instance flags even before an audio
            // asset is assigned. CreateSoundInstance() adds the backend handle
            // later without changing the source's selected bus semantics.
            nextInstance.type = ToWickedBus(safe.bus);
            nextInstance.SetEnableReverb(safe.reverb);
            nextInstance.SetLooped(safe.looped);
            if (nextResource.IsValid() &&
                !wi::audio::CreateSoundInstance(
                    &nextResource.GetSound(),
                    &nextInstance))
            {
                error = "Wicked could not create the audio playback instance.";
                return false;
            }
        }

        if (recreate)
        {
            if (wasPlaying && sound->soundinstance.IsValid())
                wi::audio::Stop(&sound->soundinstance);
            sound->filename = safe.filename;
            sound->soundResource = std::move(nextResource);
            sound->soundinstance = std::move(nextInstance);
        }
        sound->volume = safe.volume;
        if (safe.looped)
            sound->_flags |= wi::scene::SoundComponent::LOOPED;
        else
            sound->_flags &= ~wi::scene::SoundComponent::LOOPED;
        if (safe.spatial)
            sound->_flags &= ~wi::scene::SoundComponent::DISABLE_3D;
        else
            sound->_flags |= wi::scene::SoundComponent::DISABLE_3D;
        if (!sound->soundinstance.IsValid())
            sound->_flags &= ~wi::scene::SoundComponent::PLAYING;

        auto* metadata = scene.metadatas.GetComponent(entity);
        if (metadata == nullptr)
            metadata = &scene.metadatas.Create(entity);
        WriteSourceMetadata(*metadata, safe);

        if (recreate && wasPlaying && sound->soundinstance.IsValid())
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

    void ActivateSceneAudio(
        wi::scene::Scene& scene,
        SceneAudioZoneState* zoneState) noexcept
    {
        if (zoneState != nullptr)
            zoneState->entries.clear();
        ApplySceneAudioMixToWicked(CaptureSceneAudioMix(scene));
        for (std::size_t index = 0; index < scene.sounds.GetCount(); ++index)
        {
            const auto entity = scene.sounds.GetEntity(index);
            if (!IsRenegadeSoundSource(scene, entity))
                continue;
            auto& sound = scene.sounds[index];
            const auto state = CaptureSoundSource(scene, entity);
            sound.Stop();
            if (!state.zoneEnabled && state.playOnStart &&
                sound.soundResource.IsValid())
                sound.Play();
        }
    }

    void UpdateSceneAudioZones(
        wi::scene::Scene& scene,
        const XMFLOAT3& listenerPosition,
        const float deltaSeconds,
        SceneAudioZoneState& zoneState) noexcept
    {
        const float safeDelta = std::max(0.0f, FiniteOr(deltaSeconds, 0.0f));
        for (std::size_t index = 0; index < scene.sounds.GetCount(); ++index)
        {
            const auto entity = scene.sounds.GetEntity(index);
            if (!IsRenegadeSoundSource(scene, entity))
                continue;
            const auto source = CaptureSoundSource(scene, entity);
            if (!source.zoneEnabled)
                continue;
            auto* transform = scene.transforms.GetComponent(entity);
            auto* sound = scene.sounds.GetComponent(entity);
            if (transform == nullptr || sound == nullptr)
                continue;

            auto found = std::find_if(
                zoneState.entries.begin(), zoneState.entries.end(),
                [entity](const SoundZoneRuntimeEntry& entry)
                { return entry.entity == entity; });
            if (found == zoneState.entries.end())
            {
                zoneState.entries.push_back(SoundZoneRuntimeEntry{});
                found = std::prev(zoneState.entries.end());
                found->entity = entity;
            }
            auto& runtime = *found;
            const XMFLOAT3 center = transform->GetPosition();
            const float dx = listenerPosition.x - center.x;
            const float dy = listenerPosition.y - center.y;
            const float dz = listenerPosition.z - center.z;
            const bool inside = dx * dx + dy * dy + dz * dz <=
                source.zoneRadius * source.zoneRadius;
            const bool entered = inside && !runtime.wasInside;
            const bool exited = !inside && runtime.wasInside;

            if (entered && (!runtime.hasTriggered || source.repeatable) &&
                sound->soundResource.IsValid())
            {
                sound->Stop();
                sound->Play();
                runtime.hasTriggered = true;
                runtime.playing = true;
                runtime.elapsedSeconds = 0.0f;
            }

            if (runtime.playing)
            {
                runtime.elapsedSeconds += safeDelta;
                if (source.durationSeconds > 0.0f &&
                    runtime.elapsedSeconds >= source.durationSeconds)
                {
                    sound->Stop();
                    runtime.playing = false;
                }
            }

            if (exited && source.looped && runtime.playing)
            {
                sound->Stop();
                runtime.playing = false;
            }
            runtime.wasInside = inside;
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
            MakeUniqueName(), {}, transform_.translation);
        if (entity_ != wi::ecs::INVALID_ENTITY)
            scene_->sounds.Create(entity_);
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

        // A true global 2D source has no world-space meaning. Wicked only needs
        // TransformComponent for 3D audio, so remove the creation-time transform
        // before the command snapshot. This keeps global audio in the hierarchy
        // while keeping it out of the viewport/gizmo path, including Undo/Redo.
        if (!state_.zoneEnabled && !state_.spatial)
            scene_->transforms.Remove(entity_);

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
        const char* base = "Sound Source";
        if (state_.zoneEnabled)
        {
            switch (state_.bus)
            {
            case AudioBus::Music: base = "Music Zone"; break;
            case AudioBus::Ambience: base = "Ambience Zone"; break;
            case AudioBus::Voice: base = "Voice Zone"; break;
            case AudioBus::SoundEffect:
            default: base = "SFX Zone"; break;
            }
        }
        else if (!state_.spatial)
        {
            switch (state_.bus)
            {
            case AudioBus::Music: base = "Global Music"; break;
            case AudioBus::Ambience: base = "Global Ambience"; break;
            case AudioBus::Voice: base = "Global Voice"; break;
            case AudioBus::SoundEffect:
            default: base = "Global SFX"; break;
            }
        }
        return ::renegade::bridge::MakeUniqueName(*scene_, base);
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
