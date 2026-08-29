from pathlib import Path


def replace_once(path: str, old: str, new: str, label: str) -> None:
    target = Path(path)
    text = target.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected exactly one anchor, found {count}")
    target.write_text(text.replace(old, new, 1), encoding="utf-8", newline="\n")


# EngineBridge: real native-path verification + the exact pinned Wicked Editor domains.
replace_once(
    "EngineBridge/include/renegade/bridge/RenderSettingsService.h",
    """    [[nodiscard]] RenderSettingsState CaptureRenderSettings(
        const wi::scene::Scene& scene) noexcept;
    [[nodiscard]] bool WriteRenderSettings(""",
    """    [[nodiscard]] RenderSettingsState CaptureRenderSettings(
        const wi::scene::Scene& scene) noexcept;
    [[nodiscard]] bool RenderSettingsMatchPath(
        const wi::RenderPath3D& path,
        const RenderSettingsState& state) noexcept;
    [[nodiscard]] bool WriteRenderSettings(""",
    "render header live verifier",
)

render_source = Path("EngineBridge/src/RenderSettingsService.cpp")
text = render_source.read_text(encoding="utf-8")
render_ranges = {
    "result.exposure = ClampFinite(result.exposure, defaults.exposure, 0.0f, 16.0f);":
        "result.exposure = ClampFinite(result.exposure, defaults.exposure, 0.0f, 3.0f);",
    "result.brightness = ClampFinite(result.brightness, defaults.brightness, -2.0f, 2.0f);":
        "result.brightness = ClampFinite(result.brightness, defaults.brightness, -1.0f, 1.0f);",
    "result.contrast = ClampFinite(result.contrast, defaults.contrast, 0.0f, 4.0f);":
        "result.contrast = ClampFinite(result.contrast, defaults.contrast, 0.0f, 2.0f);",
    "result.saturation = ClampFinite(result.saturation, defaults.saturation, 0.0f, 4.0f);":
        "result.saturation = ClampFinite(result.saturation, defaults.saturation, 0.0f, 2.0f);",
    "result.hdrCalibration = ClampFinite(result.hdrCalibration, defaults.hdrCalibration, 0.01f, 16.0f);":
        "result.hdrCalibration = ClampFinite(result.hdrCalibration, defaults.hdrCalibration, 0.0f, 8.0f);",
    "result.bloomThreshold = ClampFinite(result.bloomThreshold, defaults.bloomThreshold, 0.0f, 64.0f);":
        "result.bloomThreshold = ClampFinite(result.bloomThreshold, defaults.bloomThreshold, 0.0f, 10.0f);",
    "result.eyeAdaptationKey = ClampFinite(result.eyeAdaptationKey, defaults.eyeAdaptationKey, 0.001f, 4.0f);":
        "result.eyeAdaptationKey = ClampFinite(result.eyeAdaptationKey, defaults.eyeAdaptationKey, 0.01f, 0.5f);",
    "result.eyeAdaptationRate = ClampFinite(result.eyeAdaptationRate, defaults.eyeAdaptationRate, 0.0f, 16.0f);":
        "result.eyeAdaptationRate = ClampFinite(result.eyeAdaptationRate, defaults.eyeAdaptationRate, 0.01f, 4.0f);",
    "result.depthOfFieldStrength = ClampFinite(result.depthOfFieldStrength, defaults.depthOfFieldStrength, 0.0f, 100.0f);":
        "result.depthOfFieldStrength = ClampFinite(result.depthOfFieldStrength, defaults.depthOfFieldStrength, 1.0f, 20.0f);",
    "result.motionBlurStrength = ClampFinite(result.motionBlurStrength, defaults.motionBlurStrength, 0.0f, 500.0f);":
        "result.motionBlurStrength = ClampFinite(result.motionBlurStrength, defaults.motionBlurStrength, 0.1f, 400.0f);",
    "result.chromaticAberrationAmount = ClampFinite(result.chromaticAberrationAmount, defaults.chromaticAberrationAmount, 0.0f, 64.0f);":
        "result.chromaticAberrationAmount = ClampFinite(result.chromaticAberrationAmount, defaults.chromaticAberrationAmount, 0.0f, 40.0f);",
}
for old, new in render_ranges.items():
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"render range anchor expected once: {old!r}, found {count}")
    text = text.replace(old, new, 1)
render_source.write_text(text, encoding="utf-8", newline="\n")

