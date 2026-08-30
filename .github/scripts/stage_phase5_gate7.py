from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


def write(path, text):
    p = ROOT / path
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text, encoding="utf-8", newline="\n")


def replace_once(path, old, new):
    text = read(path)
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected exactly one anchor, found {count}: {old[:120]!r}")
    write(path, text.replace(old, new, 1))


# -----------------------------------------------------------------------------
# EngineBridge render-settings schema v3 + real-time ray tracing state
# -----------------------------------------------------------------------------
header = "EngineBridge/include/renegade/bridge/RenderSettingsService.h"
replace_once(header, "inline constexpr int RenderSettingsSchemaVersion = 2;",
             "inline constexpr int RenderSettingsSchemaVersion = 3;")
replace_once(header,
'''        MSAO = 3,\n    };''',
'''        MSAO = 3,\n        RTAO = 4,\n    };''')
replace_once(header,
'''        bool ssrEnabled = false;\n        RenderQuality ssrQuality = RenderQuality::Medium;\n        float reflectionRoughnessCutoff = 0.6f;\n\n        bool depthOfFieldEnabled = true;''',
'''        bool ssrEnabled = false;\n        RenderQuality ssrQuality = RenderQuality::Medium;\n        float reflectionRoughnessCutoff = 0.6f;\n\n        bool raytracedShadowsEnabled = false;\n        bool raytracedReflectionsEnabled = false;\n        float raytracedReflectionsRange = 10000.0f;\n        RenderQuality raytracedReflectionsQuality = RenderQuality::Medium;\n        bool raytracedDiffuseEnabled = false;\n        float raytracedDiffuseRange = 10.0f;\n        RenderQuality raytracedDiffuseQuality = RenderQuality::Medium;\n        bool surfelGiEnabled = false;\n\n        bool depthOfFieldEnabled = true;''')
replace_once(header,
'''    [[nodiscard]] bool HasRenderSettingsChange(\n        const RenderSettingsState& before,\n        const RenderSettingsState& after) noexcept;\n\n    [[nodiscard]] wi::ecs::Entity FindRenderSettingsCarrier(''',
'''    [[nodiscard]] bool HasRenderSettingsChange(\n        const RenderSettingsState& before,\n        const RenderSettingsState& after) noexcept;\n    [[nodiscard]] bool IsHardwareRayTracingAvailable() noexcept;\n\n    [[nodiscard]] wi::ecs::Entity FindRenderSettingsCarrier(''')

source = "EngineBridge/src/RenderSettingsService.cpp"
replace_once(source,
'''        case Mode::MSAO:\n            return value;''',
'''        case Mode::MSAO:\n        case Mode::RTAO:\n            return value;''')
replace_once(source,
'''    constexpr const char* KeyReflectionRoughnessCutoff = "renegade.render.reflections.roughness_cutoff";\n    constexpr const char* KeyDepthOfFieldEnabled = "renegade.render.dof.enabled";''',
'''    constexpr const char* KeyReflectionRoughnessCutoff = "renegade.render.reflections.roughness_cutoff";\n    constexpr const char* KeyRaytracedShadowsEnabled = "renegade.render.rt.shadows.enabled";\n    constexpr const char* KeyRaytracedReflectionsEnabled = "renegade.render.rt.reflections.enabled";\n    constexpr const char* KeyRaytracedReflectionsRange = "renegade.render.rt.reflections.range";\n    constexpr const char* KeyRaytracedReflectionsQuality = "renegade.render.rt.reflections.quality";\n    constexpr const char* KeyRaytracedDiffuseEnabled = "renegade.render.rt.diffuse.enabled";\n    constexpr const char* KeyRaytracedDiffuseRange = "renegade.render.rt.diffuse.range";\n    constexpr const char* KeyRaytracedDiffuseQuality = "renegade.render.rt.diffuse.quality";\n    constexpr const char* KeySurfelGiEnabled = "renegade.render.rt.surfel_gi.enabled";\n    constexpr const char* KeyDepthOfFieldEnabled = "renegade.render.dof.enabled";''')
replace_once(source,
'''        result.ambientOcclusion = SanitizeAmbientOcclusion(result.ambientOcclusion);\n        result.ssrQuality = SanitizeRenderQuality(result.ssrQuality);''',
'''        result.ambientOcclusion = SanitizeAmbientOcclusion(result.ambientOcclusion);\n        result.ssrQuality = SanitizeRenderQuality(result.ssrQuality);\n        result.raytracedReflectionsQuality =\n            SanitizeRenderQuality(result.raytracedReflectionsQuality);\n        result.raytracedDiffuseQuality =\n            SanitizeRenderQuality(result.raytracedDiffuseQuality);''')
replace_once(source,
'''        result.reflectionRoughnessCutoff = ClampFinite(result.reflectionRoughnessCutoff, defaults.reflectionRoughnessCutoff, 0.0f, 1.0f);\n        return result;''',
'''        result.reflectionRoughnessCutoff = ClampFinite(result.reflectionRoughnessCutoff, defaults.reflectionRoughnessCutoff, 0.0f, 1.0f);\n        result.raytracedReflectionsRange = ClampFinite(\n            result.raytracedReflectionsRange, defaults.raytracedReflectionsRange, 1.0f, 10000.0f);\n        result.raytracedDiffuseRange = ClampFinite(\n            result.raytracedDiffuseRange, defaults.raytracedDiffuseRange, 1.0f, 100.0f);\n        return result;''')
replace_once(source,
'''            left.ssrEnabled != right.ssrEnabled ||\n            left.ssrQuality != right.ssrQuality ||\n            !NearlyEqual(left.reflectionRoughnessCutoff, right.reflectionRoughnessCutoff) ||\n            left.depthOfFieldEnabled != right.depthOfFieldEnabled ||''',
'''            left.ssrEnabled != right.ssrEnabled ||\n            left.ssrQuality != right.ssrQuality ||\n            !NearlyEqual(left.reflectionRoughnessCutoff, right.reflectionRoughnessCutoff) ||\n            left.raytracedShadowsEnabled != right.raytracedShadowsEnabled ||\n            left.raytracedReflectionsEnabled != right.raytracedReflectionsEnabled ||\n            !NearlyEqual(left.raytracedReflectionsRange, right.raytracedReflectionsRange) ||\n            left.raytracedReflectionsQuality != right.raytracedReflectionsQuality ||\n            left.raytracedDiffuseEnabled != right.raytracedDiffuseEnabled ||\n            !NearlyEqual(left.raytracedDiffuseRange, right.raytracedDiffuseRange) ||\n            left.raytracedDiffuseQuality != right.raytracedDiffuseQuality ||\n            left.surfelGiEnabled != right.surfelGiEnabled ||\n            left.depthOfFieldEnabled != right.depthOfFieldEnabled ||''')
replace_once(source,
'''    wi::ecs::Entity FindRenderSettingsCarrier(\n        const wi::scene::Scene& scene) noexcept''',
'''    bool IsHardwareRayTracingAvailable() noexcept\n    {\n        const auto* device = wi::graphics::GetDevice();\n        return device != nullptr && device->CheckCapability(\n            wi::graphics::GraphicsDeviceCapability::RAYTRACING);\n    }\n\n    wi::ecs::Entity FindRenderSettingsCarrier(\n        const wi::scene::Scene& scene) noexcept''')
replace_once(source,
'''        const int schema = ReadValue(metadata->int_values, KeySchema, 0);\n        if (schema != 1 && schema != RenderSettingsSchemaVersion)\n            return defaults;''',
'''        const int schema = ReadValue(metadata->int_values, KeySchema, 0);\n        if (schema < 1 || schema > RenderSettingsSchemaVersion)\n            return defaults;''')
replace_once(source,
'''            state.reflectionRoughnessCutoff = ReadValue(metadata->float_values, KeyReflectionRoughnessCutoff, defaults.reflectionRoughnessCutoff);\n        }\n        state.depthOfFieldEnabled = ReadValue(metadata->bool_values, KeyDepthOfFieldEnabled, defaults.depthOfFieldEnabled);''',
'''            state.reflectionRoughnessCutoff = ReadValue(metadata->float_values, KeyReflectionRoughnessCutoff, defaults.reflectionRoughnessCutoff);\n        }\n        if (schema >= 3)\n        {\n            state.raytracedShadowsEnabled = ReadValue(\n                metadata->bool_values, KeyRaytracedShadowsEnabled, defaults.raytracedShadowsEnabled);\n            state.raytracedReflectionsEnabled = ReadValue(\n                metadata->bool_values, KeyRaytracedReflectionsEnabled, defaults.raytracedReflectionsEnabled);\n            state.raytracedReflectionsRange = ReadValue(\n                metadata->float_values, KeyRaytracedReflectionsRange, defaults.raytracedReflectionsRange);\n            state.raytracedReflectionsQuality = static_cast<RenderQuality>(ReadValue(\n                metadata->int_values, KeyRaytracedReflectionsQuality,\n                static_cast<int>(defaults.raytracedReflectionsQuality)));\n            state.raytracedDiffuseEnabled = ReadValue(\n                metadata->bool_values, KeyRaytracedDiffuseEnabled, defaults.raytracedDiffuseEnabled);\n            state.raytracedDiffuseRange = ReadValue(\n                metadata->float_values, KeyRaytracedDiffuseRange, defaults.raytracedDiffuseRange);\n            state.raytracedDiffuseQuality = static_cast<RenderQuality>(ReadValue(\n                metadata->int_values, KeyRaytracedDiffuseQuality,\n                static_cast<int>(defaults.raytracedDiffuseQuality)));\n            state.surfelGiEnabled = ReadValue(\n                metadata->bool_values, KeySurfelGiEnabled, defaults.surfelGiEnabled);\n        }\n        state.depthOfFieldEnabled = ReadValue(metadata->bool_values, KeyDepthOfFieldEnabled, defaults.depthOfFieldEnabled);''')
replace_once(source,
'''        wi::RenderPath3D::AO ambientOcclusion = wi::RenderPath3D::AO_DISABLED;\n        switch (safe.ambientOcclusion)''',
'''        const bool hardwareRayTracing = IsHardwareRayTracingAvailable();\n        wi::RenderPath3D::AO ambientOcclusion = wi::RenderPath3D::AO_DISABLED;\n        switch (safe.ambientOcclusion)''')
replace_once(source,
'''        case RenderAmbientOcclusion::MSAO:\n            ambientOcclusion = wi::RenderPath3D::AO_MSAO;\n            break;\n        case RenderAmbientOcclusion::Off:''',
'''        case RenderAmbientOcclusion::MSAO:\n            ambientOcclusion = wi::RenderPath3D::AO_MSAO;\n            break;\n        case RenderAmbientOcclusion::RTAO:\n            if (hardwareRayTracing)\n                ambientOcclusion = wi::RenderPath3D::AO_RTAO;\n            break;\n        case RenderAmbientOcclusion::Off:''')
# The AO mapping appears in both Match and Apply; replace the second occurrence too.
text = read(source)
old = '''        case RenderAmbientOcclusion::MSAO:\n            ambientOcclusion = wi::RenderPath3D::AO_MSAO;\n            break;\n        case RenderAmbientOcclusion::Off:'''
if text.count(old) != 1:
    raise RuntimeError(f"{source}: expected one remaining Apply AO anchor, found {text.count(old)}")
