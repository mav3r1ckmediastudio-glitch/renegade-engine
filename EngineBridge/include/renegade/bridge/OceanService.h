#pragma once

#include <cstdint>

#include <WickedEngine.h>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    enum class OceanPreset
    {
        Calm,
        Coastal,
        Storm,
        Alien,
    };

    // Complete authored view of Wicked's serialized OceanParameters. The
    // renderer is an infinite camera-relative FFT surface, not a terrain
    // paint tool or bounded lake volume.
    struct OceanState
    {
        bool enabled = false;
        int displacementMapDimension = 512;
        float patchLength = 50.0f;
        float timeScale = 0.3f;
        float waveAmplitude = 1000.0f;
        float windAzimuthDegrees = 36.8699f;
        float windSpeed = 600.0f;
        float windDependency = 0.07f;
        float choppyScale = 1.3f;
        XMFLOAT4 waterColor = XMFLOAT4(
            0.0f,
            2.0f / 255.0f,
            6.0f / 255.0f,
            0.6f);
        XMFLOAT4 extinctionColor = XMFLOAT4(0.0f, 0.9f, 1.0f, 1.0f);
        float waterHeight = 0.0f;
        std::uint32_t surfaceDetail = 4;
        float surfaceDisplacementTolerance = 2.0f;
    };

    [[nodiscard]] OceanState CaptureOcean(
        const wi::scene::WeatherComponent& weather) noexcept;
    void ApplyOcean(
        wi::scene::Scene& scene,
        wi::ecs::Entity entity,
        const OceanState& state) noexcept;
    [[nodiscard]] OceanState MakeOceanPreset(
        const OceanState& current,
        OceanPreset preset) noexcept;

    class SetOceanCommand final : public ICommand
    {
    public:
        SetOceanCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            const OceanState& ocean);
        SetOceanCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            const OceanState& before,
            const OceanState& after);

        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const OceanState& state);

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        OceanState before_;
        OceanState after_;
    };
}