replace_once(
    "EngineBridge/src/RenderSettingsService.cpp",
    """    bool WriteRenderSettings(
        wi::scene::Scene& scene,
        const RenderSettingsState& state)
    {""",
    """    bool RenderSettingsMatchPath(
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
    {""",
    "native render-path verifier implementation",
)

# Studio: specific-field anchors prevent one slider from accidentally editing another.
studio = Path("Studio/src/Phase5Gate5RenderWorkspace.cpp")
text = studio.read_text(encoding="utf-8")
studio_replacements = {
    """            "Native scene exposure.", RenderField::Exposure,
            0.0f, 16.0f, 1600.0f);""":
        """            "Native scene exposure.", RenderField::Exposure,
            0.0f, 3.0f, 10000.0f);""",
    """            "Native post-process brightness offset.", RenderField::Brightness,
            -2.0f, 2.0f, 800.0f);""":
        """            "Native post-process brightness offset.", RenderField::Brightness,
            -1.0f, 1.0f, 10000.0f);""",
    """            "Native post-process contrast.", RenderField::Contrast,
            0.0f, 4.0f, 800.0f);""":
        """            "Native post-process contrast.", RenderField::Contrast,
            0.0f, 2.0f, 10000.0f);""",
    """            "Native post-process colour saturation.", RenderField::Saturation,
            0.0f, 4.0f, 800.0f);""":
        """            "Native post-process colour saturation.", RenderField::Saturation,
            0.0f, 2.0f, 10000.0f);""",
    """            "Wicked render-path HDR calibration value. This does not switch OS HDR output mode.",
            RenderField::HdrCalibration, 0.01f, 16.0f, 1600.0f);""":
        """            "HDR output calibration. This is intentionally subtle/inert while the viewport is running in SDR.",
            RenderField::HdrCalibration, 0.0f, 8.0f, 100.0f);""",
    """            "Brightness threshold for native bloom response.",
            RenderField::BloomThreshold, 0.0f, 64.0f, 1280.0f);""":
        """            "Brightness threshold for native bloom response.",
            RenderField::BloomThreshold, 0.0f, 10.0f, 1000.0f);""",
    """            "Enable Wicked eye adaptation / automatic exposure.",
            RenderToggle::EyeAdaptation);""":
        """            "Enable Wicked eye adaptation. The visible response happens over time while scene luminance changes.",
            RenderToggle::EyeAdaptation);""",
    """            "Native luminance key used by Wicked eye adaptation.",
            RenderField::EyeAdaptationKey, 0.001f, 4.0f, 1000.0f);""":
        """            "Native luminance key used by Wicked eye adaptation.",
            RenderField::EyeAdaptationKey, 0.01f, 0.5f, 10000.0f);""",
    """            "How quickly eye adaptation responds to luminance changes.",
            RenderField::EyeAdaptationRate, 0.0f, 16.0f, 1600.0f);""":
        """            "How quickly eye adaptation responds to luminance changes.",
            RenderField::EyeAdaptationRate, 0.01f, 4.0f, 10000.0f);""",
    """            "Run Wicked depth of field. Focal distance/aperture remain authored on scene cameras.",
            RenderToggle::DepthOfField);""":
        """            "Run Wicked depth of field. It requires the active view camera aperture to be greater than zero; focal distance/aperture are authored on scene cameras.",
            RenderToggle::DepthOfField);""",
    """            "Native depth-of-field strength.", RenderField::DepthOfFieldStrength,
            0.0f, 100.0f, 1000.0f);""":
        """            "Native depth-of-field strength.", RenderField::DepthOfFieldStrength,
            1.0f, 20.0f, 1000.0f);""",
    """            "Run Wicked's native motion-blur pass.", RenderToggle::MotionBlur);""":
        """            "Run Wicked's native motion-blur pass. The effect is visible while the camera or scene is moving.", RenderToggle::MotionBlur);""",
    """            "Native motion-blur strength.", RenderField::MotionBlurStrength,
            0.0f, 500.0f, 1000.0f);""":
        """            "Native motion-blur strength.", RenderField::MotionBlurStrength,
            0.1f, 400.0f, 10000.0f);""",
    """            "Native chromatic-aberration amount.",
            RenderField::ChromaticAberrationAmount, 0.0f, 64.0f, 1280.0f);""":
        """            "Native chromatic-aberration amount.",
            RenderField::ChromaticAberrationAmount, 0.0f, 40.0f, 1000.0f);""",
    """            "Enable Wicked's final-image dithering.", RenderToggle::Dither);""":
        """            "Enable Wicked's subtle final-image dithering for banding reduction.", RenderToggle::Dither);""",
}
for old, new in studio_replacements.items():
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"Studio field anchor expected once: {old!r}, found {count}")
    text = text.replace(old, new, 1)
