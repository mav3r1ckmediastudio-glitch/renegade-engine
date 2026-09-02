#include "renegade/bridge/AudioService.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    int failures = 0;

    void Check(const bool condition, const char* message)
    {
        if (condition)
            return;
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }

    bool Near(const float left, const float right)
    {
        return std::abs(left - right) < 0.0001f;
    }

    void AppendLe16(std::vector<unsigned char>& bytes, const std::uint16_t value)
    {
        bytes.push_back(static_cast<unsigned char>(value));
        bytes.push_back(static_cast<unsigned char>(value >> 8u));
    }

    void AppendLe32(std::vector<unsigned char>& bytes, const std::uint32_t value)
    {
        bytes.push_back(static_cast<unsigned char>(value));
        bytes.push_back(static_cast<unsigned char>(value >> 8u));
        bytes.push_back(static_cast<unsigned char>(value >> 16u));
        bytes.push_back(static_cast<unsigned char>(value >> 24u));
    }

    std::filesystem::path WriteWaveFixture(
        const std::filesystem::path& folder,
        const char* name,
        const std::uint32_t formatSize)
    {
        std::vector<unsigned char> bytes;
        const auto appendText = [&bytes](const char* text)
        {
            for (int i = 0; i < 4; ++i)
                bytes.push_back(static_cast<unsigned char>(text[i]));
        };
        appendText("RIFF");
        AppendLe32(bytes, 4u + 8u + formatSize + 8u + 4u);
        appendText("WAVE");
        appendText("fmt ");
        AppendLe32(bytes, formatSize);
        AppendLe16(bytes, formatSize > 18u ? 0xfffeu : 1u);
        AppendLe16(bytes, 1u);
        AppendLe32(bytes, 44100u);
        AppendLe32(bytes, 88200u);
        AppendLe16(bytes, 2u);
        AppendLe16(bytes, 16u);
        while (bytes.size() < 20u + formatSize)
            bytes.push_back(0u);
        appendText("data");
        AppendLe32(bytes, 4u);
        bytes.insert(bytes.end(), 4u, 0u);

        const auto path = folder / name;
        std::ofstream stream(path, std::ios::binary);
        stream.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        return path;
    }
}

