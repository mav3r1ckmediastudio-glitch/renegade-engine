#pragma once

#include "renegade/bridge/CreatorModelImportRecipe.h"

#include <cstdint>
#include <string>
#include <vector>

namespace renegade::bridge
{
    struct CreatorTextureSourceChoice
    {
        // false = use imported binding / suffix auto-detection.
        // true + empty path = explicitly remove/disable this texture source.
        // true + path = use exactly this creator-selected source file.
        bool overridden = false;
        std::string path;
    };

    struct CreatorMaterialSourceOverride
    {
        std::uint32_t materialIndex = 0;
        CreatorTextureSourceChoice baseColor;
        CreatorTextureSourceChoice normal;
        CreatorTextureSourceChoice surface;
        CreatorTextureSourceChoice roughness;
        CreatorTextureSourceChoice metalness;
        CreatorTextureSourceChoice occlusion;
        CreatorTextureSourceChoice emissive;
    };

    struct CreatorModelMaterialDetectionResult
    {
        bool succeeded = false;
        // Resolved creator-visible source choices. A detected path is emitted
        // as an explicit choice so Studio can display/preview exactly what the
        // final commit will use without creating any governed project assets.
        std::vector<CreatorMaterialSourceOverride> materials;
        std::string error;
    };

    struct CreatorModelMaterialPreparationRequest
    {
        const wi::scene::Scene* preparedScene = nullptr;
        std::string projectRoot;
        StableId projectId;
        std::string modelSourcePath;
        std::vector<CreatorMaterialSourceOverride> overrides;
    };

    struct CreatorModelMaterialPreparationResult
    {
        bool succeeded = false;
        CreatorModelImportRecipe recipe;
        std::size_t governedTextures = 0;
        std::size_t generatedSurfaceMaps = 0;
        std::string error;
    };

    // Read-only discovery for the importer preview. Existing model bindings are
    // preferred, then Renegade/MAX-style filename suffixes are searched:
    // _color, _normal, _surface, _roughness, _metalness, _ao, _emissive.
    // This never writes SourceAssets, Content, the registry, or generated maps.
    [[nodiscard]] CreatorModelMaterialDetectionResult DetectCreatorModelMaterials(
        const wi::scene::Scene& preparedScene,
        const std::string& modelSourcePath);

    // Converts a prepared Wicked model's creator-facing material references
    // into governed LP08 texture assets and a persistent LP07 model recipe.
    // Imported bindings win by default, then familiar suffixes are searched.
    // Creator overrides always win, including an explicit empty override which
    // removes that slot. A supplied Surface map is preferred; otherwise
    // roughness/metalness/AO are packed into Wicked's native surface layout.
    // This function mutates project asset state and therefore belongs only to
    // the final creator acceptance/commit phase, never the temporary preview.
    [[nodiscard]] CreatorModelMaterialPreparationResult PrepareCreatorModelMaterials(
        const CreatorModelMaterialPreparationRequest& request);
}
