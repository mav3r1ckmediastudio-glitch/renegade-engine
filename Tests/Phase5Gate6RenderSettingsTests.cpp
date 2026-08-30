#include <cmath>
#include <iostream>
#include <limits>
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

    bool NearlyEqual(const float left, const float right)
    {
        return std::abs(left - right) <= 0.0001f;
    }
}

int main()
{
    using namespace renegade::bridge;

    const auto defaults = DefaultRenderSettings();
    if (defaults.schemaVersion != RenderSettingsSchemaVersion ||
        defaults.ambientOcclusion != RenderAmbientOcclusion::Off ||
        !NearlyEqual(defaults.ambientOcclusionPower, 1.0f) ||
        !NearlyEqual(defaults.ambientOcclusionRange, 1.0f) ||
        defaults.ambientOcclusionSampleCount != 16 ||
        defaults.ssgiEnabled ||
        !NearlyEqual(defaults.ssgiDepthRejection, 8.0f) ||
        !NearlyEqual(defaults.giBoost, 1.0f) ||
        !defaults.planarReflectionsEnabled ||
        !NearlyEqual(defaults.planarReflectionResolutionScale, 0.25f) ||
        defaults.planarReflectionMsaaSampleCount != 4 ||
        defaults.ssrEnabled ||
        defaults.ssrQuality != RenderQuality::Medium ||
        !NearlyEqual(defaults.reflectionRoughnessCutoff, 0.6f))
    {
        return Fail("Gate 6 defaults do not match the pinned Wicked baseline");
    }

    auto malformed = defaults;
    malformed.ambientOcclusion = static_cast<RenderAmbientOcclusion>(99);
    malformed.ambientOcclusionPower = -100.0f;
    malformed.ambientOcclusionRange = 1000.0f;
    malformed.ambientOcclusionSampleCount = 99;
    malformed.ssgiDepthRejection = 0.001f;
    malformed.giBoost = 100.0f;
    malformed.planarReflectionResolutionScale = 100.0f;
    malformed.planarReflectionMsaaSampleCount = 3;
    malformed.ssrQuality = static_cast<RenderQuality>(99);
    malformed.reflectionRoughnessCutoff = -10.0f;
    const auto safe = SanitizeRenderSettings(malformed);
    if (safe.ambientOcclusion != RenderAmbientOcclusion::Off ||
        !NearlyEqual(safe.ambientOcclusionPower, 0.25f) ||
        !NearlyEqual(safe.ambientOcclusionRange, 100.0f) ||
        safe.ambientOcclusionSampleCount != 16 ||
        !NearlyEqual(safe.ssgiDepthRejection, 0.1f) ||
        !NearlyEqual(safe.giBoost, 10.0f) ||
        !NearlyEqual(safe.planarReflectionResolutionScale, 2.0f) ||
        safe.planarReflectionMsaaSampleCount != 4 ||
        safe.ssrQuality != RenderQuality::Medium ||
        !NearlyEqual(safe.reflectionRoughnessCutoff, 0.0f))
    {
        return Fail("Gate 6 sanitization is not deterministic");
    }

    // A Gate 5 schema-v1 carrier must migrate without dropping any existing
    // image-quality values. Gate 6 fields resolve to their documented defaults.
    SceneService legacyScenes;
    auto& legacyScene = legacyScenes.GetScene();
    const auto carrier = wi::ecs::CreateEntity();
    legacyScene.names.Create(carrier) = RenderSettingsCarrierName;
    auto& metadata = legacyScene.metadatas.Create(carrier);
    metadata.int_values.set("renegade.render.schema", 1);
    metadata.int_values.set(
        "renegade.render.tonemap",
        static_cast<int>(RenderTonemap::Uchimura));
    metadata.float_values.set("renegade.render.exposure", 1.75f);
    metadata.float_values.set("renegade.render.saturation", 0.55f);
    metadata.bool_values.set("renegade.render.bloom.enabled", false);
    metadata.int_values.set(
        "renegade.render.anti_aliasing",
        static_cast<int>(AntiAliasingMode::TAA));
    metadata.bool_values.set("renegade.render.dither.enabled", false);

    const auto migrated = CaptureRenderSettings(legacyScene);
    if (migrated.schemaVersion != RenderSettingsSchemaVersion ||
        migrated.tonemap != RenderTonemap::Uchimura ||
        !NearlyEqual(migrated.exposure, 1.75f) ||
        !NearlyEqual(migrated.saturation, 0.55f) ||
        migrated.bloomEnabled ||
        migrated.antiAliasing != AntiAliasingMode::TAA ||
        migrated.ditherEnabled ||
        migrated.ambientOcclusion != defaults.ambientOcclusion ||
        migrated.ssgiEnabled != defaults.ssgiEnabled ||
        migrated.ssrEnabled != defaults.ssrEnabled ||
        migrated.planarReflectionsEnabled != defaults.planarReflectionsEnabled)
    {
        return Fail("Gate 5 schema-v1 carrier did not migrate losslessly to Gate 6");
    }

    SceneService scenes;
    auto authored = defaults;
    authored.ambientOcclusion = RenderAmbientOcclusion::MSAO;
    authored.ambientOcclusionPower = 3.0f;
    authored.ambientOcclusionRange = 12.0f;
    authored.ambientOcclusionSampleCount = 7;
    authored.ssgiEnabled = true;
    authored.ssgiDepthRejection = 14.0f;
    authored.giBoost = 1.8f;
    authored.planarReflectionsEnabled = false;
    authored.planarReflectionResolutionScale = 0.75f;
    authored.planarReflectionMsaaSampleCount = 8;
    authored.ssrEnabled = true;
    authored.ssrQuality = RenderQuality::High;
    authored.reflectionRoughnessCutoff = 0.35f;

    if (!WriteRenderSettings(scenes.GetScene(), authored) ||
        HasRenderSettingsChange(authored, CaptureRenderSettings(scenes.GetScene())))
    {
        return Fail("Gate 6 state did not round-trip through the shared Metadata carrier");
    }

    // Keep resource-owning effects disabled for the headless native readback.
    // Parameters still prove the exact native fields are reached; enabled GPU
    // modes are reserved for Studio/Runtime owner acceptance.
    auto headless = authored;
    headless.ambientOcclusion = RenderAmbientOcclusion::Off;
    headless.ssgiEnabled = false;
    headless.ssrEnabled = false;
    headless.planarReflectionsEnabled = false;
    headless.bloomEnabled = false;
    headless.eyeAdaptationEnabled = false;
    headless.antiAliasing = AntiAliasingMode::Off;
    wi::RenderPath3D nativePath;
    ApplyRenderSettingsToPath(nativePath, headless, false);
    if (!RenderSettingsMatchPath(nativePath, headless))
        return Fail("Gate 6 non-resource native fields did not reach Wicked RenderPath");
    wi::renderer::SetGIBoost(1.0f);

    CommandService commands;
    SceneService commandScenes;
    if (!commands.Execute(std::make_unique<SetRenderSettingsCommand>(
            commandScenes.GetScene(), authored)) ||
        !commands.Undo() ||
        FindRenderSettingsCarrier(commandScenes.GetScene()) != wi::ecs::INVALID_ENTITY ||
        !commands.Redo() ||
        HasRenderSettingsChange(
            authored,
            CaptureRenderSettings(commandScenes.GetScene())))
    {
        return Fail("Gate 6 command Undo/Redo did not preserve extended render state");
    }

    const auto undoCount = commands.UndoCount();
    if (commands.Execute(std::make_unique<SetRenderSettingsCommand>(
            commandScenes.GetScene(), authored)) ||
        commands.UndoCount() != undoCount)
    {
        return Fail("Gate 6 no-op edit polluted command history");
    }

    std::cout << "Phase 5 Gate 6 render settings tests passed\n";
    return 0;
}
