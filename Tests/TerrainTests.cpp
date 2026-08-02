#include <cmath>
#include <iostream>
#include <memory>

#include "renegade/bridge/TerrainService.h"

namespace
{
    class PreviewCommand final : public renegade::bridge::ICommand
    {
    public:
        explicit PreviewCommand(int& value) : value_(&value) {}
        bool Execute() override
        {
            *value_ = 2;
            return true;
        }
        void Undo() override { *value_ = 1; }
    private:
        int* value_ = nullptr;
    };

    bool NearlyEqual(float left, float right)
    {
        return std::abs(left - right) < 0.0001f;
    }

    int Fail(const char* message)
    {
        std::cerr << "FAIL: " << message << '\n';
        return 1;
    }
}

int main()
{
    wi::scene::Scene scene;
    const auto entity = wi::ecs::CreateEntity();
    auto& terrain = scene.terrains.Create(entity);

    const auto native = renegade::bridge::CaptureTerrain(terrain);
    if (!native.centerToCamera || !native.removeDistantChunks ||
        native.physics || native.visibleChunkRadius != 12 ||
        !NearlyEqual(native.minimumHeight, -60.0f) ||
        !NearlyEqual(native.maximumHeight, 380.0f))
    {
        return Fail("native terrain defaults were not captured");
    }

    const renegade::bridge::TerrainState standard;
    renegade::bridge::CommandService commands;
    if (!commands.Execute(
            std::make_unique<renegade::bridge::SetTerrainCommand>(
                scene,
                entity,
                standard)))
    {
        return Fail("standard terrain state did not execute");
    }

    const auto applied = renegade::bridge::CaptureTerrain(terrain);
    if (applied.centerToCamera || applied.removeDistantChunks ||
        !applied.physics || applied.visibleChunkRadius != 6 ||
        !NearlyEqual(applied.minimumHeight, -20.0f) ||
        !NearlyEqual(applied.maximumHeight, 120.0f) ||
        !NearlyEqual(applied.chunkScale, 2.0f))
    {
        return Fail("standard terrain state did not apply");
    }
    if (!commands.Undo() || !terrain.IsCenterToCamEnabled() ||
        !NearlyEqual(terrain.topLevel, 380.0f))
    {
        return Fail("terrain Undo did not restore native state");
    }
    if (!commands.Redo() || terrain.IsCenterToCamEnabled() ||
        !NearlyEqual(terrain.topLevel, 120.0f))
    {
        return Fail("terrain Redo did not restore authored state");
    }

    auto unsafe = renegade::bridge::CaptureTerrain(terrain);
    unsafe.visibleChunkRadius = 999;
    unsafe.physicsChunkRadius = -10;
    unsafe.chunkScale = 0.0f;
    unsafe.minimumHeight = 2500.0f;
    unsafe.maximumHeight = -2500.0f;
    unsafe.lowAltitudeBlend = -1.0f;
    unsafe.baseBlend = 2.0f;
    unsafe.slopeBlend = 99.0f;
    unsafe.lodBias = 20.0f;
    renegade::bridge::ApplyTerrain(terrain, unsafe, false);
    const auto safe = renegade::bridge::CaptureTerrain(terrain);
    if (safe.visibleChunkRadius != 16 || safe.physicsChunkRadius != 0 ||
        !NearlyEqual(safe.chunkScale, 0.25f) ||
        !NearlyEqual(safe.minimumHeight, 1999.0f) ||
        !NearlyEqual(safe.maximumHeight, 2000.0f) ||
        !NearlyEqual(safe.lowAltitudeBlend, 0.0f) ||
        !NearlyEqual(safe.baseBlend, 1.0f) ||
        !NearlyEqual(safe.slopeBlend, 1.0f) ||
        !NearlyEqual(safe.lodBias, 4.0f))
    {
        return Fail("terrain safety bounds did not apply");
    }

    renegade::bridge::SetTerrainCommand noOp(scene, entity, safe, safe);
    if (noOp.Execute())
    {
        return Fail("identical terrain state polluted Undo history");
    }

    renegade::bridge::TerrainMaterialState material;
    renegade::bridge::SetTerrainTextureScale(material, 8.0f);
    if (!NearlyEqual(material.slots[0].texMulAdd.x, 0.25f) ||
        !NearlyEqual(
            renegade::bridge::MakeTerrainMaterialPreset(
                renegade::bridge::TerrainMaterialPreset::CoarseGrass),
            12.0f))
    {
        return Fail("terrain material scale was not mapped to packed UVs");
    }

    int previewValue = 2;
    renegade::bridge::CommandService previewCommands;
    if (!previewCommands.RecordExecuted(
            std::make_unique<PreviewCommand>(previewValue)) ||
        previewValue != 2 ||
        !previewCommands.Undo() || previewValue != 1 ||
        !previewCommands.Redo() || previewValue != 2)
    {
        return Fail("completed preview was not retained for Undo/Redo");
    }

    std::cout << "PASS: standard terrain state, material scale, preview history and Undo/Redo\n";
    return 0;
}
