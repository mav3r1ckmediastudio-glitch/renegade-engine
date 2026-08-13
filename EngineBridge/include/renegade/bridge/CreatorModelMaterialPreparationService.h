#pragma once

#include "renegade/bridge/CreatorModelImportRecipe.h"

#include <string>

namespace renegade::bridge
{
    struct CreatorModelMaterialPreparationResult
    {
        bool succeeded = false;
        CreatorModelImportRecipe recipe;
        std::size_t governedTextures = 0;
        std::size_t generatedSurfaceMaps = 0;
        std::string error;
    };

    // Converts a prepared Wicked model's creator-facing material references
    // into governed LP08 texture assets and a persistent LP07 model recipe.
    // Embedded/declared bindings win. Missing bindings can be discovered from
    // familiar suffixes beside the model source. A supplied *_surface map is
    // preferred; otherwise roughness/metalness/AO are packed into Wicked's
    // native surface layout before the generated map is governed.
    [[nodiscard]] CreatorModelMaterialPreparationResult PrepareCreatorModelMaterials(
        const wi::scene::Scene& preparedScene,
        const std::string& projectRoot,
        const StableId& projectId,
        const std::string& modelSourcePath);
}
