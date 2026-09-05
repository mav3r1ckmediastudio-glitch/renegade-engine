#include <cmath>
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
