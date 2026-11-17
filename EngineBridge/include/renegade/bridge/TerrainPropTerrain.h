#pragma once
#include "renegade/bridge/TerrainPropAuthoring.h"
#include <WickedEngine.h>

namespace renegade::bridge
{
    // Capture once at a scene-safe point. Returned closure owns copied grids
    // and can be sampled on a worker without touching the scene or GPU.
    struct TerrainPropTerrainSnapshot
    {
        TerrainPropBounds bounds;
        TerrainPropHeightSampler sample;
    };
    bool CaptureTerrainPropTerrain(const wi::scene::Scene& scene,
        wi::ecs::Entity terrainEntity, TerrainPropTerrainSnapshot& output,
        std::string& error);
}