int main()
{
    using namespace renegade::bridge;

    {
        SoundSourceState unsafe;
        unsafe.volume = 42.0f;
        unsafe.bus = static_cast<AudioBus>(999u);
        const auto safe = SanitizeSoundSourceState(unsafe);
        Check(Near(safe.volume, 1.0f), "sound volume was not clamped");
        Check(safe.bus == AudioBus::SoundEffect, "invalid audio bus was not rejected");
    }

    {
        SceneAudioMixState unsafe;
        unsafe.masterVolume = -2.0f;
        unsafe.soundEffectVolume = 5.0f;
        unsafe.musicVolume = -5.0f;
        unsafe.ambienceVolume = 2.0f;
        unsafe.voiceVolume = 0.25f;
        unsafe.reverbPreset = static_cast<wi::audio::REVERB_PRESET>(999);
        const auto safe = SanitizeSceneAudioMixState(unsafe);
        Check(Near(safe.masterVolume, 0.0f), "master mix volume was not clamped");
        Check(Near(safe.soundEffectVolume, 1.0f), "SFX mix volume was not clamped");
        Check(Near(safe.musicVolume, 0.0f), "music mix volume was not clamped");
        Check(Near(safe.ambienceVolume, 1.0f), "ambience mix volume was not clamped");
        Check(Near(safe.voiceVolume, 0.25f), "voice mix volume changed unexpectedly");
        Check(
            safe.reverbPreset == wi::audio::REVERB_PRESET_PLATE,
            "reverb preset was not bounded to Wicked's native preset range");
    }

    {
        wi::scene::Scene scene;
        const auto entity = scene.Entity_CreateTransform("Audio Test Source");
        scene.sounds.Create(entity);

        SoundSourceState authored;
        authored.volume = 0.4f;
        authored.looped = true;
        authored.spatial = false;
        authored.playOnStart = false;
        authored.bus = AudioBus::SoundEffect;
        authored.zoneEnabled = true;
        authored.zoneRadius = 12.5f;
        authored.durationSeconds = 4.0f;
        authored.repeatable = false;

        std::string error;
        Check(
            ApplySoundSource(scene, entity, authored, error),
            "native Wicked sound component could not accept Renegade source state");
        Check(error.empty(), "successful audio apply returned an error");
        Check(IsRenegadeSoundSource(scene, entity), "source metadata marker was not created");

        const auto captured = CaptureSoundSource(scene, entity);
        Check(Near(captured.volume, 0.4f), "source volume did not round-trip");
        Check(captured.looped, "source loop state did not round-trip");
        Check(!captured.spatial, "source 2D/3D state did not round-trip");
        Check(!captured.playOnStart, "source Play On Start did not round-trip");
        Check(captured.bus == AudioBus::SoundEffect, "source bus did not round-trip");
        Check(captured.zoneEnabled, "sound-zone marker did not round-trip");
        Check(Near(captured.zoneRadius, 12.5f), "sound-zone radius did not round-trip");
        Check(Near(captured.durationSeconds, 4.0f), "sound-zone duration did not round-trip");
        Check(!captured.repeatable, "sound-zone once mode did not round-trip");

        auto* nativeSound = scene.sounds.GetComponent(entity);
        nativeSound->_flags |= wi::scene::SoundComponent::PLAYING;
        SceneAudioPauseState pauseState;
        SetSceneAudioPaused(scene, true, pauseState);
        Check(!nativeSound->IsPlaying(), "pause left Wicked PLAYING intent active");
        Check(
            pauseState.playingSources.size() == 1u &&
                pauseState.playingSources.front() == entity,
            "pause did not retain transient resume ownership");
    }

    {
        wi::scene::Scene scene;
        SoundSourceState global;
        global.looped = true;
        global.spatial = false;
        global.playOnStart = true;
        global.bus = AudioBus::Ambience;
        global.zoneEnabled = false;
        TransformState transform;
        transform.translation = XMFLOAT3(40.0f, 8.0f, -20.0f);

        CreateSoundSourceCommand command(scene, global, transform);
        Check(command.Execute(), "global 2D sound command could not create a source");
        const auto entity = command.CreatedEntity();
        Check(IsRenegadeSoundSource(scene, entity), "global source was not marked as Renegade audio");
        Check(!scene.transforms.Contains(entity), "global 2D source retained a meaningless world transform");
        const auto* name = scene.names.GetComponent(entity);
        Check(name != nullptr && name->name == "Global Ambience", "global ambience did not receive a clear hierarchy name");
        const auto captured = CaptureSoundSource(scene, entity);
        Check(!captured.spatial && !captured.zoneEnabled, "global source did not remain non-spatial and non-zone");
        Check(captured.looped && captured.playOnStart, "global source defaults did not retain persistent playback intent");
        Check(captured.bus == AudioBus::Ambience, "global source did not retain ambience bus ownership");

        command.Undo();
        Check(!IsRenegadeSoundSource(scene, entity), "global source Undo did not remove the entity");
        Check(command.Execute(), "global source Redo could not restore the entity snapshot");
        Check(IsRenegadeSoundSource(scene, entity), "global source Redo lost audio metadata");
        Check(!scene.transforms.Contains(entity), "global source Redo restored a viewport transform");
    }

    {
        wi::scene::Scene empty;
        const auto mix = CaptureSceneAudioMix(empty);
        Check(Near(mix.masterVolume, 1.0f), "scene without mix did not retain default master volume");
        Check(
            mix.reverbPreset == wi::audio::REVERB_PRESET_DEFAULT,
            "scene without mix did not retain default Wicked reverb");
    }

    {
        const auto folder = std::filesystem::temp_directory_path() /
            "renegade_phase6_gate3_audio_tests";
        std::error_code ec;
        std::filesystem::create_directories(folder, ec);
        const auto pcm = WriteWaveFixture(folder, "standard_pcm.wav", 16u);
        const auto extended = WriteWaveFixture(folder, "extended.wav", 40u);
        std::string error;
        Check(
            ValidateAudioAssetForWicked(pcm.generic_u8string(), error),
            "standard PCM WAV was rejected by the Wicked safety preflight");
        Check(
            !ValidateAudioAssetForWicked(extended.generic_u8string(), error),
            "unsafe extended WAV reached the pinned Wicked loader");
        Check(
            error.find("Extended WAV") != std::string::npos,
            "extended WAV rejection did not explain the compatibility boundary");
        std::filesystem::remove_all(folder, ec);
    }

    if (failures != 0)
        return 1;

    std::cout << "PASS: Phase 6 Gate 3 native audio service contract\n";
    return 0;
}
