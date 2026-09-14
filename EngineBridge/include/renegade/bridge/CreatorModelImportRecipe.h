#pragma once

#include "renegade/bridge/MaterialTextureAssetService.h"

#include <cstdint>
#include <string>
#include <vector>

namespace renegade::bridge
{
    // Classification is authored with the import recipe rather than inferred
    // from a later scene conversion. The default preserves existing recipes.
    enum class CreatorAssetImportKind : std::uint8_t
    {
        Model,
        Character,
    };

    struct CreatorMaterialImportRecipe
    {
        std::uint32_t materialIndex = 0;
        StableId baseColorAssetId;
        StableId normalAssetId;
        StableId surfaceAssetId;
        StableId emissiveAssetId;
        StableId occlusionAssetId;
        bool hasScalarSettings = false;
        float roughness = 0.75f;
        float metalness = 0.0f;
        float reflectance = 0.04f;
        float normalStrength = 1.0f;
        float aoStrength = 1.0f;
        float emissiveStrength = 0.0f;
    };

    struct CreatorAnimationImportRecipe
    {
        std::uint32_t sourceAnimationIndex = 0;
        std::string name;
        float start = 0.0f;
        float end = 0.0f;
        bool enabled = true;
    };

    // External clips are project-governed import provenance. They deliberately
    // identify an action/take, never a transient scene entity or raw ECS ID.
    struct CreatorExternalAnimationImportRecipe
    {
        std::string sourceProjectRelativePath;
        std::uint32_t sourceAnimationIndex = 0;
        // Human-readable source action/take identity retained alongside the
        // numeric source index for deterministic reimport diagnostics. Older
        // recipes may leave this empty and remain valid.
        std::string sourceActionName;
        std::string name;
        float start = 0.0f;
        float end = 0.0f;
        bool enabled = true;
    };

    struct CreatorModelTransformRecipe
    {
        bool authored = false;
        float positionX = 0.0f;
        float positionY = 0.0f;
        float positionZ = 0.0f;
        float rotationXDegrees = 0.0f;
        float rotationYDegrees = 0.0f;
        float rotationZDegrees = 0.0f;
        float scaleX = 1.0f;
        float scaleY = 1.0f;
        float scaleZ = 1.0f;
    };

    struct CreatorModelImportRecipe
    {
        CreatorAssetImportKind assetKind = CreatorAssetImportKind::Model;
        CreatorModelTransformRecipe transform;
        std::vector<CreatorMaterialImportRecipe> materials;
        std::vector<CreatorAnimationImportRecipe> animations;
        std::vector<CreatorExternalAnimationImportRecipe> externalAnimations;
    };

    inline constexpr const char* CreatorAuthoredTransformRootName =
        "__renegade_creator_authored_transform";

    [[nodiscard]] bool HasCreatorAuthoredTransform(
        const wi::scene::Scene& scene) noexcept;

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