studio.write_text(text, encoding="utf-8", newline="\n")

replace_once(
    "Studio/src/Phase5Gate5RenderWorkspace.cpp",
    """        const auto authored = bridge::CaptureRenderSettings(scene);
        if (appliedRenderSettingsInitialized_ &&
            !bridge::HasRenderSettingsChange(appliedRenderSettings_, authored))
        {
            return;
        }
        ApplyRenderSettingsState(authored, resizeBuffersForMSAA);""",
    """        const auto authored = bridge::CaptureRenderSettings(scene);
        if (bridge::RenderSettingsMatchPath(*this, authored))
        {
            appliedRenderSettings_ = authored;
            appliedRenderSettingsInitialized_ = true;
            return;
        }
        ApplyRenderSettingsState(authored, resizeBuffersForMSAA);""",
    "Studio live native sync",
)

replace_once(
    "Studio/src/Phase5Gate5RenderWorkspace.cpp",
    """        bridge::ApplyRenderSettingsToPath(*this, safe, resizeBuffersForMSAA);
        appliedRenderSettings_ = safe;
        appliedRenderSettingsInitialized_ = true;""",
    """        bridge::ApplyRenderSettingsToPath(*this, safe, resizeBuffersForMSAA);
        appliedRenderSettings_ = safe;
        appliedRenderSettingsInitialized_ = true;
        if (!bridge::RenderSettingsMatchPath(*this, safe))
        {
            wi::backlog::post(
                "Renegade Gate 5: native RenderPath did not retain the authored image-quality state.",
                wi::backlog::LogLevel::Warning);
        }""",
    "Studio post-apply native verifier",
)

replace_once(
    "Runtime/src/RuntimeApplication.cpp",
    """        const auto authored =
            bridge::CaptureRenderSettings(activeScene);
        if (renderSettingsInitialized_ &&
            !bridge::HasRenderSettingsChange(renderSettings_, authored))
        {
            return;
        }

        bridge::ApplyRenderSettingsToPath(
            *this,
            authored,
            resizeBuffersForMSAA);
        renderSettings_ = authored;
        renderSettingsInitialized_ = true;""",
    """        const auto authored =
            bridge::CaptureRenderSettings(activeScene);
        if (bridge::RenderSettingsMatchPath(*this, authored))
        {
            renderSettings_ = authored;
            renderSettingsInitialized_ = true;
            return;
        }

        bridge::ApplyRenderSettingsToPath(
            *this,
            authored,
            resizeBuffersForMSAA);
        renderSettings_ = authored;
        renderSettingsInitialized_ = true;""",
    "Runtime live native sync",
)

# Functional regression test: persistence is no longer enough; verify every field on RenderPath3D.
test = Path("Tests/Phase5Gate5RenderSettingsTests.cpp")
text = test.read_text(encoding="utf-8")
if text.count("#include <cmath>") != 1:
    raise SystemExit("settings test include anchor changed")
text = text.replace("#include <cmath>", "#include <array>\n#include <cmath>", 1)
replace_old = """    malformed.hdrCalibration = 0.0f;
    malformed.chromaticAberrationAmount = 1000.0f;"""
replace_new = """    malformed.hdrCalibration = -100.0f;
    malformed.saturation = 100.0f;
    malformed.bloomThreshold = 1000.0f;
    malformed.eyeAdaptationKey = 100.0f;
    malformed.eyeAdaptationRate = 100.0f;
    malformed.depthOfFieldStrength = 1000.0f;
    malformed.motionBlurStrength = 1000.0f;
    malformed.sharpenAmount = 100.0f;
    malformed.chromaticAberrationAmount = 1000.0f;"""
if text.count(replace_old) != 1:
    raise SystemExit("settings malformed-range seed anchor changed")
text = text.replace(replace_old, replace_new, 1)
old = """        !NearlyEqual(safe.brightness, -2.0f) ||
        !NearlyEqual(safe.contrast, 4.0f) ||
        !NearlyEqual(safe.hdrCalibration, 0.01f) ||
        !NearlyEqual(safe.chromaticAberrationAmount, 64.0f))"""
