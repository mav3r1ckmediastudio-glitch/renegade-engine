#include "renegade/bridge/RenderSettingsService.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
    constexpr float Epsilon = 0.00001f;

    float ClampFinite(
        const float value,
        const float fallback,
        const float minimum,
        const float maximum) noexcept
    {
        if (!std::isfinite(value))
            return fallback;
        return std::clamp(value, minimum, maximum);
    }

    bool NearlyEqual(const float left, const float right) noexcept
    {
        return std::abs(left - right) <= Epsilon;
    }

    renegade::bridge::RenderTonemap SanitizeTonemap(
        const renegade::bridge::RenderTonemap value) noexcept
    {
        using Tonemap = renegade::bridge::RenderTonemap;
        switch (value)
        {
        case Tonemap::ACES:
        case Tonemap::Reinhard:
        case Tonemap::Uchimura:
            return value;
        default:
            return Tonemap::ACES;
        }
    }

    renegade::bridge::AntiAliasingMode SanitizeAntiAliasing(
        const renegade::bridge::AntiAliasingMode value) noexcept
    {
        using Mode = renegade::bridge::AntiAliasingMode;
        switch (value)
        {
        case Mode::Off:
        case Mode::FXAA:
        case Mode::TAA:
        case Mode::MSAA2X:
        case Mode::MSAA4X:
        case Mode::MSAA8X:
            return value;
        default:
            return Mode::Off;
        }
    }

    template<typename T>
    T ReadValue(
        const wi::scene::MetadataComponent::OrderedNamedValues<T>& values,
        const char* name,
        const T& fallback)
    {
        return values.has(name) ? values.get(name) : fallback;
    }

    constexpr const char* KeySchema = "renegade.render.schema";
    constexpr const char* KeyTonemap = "renegade.render.tonemap";
    constexpr const char* KeyExposure = "renegade.render.exposure";
    constexpr const char* KeyBrightness = "renegade.render.brightness";
    constexpr const char* KeyContrast = "renegade.render.contrast";
    constexpr const char* KeySaturation = "renegade.render.saturation";
    constexpr const char* KeyHDRCalibration = "renegade.render.hdr_calibration";
    constexpr const char* KeyColorGradingEnabled = "renegade.render.color_grading.enabled";
    constexpr const char* KeyBloomEnabled = "renegade.render.bloom.enabled";
    constexpr const char* KeyBloomThreshold = "renegade.render.bloom.threshold";
    constexpr const char* KeyEyeAdaptationEnabled = "renegade.render.eye_adaptation.enabled";
    constexpr const char* KeyEyeAdaptationKey = "renegade.render.eye_adaptation.key";
    constexpr const char* KeyEyeAdaptationRate = "renegade.render.eye_adaptation.rate";
    constexpr const char* KeyAntiAliasing = "renegade.render.anti_aliasing";
    constexpr const char* KeyDepthOfFieldEnabled = "renegade.render.dof.enabled";
    constexpr const char* KeyDepthOfFieldStrength = "renegade.render.dof.strength";
    constexpr const char* KeyMotionBlurEnabled = "renegade.render.motion_blur.enabled";
    constexpr const char* KeyMotionBlurStrength = "renegade.render.motion_blur.strength";
    constexpr const char* KeySharpenEnabled = "renegade.render.sharpen.enabled";
    constexpr const char* KeySharpenAmount = "renegade.render.sharpen.amount";
    constexpr const char* KeyChromaticAberrationEnabled = "renegade.render.chromatic_aberration.enabled";
    constexpr const char* KeyChromaticAberrationAmount = "renegade.render.chromatic_aberration.amount";
    constexpr const char* KeyDitherEnabled = "renegade.render.dither.enabled";
}

namespace renegade::bridge
{
    RenderSettingsState DefaultRenderSettings() noexcept
    {
        return RenderSettingsState{};
    }

