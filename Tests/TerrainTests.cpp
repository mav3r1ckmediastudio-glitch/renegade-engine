#include <cmath>
#include <iostream>
#include <memory>

#include "renegade/bridge/TerrainService.h"

namespace
{
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

    auto highlands = renegade::bridge::MakeTerrainPreset(
        native,
        renegade::bridge::TerrainPreset::Highlands);
    renegade::bridge::CommandService commands;
    if (!commands.Execute(
            std::make_unique<renegade::bridge::SetTerrainCommand>(
                scene,
                entity,
                highlands)))
    {
        return Fail("Highlands preset did not execute");
    }

    const auto applied = renegade::bridge::CaptureTerrain(terrain);
    if (applied.centerToCamera || applied.removeDistantChunks ||
        !applied.physics || !NearlyEqual(applied.maximumHeight, 240.0f) ||
        !NearlyEqual(applied.chunkScale, 3.0f))
    {
        return Fail("Highlands terrain state did not apply");
    }
    if (!commands.Undo() || !terrain.IsCenterToCamEnabled() ||
        !NearlyEqual(terrain.topLevel, 380.0f))
    {
        return Fail("terrain Undo did not restore native state");
    }
    if (!commands.Redo() || terrain.IsCenterToCamEnabled() ||
        !NearlyEqual(terrain.topLevel, 240.0f))
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

    const auto flat = renegade::bridge::MakeTerrainPreset(
        safe,
        renegade::bridge::TerrainPreset::FlatWorld);
    const auto island = renegade::bridge::MakeTerrainPreset(
        safe,
        renegade::bridge::TerrainPreset::Island);
    const auto coast = renegade::bridge::MakeTerrainPreset(
        safe,
        renegade::bridge::TerrainPreset::Coastline);
    if (!NearlyEqual(flat.maximumHeight, 2.0f) ||
        !NearlyEqual(island.minimumHeight, -35.0f) ||
        !NearlyEqual(coast.maximumHeight, 70.0f))
    {
        return Fail("restrained terrain presets were not distinct");
    }

    std::cout << "PASS: native terrain state, presets, safety and Undo/Redo\n";
    return 0;
}
