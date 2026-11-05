#pragma once
#include "wiScene.h"

namespace renegade::studio
{
    // Authoring must target the same terrain presented by the workspace,
    // even while a river node or Player Start remains selected for preview.
    inline wi::ecs::Entity TerrainInspectorEntity(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity selection,
        const bool terrainWorkspace) noexcept
    {
        if (!terrainWorkspace) return selection;
        return scene.terrains.GetCount() > 0
            ? scene.terrains.GetEntity(0) : wi::ecs::INVALID_ENTITY;
    }
}