    RenderSettingsState SanitizeRenderSettings(
        const RenderSettingsState& state) noexcept
    {
        const auto defaults = DefaultRenderSettings();
        RenderSettingsState result = state;
        result.schemaVersion = RenderSettingsSchemaVersion;
        result.tonemap = SanitizeTonemap(result.tonemap);
        result.antiAliasing = SanitizeAntiAliasing(result.antiAliasing);

        result.exposure = ClampFinite(result.exposure, defaults.exposure, 0.0f, 3.0f);
        result.brightness = ClampFinite(result.brightness, defaults.brightness, -1.0f, 1.0f);
        result.contrast = ClampFinite(result.contrast, defaults.contrast, 0.0f, 2.0f);
        result.saturation = ClampFinite(result.saturation, defaults.saturation, 0.0f, 2.0f);
        result.hdrCalibration = ClampFinite(result.hdrCalibration, defaults.hdrCalibration, 0.0f, 8.0f);
        result.bloomThreshold = ClampFinite(result.bloomThreshold, defaults.bloomThreshold, 0.0f, 10.0f);
        result.eyeAdaptationKey = ClampFinite(result.eyeAdaptationKey, defaults.eyeAdaptationKey, 0.01f, 0.5f);
        result.eyeAdaptationRate = ClampFinite(result.eyeAdaptationRate, defaults.eyeAdaptationRate, 0.01f, 4.0f);
        result.depthOfFieldStrength = ClampFinite(result.depthOfFieldStrength, defaults.depthOfFieldStrength, 1.0f, 20.0f);
        result.motionBlurStrength = ClampFinite(result.motionBlurStrength, defaults.motionBlurStrength, 0.1f, 400.0f);
        result.sharpenAmount = ClampFinite(result.sharpenAmount, defaults.sharpenAmount, 0.0f, 4.0f);
        result.chromaticAberrationAmount = ClampFinite(result.chromaticAberrationAmount, defaults.chromaticAberrationAmount, 0.0f, 40.0f);
        return result;
    }

    bool HasRenderSettingsChange(
        const RenderSettingsState& before,
        const RenderSettingsState& after) noexcept
    {
        const auto left = SanitizeRenderSettings(before);
        const auto right = SanitizeRenderSettings(after);
        return left.tonemap != right.tonemap ||
            !NearlyEqual(left.exposure, right.exposure) ||
            !NearlyEqual(left.brightness, right.brightness) ||
            !NearlyEqual(left.contrast, right.contrast) ||
            !NearlyEqual(left.saturation, right.saturation) ||
            !NearlyEqual(left.hdrCalibration, right.hdrCalibration) ||
            left.colorGradingEnabled != right.colorGradingEnabled ||
            left.bloomEnabled != right.bloomEnabled ||
            !NearlyEqual(left.bloomThreshold, right.bloomThreshold) ||
            left.eyeAdaptationEnabled != right.eyeAdaptationEnabled ||
            !NearlyEqual(left.eyeAdaptationKey, right.eyeAdaptationKey) ||
            !NearlyEqual(left.eyeAdaptationRate, right.eyeAdaptationRate) ||
            left.antiAliasing != right.antiAliasing ||
            left.depthOfFieldEnabled != right.depthOfFieldEnabled ||
            !NearlyEqual(left.depthOfFieldStrength, right.depthOfFieldStrength) ||
            left.motionBlurEnabled != right.motionBlurEnabled ||
            !NearlyEqual(left.motionBlurStrength, right.motionBlurStrength) ||
            left.sharpenEnabled != right.sharpenEnabled ||
            !NearlyEqual(left.sharpenAmount, right.sharpenAmount) ||
            left.chromaticAberrationEnabled != right.chromaticAberrationEnabled ||
            !NearlyEqual(left.chromaticAberrationAmount, right.chromaticAberrationAmount) ||
            left.ditherEnabled != right.ditherEnabled;
    }

    wi::ecs::Entity FindRenderSettingsCarrier(
        const wi::scene::Scene& scene) noexcept
    {
        for (std::size_t index = 0; index < scene.names.GetCount(); ++index)
        {
            if (scene.names[index].name == RenderSettingsCarrierName)
                return scene.names.GetEntity(index);
        }
        return wi::ecs::INVALID_ENTITY;
    }

