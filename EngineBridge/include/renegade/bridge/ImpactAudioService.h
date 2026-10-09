#pragma once
#include "renegade/bridge/ImpactSurface.h"
#include "renegade/bridge/ResourceAssetRuntimeService.h"
#include <array>
#include <random>

namespace renegade::bridge
{
    inline constexpr const char* ImpactAudioBankPath =
        "Content/Audio/Impacts/ImpactAudio.renegade-impact-audio";
    struct ImpactAudioBank
    {
        StableId projectId;
        std::array<std::vector<StableId>, 9> surfaces;
    };
    bool ReadImpactAudioBank(const std::string& projectRoot,
        const StableId& projectId, ImpactAudioBank& bank, std::string& error);
    bool WriteImpactAudioBank(const std::string& projectRoot,
        const ImpactAudioBank& bank, std::string& error);
    bool ValidateImpactAudioBank(const ImpactAudioBank& bank, std::string& error);
    bool PrepareImpactAudioAsset(const std::string& projectRoot,
        const std::string& packageRoot, const StableId& projectId,
        const StableId& assetId, std::vector<std::uint8_t>& payload,
        std::string& error);
    // Strict bounded PCM16 WAV validation before the pinned native decoder.
    bool ValidateImpactAudioPayload(const std::vector<std::uint8_t>& payload,
        std::string& error);
    bool AddImpactAudioDependencies(const std::string& projectRoot,
        const StableId& projectId, DependencyCollector& collector, std::string& error);

    bool SnapshotImpactAudio(const std::string& projectRoot, const std::string& snapshotRoot,
        const StableId& projectId, std::string& error);

    // Transient native audio voices: never serialized as authored scene sources.
    class ImpactAudioPlayer
    {
    public:
        static constexpr std::size_t MaxVoices = 32;
        bool Prepare(const std::string& projectRoot, const std::string& packageRoot,
            const StableId& projectId, std::string& error);
        bool Play(ImpactSurfaceType surface, const XMFLOAT3& position,
            const wi::audio::SoundInstance3D& listener, StableId& playedAssetId);
        void Update(float dt, const wi::audio::SoundInstance3D& listener);
        void SetPaused(bool paused);
        void StopVoices();
        void Reset();
        std::size_t VoiceCount() const { return voices_.size(); }
        std::size_t ClipCount() const;
    private:
        struct Clip { StableId id; wi::audio::Sound sound; };
        struct Voice { wi::audio::SoundInstance instance; XMFLOAT3 position; float age = 0; };
        std::array<std::vector<Clip>, 9> clips_;
        std::array<int, 9> previous_{{-1,-1,-1,-1,-1,-1,-1,-1,-1}};
        std::vector<Voice> voices_;
        std::mt19937 random_{std::random_device{}()};
        bool paused_ = false;
    };
}
