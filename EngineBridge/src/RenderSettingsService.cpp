#include "renegade/bridge/RenderSettingsService.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
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

    renegade::bridge::RenderAmbientOcclusion SanitizeAmbientOcclusion(
        const renegade::bridge::RenderAmbientOcclusion value) noexcept
    {
        using Mode = renegade::bridge::RenderAmbientOcclusion;
        switch (value)
        {
        case Mode::Off:
        case Mode::SSAO:
        case Mode::HBAO:
        case Mode::MSAO:
        case Mode::RTAO:
            return value;
        default:
            return Mode::Off;
        }
    }

    renegade::bridge::RenderQuality SanitizeRenderQuality(
        const renegade::bridge::RenderQuality value) noexcept
    {
        using Quality = renegade::bridge::RenderQuality;
        switch (value)
        {
        case Quality::Low:
        case Quality::Medium:
        case Quality::High:
            return value;
        default:
            return Quality::Medium;
        }
    }

    std::uint32_t SanitizePlanarMsaaSampleCount(
        const std::uint32_t value) noexcept
    {
        switch (value)
        {
        case 1:
        case 2:
        case 4:
        case 8:
            return value;
        default:
            return 4;
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
    constexpr const char* KeyAmbientOcclusion = "renegade.render.ao.mode";
    constexpr const char* KeyAmbientOcclusionPower = "renegade.render.ao.power";
    constexpr const char* KeyAmbientOcclusionRange = "renegade.render.ao.range";
    constexpr const char* KeyAmbientOcclusionSamples = "renegade.render.ao.samples";
    constexpr const char* KeySsgiEnabled = "renegade.render.ssgi.enabled";
    constexpr const char* KeySsgiDepthRejection = "renegade.render.ssgi.depth_rejection";
    constexpr const char* KeyGiBoost = "renegade.render.gi_boost";
    constexpr const char* KeyPlanarReflectionsEnabled = "renegade.render.planar_reflections.enabled";
    constexpr const char* KeyPlanarReflectionResolutionScale = "renegade.render.planar_reflections.resolution_scale";
    constexpr const char* KeyPlanarReflectionMsaa = "renegade.render.planar_reflections.msaa";
    constexpr const char* KeySsrEnabled = "renegade.render.ssr.enabled";
    constexpr const char* KeySsrQuality = "renegade.render.ssr.quality";
    constexpr const char* KeyReflectionRoughnessCutoff = "renegade.render.reflections.roughness_cutoff";
    constexpr const char* KeyRaytracedShadowsEnabled = "renegade.render.rt.shadows.enabled";
    constexpr const char* KeyRaytracedReflectionsEnabled = "renegade.render.rt.reflections.enabled";
    constexpr const char* KeyRaytracedReflectionsRange = "renegade.render.rt.reflections.range";
    constexpr const char* KeyRaytracedReflectionsQuality = "renegade.render.rt.reflections.quality";
    constexpr const char* KeyRaytracedDiffuseEnabled = "renegade.render.rt.diffuse.enabled";
    constexpr const char* KeyRaytracedDiffuseRange = "renegade.render.rt.diffuse.range";
    constexpr const char* KeyRaytracedDiffuseQuality = "renegade.render.rt.diffuse.quality";
    constexpr const char* KeySurfelGiEnabled = "renegade.render.rt.surfel_gi.enabled";
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
        result.ambientOcclusion = SanitizeAmbientOcclusion(result.ambientOcclusion);
        result.ssrQuality = SanitizeRenderQuality(result.ssrQuality);
        result.raytracedReflectionsQuality =
            SanitizeRenderQuality(result.raytracedReflectionsQuality);
        result.raytracedDiffuseQuality =
            SanitizeRenderQuality(result.raytracedDiffuseQuality);

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
        result.ambientOcclusionPower = ClampFinite(result.ambientOcclusionPower, defaults.ambientOcclusionPower, 0.25f, 8.0f);
        result.ambientOcclusionRange = ClampFinite(result.ambientOcclusionRange, defaults.ambientOcclusionRange, 1.0f, 100.0f);
        result.ambientOcclusionSampleCount = std::clamp(result.ambientOcclusionSampleCount, 1u, 16u);
        result.ssgiDepthRejection = ClampFinite(result.ssgiDepthRejection, defaults.ssgiDepthRejection, 0.1f, 100.0f);
        result.giBoost = ClampFinite(result.giBoost, defaults.giBoost, 1.0f, 10.0f);
        result.planarReflectionResolutionScale = ClampFinite(result.planarReflectionResolutionScale, defaults.planarReflectionResolutionScale, 0.25f, 2.0f);
        result.planarReflectionMsaaSampleCount = SanitizePlanarMsaaSampleCount(result.planarReflectionMsaaSampleCount);
        result.reflectionRoughnessCutoff = ClampFinite(result.reflectionRoughnessCutoff, defaults.reflectionRoughnessCutoff, 0.0f, 1.0f);
        result.raytracedReflectionsRange = ClampFinite(
            result.raytracedReflectionsRange, defaults.raytracedReflectionsRange, 1.0f, 10000.0f);
        result.raytracedDiffuseRange = ClampFinite(
            result.raytracedDiffuseRange, defaults.raytracedDiffuseRange, 1.0f, 100.0f);
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
            left.ambientOcclusion != right.ambientOcclusion ||
            !NearlyEqual(left.ambientOcclusionPower, right.ambientOcclusionPower) ||
            !NearlyEqual(left.ambientOcclusionRange, right.ambientOcclusionRange) ||
            left.ambientOcclusionSampleCount != right.ambientOcclusionSampleCount ||
            left.ssgiEnabled != right.ssgiEnabled ||
            !NearlyEqual(left.ssgiDepthRejection, right.ssgiDepthRejection) ||
            !NearlyEqual(left.giBoost, right.giBoost) ||
            left.planarReflectionsEnabled != right.planarReflectionsEnabled ||
            !NearlyEqual(left.planarReflectionResolutionScale, right.planarReflectionResolutionScale) ||
            left.planarReflectionMsaaSampleCount != right.planarReflectionMsaaSampleCount ||
            left.ssrEnabled != right.ssrEnabled ||
            left.ssrQuality != right.ssrQuality ||
            !NearlyEqual(left.reflectionRoughnessCutoff, right.reflectionRoughnessCutoff) ||
            left.raytracedShadowsEnabled != right.raytracedShadowsEnabled ||
            left.raytracedReflectionsEnabled != right.raytracedReflectionsEnabled ||
            !NearlyEqual(left.raytracedReflectionsRange, right.raytracedReflectionsRange) ||
            left.raytracedReflectionsQuality != right.raytracedReflectionsQuality ||
            left.raytracedDiffuseEnabled != right.raytracedDiffuseEnabled ||
            !NearlyEqual(left.raytracedDiffuseRange, right.raytracedDiffuseRange) ||
            left.raytracedDiffuseQuality != right.raytracedDiffuseQuality ||
            left.surfelGiEnabled != right.surfelGiEnabled ||
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

    bool IsHardwareRayTracingAvailable() noexcept
    {
        const auto* device = wi::graphics::GetDevice();
        return device != nullptr && device->CheckCapability(
            wi::graphics::GraphicsDeviceCapability::RAYTRACING);
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
        if (schema < 1 || schema > RenderSettingsSchemaVersion)
            return defaults;

        RenderSettingsState state = defaults;
        state.schemaVersion = RenderSettingsSchemaVersion;
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
        if (schema >= 2)
        {
            state.ambientOcclusion = static_cast<RenderAmbientOcclusion>(
                ReadValue(metadata->int_values, KeyAmbientOcclusion, static_cast<int>(defaults.ambientOcclusion)));
            state.ambientOcclusionPower = ReadValue(metadata->float_values, KeyAmbientOcclusionPower, defaults.ambientOcclusionPower);
            state.ambientOcclusionRange = ReadValue(metadata->float_values, KeyAmbientOcclusionRange, defaults.ambientOcclusionRange);
            state.ambientOcclusionSampleCount = static_cast<std::uint32_t>(
                ReadValue(metadata->int_values, KeyAmbientOcclusionSamples, static_cast<int>(defaults.ambientOcclusionSampleCount)));
            state.ssgiEnabled = ReadValue(metadata->bool_values, KeySsgiEnabled, defaults.ssgiEnabled);
            state.ssgiDepthRejection = ReadValue(metadata->float_values, KeySsgiDepthRejection, defaults.ssgiDepthRejection);
            state.giBoost = ReadValue(metadata->float_values, KeyGiBoost, defaults.giBoost);
            state.planarReflectionsEnabled = ReadValue(metadata->bool_values, KeyPlanarReflectionsEnabled, defaults.planarReflectionsEnabled);
            state.planarReflectionResolutionScale = ReadValue(metadata->float_values, KeyPlanarReflectionResolutionScale, defaults.planarReflectionResolutionScale);
            state.planarReflectionMsaaSampleCount = static_cast<std::uint32_t>(
                ReadValue(metadata->int_values, KeyPlanarReflectionMsaa, static_cast<int>(defaults.planarReflectionMsaaSampleCount)));
            state.ssrEnabled = ReadValue(metadata->bool_values, KeySsrEnabled, defaults.ssrEnabled);
            state.ssrQuality = static_cast<RenderQuality>(
                ReadValue(metadata->int_values, KeySsrQuality, static_cast<int>(defaults.ssrQuality)));
            state.reflectionRoughnessCutoff = ReadValue(metadata->float_values, KeyReflectionRoughnessCutoff, defaults.reflectionRoughnessCutoff);
        }
        if (schema >= 3)
        {
            state.raytracedShadowsEnabled = ReadValue(
                metadata->bool_values, KeyRaytracedShadowsEnabled, defaults.raytracedShadowsEnabled);
            state.raytracedReflectionsEnabled = ReadValue(
                metadata->bool_values, KeyRaytracedReflectionsEnabled, defaults.raytracedReflectionsEnabled);
            state.raytracedReflectionsRange = ReadValue(
                metadata->float_values, KeyRaytracedReflectionsRange, defaults.raytracedReflectionsRange);
            state.raytracedReflectionsQuality = static_cast<RenderQuality>(ReadValue(
                metadata->int_values, KeyRaytracedReflectionsQuality,
                static_cast<int>(defaults.raytracedReflectionsQuality)));
            state.raytracedDiffuseEnabled = ReadValue(
                metadata->bool_values, KeyRaytracedDiffuseEnabled, defaults.raytracedDiffuseEnabled);
            state.raytracedDiffuseRange = ReadValue(
                metadata->float_values, KeyRaytracedDiffuseRange, defaults.raytracedDiffuseRange);
            state.raytracedDiffuseQuality = static_cast<RenderQuality>(ReadValue(
                metadata->int_values, KeyRaytracedDiffuseQuality,
                static_cast<int>(defaults.raytracedDiffuseQuality)));
            state.surfelGiEnabled = ReadValue(
                metadata->bool_values, KeySurfelGiEnabled, defaults.surfelGiEnabled);
        }
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

        const bool hardwareRayTracing = IsHardwareRayTracingAvailable();
        wi::RenderPath3D::AO ambientOcclusion = wi::RenderPath3D::AO_DISABLED;
        switch (safe.ambientOcclusion)
        {
        case RenderAmbientOcclusion::SSAO:
            ambientOcclusion = wi::RenderPath3D::AO_SSAO;
            break;
        case RenderAmbientOcclusion::HBAO:
            ambientOcclusion = wi::RenderPath3D::AO_HBAO;
            break;
        case RenderAmbientOcclusion::MSAO:
            ambientOcclusion = wi::RenderPath3D::AO_MSAO;
            break;
        case RenderAmbientOcclusion::RTAO:
            if (hardwareRayTracing)
                ambientOcclusion = wi::RenderPath3D::AO_RTAO;
            break;
        case RenderAmbientOcclusion::Off:
        default:
            break;
        }

        wi::renderer::PostProcessQuality ssrQuality = wi::renderer::PostProcessQuality::Medium;
        switch (safe.ssrQuality)
        {
        case RenderQuality::Low:
            ssrQuality = wi::renderer::PostProcessQuality::Low;
            break;
        case RenderQuality::High:
            ssrQuality = wi::renderer::PostProcessQuality::High;
            break;
        case RenderQuality::Medium:
        default:
            break;
        }

        wi::renderer::PostProcessQuality rtReflectionsQuality = wi::renderer::PostProcessQuality::Medium;
        switch (safe.raytracedReflectionsQuality)
        {
        case RenderQuality::Low: rtReflectionsQuality = wi::renderer::PostProcessQuality::Low; break;
        case RenderQuality::High: rtReflectionsQuality = wi::renderer::PostProcessQuality::High; break;
        case RenderQuality::Medium: default: break;
        }
        wi::renderer::PostProcessQuality rtDiffuseQuality = wi::renderer::PostProcessQuality::Medium;
        switch (safe.raytracedDiffuseQuality)
        {
        case RenderQuality::Low: rtDiffuseQuality = wi::renderer::PostProcessQuality::Low; break;
        case RenderQuality::High: rtDiffuseQuality = wi::renderer::PostProcessQuality::High; break;
        case RenderQuality::Medium: default: break;
        }
        const bool expectedRtShadows = safe.raytracedShadowsEnabled && hardwareRayTracing;
        const bool expectedRtReflections = safe.raytracedReflectionsEnabled && hardwareRayTracing;
        const bool expectedRtDiffuse = safe.raytracedDiffuseEnabled && hardwareRayTracing;

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
            path.getAO() != ambientOcclusion ||
            !NearlyEqual(path.getAOPower(), safe.ambientOcclusionPower) ||
            !NearlyEqual(path.getAORange(), safe.ambientOcclusionRange) ||
            path.getAOSampleCount() != safe.ambientOcclusionSampleCount ||
            path.getSSGIEnabled() != safe.ssgiEnabled ||
            !NearlyEqual(path.getSSGIDepthRejection(), safe.ssgiDepthRejection) ||
            !NearlyEqual(wi::renderer::GetGIBoost(), safe.giBoost) ||
            path.getReflectionsEnabled() != safe.planarReflectionsEnabled ||
            !NearlyEqual(path.getPlanarReflectionResolutionScale(), safe.planarReflectionResolutionScale) ||
            path.getPlanarReflectionMSAASampleCount() != safe.planarReflectionMsaaSampleCount ||
            path.getSSREnabled() != safe.ssrEnabled ||
            path.getSSRQuality() != ssrQuality ||
            !NearlyEqual(path.getReflectionRoughnessCutoff(), safe.reflectionRoughnessCutoff) ||
            wi::renderer::GetRaytracedShadowsEnabled() != expectedRtShadows ||
            path.getRaytracedReflectionEnabled() != expectedRtReflections ||
            !NearlyEqual(path.getRaytracedReflectionsRange(), safe.raytracedReflectionsRange) ||
            path.getRaytracedReflectionsQuality() != rtReflectionsQuality ||
            path.getRaytracedDiffuseEnabled() != expectedRtDiffuse ||
            !NearlyEqual(path.getRaytracedDiffuseRange(), safe.raytracedDiffuseRange) ||
            path.getRaytracedDiffuseQuality() != rtDiffuseQuality ||
            wi::renderer::GetSurfelGIEnabled() != safe.surfelGiEnabled ||
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
        metadata->int_values.set(KeyAmbientOcclusion, static_cast<int>(safe.ambientOcclusion));
        metadata->float_values.set(KeyAmbientOcclusionPower, safe.ambientOcclusionPower);
        metadata->float_values.set(KeyAmbientOcclusionRange, safe.ambientOcclusionRange);
        metadata->int_values.set(KeyAmbientOcclusionSamples, static_cast<int>(safe.ambientOcclusionSampleCount));
        metadata->bool_values.set(KeySsgiEnabled, safe.ssgiEnabled);
        metadata->float_values.set(KeySsgiDepthRejection, safe.ssgiDepthRejection);
        metadata->float_values.set(KeyGiBoost, safe.giBoost);
        metadata->bool_values.set(KeyPlanarReflectionsEnabled, safe.planarReflectionsEnabled);
        metadata->float_values.set(KeyPlanarReflectionResolutionScale, safe.planarReflectionResolutionScale);
        metadata->int_values.set(KeyPlanarReflectionMsaa, static_cast<int>(safe.planarReflectionMsaaSampleCount));
        metadata->bool_values.set(KeySsrEnabled, safe.ssrEnabled);
        metadata->int_values.set(KeySsrQuality, static_cast<int>(safe.ssrQuality));
        metadata->float_values.set(KeyReflectionRoughnessCutoff, safe.reflectionRoughnessCutoff);
        metadata->bool_values.set(KeyRaytracedShadowsEnabled, safe.raytracedShadowsEnabled);
        metadata->bool_values.set(KeyRaytracedReflectionsEnabled, safe.raytracedReflectionsEnabled);
        metadata->float_values.set(KeyRaytracedReflectionsRange, safe.raytracedReflectionsRange);
        metadata->int_values.set(KeyRaytracedReflectionsQuality, static_cast<int>(safe.raytracedReflectionsQuality));
        metadata->bool_values.set(KeyRaytracedDiffuseEnabled, safe.raytracedDiffuseEnabled);
        metadata->float_values.set(KeyRaytracedDiffuseRange, safe.raytracedDiffuseRange);
        metadata->int_values.set(KeyRaytracedDiffuseQuality, static_cast<int>(safe.raytracedDiffuseQuality));
        metadata->bool_values.set(KeySurfelGiEnabled, safe.surfelGiEnabled);
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
        if (path.getBloomEnabled() != safe.bloomEnabled)
            path.setBloomEnabled(safe.bloomEnabled);
        path.setBloomThreshold(safe.bloomThreshold);
        if (path.getEyeAdaptionEnabled() != safe.eyeAdaptationEnabled)
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

        path.setAOPower(safe.ambientOcclusionPower);
        path.setAORange(safe.ambientOcclusionRange);
        path.setAOSampleCount(safe.ambientOcclusionSampleCount);
        const bool hardwareRayTracing = IsHardwareRayTracingAvailable();
        wi::RenderPath3D::AO ambientOcclusion = wi::RenderPath3D::AO_DISABLED;
        switch (safe.ambientOcclusion)
        {
        case RenderAmbientOcclusion::SSAO:
            ambientOcclusion = wi::RenderPath3D::AO_SSAO;
            break;
        case RenderAmbientOcclusion::HBAO:
            ambientOcclusion = wi::RenderPath3D::AO_HBAO;
            break;
        case RenderAmbientOcclusion::MSAO:
            ambientOcclusion = wi::RenderPath3D::AO_MSAO;
            break;
        case RenderAmbientOcclusion::RTAO:
            if (hardwareRayTracing)
                ambientOcclusion = wi::RenderPath3D::AO_RTAO;
            break;
        case RenderAmbientOcclusion::Off:
        default:
            break;
        }
        if (path.getAO() != ambientOcclusion)
            path.setAO(ambientOcclusion);

        path.setSSGIDepthRejection(safe.ssgiDepthRejection);
        if (path.getSSGIEnabled() != safe.ssgiEnabled)
            path.setSSGIEnabled(safe.ssgiEnabled);
        wi::renderer::SetGIBoost(safe.giBoost);

        if (path.getReflectionsEnabled() != safe.planarReflectionsEnabled)
            path.setReflectionsEnabled(safe.planarReflectionsEnabled);
        if (!NearlyEqual(path.getPlanarReflectionResolutionScale(), safe.planarReflectionResolutionScale) ||
            path.getPlanarReflectionMSAASampleCount() != safe.planarReflectionMsaaSampleCount)
        {
            path.setPlanarReflectionQuality(
                safe.planarReflectionResolutionScale,
                safe.planarReflectionMsaaSampleCount);
        }

        path.setReflectionRoughnessCutoff(safe.reflectionRoughnessCutoff);
        wi::renderer::PostProcessQuality ssrQuality = wi::renderer::PostProcessQuality::Medium;
        switch (safe.ssrQuality)
        {
        case RenderQuality::Low:
            ssrQuality = wi::renderer::PostProcessQuality::Low;
            break;
        case RenderQuality::High:
            ssrQuality = wi::renderer::PostProcessQuality::High;
            break;
        case RenderQuality::Medium:
        default:
            break;
        }
        if (path.getSSRQuality() != ssrQuality)
        {
            const bool wasEnabled = path.getSSREnabled();
            path.setSSRQuality(ssrQuality);
            if (wasEnabled)
                path.setSSREnabled(true);
        }
        if (path.getSSREnabled() != safe.ssrEnabled)
            path.setSSREnabled(safe.ssrEnabled);

        const bool enableRtShadows = safe.raytracedShadowsEnabled && hardwareRayTracing;
        if (wi::renderer::GetRaytracedShadowsEnabled() != enableRtShadows)
            wi::renderer::SetRaytracedShadowsEnabled(enableRtShadows);

        path.setRaytracedReflectionsRange(safe.raytracedReflectionsRange);
        wi::renderer::PostProcessQuality rtReflectionsQuality = wi::renderer::PostProcessQuality::Medium;
        switch (safe.raytracedReflectionsQuality)
        {
        case RenderQuality::Low: rtReflectionsQuality = wi::renderer::PostProcessQuality::Low; break;
        case RenderQuality::High: rtReflectionsQuality = wi::renderer::PostProcessQuality::High; break;
        case RenderQuality::Medium: default: break;
        }
        if (path.getRaytracedReflectionsQuality() != rtReflectionsQuality)
        {
            const bool wasEnabled = path.getRaytracedReflectionEnabled();
            path.setRaytracedReflectionsQuality(rtReflectionsQuality);
            if (wasEnabled && hardwareRayTracing)
                path.setRaytracedReflectionsEnabled(true);
        }
        const bool enableRtReflections = safe.raytracedReflectionsEnabled && hardwareRayTracing;
        if (path.getRaytracedReflectionEnabled() != enableRtReflections)
            path.setRaytracedReflectionsEnabled(enableRtReflections);

        path.setRaytracedDiffuseRange(safe.raytracedDiffuseRange);
        wi::renderer::PostProcessQuality rtDiffuseQuality = wi::renderer::PostProcessQuality::Medium;
        switch (safe.raytracedDiffuseQuality)
        {
        case RenderQuality::Low: rtDiffuseQuality = wi::renderer::PostProcessQuality::Low; break;
        case RenderQuality::High: rtDiffuseQuality = wi::renderer::PostProcessQuality::High; break;
        case RenderQuality::Medium: default: break;
        }
        if (path.getRaytracedDiffuseQuality() != rtDiffuseQuality)
        {
            const bool wasEnabled = path.getRaytracedDiffuseEnabled();
            path.setRaytracedDiffuseQuality(rtDiffuseQuality);
            if (wasEnabled && hardwareRayTracing)
                path.setRaytracedDiffuseEnabled(true);
        }
        const bool enableRtDiffuse = safe.raytracedDiffuseEnabled && hardwareRayTracing;
        if (path.getRaytracedDiffuseEnabled() != enableRtDiffuse)
            path.setRaytracedDiffuseEnabled(enableRtDiffuse);

        if (wi::renderer::GetSurfelGIEnabled() != safe.surfelGiEnabled)
            wi::renderer::SetSurfelGIEnabled(safe.surfelGiEnabled);

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