    RenderSettingsState CaptureRenderSettings(
        const wi::scene::Scene& scene) noexcept
    {
        const auto defaults = DefaultRenderSettings();
        const auto carrier = FindRenderSettingsCarrier(scene);
        if (carrier == wi::ecs::INVALID_ENTITY)
            return defaults;

        const auto* metadata = scene.metadatas.GetComponent(carrier);
        if (metadata == nullptr)
            return defaults;

        const int schema = ReadValue(metadata->int_values, KeySchema, 0);
        if (schema != RenderSettingsSchemaVersion)
            return defaults;

        RenderSettingsState state = defaults;
        state.schemaVersion = schema;
        state.tonemap = static_cast<RenderTonemap>(
            ReadValue(metadata->int_values, KeyTonemap, static_cast<int>(defaults.tonemap)));
        state.exposure = ReadValue(metadata->float_values, KeyExposure, defaults.exposure);
        state.brightness = ReadValue(metadata->float_values, KeyBrightness, defaults.brightness);
        state.contrast = ReadValue(metadata->float_values, KeyContrast, defaults.contrast);
        state.saturation = ReadValue(metadata->float_values, KeySaturation, defaults.saturation);
        state.hdrCalibration = ReadValue(metadata->float_values, KeyHDRCalibration, defaults.hdrCalibration);
        state.colorGradingEnabled = ReadValue(metadata->bool_values, KeyColorGradingEnabled, defaults.colorGradingEnabled);
        state.bloomEnabled = ReadValue(metadata->bool_values, KeyBloomEnabled, defaults.bloomEnabled);
        state.bloomThreshold = ReadValue(metadata->float_values, KeyBloomThreshold, defaults.bloomThreshold);
        state.eyeAdaptationEnabled = ReadValue(metadata->bool_values, KeyEyeAdaptationEnabled, defaults.eyeAdaptationEnabled);
        state.eyeAdaptationKey = ReadValue(metadata->float_values, KeyEyeAdaptationKey, defaults.eyeAdaptationKey);
        state.eyeAdaptationRate = ReadValue(metadata->float_values, KeyEyeAdaptationRate, defaults.eyeAdaptationRate);
        state.antiAliasing = static_cast<AntiAliasingMode>(
            ReadValue(metadata->int_values, KeyAntiAliasing, static_cast<int>(defaults.antiAliasing)));
        state.depthOfFieldEnabled = ReadValue(metadata->bool_values, KeyDepthOfFieldEnabled, defaults.depthOfFieldEnabled);
        state.depthOfFieldStrength = ReadValue(metadata->float_values, KeyDepthOfFieldStrength, defaults.depthOfFieldStrength);
        state.motionBlurEnabled = ReadValue(metadata->bool_values, KeyMotionBlurEnabled, defaults.motionBlurEnabled);
        state.motionBlurStrength = ReadValue(metadata->float_values, KeyMotionBlurStrength, defaults.motionBlurStrength);
        state.sharpenEnabled = ReadValue(metadata->bool_values, KeySharpenEnabled, defaults.sharpenEnabled);
        state.sharpenAmount = ReadValue(metadata->float_values, KeySharpenAmount, defaults.sharpenAmount);
        state.chromaticAberrationEnabled = ReadValue(metadata->bool_values, KeyChromaticAberrationEnabled, defaults.chromaticAberrationEnabled);
        state.chromaticAberrationAmount = ReadValue(metadata->float_values, KeyChromaticAberrationAmount, defaults.chromaticAberrationAmount);
        state.ditherEnabled = ReadValue(metadata->bool_values, KeyDitherEnabled, defaults.ditherEnabled);
        return SanitizeRenderSettings(state);
    }

    bool RenderSettingsMatchPath(
        const wi::RenderPath3D& path,
        const RenderSettingsState& state) noexcept
    {
        const auto safe = SanitizeRenderSettings(state);

        wi::renderer::Tonemap tonemap = wi::renderer::Tonemap::ACES;
        switch (safe.tonemap)
        {
        case RenderTonemap::Reinhard:
            tonemap = wi::renderer::Tonemap::Reinhard;
            break;
        case RenderTonemap::Uchimura:
            tonemap = wi::renderer::Tonemap::Uchimura;
            break;
        case RenderTonemap::ACES:
        default:
            break;
        }

        if (path.getTonemap() != tonemap ||
            !NearlyEqual(path.getExposure(), safe.exposure) ||
            !NearlyEqual(path.getBrightness(), safe.brightness) ||
            !NearlyEqual(path.getContrast(), safe.contrast) ||
            !NearlyEqual(path.getSaturation(), safe.saturation) ||
            !NearlyEqual(path.getHDRCalibration(), safe.hdrCalibration) ||
            path.getColorGradingEnabled() != safe.colorGradingEnabled ||
            path.getBloomEnabled() != safe.bloomEnabled ||
            !NearlyEqual(path.getBloomThreshold(), safe.bloomThreshold) ||
            path.getEyeAdaptionEnabled() != safe.eyeAdaptationEnabled ||
            !NearlyEqual(path.getEyeAdaptionKey(), safe.eyeAdaptationKey) ||
            !NearlyEqual(path.getEyeAdaptionRate(), safe.eyeAdaptationRate) ||
            path.getDepthOfFieldEnabled() != safe.depthOfFieldEnabled ||
            !NearlyEqual(path.getDepthOfFieldStrength(), safe.depthOfFieldStrength) ||
            path.getMotionBlurEnabled() != safe.motionBlurEnabled ||
            !NearlyEqual(path.getMotionBlurStrength(), safe.motionBlurStrength) ||
            path.getSharpenFilterEnabled() !=
                (safe.sharpenEnabled && safe.sharpenAmount > Epsilon) ||
            !NearlyEqual(path.getSharpenFilterAmount(), safe.sharpenAmount) ||
            path.getChromaticAberrationEnabled() != safe.chromaticAberrationEnabled ||
            !NearlyEqual(path.getChromaticAberrationAmount(), safe.chromaticAberrationAmount) ||
            path.getDitherEnabled() != safe.ditherEnabled)
        {
            return false;
        }

        const bool taa = wi::renderer::GetTemporalAAEnabled();
        const bool fxaa = path.getFXAAEnabled();
        const std::uint32_t msaa = path.getMSAASampleCount();
        switch (safe.antiAliasing)
        {
        case AntiAliasingMode::FXAA:
            return fxaa && !taa && msaa == 1;
        case AntiAliasingMode::TAA:
            return !fxaa && taa && msaa == 1;
        case AntiAliasingMode::MSAA2X:
            return !fxaa && !taa && msaa == 2;
        case AntiAliasingMode::MSAA4X:
            return !fxaa && !taa && msaa >= 2 && msaa <= 4;
        case AntiAliasingMode::MSAA8X:
            return !fxaa && !taa && msaa >= 2 && msaa <= 8;
        case AntiAliasingMode::Off:
        default:
            return !fxaa && !taa && msaa == 1;
        }
    }