write(source, text.replace(old,
'''        case RenderAmbientOcclusion::MSAO:\n            ambientOcclusion = wi::RenderPath3D::AO_MSAO;\n            break;\n        case RenderAmbientOcclusion::RTAO:\n            if (hardwareRayTracing)\n                ambientOcclusion = wi::RenderPath3D::AO_RTAO;\n            break;\n        case RenderAmbientOcclusion::Off:''', 1))
replace_once(source,
'''        if (path.getTonemap() != tonemap ||''',
'''        wi::renderer::PostProcessQuality rtReflectionsQuality = wi::renderer::PostProcessQuality::Medium;\n        switch (safe.raytracedReflectionsQuality)\n        {\n        case RenderQuality::Low: rtReflectionsQuality = wi::renderer::PostProcessQuality::Low; break;\n        case RenderQuality::High: rtReflectionsQuality = wi::renderer::PostProcessQuality::High; break;\n        case RenderQuality::Medium: default: break;\n        }\n        wi::renderer::PostProcessQuality rtDiffuseQuality = wi::renderer::PostProcessQuality::Medium;\n        switch (safe.raytracedDiffuseQuality)\n        {\n        case RenderQuality::Low: rtDiffuseQuality = wi::renderer::PostProcessQuality::Low; break;\n        case RenderQuality::High: rtDiffuseQuality = wi::renderer::PostProcessQuality::High; break;\n        case RenderQuality::Medium: default: break;\n        }\n        const bool expectedRtShadows = safe.raytracedShadowsEnabled && hardwareRayTracing;\n        const bool expectedRtReflections = safe.raytracedReflectionsEnabled && hardwareRayTracing;\n        const bool expectedRtDiffuse = safe.raytracedDiffuseEnabled && hardwareRayTracing;\n\n        if (path.getTonemap() != tonemap ||''')
replace_once(source,
'''            path.getSSRQuality() != ssrQuality ||\n            !NearlyEqual(path.getReflectionRoughnessCutoff(), safe.reflectionRoughnessCutoff) ||\n            path.getDepthOfFieldEnabled() != safe.depthOfFieldEnabled ||''',
'''            path.getSSRQuality() != ssrQuality ||\n            !NearlyEqual(path.getReflectionRoughnessCutoff(), safe.reflectionRoughnessCutoff) ||\n            wi::renderer::GetRaytracedShadowsEnabled() != expectedRtShadows ||\n            path.getRaytracedReflectionEnabled() != expectedRtReflections ||\n            !NearlyEqual(path.getRaytracedReflectionsRange(), safe.raytracedReflectionsRange) ||\n            path.getRaytracedReflectionsQuality() != rtReflectionsQuality ||\n            path.getRaytracedDiffuseEnabled() != expectedRtDiffuse ||\n            !NearlyEqual(path.getRaytracedDiffuseRange(), safe.raytracedDiffuseRange) ||\n            path.getRaytracedDiffuseQuality() != rtDiffuseQuality ||\n            wi::renderer::GetSurfelGIEnabled() != safe.surfelGiEnabled ||\n            path.getDepthOfFieldEnabled() != safe.depthOfFieldEnabled ||''')
replace_once(source,
'''        metadata->float_values.set(KeyReflectionRoughnessCutoff, safe.reflectionRoughnessCutoff);\n        metadata->bool_values.set(KeyDepthOfFieldEnabled, safe.depthOfFieldEnabled);''',
'''        metadata->float_values.set(KeyReflectionRoughnessCutoff, safe.reflectionRoughnessCutoff);\n        metadata->bool_values.set(KeyRaytracedShadowsEnabled, safe.raytracedShadowsEnabled);\n        metadata->bool_values.set(KeyRaytracedReflectionsEnabled, safe.raytracedReflectionsEnabled);\n        metadata->float_values.set(KeyRaytracedReflectionsRange, safe.raytracedReflectionsRange);\n        metadata->int_values.set(KeyRaytracedReflectionsQuality, static_cast<int>(safe.raytracedReflectionsQuality));\n        metadata->bool_values.set(KeyRaytracedDiffuseEnabled, safe.raytracedDiffuseEnabled);\n        metadata->float_values.set(KeyRaytracedDiffuseRange, safe.raytracedDiffuseRange);\n        metadata->int_values.set(KeyRaytracedDiffuseQuality, static_cast<int>(safe.raytracedDiffuseQuality));\n        metadata->bool_values.set(KeySurfelGiEnabled, safe.surfelGiEnabled);\n        metadata->bool_values.set(KeyDepthOfFieldEnabled, safe.depthOfFieldEnabled);''')
# Insert capability variable in Apply before its AO mapping (the Match occurrence already has one).
replace_once(source,
'''        path.setAOPower(safe.ambientOcclusionPower);\n        path.setAORange(safe.ambientOcclusionRange);\n        path.setAOSampleCount(safe.ambientOcclusionSampleCount);\n        wi::RenderPath3D::AO ambientOcclusion = wi::RenderPath3D::AO_DISABLED;''',
'''        path.setAOPower(safe.ambientOcclusionPower);\n        path.setAORange(safe.ambientOcclusionRange);\n        path.setAOSampleCount(safe.ambientOcclusionSampleCount);\n        const bool hardwareRayTracing = IsHardwareRayTracingAvailable();\n        wi::RenderPath3D::AO ambientOcclusion = wi::RenderPath3D::AO_DISABLED;''')
replace_once(source,
'''        if (path.getSSREnabled() != safe.ssrEnabled)\n            path.setSSREnabled(safe.ssrEnabled);\n\n        path.setDepthOfFieldEnabled(safe.depthOfFieldEnabled);''',
'''        if (path.getSSREnabled() != safe.ssrEnabled)\n            path.setSSREnabled(safe.ssrEnabled);\n\n        const bool enableRtShadows = safe.raytracedShadowsEnabled && hardwareRayTracing;\n        if (wi::renderer::GetRaytracedShadowsEnabled() != enableRtShadows)\n            wi::renderer::SetRaytracedShadowsEnabled(enableRtShadows);\n\n        path.setRaytracedReflectionsRange(safe.raytracedReflectionsRange);\n        wi::renderer::PostProcessQuality rtReflectionsQuality = wi::renderer::PostProcessQuality::Medium;\n        switch (safe.raytracedReflectionsQuality)\n        {\n        case RenderQuality::Low: rtReflectionsQuality = wi::renderer::PostProcessQuality::Low; break;\n        case RenderQuality::High: rtReflectionsQuality = wi::renderer::PostProcessQuality::High; break;\n        case RenderQuality::Medium: default: break;\n        }\n        if (path.getRaytracedReflectionsQuality() != rtReflectionsQuality)\n        {\n            const bool wasEnabled = path.getRaytracedReflectionEnabled();\n            path.setRaytracedReflectionsQuality(rtReflectionsQuality);\n            if (wasEnabled && hardwareRayTracing)\n                path.setRaytracedReflectionsEnabled(true);\n        }\n        const bool enableRtReflections = safe.raytracedReflectionsEnabled && hardwareRayTracing;\n        if (path.getRaytracedReflectionEnabled() != enableRtReflections)\n            path.setRaytracedReflectionsEnabled(enableRtReflections);\n\n        path.setRaytracedDiffuseRange(safe.raytracedDiffuseRange);\n        wi::renderer::PostProcessQuality rtDiffuseQuality = wi::renderer::PostProcessQuality::Medium;\n        switch (safe.raytracedDiffuseQuality)\n        {\n        case RenderQuality::Low: rtDiffuseQuality = wi::renderer::PostProcessQuality::Low; break;\n        case RenderQuality::High: rtDiffuseQuality = wi::renderer::PostProcessQuality::High; break;\n        case RenderQuality::Medium: default: break;\n        }\n        if (path.getRaytracedDiffuseQuality() != rtDiffuseQuality)\n        {\n            const bool wasEnabled = path.getRaytracedDiffuseEnabled();\n            path.setRaytracedDiffuseQuality(rtDiffuseQuality);\n            if (wasEnabled && hardwareRayTracing)\n                path.setRaytracedDiffuseEnabled(true);\n        }\n        const bool enableRtDiffuse = safe.raytracedDiffuseEnabled && hardwareRayTracing;\n        if (path.getRaytracedDiffuseEnabled() != enableRtDiffuse)\n            path.setRaytracedDiffuseEnabled(enableRtDiffuse);\n\n        if (wi::renderer::GetSurfelGIEnabled() != safe.surfelGiEnabled)\n            wi::renderer::SetSurfelGIEnabled(safe.surfelGiEnabled);\n\n        path.setDepthOfFieldEnabled(safe.depthOfFieldEnabled);''')

