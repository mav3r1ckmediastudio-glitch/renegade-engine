#pragma once

#include "renegade/bridge/MaterialTextureAssetService.h"

#include <cstdint>
#include <string>
#include <vector>

namespace renegade::bridge
{
    struct CreatorMaterialImportRecipe
    {
        std::uint32_t materialIndex = 0;
        StableId baseColorAssetId;
        StableId normalAssetId;
        StableId surfaceAssetId;
        StableId emissiveAssetId;
        StableId occlusionAssetId;
    };

    struct CreatorAnimationImportRecipe
    {
        std::uint32_t sourceAnimationIndex = 0;
        std::string name;
        float start = 0.0f;
        float end = 0.0f;
        bool enabled = true;
    };

    struct CreatorModelImportRecipe
    {
        std::vector<CreatorMaterialImportRecipe> materials;
        std::vector<CreatorAnimationImportRecipe> animations;
    };

    // The public settings string is the canonical JSON object stored under the
    // reusable-model recipe's `options` member. Empty arrays may be omitted.
    [[nodiscard]] bool ParseCreatorModelImportOptions(
        const std::string& optionsJson,
        CreatorModelImportRecipe& recipe,
        std::string& error);

    [[nodiscard]] bool SerializeCreatorModelImportOptions(
        const CreatorModelImportRecipe& recipe,
        std::string& optionsJson,
        std::string& error);

    // Applies creator choices to Wicked's isolated converted scene before the
    // WISCENE payload is serialized. Governed texture IDs are resolved through
    // LP08 and written as durable material metadata; animation entries are an
    // authoritative clip list when present.
    [[nodiscard]] bool ApplyCreatorModelImportRecipe(
        wi::scene::Scene& scene,
        const std::string& projectRoot,
        const StableId& projectId,
        const CreatorModelImportRecipe& recipe,
        std::string& error);
}