    bool WriteRenderSettings(
        wi::scene::Scene& scene,
        const RenderSettingsState& state)
    {
        const auto safe = SanitizeRenderSettings(state);
        auto carrier = FindRenderSettingsCarrier(scene);
        if (carrier == wi::ecs::INVALID_ENTITY)
        {
            carrier = wi::ecs::CreateEntity();
            if (carrier == wi::ecs::INVALID_ENTITY)
                return false;
            scene.names.Create(carrier) = RenderSettingsCarrierName;
        }

        auto* metadata = scene.metadatas.GetComponent(carrier);
        if (metadata == nullptr)
            metadata = &scene.metadatas.Create(carrier);

        metadata->int_values.set(KeySchema, RenderSettingsSchemaVersion);
        metadata->int_values.set(KeyTonemap, static_cast<int>(safe.tonemap));
        metadata->float_values.set(KeyExposure, safe.exposure);
        metadata->float_values.set(KeyBrightness, safe.brightness);
        metadata->float_values.set(KeyContrast, safe.contrast);
        metadata->float_values.set(KeySaturation, safe.saturation);
        metadata->float_values.set(KeyHDRCalibration, safe.hdrCalibration);
        metadata->bool_values.set(KeyColorGradingEnabled, safe.colorGradingEnabled);
        metadata->bool_values.set(KeyBloomEnabled, safe.bloomEnabled);
        metadata->float_values.set(KeyBloomThreshold, safe.bloomThreshold);
        metadata->bool_values.set(KeyEyeAdaptationEnabled, safe.eyeAdaptationEnabled);
        metadata->float_values.set(KeyEyeAdaptationKey, safe.eyeAdaptationKey);
        metadata->float_values.set(KeyEyeAdaptationRate, safe.eyeAdaptationRate);
        metadata->int_values.set(KeyAntiAliasing, static_cast<int>(safe.antiAliasing));
        metadata->bool_values.set(KeyDepthOfFieldEnabled, safe.depthOfFieldEnabled);
        metadata->float_values.set(KeyDepthOfFieldStrength, safe.depthOfFieldStrength);
        metadata->bool_values.set(KeyMotionBlurEnabled, safe.motionBlurEnabled);
        metadata->float_values.set(KeyMotionBlurStrength, safe.motionBlurStrength);
        metadata->bool_values.set(KeySharpenEnabled, safe.sharpenEnabled);
        metadata->float_values.set(KeySharpenAmount, safe.sharpenAmount);
        metadata->bool_values.set(KeyChromaticAberrationEnabled, safe.chromaticAberrationEnabled);
        metadata->float_values.set(KeyChromaticAberrationAmount, safe.chromaticAberrationAmount);
        metadata->bool_values.set(KeyDitherEnabled, safe.ditherEnabled);
        return true;
    }

    void RemoveRenderSettings(wi::scene::Scene& scene) noexcept
    {
        const auto carrier = FindRenderSettingsCarrier(scene);
        if (carrier != wi::ecs::INVALID_ENTITY)
            scene.Entity_Remove(carrier);
    }