# -----------------------------------------------------------------------------
# Studio declaration: inherit native path tracer, add bounded Gate 7 seam
# -----------------------------------------------------------------------------
studio_header = "Studio/src/StudioApplication.h"
replace_once(studio_header,
             "class StudioRenderPath : public wi::RenderPath3D",
             "class StudioRenderPath : public wi::RenderPath3D_PathTracing")
replace_once(studio_header,
'''            PlanarReflectionResolutionScale,\n            ReflectionRoughnessCutoff,\n        };''',
'''            PlanarReflectionResolutionScale,\n            ReflectionRoughnessCutoff,\n            RaytracedReflectionsRange,\n            RaytracedDiffuseRange,\n        };''')
replace_once(studio_header,
'''        void ClearRenderLut();\n        void BeginRenderSlider(RenderField field);''',
'''        void ClearRenderLut();\n        void CreateGate7RenderControls();\n        void LayoutGate7RenderControls(float fieldWidth, float& y);\n        void RefreshGate7RenderControls(const bridge::RenderSettingsState& state);\n        void SetPathTracePreviewActive(bool active);\n        void RestartPathTracePreview();\n        void RefreshPathTracePreviewStatus();\n        void BeginRenderSlider(RenderField field);''')
replace_once(studio_header,
'''        SceneInspectorComboBox renderSsrQuality_;\n        SceneInspectorSlider renderReflectionRoughnessCutoff_;\n        SceneInspectorButton focusButton_;''',
'''        SceneInspectorComboBox renderSsrQuality_;\n        SceneInspectorSlider renderReflectionRoughnessCutoff_;\n        wi::gui::Label renderRayTracingLabel_;\n        wi::gui::Label renderRayTracingCapability_;\n        SceneInspectorCheckBox renderRaytracedShadowsEnabled_;\n        SceneInspectorCheckBox renderRaytracedReflectionsEnabled_;\n        SceneInspectorSlider renderRaytracedReflectionsRange_;\n        SceneInspectorComboBox renderRaytracedReflectionsQuality_;\n        SceneInspectorCheckBox renderRaytracedDiffuseEnabled_;\n        SceneInspectorSlider renderRaytracedDiffuseRange_;\n        SceneInspectorComboBox renderRaytracedDiffuseQuality_;\n        SceneInspectorCheckBox renderSurfelGiEnabled_;\n        wi::gui::Label renderPathTraceLabel_;\n        wi::gui::Label renderPathTraceStatus_;\n        SceneInspectorButton renderPathTraceToggle_;\n        SceneInspectorSlider renderPathTraceSamples_;\n        SceneInspectorSlider renderPathTraceBounces_;\n        SceneInspectorButton renderPathTraceRestart_;\n        SceneInspectorButton focusButton_;''')
replace_once(studio_header,
'''        bridge::RenderSettingsState appliedRenderSettings_;\n        bool appliedRenderSettingsInitialized_ = false;''',
'''        bridge::RenderSettingsState appliedRenderSettings_;\n        bool appliedRenderSettingsInitialized_ = false;\n        bool pathTracePreviewActive_ = false;\n        int pathTraceTargetSamples_ = 1024;\n        std::uint32_t pathTraceBounceCount_ = 1;\n        std::uint32_t pathTracePreviousBounceCount_ = 1;''')

