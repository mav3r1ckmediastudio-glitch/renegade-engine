#include "renegade/bridge/AudioService.h"

#include <cmath>
#include <iostream>
#include <string>

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
    }

    {
        wi::scene::Scene empty;
        const auto mix = CaptureSceneAudioMix(empty);
        Check(Near(mix.masterVolume, 1.0f), "scene without mix did not retain default master volume");
        Check(
            mix.reverbPreset == wi::audio::REVERB_PRESET_DEFAULT,
            "scene without mix did not retain default Wicked reverb");
    }

    if (failures != 0)
        return 1;

    std::cout << "PASS: Phase 6 Gate 3 native audio service contract\n";
    return 0;
}