    void ApplyRenderSettingsToPath(
        wi::RenderPath3D& path,
        const RenderSettingsState& state,
        const bool resizeBuffersForMSAA)
    {
        const auto safe = SanitizeRenderSettings(state);
        switch (safe.tonemap)
        {
        case RenderTonemap::Reinhard:
            path.setTonemap(wi::renderer::Tonemap::Reinhard);
            break;
        case RenderTonemap::Uchimura:
            path.setTonemap(wi::renderer::Tonemap::Uchimura);
            break;
        case RenderTonemap::ACES:
        default:
            path.setTonemap(wi::renderer::Tonemap::ACES);
            break;
        }

        path.setExposure(safe.exposure);
        path.setBrightness(safe.brightness);
        path.setContrast(safe.contrast);
        path.setSaturation(safe.saturation);
        path.setHDRCalibration(safe.hdrCalibration);
        path.setColorGradingEnabled(safe.colorGradingEnabled);
        path.setBloomEnabled(safe.bloomEnabled);
        path.setBloomThreshold(safe.bloomThreshold);
        path.setEyeAdaptionEnabled(safe.eyeAdaptationEnabled);
        path.setEyeAdaptionKey(safe.eyeAdaptationKey);
        path.setEyeAdaptionRate(safe.eyeAdaptationRate);

        path.setFXAAEnabled(false);
        wi::renderer::SetTemporalAAEnabled(false);
        std::uint32_t sampleCount = 1;
        switch (safe.antiAliasing)
        {
        case AntiAliasingMode::FXAA:
            path.setFXAAEnabled(true);
            break;
        case AntiAliasingMode::TAA:
            wi::renderer::SetTemporalAAEnabled(true);
            break;
        case AntiAliasingMode::MSAA2X:
            sampleCount = 2;
            break;
        case AntiAliasingMode::MSAA4X:
            sampleCount = 4;
            break;
        case AntiAliasingMode::MSAA8X:
            sampleCount = 8;
            break;
        case AntiAliasingMode::Off:
        default:
            break;
        }
        if (path.getMSAASampleCount() != sampleCount)
        {
            path.setMSAASampleCount(sampleCount);
            if (resizeBuffersForMSAA)
                path.ResizeBuffers();
        }

        path.setDepthOfFieldEnabled(safe.depthOfFieldEnabled);
        path.setDepthOfFieldStrength(safe.depthOfFieldStrength);
        path.setMotionBlurEnabled(safe.motionBlurEnabled);
        path.setMotionBlurStrength(safe.motionBlurStrength);
        path.setSharpenFilterEnabled(safe.sharpenEnabled);
        path.setSharpenFilterAmount(safe.sharpenAmount);
        path.setChromaticAberrationEnabled(safe.chromaticAberrationEnabled);
        path.setChromaticAberrationAmount(safe.chromaticAberrationAmount);
        path.setDitherEnabled(safe.ditherEnabled);
    }

    SetRenderSettingsCommand::SetRenderSettingsCommand(
        wi::scene::Scene& scene,
        const RenderSettingsState& after,
        RenderSettingsApplyCallback apply)
        : scene_(&scene)
        , before_(CaptureRenderSettings(scene))
        , after_(SanitizeRenderSettings(after))
        , hadCarrierBefore_(
            FindRenderSettingsCarrier(scene) != wi::ecs::INVALID_ENTITY)
        , apply_(std::move(apply))
    {
    }

    SetRenderSettingsCommand::SetRenderSettingsCommand(
        wi::scene::Scene& scene,
        const RenderSettingsState& before,
        const RenderSettingsState& after,
        const bool hadCarrierBefore,
        RenderSettingsApplyCallback apply)
        : scene_(&scene)
        , before_(SanitizeRenderSettings(before))
        , after_(SanitizeRenderSettings(after))
        , hadCarrierBefore_(hadCarrierBefore)
        , apply_(std::move(apply))
    {
    }

    bool SetRenderSettingsCommand::Execute()
    {
        return HasRenderSettingsChange(before_, after_) && Apply(after_);
    }

    void SetRenderSettingsCommand::Undo()
    {
        if (scene_ == nullptr)
            return;

        if (hadCarrierBefore_)
            WriteRenderSettings(*scene_, before_);
        else
            RemoveRenderSettings(*scene_);

        if (apply_)
            apply_(before_);
    }

    bool SetRenderSettingsCommand::Apply(const RenderSettingsState& state)
    {
        if (scene_ == nullptr || !WriteRenderSettings(*scene_, state))
            return false;
        if (apply_)
            apply_(SanitizeRenderSettings(state));
        return true;
    }
}