# -----------------------------------------------------------------------------
# Existing RENDER workspace: only bounded Gate7 hooks + direct range previews
# -----------------------------------------------------------------------------
workspace = "Studio/src/Phase5Gate5RenderWorkspace.cpp"
replace_once(workspace,
'''        renderAmbientOcclusion_.AddItem("MSAO", static_cast<std::uint64_t>(bridge::RenderAmbientOcclusion::MSAO));\n        renderAmbientOcclusion_.SetTooltip(\n            "Choose Wicked ambient occlusion. Ray-traced AO is reserved for Gate 7.");''',
'''        renderAmbientOcclusion_.AddItem("MSAO", static_cast<std::uint64_t>(bridge::RenderAmbientOcclusion::MSAO));\n        if (bridge::IsHardwareRayTracingAvailable())\n        {\n            renderAmbientOcclusion_.AddItem(\n                "RTAO", static_cast<std::uint64_t>(bridge::RenderAmbientOcclusion::RTAO));\n        }\n        renderAmbientOcclusion_.SetTooltip(\n            "Choose Wicked ambient occlusion. RTAO is available on hardware ray-tracing GPUs.");''')
replace_once(workspace,
'''        createSlider(\n            renderReflectionRoughnessCutoff_, "Render Reflection Roughness Cutoff",\n            "REFLECTIONS // ROUGHNESS CUTOFF",\n            "Maximum material roughness that receives screen-space reflections.",\n            RenderField::ReflectionRoughnessCutoff, 0.0f, 1.0f, 1000.0f);\n\n        // Wicked Window::AddWidget() copies Window::IsEnabled() into each child,''',
'''        createSlider(\n            renderReflectionRoughnessCutoff_, "Render Reflection Roughness Cutoff",\n            "REFLECTIONS // ROUGHNESS CUTOFF",\n            "Maximum material roughness that receives screen-space reflections.",\n            RenderField::ReflectionRoughnessCutoff, 0.0f, 1.0f, 1000.0f);\n\n        CreateGate7RenderControls();\n\n        // Wicked Window::AddWidget() copies Window::IsEnabled() into each child,''')
replace_once(workspace,
'''        full(renderSsrQuality_);\n        full(renderReflectionRoughnessCutoff_);\n    }''',
'''        full(renderSsrQuality_);\n        full(renderReflectionRoughnessCutoff_);\n\n        LayoutGate7RenderControls(fieldWidth, y);\n    }''')
replace_once(workspace,
'''        renderReflectionRoughnessCutoff_.SetValue(state.reflectionRoughnessCutoff);\n\n        renderBloomThreshold_.SetEnabled(state.bloomEnabled);''',
'''        renderReflectionRoughnessCutoff_.SetValue(state.reflectionRoughnessCutoff);\n        RefreshGate7RenderControls(state);\n\n        renderBloomThreshold_.SetEnabled(state.bloomEnabled);''')
replace_once(workspace,
'''        const bool ssao =\n            state.ambientOcclusion == bridge::RenderAmbientOcclusion::SSAO;\n        renderAmbientOcclusionPower_.SetEnabled(\n            state.ambientOcclusion != bridge::RenderAmbientOcclusion::Off);\n        renderAmbientOcclusionRange_.SetEnabled(ssao);\n        renderAmbientOcclusionSampleCount_.SetEnabled(ssao);''',
'''        const bool ssao =\n            state.ambientOcclusion == bridge::RenderAmbientOcclusion::SSAO;\n        const bool rtao =\n            state.ambientOcclusion == bridge::RenderAmbientOcclusion::RTAO &&\n            bridge::IsHardwareRayTracingAvailable();\n        renderAmbientOcclusionPower_.SetEnabled(\n            state.ambientOcclusion != bridge::RenderAmbientOcclusion::Off);\n        renderAmbientOcclusionRange_.SetEnabled(ssao || rtao);\n        renderAmbientOcclusionSampleCount_.SetEnabled(ssao);''')
replace_once(workspace,
'''        else\n        {\n            studioChrome_.SetRenderWorkspaceActive(false);''',
'''        else\n        {\n            if (pathTracePreviewActive_)\n                SetPathTracePreviewActive(false);\n            studioChrome_.SetRenderWorkspaceActive(false);''')
replace_once(workspace,
'''        case RenderField::ReflectionRoughnessCutoff:\n            setReflectionRoughnessCutoff(renderSliderAfter_.reflectionRoughnessCutoff);\n            break;\n        }''',
'''        case RenderField::ReflectionRoughnessCutoff:\n            setReflectionRoughnessCutoff(renderSliderAfter_.reflectionRoughnessCutoff);\n            break;\n        case RenderField::RaytracedReflectionsRange:\n            setRaytracedReflectionsRange(renderSliderAfter_.raytracedReflectionsRange);\n            break;\n        case RenderField::RaytracedDiffuseRange:\n            setRaytracedDiffuseRange(renderSliderAfter_.raytracedDiffuseRange);\n            break;\n        }''')
replace_once(workspace,
'''        case RenderField::ReflectionRoughnessCutoff:\n            state.reflectionRoughnessCutoff = value;\n            break;\n        }''',
'''        case RenderField::ReflectionRoughnessCutoff:\n            state.reflectionRoughnessCutoff = value;\n            break;\n        case RenderField::RaytracedReflectionsRange:\n            state.raytracedReflectionsRange = value;\n            break;\n        case RenderField::RaytracedDiffuseRange:\n            state.raytracedDiffuseRange = value;\n            break;\n        }''')

