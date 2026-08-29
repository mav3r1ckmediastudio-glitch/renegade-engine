#pragma once

#include <functional>

#include <WickedEngine.h>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    inline constexpr int RenderSettingsSchemaVersion = 1;
    inline constexpr const char* RenderSettingsCarrierName =
        "__renegade_internal_render_settings";

    enum class RenderTonemap
    {
        ACES = 0,
        Reinhard = 1,
        Uchimura = 2,
    };

    enum class AntiAliasingMode
    {
        Off = 0,
        FXAA = 1,
        TAA = 2,
        MSAA2X = 3,
        MSAA4X = 4,
        MSAA8X = 5,
    };

    struct RenderSettingsState
    {
        int schemaVersion = RenderSettingsSchemaVersion;
        RenderTonemap tonemap = RenderTonemap::ACES;

        float exposure = 1.0f;
        float brightness = 0.0f;
        float contrast = 1.0f;
        float saturation = 1.0f;
        float hdrCalibration = 1.0f;
        bool colorGradingEnabled = true;

        bool bloomEnabled = true;
        float bloomThreshold = 1.0f;
        bool eyeAdaptationEnabled = false;
        float eyeAdaptationKey = 0.115f;
        float eyeAdaptationRate = 1.0f;

        AntiAliasingMode antiAliasing = AntiAliasingMode::Off;

        bool depthOfFieldEnabled = true;
        float depthOfFieldStrength = 10.0f;
        bool motionBlurEnabled = false;
        float motionBlurStrength = 100.0f;
        bool sharpenEnabled = false;
        float sharpenAmount = 0.28f;
        bool chromaticAberrationEnabled = false;
        float chromaticAberrationAmount = 2.0f;
        bool ditherEnabled = true;
    };

    using RenderSettingsApplyCallback =
        std::function<void(const RenderSettingsState&)>;

    [[nodiscard]] RenderSettingsState DefaultRenderSettings() noexcept;
    [[nodiscard]] RenderSettingsState SanitizeRenderSettings(
        const RenderSettingsState& state) noexcept;
    [[nodiscard]] bool HasRenderSettingsChange(
        const RenderSettingsState& before,
        const RenderSettingsState& after) noexcept;

    [[nodiscard]] wi::ecs::Entity FindRenderSettingsCarrier(
        const wi::scene::Scene& scene) noexcept;
    [[nodiscard]] RenderSettingsState CaptureRenderSettings(
        const wi::scene::Scene& scene) noexcept;
    [[nodiscard]] bool WriteRenderSettings(
        wi::scene::Scene& scene,
        const RenderSettingsState& state);
    void RemoveRenderSettings(wi::scene::Scene& scene) noexcept;

    // Applies the persisted creator state to Wicked's native RenderPath3D.
    // Interactive Studio calls should allow buffer resize so MSAA changes take
    // effect immediately. Runtime startup can pass false before Load(), where
    // the selected sample count is consumed by normal render-path creation.
    void ApplyRenderSettingsToPath(
        wi::RenderPath3D& path,
        const RenderSettingsState& state,
        bool resizeBuffersForMSAA);

    class SetRenderSettingsCommand final : public ICommand
    {
    public:
        SetRenderSettingsCommand(
            wi::scene::Scene& scene,
            const RenderSettingsState& after,
            RenderSettingsApplyCallback apply = {});
        SetRenderSettingsCommand(
            wi::scene::Scene& scene,
            const RenderSettingsState& before,
            const RenderSettingsState& after,
            bool hadCarrierBefore,
            RenderSettingsApplyCallback apply = {});

        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const RenderSettingsState& state);

        wi::scene::Scene* scene_ = nullptr;
        RenderSettingsState before_;
        RenderSettingsState after_;
        bool hadCarrierBefore_ = false;
        RenderSettingsApplyCallback apply_;
    };
}
