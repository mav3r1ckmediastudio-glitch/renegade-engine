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
        defaults.tonemap != RenderTonemap::ACES ||
        !NearlyEqual(defaults.exposure, 1.0f) ||
        !defaults.colorGradingEnabled || !defaults.bloomEnabled || defaults.eyeAdaptationEnabled ||
        defaults.antiAliasing != AntiAliasingMode::Off ||
        defaults.motionBlurEnabled || !defaults.depthOfFieldEnabled ||
        defaults.sharpenEnabled || defaults.chromaticAberrationEnabled ||
        !defaults.ditherEnabled)
    {
        return Fail("Gate 5 defaults do not match the documented baseline");
    }

    auto malformed = defaults;
    malformed.schemaVersion = 99;
    malformed.tonemap = static_cast<RenderTonemap>(99);
    malformed.antiAliasing = static_cast<AntiAliasingMode>(99);
    malformed.exposure = std::numeric_limits<float>::quiet_NaN();
    malformed.brightness = -100.0f;
    malformed.contrast = 100.0f;
    malformed.hdrCalibration = 0.0f;
    malformed.chromaticAberrationAmount = 1000.0f;
    const auto safe = SanitizeRenderSettings(malformed);
    if (safe.schemaVersion != RenderSettingsSchemaVersion ||
        safe.tonemap != RenderTonemap::ACES ||
        safe.antiAliasing != AntiAliasingMode::Off ||
        !NearlyEqual(safe.exposure, defaults.exposure) ||
        !NearlyEqual(safe.brightness, -2.0f) ||
        !NearlyEqual(safe.contrast, 4.0f) ||
        !NearlyEqual(safe.hdrCalibration, 0.01f) ||
        !NearlyEqual(safe.chromaticAberrationAmount, 64.0f))
    {
        return Fail("Gate 5 render state sanitization is not deterministic");
    }

    SceneService scenes;
    if (FindRenderSettingsCarrier(scenes.GetScene()) != wi::ecs::INVALID_ENTITY ||
        HasRenderSettingsChange(defaults, CaptureRenderSettings(scenes.GetScene())))
    {
        return Fail("fresh scene did not resolve to deterministic Gate 5 defaults");
    }

    auto authored = defaults;
    authored.tonemap = RenderTonemap::Uchimura;
    authored.exposure = 1.75f;
    authored.brightness = 0.12f;
    authored.contrast = 1.25f;
    authored.saturation = 0.85f;
    authored.hdrCalibration = 1.5f;
    authored.colorGradingEnabled = false;
    authored.bloomEnabled = false;
    authored.bloomThreshold = 2.4f;
    authored.eyeAdaptationEnabled = true;
    authored.eyeAdaptationKey = 0.2f;
    authored.eyeAdaptationRate = 2.0f;
    authored.antiAliasing = AntiAliasingMode::MSAA4X;
    authored.depthOfFieldEnabled = false;
    authored.depthOfFieldStrength = 18.0f;
    authored.motionBlurEnabled = true;
    authored.motionBlurStrength = 42.0f;
    authored.sharpenEnabled = true;
    authored.sharpenAmount = 0.7f;
    authored.chromaticAberrationEnabled = true;
    authored.chromaticAberrationAmount = 3.5f;
    authored.ditherEnabled = false;

    if (!WriteRenderSettings(scenes.GetScene(), authored))
        return Fail("could not persist authored Gate 5 render settings");

    const auto carrier = FindRenderSettingsCarrier(scenes.GetScene());
    const auto captured = CaptureRenderSettings(scenes.GetScene());
    if (carrier == wi::ecs::INVALID_ENTITY ||
        !HasRenderSettingsChange(defaults, captured) ||
        HasRenderSettingsChange(authored, captured))
    {
        return Fail("Gate 5 persisted state did not round-trip through Metadata");
    }

    // The serialized carrier is an implementation detail, not a creator-facing
    // Hierarchy entity. SceneService already filters the reserved internal prefix.
    if (!scenes.ListEntities().empty())
        return Fail("Gate 5 render-settings carrier leaked into the creator hierarchy");

    // Prove the exact native Metadata carrier survives Wicked entity archive
    // serialization, which is the same component serializer used by WISCENE.
    wi::Archive snapshot;
    snapshot.SetReadModeAndResetPos(false);
    wi::ecs::EntitySerializer writeSerializer;
    scenes.GetScene().Entity_Serialize(snapshot, writeSerializer, carrier);

    wi::scene::Scene restoredScene;
    snapshot.SetReadModeAndResetPos(true);
    wi::ecs::EntitySerializer readSerializer;
    restoredScene.Entity_Serialize(snapshot, readSerializer);
    const auto restored = CaptureRenderSettings(restoredScene);
    if (HasRenderSettingsChange(authored, restored))
        return Fail("Gate 5 Metadata carrier did not survive native entity serialization");

    // Fresh-scene command: execution creates the carrier, Undo removes it so
    // the old scene remains byte-semantically free of Gate 5 state, and Redo
    // recreates it. The callback is the renderer-independent Studio seam.
    SceneService commandScenes;
    CommandService commands;
    RenderSettingsState applied = defaults;
    int applyCount = 0;
    auto apply = [&](const RenderSettingsState& state)
    {
        applied = state;
        ++applyCount;
    };

    if (!commands.Execute(std::make_unique<SetRenderSettingsCommand>(
            commandScenes.GetScene(), authored, apply)) ||
        !commands.IsDirty() || applyCount != 1 ||
        HasRenderSettingsChange(authored, applied) ||
        FindRenderSettingsCarrier(commandScenes.GetScene()) == wi::ecs::INVALID_ENTITY)
    {
        return Fail("Gate 5 render-settings command did not execute cleanly");
    }

    if (!commands.Undo() || applyCount != 2 ||
        HasRenderSettingsChange(defaults, applied) ||
        FindRenderSettingsCarrier(commandScenes.GetScene()) != wi::ecs::INVALID_ENTITY)
    {
        return Fail("Gate 5 Undo did not restore a carrier-free default scene");
    }

    if (!commands.Redo() || applyCount != 3 ||
        HasRenderSettingsChange(authored, CaptureRenderSettings(commandScenes.GetScene())))
    {
        return Fail("Gate 5 Redo did not restore the authored render state");
    }

    const auto undoCount = commands.UndoCount();
    if (commands.Execute(std::make_unique<SetRenderSettingsCommand>(
            commandScenes.GetScene(), authored, apply)) ||
        commands.UndoCount() != undoCount)
    {
        return Fail("Gate 5 no-op edit polluted command history");
    }

    // Existing-carrier command Undo must preserve the carrier and restore its
    // exact prior values rather than removing it.
    SceneService existingScenes;
    auto existing = defaults;
    existing.tonemap = RenderTonemap::Reinhard;
    existing.antiAliasing = AntiAliasingMode::TAA;
    existing.exposure = 0.65f;
    if (!WriteRenderSettings(existingScenes.GetScene(), existing))
        return Fail("could not seed existing Gate 5 state");

    CommandService existingCommands;
    if (!existingCommands.Execute(std::make_unique<SetRenderSettingsCommand>(
            existingScenes.GetScene(), authored)) ||
        !existingCommands.Undo() ||
        FindRenderSettingsCarrier(existingScenes.GetScene()) == wi::ecs::INVALID_ENTITY ||
        HasRenderSettingsChange(existing, CaptureRenderSettings(existingScenes.GetScene())))
    {
        return Fail("Gate 5 Undo did not preserve pre-existing render state");
    }

    std::cout << "Phase 5 Gate 5 render settings tests passed\n";
    return 0;
}