# -----------------------------------------------------------------------------
# Studio native renderer hooks: four tiny guarded substitutions
# -----------------------------------------------------------------------------
studio_cpp = "Studio/src/StudioApplication.cpp"
replace_once(studio_cpp,
'''    void StudioRenderPath::Update(const float dt)\n    {\n        SyncRenderSettingsFromScene(true);''',
'''    void StudioRenderPath::Update(const float dt)\n    {\n        if (!pathTracePreviewActive_)\n            SyncRenderSettingsFromScene(true);''')
replace_once(studio_cpp,
'''        RenderPath3D::Update(dt);\n\n        if (session_ == nullptr || projectHubVisible_)''',
'''        if (pathTracePreviewActive_)\n        {\n            RenderPath3D_PathTracing::Update(dt);\n            RefreshPathTracePreviewStatus();\n        }\n        else\n        {\n            RenderPath3D::Update(dt);\n        }\n\n        if (session_ == nullptr || projectHubVisible_)''')
replace_once(studio_cpp,
'''    void StudioRenderPath::Render() const\n    {\n        RenderPath3D::Render();''',
'''    void StudioRenderPath::Render() const\n    {\n        if (pathTracePreviewActive_)\n        {\n            RenderPath3D_PathTracing::Render();\n            return;\n        }\n        RenderPath3D::Render();''')
replace_once(studio_cpp,
'''    void StudioRenderPath::Compose(const wi::graphics::CommandList cmd) const\n    {\n        RenderPath3D::Compose(cmd);''',
'''    void StudioRenderPath::Compose(const wi::graphics::CommandList cmd) const\n    {\n        if (pathTracePreviewActive_)\n        {\n            RenderPath3D_PathTracing::Compose(cmd);\n            return;\n        }\n        RenderPath3D::Compose(cmd);''')
replace_once(studio_cpp,
'''    void StudioRenderPath::ResizeBuffers()\n    {\n        RenderPath3D::ResizeBuffers();''',
'''    void StudioRenderPath::ResizeBuffers()\n    {\n        if (pathTracePreviewActive_)\n        {\n            selectionOutlineMask_ = {};\n            selectionOutlineMaskMsaa_ = {};\n            RenderPath3D_PathTracing::ResizeBuffers();\n            return;\n        }\n        RenderPath3D::ResizeBuffers();''')