new = """        !NearlyEqual(safe.brightness, -1.0f) ||
        !NearlyEqual(safe.contrast, 2.0f) ||
        !NearlyEqual(safe.saturation, 2.0f) ||
        !NearlyEqual(safe.hdrCalibration, 0.0f) ||
        !NearlyEqual(safe.bloomThreshold, 10.0f) ||
        !NearlyEqual(safe.eyeAdaptationKey, 0.5f) ||
        !NearlyEqual(safe.eyeAdaptationRate, 4.0f) ||
        !NearlyEqual(safe.depthOfFieldStrength, 20.0f) ||
        !NearlyEqual(safe.motionBlurStrength, 400.0f) ||
        !NearlyEqual(safe.sharpenAmount, 4.0f) ||
        !NearlyEqual(safe.chromaticAberrationAmount, 40.0f))"""
if text.count(old) != 1:
    raise SystemExit("settings sanitize expectation anchor changed")
text = text.replace(old, new, 1)
anchor = """    authored.chromaticAberrationAmount = 3.5f;
    authored.ditherEnabled = false;

    if (!WriteRenderSettings(scenes.GetScene(), authored))"""
insert = """    authored.chromaticAberrationAmount = 3.5f;
    authored.ditherEnabled = false;

    wi::RenderPath3D nativePath;
    ApplyRenderSettingsToPath(nativePath, authored, false);
    if (!RenderSettingsMatchPath(nativePath, authored))
        return Fail("Gate 5 authored state did not reach the native Wicked RenderPath");

    constexpr std::array<AntiAliasingMode, 6> aaModes = {
        AntiAliasingMode::Off,
        AntiAliasingMode::FXAA,
        AntiAliasingMode::TAA,
        AntiAliasingMode::MSAA2X,
        AntiAliasingMode::MSAA4X,
        AntiAliasingMode::MSAA8X,
    };
    for (const auto mode : aaModes)
    {
        auto aaState = authored;
        aaState.antiAliasing = mode;
        ApplyRenderSettingsToPath(nativePath, aaState, false);
        if (!RenderSettingsMatchPath(nativePath, aaState))
            return Fail("Gate 5 AA mode did not reach the native Wicked RenderPath");
    }

    if (!WriteRenderSettings(scenes.GetScene(), authored))"""
if text.count(anchor) != 1:
    raise SystemExit("settings native apply insertion anchor changed")
text = text.replace(anchor, insert, 1)
test.write_text(text, encoding="utf-8", newline="\n")

# Source contract: require functional native verification and reject the former dead ranges.
contract = Path("Tests/Phase5Gate5SourceContract.cmake")
text = contract.read_text(encoding="utf-8")
old = """    "colorGradingEnabled"
    "SetRenderSettingsCommand")"""
new = """    "colorGradingEnabled"
    "SetRenderSettingsCommand"
    "RenderSettingsMatchPath")"""
if text.count(old) != 1:
    raise SystemExit("render header contract token anchor changed")
text = text.replace(old, new, 1)
old = """    "colorGradingEnabled"
    "AntiAliasingMode::MSAA4X"
    "SetRenderSettingsCommand")"""
new = """    "colorGradingEnabled"
    "AntiAliasingMode::MSAA4X"
    "SetRenderSettingsCommand"
    "RenderSettingsMatchPath(nativePath, authored)")"""
if text.count(old) != 1:
    raise SystemExit("settings test contract token anchor changed")
text = text.replace(old, new, 1)
marker = 'message(STATUS "Phase 5 Gate 5 source contract passed")'
if text.count(marker) != 1:
    raise SystemExit("source contract final marker changed")
checks = """
# These ranges are the pinned Wicked Editor authoring domains. Owner testing
# proved the previous oversized domains made most slider travel ineffective.
foreach(token IN ITEMS
    "0.0f, 3.0f, 10000.0f"
    "-1.0f, 1.0f, 10000.0f"
    "0.0f, 10.0f, 1000.0f"
    "0.01f, 0.5f, 10000.0f"
    "1.0f, 20.0f, 1000.0f"
    "0.1f, 400.0f, 10000.0f"
    "0.0f, 40.0f, 1000.0f")
    string(FIND "${studio_render_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 5 Wicked-native creator range missing ${token}")
    endif()
endforeach()

foreach(forbidden IN ITEMS
    "0.0f, 16.0f, 1600.0f"
    "0.0f, 64.0f, 1280.0f"
    "0.0f, 100.0f, 1000.0f"
    "0.0f, 500.0f, 1000.0f")
    string(FIND "${studio_render_text}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Gate 5 oversized dead creator range returned: ${forbidden}")
    endif()
endforeach()

"""
text = text.replace(marker, checks + marker, 1)
contract.write_text(text, encoding="utf-8", newline="\n")
