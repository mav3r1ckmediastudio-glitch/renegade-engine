#include <cmath>
#include <iostream>
#include <memory>

#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/MaterialService.h"
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
    using Material = wi::scene::MaterialComponent;

    // Mesh blending is a normal MaterialComponent capability in Wicked. Prove
    // Renegade can author it on standard PBR without silently changing shader.
    SceneService materialScenes;
    auto& materialScene = materialScenes.GetScene();
    const auto materialEntity = wi::ecs::CreateEntity();
    auto& material = materialScene.materials.Create(materialEntity);
    material.shaderType = Material::SHADERTYPE_PBR;

    auto materialState = CaptureMaterial(material);
    materialState.meshBlend = 1.25f;
    CommandService materialCommands;
    if (!materialCommands.Execute(std::make_unique<SetMaterialCommand>(
            materialScene, materialEntity, materialState)))
    {
        return Fail("standard PBR mesh-blend edit did not execute");
    }
    if (material.shaderType != Material::SHADERTYPE_PBR ||
        !NearlyEqual(material.GetMeshBlend(), 1.25f))
    {
        return Fail("mesh blending was incorrectly tied to Terrain Blended shader");
    }
    if (!materialCommands.Undo() || !NearlyEqual(material.GetMeshBlend(), 0.0f) ||
        !materialCommands.Redo() || !NearlyEqual(material.GetMeshBlend(), 1.25f))
    {
        return Fail("material mesh-blend Undo/Redo did not round-trip");
    }

    // Wicked's RenderPath3D default is enabled; Renegade must preserve that for
    // old projects while allowing an authored global off switch.
    const auto defaults = DefaultRenderSettings();
    if (defaults.schemaVersion != RenderSettingsSchemaVersion ||
        RenderSettingsSchemaVersion != 3 || !defaults.meshBlendingEnabled)
    {
        return Fail("Phase 7F changed schema or native mesh-blend default");
    }

    wi::scene::Scene legacyScene;
    const auto legacyCarrier = wi::ecs::CreateEntity();
    legacyScene.names.Create(legacyCarrier) = RenderSettingsCarrierName;
    auto& legacyMetadata = legacyScene.metadatas.Create(legacyCarrier);
    legacyMetadata.int_values.set("renegade.render.schema", RenderSettingsSchemaVersion);
    if (!CaptureRenderSettings(legacyScene).meshBlendingEnabled)
    {
        return Fail("schema-v3 scene without 7F key did not inherit enabled default");
    }

    auto authored = defaults;
    authored.meshBlendingEnabled = false;
    wi::RenderPath3D nativePath;
    ApplyRenderSettingsToPath(nativePath, authored, false);
    if (nativePath.getMeshBlendEnabled() ||
        !RenderSettingsMatchPath(nativePath, authored))
    {
        return Fail("global mesh-blend off did not reach native Wicked RenderPath3D");
    }
    authored.meshBlendingEnabled = true;
    ApplyRenderSettingsToPath(nativePath, authored, false);
    if (!nativePath.getMeshBlendEnabled() ||
        !RenderSettingsMatchPath(nativePath, authored))
    {
        return Fail("global mesh-blend on did not reach native Wicked RenderPath3D");
    }

    // Persist the global state through the same metadata/WISCENE seam used by
    // Studio and Runtime without a render-settings schema bump.
    SceneService renderScenes;
    authored.meshBlendingEnabled = false;
    if (!WriteRenderSettings(renderScenes.GetScene(), authored))
        return Fail("could not persist mesh-blend render state");
    const auto captured = CaptureRenderSettings(renderScenes.GetScene());
    if (captured.meshBlendingEnabled || captured.schemaVersion != 3)
        return Fail("persisted mesh-blend state did not round-trip");

    const auto carrier = FindRenderSettingsCarrier(renderScenes.GetScene());
    wi::Archive snapshot;
    snapshot.SetReadModeAndResetPos(false);
    wi::ecs::EntitySerializer writer;
    renderScenes.GetScene().Entity_Serialize(snapshot, writer, carrier);
    wi::scene::Scene reopened;
    snapshot.SetReadModeAndResetPos(true);
    wi::ecs::EntitySerializer reader;
    reopened.Entity_Serialize(snapshot, reader);
    if (CaptureRenderSettings(reopened).meshBlendingEnabled)
        return Fail("mesh-blend global state did not survive native serialization");

    // Command-backed global authoring must restore the native path on Undo/Redo.
    SceneService commandScenes;
    wi::RenderPath3D commandPath;
    CommandService commands;
    auto disabled = DefaultRenderSettings();
    disabled.meshBlendingEnabled = false;
    auto apply = [&](const RenderSettingsState& state)
    {
        commandPath.setMeshBlendEnabled(state.meshBlendingEnabled);
    };
    if (!commands.Execute(std::make_unique<SetRenderSettingsCommand>(
            commandScenes.GetScene(), disabled, apply)) ||
        commandPath.getMeshBlendEnabled())
    {
        return Fail("command-backed global mesh-blend disable failed");
    }
    if (!commands.Undo() || !commandPath.getMeshBlendEnabled())
        return Fail("global mesh-blend Undo failed");
    if (!commands.Redo() || commandPath.getMeshBlendEnabled())
        return Fail("global mesh-blend Redo failed");

    std::cout << "Phase 7F native mesh blending parity tests passed\n";
    return 0;
}