# -----------------------------------------------------------------------------
# Dedicated Gate 7 Studio implementation
# -----------------------------------------------------------------------------
gate7_cpp = r'''#include "StudioApplication.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace
{
    constexpr wi::Color RenderMuted = wi::Color(178, 178, 176, 255);
}

namespace renegade::studio
{
    void StudioRenderPath::CreateGate7RenderControls()
    {
        const auto createSection = [this](wi::gui::Label& label, const char* name, const char* text)
        {
            label.Create(name);
            label.SetText(text);
            label.font.params.size = 13;
            label.font.params.color = RenderMuted;
            label.font.params.h_align = wi::font::WIFALIGN_LEFT;
            label.SetColor(wi::Color::Transparent());
            renderWorkspacePanel_.AddWidget(&label);
        };
        const auto createToggle = [this](SceneInspectorCheckBox& checkbox, const char* name, const char* tooltip,
                                         auto mutate)
        {
            checkbox.Create(name);
            checkbox.SetTooltip(tooltip);
            checkbox.OnClick([this, mutate](const wi::gui::EventArgs& args)
            {
                if (session_ == nullptr)
                    return;
                auto state = bridge::CaptureRenderSettings(session_->Scenes().GetScene());
                mutate(state, args.bValue);
                CommitRenderSettings(state);
            });
            renderWorkspacePanel_.AddWidget(&checkbox);
        };
        const auto createRange = [this](SceneInspectorSlider& slider, const char* name, const char* label,
                                        const char* tooltip, const RenderField field,
                                        const float minimum, const float maximum, const float steps)
        {
            slider.Create(minimum, maximum, minimum, steps, name, label);
            slider.SetTooltip(tooltip);
            slider.OnDragStarted([this, field](const float) { BeginRenderSlider(field); });
            slider.OnValuePreview([this, field](const float value) { PreviewRenderSlider(field, value); });
            slider.OnValueCommitted([this, field](const float value) { CommitRenderSlider(field, value); });
            renderWorkspacePanel_.AddWidget(&slider);
        };

        createSection(renderRayTracingLabel_, "Render Ray Tracing Section", "RAY TRACING // REALTIME");
        renderRayTracingCapability_.Create("Render Ray Tracing Capability");
        renderRayTracingCapability_.font.params.size = 12;
        renderRayTracingCapability_.font.params.color = RenderMuted;
        renderRayTracingCapability_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        renderRayTracingCapability_.SetColor(wi::Color::Transparent());
        renderWorkspacePanel_.AddWidget(&renderRayTracingCapability_);

        createToggle(renderRaytracedShadowsEnabled_, "Ray-traced shadows: ",
            "Replace shadow-map lighting with Wicked hardware ray-traced shadows when supported.",
            [](bridge::RenderSettingsState& state, bool value) { state.raytracedShadowsEnabled = value; });
        createToggle(renderRaytracedReflectionsEnabled_, "Ray-traced reflections: ",
            "Enable Wicked hardware ray-traced reflections.",
            [](bridge::RenderSettingsState& state, bool value) { state.raytracedReflectionsEnabled = value; });
        createRange(renderRaytracedReflectionsRange_, "Render RT Reflection Range", "RT REFLECTIONS // RANGE",
            "Maximum ray distance for Wicked ray-traced reflections.",
            RenderField::RaytracedReflectionsRange, 1.0f, 10000.0f, 1000.0f);
        renderRaytracedReflectionsQuality_.Create("Render RT Reflection Quality");
        renderRaytracedReflectionsQuality_.AddItem("LOW", static_cast<std::uint64_t>(bridge::RenderQuality::Low));
        renderRaytracedReflectionsQuality_.AddItem("MEDIUM", static_cast<std::uint64_t>(bridge::RenderQuality::Medium));
        renderRaytracedReflectionsQuality_.AddItem("HIGH", static_cast<std::uint64_t>(bridge::RenderQuality::High));
        renderRaytracedReflectionsQuality_.SetTooltip("Native Wicked ray-traced reflection quality.");
        renderRaytracedReflectionsQuality_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            if (session_ == nullptr)
                return;
            auto state = bridge::CaptureRenderSettings(session_->Scenes().GetScene());
            state.raytracedReflectionsQuality = static_cast<bridge::RenderQuality>(args.userdata);
            CommitRenderSettings(state);
        });
        renderWorkspacePanel_.AddWidget(&renderRaytracedReflectionsQuality_);

        createToggle(renderRaytracedDiffuseEnabled_, "Ray-traced diffuse: ",
            "Enable Wicked single-bounce ray-traced diffuse lighting.",
            [](bridge::RenderSettingsState& state, bool value) { state.raytracedDiffuseEnabled = value; });
        createRange(renderRaytracedDiffuseRange_, "Render RT Diffuse Range", "RT DIFFUSE // RANGE",
            "Maximum ray distance for Wicked ray-traced diffuse lighting.",
            RenderField::RaytracedDiffuseRange, 1.0f, 100.0f, 1000.0f);
        renderRaytracedDiffuseQuality_.Create("Render RT Diffuse Quality");
        renderRaytracedDiffuseQuality_.AddItem("LOW", static_cast<std::uint64_t>(bridge::RenderQuality::Low));
        renderRaytracedDiffuseQuality_.AddItem("MEDIUM", static_cast<std::uint64_t>(bridge::RenderQuality::Medium));
        renderRaytracedDiffuseQuality_.AddItem("HIGH", static_cast<std::uint64_t>(bridge::RenderQuality::High));
        renderRaytracedDiffuseQuality_.SetTooltip("Native Wicked ray-traced diffuse quality.");
        renderRaytracedDiffuseQuality_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            if (session_ == nullptr)
                return;
            auto state = bridge::CaptureRenderSettings(session_->Scenes().GetScene());
            state.raytracedDiffuseQuality = static_cast<bridge::RenderQuality>(args.userdata);
            CommitRenderSettings(state);
        });
        renderWorkspacePanel_.AddWidget(&renderRaytracedDiffuseQuality_);
        createToggle(renderSurfelGiEnabled_, "Surfel GI: ",
            "Enable Wicked ray-traced diffuse GI using its surface cache.",
            [](bridge::RenderSettingsState& state, bool value) { state.surfelGiEnabled = value; });

        createSection(renderPathTraceLabel_, "Render Path Trace Section", "PATH TRACE // REFERENCE PREVIEW");
        renderPathTraceStatus_.Create("Render Path Trace Status");
        renderPathTraceStatus_.font.params.size = 12;
        renderPathTraceStatus_.font.params.color = RenderMuted;
        renderPathTraceStatus_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        renderPathTraceStatus_.SetColor(wi::Color::Transparent());
        renderWorkspacePanel_.AddWidget(&renderPathTraceStatus_);

        renderPathTraceToggle_.Create("Render Path Trace Toggle");
        renderPathTraceToggle_.SetText("START PATH TRACE PREVIEW");
        renderPathTraceToggle_.SetTooltip(
            "Use Wicked's native progressive path tracer as a Studio reference preview. On GPUs without hardware ray tracing the fallback can be very slow.");
        renderPathTraceToggle_.OnClick([this](const wi::gui::EventArgs&) {
            SetPathTracePreviewActive(!pathTracePreviewActive_);
        });
        renderWorkspacePanel_.AddWidget(&renderPathTraceToggle_);

        renderPathTraceSamples_.Create(1.0f, 2048.0f, 1024.0f, 2047.0f,
            "Render Path Trace Samples", "TARGET SAMPLES");
        renderPathTraceSamples_.SetTooltip("Target progressive samples per pixel (1-2048).");
        renderPathTraceSamples_.OnValueCommitted([this](const float value)
        {
            pathTraceTargetSamples_ = std::clamp(static_cast<int>(std::lround(value)), 1, 2048);
            setTargetSampleCount(pathTraceTargetSamples_);
            if (pathTracePreviewActive_)
                RestartPathTracePreview();
        });
        renderWorkspacePanel_.AddWidget(&renderPathTraceSamples_);

        renderPathTraceBounces_.Create(1.0f, 10.0f, 1.0f, 9.0f,
            "Render Path Trace Bounces", "RAYTRACE BOUNCES");
        renderPathTraceBounces_.SetTooltip("Wicked path-trace light-bounce count (1-10).");
        renderPathTraceBounces_.OnValueCommitted([this](const float value)
        {
            pathTraceBounceCount_ = std::clamp(
                static_cast<std::uint32_t>(std::lround(value)), 1u, 10u);
            if (pathTracePreviewActive_)
            {
                wi::renderer::SetRaytraceBounceCount(pathTraceBounceCount_);
                RestartPathTracePreview();
            }
        });
        renderWorkspacePanel_.AddWidget(&renderPathTraceBounces_);

        renderPathTraceRestart_.Create("Render Path Trace Restart");
        renderPathTraceRestart_.SetText("RESTART ACCUMULATION");
        renderPathTraceRestart_.SetTooltip("Discard accumulated samples and restart the reference preview.");
        renderPathTraceRestart_.OnClick([this](const wi::gui::EventArgs&) { RestartPathTracePreview(); });
        renderWorkspacePanel_.AddWidget(&renderPathTraceRestart_);
    }

    void StudioRenderPath::LayoutGate7RenderControls(const float fieldWidth, float& y)
    {
        const auto section = [fieldWidth, &y](wi::gui::Widget& widget)
        {
            widget.SetPos(XMFLOAT2(12.0f, y));
            widget.SetSize(XMFLOAT2(fieldWidth, 20.0f));
            y += 24.0f;
        };
        const auto full = [fieldWidth, &y](wi::gui::Widget& widget)
        {
            widget.SetPos(XMFLOAT2(12.0f, y));
            widget.SetSize(XMFLOAT2(fieldWidth, 28.0f));
            y += 32.0f;
        };

        section(renderRayTracingLabel_);
        full(renderRayTracingCapability_);
        full(renderRaytracedShadowsEnabled_);
        full(renderRaytracedReflectionsEnabled_);
        full(renderRaytracedReflectionsRange_);
        full(renderRaytracedReflectionsQuality_);
        full(renderRaytracedDiffuseEnabled_);
        full(renderRaytracedDiffuseRange_);
        full(renderRaytracedDiffuseQuality_);
        full(renderSurfelGiEnabled_);

        section(renderPathTraceLabel_);
        full(renderPathTraceStatus_);
        full(renderPathTraceToggle_);
        full(renderPathTraceSamples_);
        full(renderPathTraceBounces_);
        full(renderPathTraceRestart_);
    }

    void StudioRenderPath::RefreshGate7RenderControls(const bridge::RenderSettingsState& state)
    {
        const bool hardware = bridge::IsHardwareRayTracingAvailable();
        renderRayTracingCapability_.SetText(
            hardware ? "HARDWARE RAY TRACING // AVAILABLE" : "HARDWARE RAY TRACING // UNAVAILABLE");

        if (!hardware && state.ambientOcclusion == bridge::RenderAmbientOcclusion::RTAO)
        {
            renderAmbientOcclusion_.SetSelectedByUserdataWithoutCallback(
                static_cast<std::uint64_t>(bridge::RenderAmbientOcclusion::Off));
        }

        renderRaytracedShadowsEnabled_.SetCheck(state.raytracedShadowsEnabled);
        renderRaytracedReflectionsEnabled_.SetCheck(state.raytracedReflectionsEnabled);
        renderRaytracedReflectionsRange_.SetValue(state.raytracedReflectionsRange);
        renderRaytracedReflectionsQuality_.SetSelectedByUserdataWithoutCallback(
            static_cast<std::uint64_t>(state.raytracedReflectionsQuality));
        renderRaytracedDiffuseEnabled_.SetCheck(state.raytracedDiffuseEnabled);
        renderRaytracedDiffuseRange_.SetValue(state.raytracedDiffuseRange);
        renderRaytracedDiffuseQuality_.SetSelectedByUserdataWithoutCallback(
            static_cast<std::uint64_t>(state.raytracedDiffuseQuality));
        renderSurfelGiEnabled_.SetCheck(state.surfelGiEnabled);

        renderRaytracedShadowsEnabled_.SetEnabled(hardware);
        renderRaytracedReflectionsEnabled_.SetEnabled(hardware);
        renderRaytracedReflectionsRange_.SetEnabled(hardware && state.raytracedReflectionsEnabled);
        renderRaytracedReflectionsQuality_.SetEnabled(hardware && state.raytracedReflectionsEnabled);
        renderRaytracedDiffuseEnabled_.SetEnabled(hardware);
        renderRaytracedDiffuseRange_.SetEnabled(hardware && state.raytracedDiffuseEnabled);
        renderRaytracedDiffuseQuality_.SetEnabled(hardware && state.raytracedDiffuseEnabled);
        renderSurfelGiEnabled_.SetEnabled(true);

        renderPathTraceSamples_.SetValue(static_cast<float>(pathTraceTargetSamples_));
        renderPathTraceBounces_.SetValue(static_cast<float>(pathTraceBounceCount_));
        renderPathTraceToggle_.SetText(
            pathTracePreviewActive_ ? "RETURN TO REALTIME" : "START PATH TRACE PREVIEW");
        renderPathTraceRestart_.SetEnabled(pathTracePreviewActive_);
        RefreshPathTracePreviewStatus();
    }

    void StudioRenderPath::SetPathTracePreviewActive(const bool active)
    {
        if (pathTracePreviewActive_ == active)
            return;

        if (active)
        {
            pathTracePreviousBounceCount_ = wi::renderer::GetRaytraceBounceCount();
            pathTracePreviewActive_ = true;
            setTargetSampleCount(pathTraceTargetSamples_);
            wi::renderer::SetRaytraceBounceCount(pathTraceBounceCount_);
            resetProgress();
            ResizeBuffers();
            studioChrome_.SetStatusText("RENDER // PATH TRACE REFERENCE PREVIEW");
        }
        else
        {
            pathTracePreviewActive_ = false;
            resetProgress();
            wi::renderer::SetRaytraceBounceCount(pathTracePreviousBounceCount_);
            ResizeBuffers();
            if (session_ != nullptr)
                ApplyRenderSettingsState(bridge::CaptureRenderSettings(session_->Scenes().GetScene()), true);
            studioChrome_.SetStatusText("RENDER // NATIVE WICKED");
        }
        RefreshGate7RenderControls(
            session_ != nullptr ? bridge::CaptureRenderSettings(session_->Scenes().GetScene())
                                : bridge::DefaultRenderSettings());
    }

    void StudioRenderPath::RestartPathTracePreview()
    {
        if (!pathTracePreviewActive_)
            return;
        setTargetSampleCount(pathTraceTargetSamples_);
        wi::renderer::SetRaytraceBounceCount(pathTraceBounceCount_);
        resetProgress();
        RefreshPathTracePreviewStatus();
    }

    void StudioRenderPath::RefreshPathTracePreviewStatus()
    {
        const bool hardware = bridge::IsHardwareRayTracingAvailable();
        if (!pathTracePreviewActive_)
        {
            renderPathTraceStatus_.SetText(
                hardware ? "REFERENCE PREVIEW // IDLE // HARDWARE RT"
                         : "REFERENCE PREVIEW // IDLE // SOFTWARE FALLBACK WILL BE SLOW");
            return;
        }

        const int sample = std::max(0, getCurrentSampleCount());
        const float progress = std::clamp(getProgress(), 0.0f, 1.0f) * 100.0f;
        std::ostringstream text;
        text << "PATH TRACE // " << sample << "/" << pathTraceTargetSamples_
             << " SAMPLES // " << std::fixed << std::setprecision(1) << progress << "%";
        if (isDenoiserAvailable())
        {
            text << " // DENOISER "
                 << std::fixed << std::setprecision(0)
                 << std::clamp(getDenoiserProgress(), 0.0f, 1.0f) * 100.0f << "%";
        }
        else
        {
            text << " // DENOISER UNAVAILABLE";
        }
        renderPathTraceStatus_.SetText(text.str());
    }
}
'''
write("Studio/src/Phase5Gate7RayPathTracing.cpp", gate7_cpp)

# Add dedicated source file to Studio target.
replace_once("Studio/CMakeLists.txt",
'''    src/Phase5Gate5RenderWorkspace.cpp\n    src/StudioApplication.cpp''',
'''    src/Phase5Gate5RenderWorkspace.cpp\n    src/Phase5Gate7RayPathTracing.cpp\n    src/StudioApplication.cpp''')

# -----------------------------------------------------------------------------
# Gate 7 automated tests / source contract
# -----------------------------------------------------------------------------
gate7_test = r'''#include <cmath>
#include <iostream>
#include <memory>

#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/RenderSettingsService.h"
#include "renegade/bridge/SceneService.h"

namespace
{
    int Fail(const char* message)
    {
        std::cerr << "FAIL: " << message << '\n';
        return 1;
    }
    bool NearlyEqual(float a, float b) { return std::abs(a - b) <= 0.0001f; }
}

int main()
{
    using namespace renegade::bridge;
    const auto defaults = DefaultRenderSettings();
    if (defaults.schemaVersion != 3 ||
        defaults.raytracedShadowsEnabled || defaults.raytracedReflectionsEnabled ||
        !NearlyEqual(defaults.raytracedReflectionsRange, 10000.0f) ||
        defaults.raytracedReflectionsQuality != RenderQuality::Medium ||
        defaults.raytracedDiffuseEnabled || !NearlyEqual(defaults.raytracedDiffuseRange, 10.0f) ||
        defaults.raytracedDiffuseQuality != RenderQuality::Medium || defaults.surfelGiEnabled)
        return Fail("Gate 7 defaults do not match pinned Wicked");

    auto malformed = defaults;
    malformed.ambientOcclusion = static_cast<RenderAmbientOcclusion>(99);
    malformed.raytracedReflectionsRange = 99999.0f;
    malformed.raytracedDiffuseRange = -10.0f;
    malformed.raytracedReflectionsQuality = static_cast<RenderQuality>(99);
    malformed.raytracedDiffuseQuality = static_cast<RenderQuality>(99);
    const auto safe = SanitizeRenderSettings(malformed);
    if (safe.ambientOcclusion != RenderAmbientOcclusion::Off ||
        !NearlyEqual(safe.raytracedReflectionsRange, 10000.0f) ||
        !NearlyEqual(safe.raytracedDiffuseRange, 1.0f) ||
        safe.raytracedReflectionsQuality != RenderQuality::Medium ||
        safe.raytracedDiffuseQuality != RenderQuality::Medium)
        return Fail("Gate 7 sanitization is not deterministic");

    SceneService legacy;
    auto& legacyScene = legacy.GetScene();
    const auto carrier = wi::ecs::CreateEntity();
    legacyScene.names.Create(carrier) = RenderSettingsCarrierName;
    auto& metadata = legacyScene.metadatas.Create(carrier);
    metadata.int_values.set("renegade.render.schema", 2);
    metadata.float_values.set("renegade.render.exposure", 1.7f);
    metadata.int_values.set("renegade.render.ao.mode", static_cast<int>(RenderAmbientOcclusion::HBAO));
    metadata.bool_values.set("renegade.render.ssgi.enabled", true);
    metadata.float_values.set("renegade.render.gi_boost", 1.8f);
    metadata.bool_values.set("renegade.render.ssr.enabled", true);
    const auto migrated = CaptureRenderSettings(legacyScene);
    if (migrated.schemaVersion != 3 || !NearlyEqual(migrated.exposure, 1.7f) ||
        migrated.ambientOcclusion != RenderAmbientOcclusion::HBAO || !migrated.ssgiEnabled ||
        !NearlyEqual(migrated.giBoost, 1.8f) || !migrated.ssrEnabled ||
        migrated.raytracedShadowsEnabled || migrated.raytracedReflectionsEnabled ||
        migrated.raytracedDiffuseEnabled || migrated.surfelGiEnabled)
        return Fail("Gate 6 schema-v2 carrier did not migrate losslessly to Gate 7");

    SceneService scenes;
    auto authored = defaults;
    authored.ambientOcclusion = RenderAmbientOcclusion::RTAO;
    authored.raytracedShadowsEnabled = true;
    authored.raytracedReflectionsEnabled = true;
    authored.raytracedReflectionsRange = 750.0f;
    authored.raytracedReflectionsQuality = RenderQuality::High;
    authored.raytracedDiffuseEnabled = true;
    authored.raytracedDiffuseRange = 45.0f;
    authored.raytracedDiffuseQuality = RenderQuality::Low;
    authored.surfelGiEnabled = true;
    if (!WriteRenderSettings(scenes.GetScene(), authored) ||
        HasRenderSettingsChange(authored, CaptureRenderSettings(scenes.GetScene())))
        return Fail("Gate 7 state did not round-trip through Metadata carrier");

    // Headless environment has no graphics device: hardware-only authored state
    // must remain persisted while the live path safely falls back to disabled.
    auto headless = authored;
    headless.bloomEnabled = false;
    headless.eyeAdaptationEnabled = false;
    headless.antiAliasing = AntiAliasingMode::Off;
    headless.planarReflectionsEnabled = false;
    headless.ssgiEnabled = false;
    headless.surfelGiEnabled = false;
    wi::RenderPath3D nativePath;
    ApplyRenderSettingsToPath(nativePath, headless, false);
    if (!RenderSettingsMatchPath(nativePath, headless) ||
        nativePath.getAO() != wi::RenderPath3D::AO_DISABLED ||
        nativePath.getRaytracedReflectionEnabled() || nativePath.getRaytracedDiffuseEnabled() ||
        wi::renderer::GetRaytracedShadowsEnabled())
        return Fail("Unsupported hardware ray tracing did not fail closed");

    CommandService commands;
    SceneService commandScenes;
    if (!commands.Execute(std::make_unique<SetRenderSettingsCommand>(commandScenes.GetScene(), authored)) ||
        !commands.Undo() || FindRenderSettingsCarrier(commandScenes.GetScene()) != wi::ecs::INVALID_ENTITY ||
        !commands.Redo() || HasRenderSettingsChange(authored, CaptureRenderSettings(commandScenes.GetScene())))
        return Fail("Gate 7 command Undo/Redo failed");

    const auto undoCount = commands.UndoCount();
    if (commands.Execute(std::make_unique<SetRenderSettingsCommand>(commandScenes.GetScene(), authored)) ||
        commands.UndoCount() != undoCount)
        return Fail("Gate 7 no-op edit polluted history");

    std::cout << "Phase 5 Gate 7 render settings tests passed\n";
    return 0;
}
'''
write("Tests/Phase5Gate7RenderSettingsTests.cpp", gate7_test)

cmake = r'''add_executable(RenegadePhase5Gate7RenderSettingsTests
    ${CMAKE_CURRENT_LIST_DIR}/Phase5Gate7RenderSettingsTests.cpp
)

target_link_libraries(RenegadePhase5Gate7RenderSettingsTests
    PRIVATE Renegade::EngineBridge
)

target_compile_options(RenegadePhase5Gate7RenderSettingsTests
    PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(RenegadePhase5Gate7RenderSettingsTests PROPERTIES
    FOLDER "Renegade/Tests"
)

add_dependencies(RenegadeBridgeTests RenegadePhase5Gate7RenderSettingsTests)
add_test(NAME RenegadePhase5Gate7RenderSettingsTests COMMAND RenegadePhase5Gate7RenderSettingsTests)
add_test(
    NAME RenegadePhase5Gate7SourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_SOURCE_DIR}/Tests/Phase5Gate7SourceContract.cmake
)
'''
write("Tests/Phase5Gate7.cmake", cmake)

source_contract = r'''file(READ "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/RenderSettingsService.h" render_header)
file(READ "${RENEGADE_SOURCE_DIR}/EngineBridge/src/RenderSettingsService.cpp" render_source)
file(READ "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.h" studio_header)
file(READ "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.cpp" studio_source)
file(READ "${RENEGADE_SOURCE_DIR}/Studio/src/Phase5Gate5RenderWorkspace.cpp" render_workspace)
file(READ "${RENEGADE_SOURCE_DIR}/Studio/src/Phase5Gate7RayPathTracing.cpp" gate7_studio)

function(require_text haystack needle message_text)
    string(FIND "${${haystack}}" "${needle}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "${message_text}")
    endif()
endfunction()

require_text(render_header "RenderSettingsSchemaVersion = 3" "Gate 7 render schema is not v3")
require_text(render_header "RTAO = 4" "Gate 7 RTAO state missing")
require_text(render_source "schema < 1 || schema > RenderSettingsSchemaVersion" "Gate 7 legacy schema migration guard missing")
require_text(render_source "setRaytracedReflectionsEnabled" "Gate 7 RT reflections native setter missing")
require_text(render_source "setRaytracedDiffuseEnabled" "Gate 7 RT diffuse native setter missing")
require_text(render_source "SetRaytracedShadowsEnabled" "Gate 7 RT shadows native setter missing")
require_text(render_source "SetSurfelGIEnabled" "Gate 7 Surfel GI native setter missing")
require_text(render_source "GraphicsDeviceCapability::RAYTRACING" "Gate 7 hardware capability guard missing")
require_text(studio_header "public wi::RenderPath3D_PathTracing" "Studio does not derive from pinned Wicked path tracer")
require_text(studio_source "RenderPath3D_PathTracing::Update(dt)" "Path trace Update routing missing")
require_text(studio_source "RenderPath3D_PathTracing::Render()" "Path trace Render routing missing")
require_text(studio_source "RenderPath3D_PathTracing::Compose(cmd)" "Path trace Compose routing missing")
require_text(studio_source "RenderPath3D_PathTracing::ResizeBuffers()" "Path trace ResizeBuffers routing missing")
require_text(gate7_studio "setTargetSampleCount" "Path trace target sample hook missing")
require_text(gate7_studio "resetProgress" "Path trace restart hook missing")
require_text(gate7_studio "SetRaytraceBounceCount" "Path trace bounce hook missing")
require_text(gate7_studio "getCurrentSampleCount" "Path trace progress hook missing")
require_text(gate7_studio "isDenoiserAvailable" "Path trace denoiser status hook missing")
require_text(render_workspace "CreateGate7RenderControls();" "Gate 7 RENDER controls are not attached")
require_text(render_workspace "LayoutGate7RenderControls(fieldWidth, y);" "Gate 7 RENDER controls are not laid out")
require_text(render_workspace "renderWorkspacePanel_.SetEnabled(true);" "Gate 5 child-enable regression guard lost")
require_text(render_workspace "renderWorkspacePanel_.SetVisible(false);" "RENDER final hide missing")

string(FIND "${render_workspace}" "CreateGate7RenderControls();" gate7_create)
string(FIND "${render_workspace}" "renderWorkspacePanel_.SetVisible(false);" render_hide)
if(gate7_create EQUAL -1 OR render_hide EQUAL -1 OR render_hide LESS gate7_create)
    message(FATAL_ERROR "Gate 7 controls are hidden before child attachment completes")
endif()

message(STATUS "Phase 5 Gate 7 source contract passed")
'''
write("Tests/Phase5Gate7SourceContract.cmake", source_contract)

replace_once("CMakeLists.txt",
'''    include(Tests/Phase5Gate6.cmake)\n    include(Tests/SceneUiGate2.cmake)''',
'''    include(Tests/Phase5Gate6.cmake)\n    include(Tests/Phase5Gate7.cmake)\n    include(Tests/SceneUiGate2.cmake)''')

print("Gate 7 guarded product patch applied successfully")
