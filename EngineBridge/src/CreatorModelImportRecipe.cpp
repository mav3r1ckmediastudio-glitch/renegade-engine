#include "renegade/bridge/CreatorModelImportRecipe.h"
#include "renegade/bridge/HumanoidRetargetService.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <set>
#include <utility>

#include "json.hpp"

namespace renegade::bridge
{
    namespace
    {
        namespace fs = std::filesystem;

        bool ReadOptionalStableId(
            const nlohmann::json& object,
            const char* key,
            StableId& value,
            std::string& error)
        {
            value.clear();
            if (!object.contains(key))
                return true;
            if (!object.at(key).is_string())
            {
                error = std::string("Creator model recipe '") + key +
                    "' must be a stable-ID string.";
                return false;
            }
            value = object.at(key).get<std::string>();
            if (!value.empty() && !IsValidStableId(value))
            {
                error = std::string("Creator model recipe '") + key +
                    "' is not a valid stable asset ID.";
                return false;
            }
            return true;
        }

        void WriteOptionalStableId(
            nlohmann::json& object,
            const char* key,
            const StableId& value)
        {
            if (!value.empty())
                object[key] = value;
        }

        bool ReadScalar(
            const nlohmann::json& object,
            const char* key,
            float& value,
            const float minimum,
            const float maximum,
            std::string& error)
        {
            if (!object.contains(key))
                return true;
            if (!object.at(key).is_number())
            {
                error = std::string("Creator material recipe '") + key +
                    "' must be numeric.";
                return false;
            }
            const float candidate = object.at(key).get<float>();
            if (!std::isfinite(candidate) || candidate < minimum || candidate > maximum)
            {
                error = std::string("Creator material recipe '") + key +
                    "' is outside its supported range.";
                return false;
            }
            value = candidate;
            return true;
        }

        bool ReadVector3(
            const nlohmann::json& object,
            const char* key,
            float& x,
            float& y,
            float& z,
            const float minimum,
            const float maximum,
            std::string& error)
        {
            if (!object.contains(key) || !object.at(key).is_array() ||
                object.at(key).size() != 3)
            {
                error = std::string("Creator model transform '") + key +
                    "' must be a three-number array.";
                return false;
            }
            const auto& values = object.at(key);
            if (!values[0].is_number() || !values[1].is_number() ||
                !values[2].is_number())
            {
                error = std::string("Creator model transform '") + key +
                    "' must contain only numbers.";
                return false;
            }
            x = values[0].get<float>();
            y = values[1].get<float>();
            z = values[2].get<float>();
            if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
                x < minimum || x > maximum || y < minimum || y > maximum ||
                z < minimum || z > maximum)
            {
                error = std::string("Creator model transform '") + key +
                    "' is outside its supported range.";
                return false;
            }
            return true;
        }

        bool ApplyTexture(
            wi::scene::Scene& scene,
            const wi::ecs::Entity materialEntity,
            const MaterialTextureSlot slot,
            const StableId& assetId,
            const std::string& projectRoot,
            const StableId& projectId,
            std::string& error)
        {
            if (assetId.empty())
                return true;
            PreparedMaterialTextureAsset prepared;
            if (!PrepareMaterialTextureAsset(
                    projectRoot, projectId, assetId, prepared, error))
                return false;
            return ApplyPreparedMaterialTextureAsset(
                scene, materialEntity, slot, prepared, {}, error);
        }

        struct AnimationSnapshot
        {
            wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
            wi::scene::AnimationComponent animation;
            std::string name;
        };

        struct ExternalAnimationSourceGroup
        {
            std::string projectRelativePath;
            std::vector<const CreatorExternalAnimationImportRecipe*> clips;
        };

        bool IsWithinPath(const fs::path& candidate, const fs::path& root)
        {
            auto candidatePart = candidate.begin();
            for (auto rootPart = root.begin(); rootPart != root.end();
                ++rootPart, ++candidatePart)
            {
                if (candidatePart == candidate.end() || *candidatePart != *rootPart)
                    return false;
            }
            return true;
        }

        bool ResolveExternalAnimationSource(
            const std::string& projectRoot,
            const std::string& projectRelativePath,
            std::string& sourcePath,
            std::string& error)
        {
            sourcePath.clear();
            if (projectRoot.empty())
            {
                error = "Character external animation retarget requires an active project root.";
                return false;
            }
            if (projectRelativePath.empty() ||
                projectRelativePath.find('\\') != std::string::npos)
            {
                error = "Character external animation provenance is not a canonical project path.";
                return false;
            }

            const fs::path relative = fs::u8path(projectRelativePath);
            if (relative.is_absolute() || relative.has_root_name() ||
                relative.lexically_normal().generic_u8string() != projectRelativePath)
            {
                error = "Character external animation provenance is not a canonical project path.";
                return false;
            }

            std::vector<std::string> parts;
            for (const auto& part : relative)
                parts.push_back(part.generic_u8string());
            if (parts.size() < 4 || parts[0] != "SourceAssets" ||
                parts[1] != "Animations" || parts[2] != "Snapshots")
            {
                error =
                    "Character external animation source is outside the governed SourceAssets/Animations/Snapshots tree.";
                return false;
            }

            std::error_code ec;
            const fs::path absoluteRoot = fs::absolute(fs::u8path(projectRoot), ec);
            if (ec)
            {
                error = "Character animation project root could not be resolved: " + ec.message();
                return false;
            }
            const fs::path root = fs::weakly_canonical(absoluteRoot, ec);
            if (ec || root.empty() || !fs::is_directory(root, ec) || ec)
            {
                error = "Character animation project root is unavailable.";
                return false;
            }

            const fs::path candidate = fs::weakly_canonical(root / relative, ec);
            if (ec || candidate.empty() || !fs::is_regular_file(candidate, ec) || ec ||
                !IsWithinPath(candidate, root))
            {
                error =
                    "Retained Character animation source is missing or resolves outside the project: " +
                    projectRelativePath;
                return false;
            }

            sourcePath = candidate.generic_u8string();
            error.clear();
            return true;
        }

        bool PrepareCharacterHumanoid(
            wi::scene::Scene& scene,
            const bool mappingRequired,
            wi::ecs::Entity& humanoidEntity,
            std::string& error)
        {
            humanoidEntity = wi::ecs::INVALID_ENTITY;

            std::vector<wi::ecs::Entity> validHumanoids;
            for (std::size_t index = 0; index < scene.humanoids.GetCount(); ++index)
            {
                const auto entity = scene.humanoids.GetEntity(index);
                const auto* humanoid = scene.humanoids.GetComponent(entity);
                if (humanoid != nullptr && humanoid->IsValid())
                    validHumanoids.push_back(entity);
            }

            if (validHumanoids.size() == 1)
            {
                humanoidEntity = validHumanoids.front();
                error.clear();
                return true;
            }
            if (validHumanoids.size() > 1)
            {
                if (!mappingRequired)
                {
                    error.clear();
                    return true;
                }
                error =
                    "Character import contains multiple valid humanoid rigs, so external animations cannot choose a deterministic retarget destination. "
                    "Keep one playable humanoid in the Character source or split the rigs into separate Character Assets.";
                return false;
            }

            wi::ecs::Entity candidate = wi::ecs::INVALID_ENTITY;
            if (scene.humanoids.GetCount() == 1)
            {
                const auto existing = scene.humanoids.GetEntity(0);
                if (scene.armatures.Contains(existing))
                    candidate = existing;
            }
            if (candidate == wi::ecs::INVALID_ENTITY && scene.armatures.GetCount() == 1)
                candidate = scene.armatures.GetEntity(0);

            if (candidate == wi::ecs::INVALID_ENTITY)
            {
                if (!mappingRequired)
                {
                    error.clear();
                    return true;
                }
                error =
                    "Character external animations require one humanoid armature, but the imported Character does not expose a single deterministic rig. "
                    "Repair the source skeleton or split multiple rigs before reimporting.";
                return false;
            }

            const auto automatic = BuildAutoHumanoidMapping(scene, candidate);
            if (!automatic.valid)
            {
                if (!mappingRequired && scene.humanoids.GetCount() == 0)
                {
                    // A Character Asset can still represent a non-humanoid actor.
                    // Only external humanoid retargeting makes a valid map mandatory.
                    error.clear();
                    return true;
                }
                error =
                    "Character humanoid mapping is incomplete and automatic Mixamo/VRM mapping could not establish the required bones. "
                    "Repair the source bone names/mapping, or import without external animations and use HUMANOID / RETARGET for manual diagnosis before correcting the source.";
                if (!automatic.error.empty())
                    error += " Auto-map: " + automatic.error;
                return false;
            }

            SetHumanoidMappingCommand command(scene, candidate, automatic.mapping);
            if (!command.Execute())
            {
                const auto captured = CaptureHumanoidMapping(scene, candidate);
                if (!IsHumanoidMappingValid(captured))
                {
                    error =
                        "Character humanoid auto-map produced a valid mapping but it could not be committed to the imported scene.";
                    return false;
                }
            }

            const auto captured = CaptureHumanoidMapping(scene, candidate);
            if (!IsHumanoidMappingValid(captured))
            {
                error =
                    "Character humanoid mapping did not validate after automatic preparation.";
                return false;
            }

            humanoidEntity = candidate;
            error.clear();
            return true;
        }

        bool RetargetExternalAnimations(
            wi::scene::Scene& scene,
            const std::string& projectRoot,
            const wi::ecs::Entity destinationHumanoid,
            const std::vector<CreatorExternalAnimationImportRecipe>& clips,
            std::string& error)
        {
            std::vector<ExternalAnimationSourceGroup> groups;
            for (const auto& clip : clips)
            {
                auto found = std::find_if(groups.begin(), groups.end(),
                    [&clip](const ExternalAnimationSourceGroup& group)
                    {
                        return group.projectRelativePath == clip.sourceProjectRelativePath;
                    });
                if (found == groups.end())
                {
                    groups.push_back({clip.sourceProjectRelativePath, {}});
                    found = std::prev(groups.end());
                }
                found->clips.push_back(&clip);
            }

            for (const auto& group : groups)
            {
                std::set<std::uint32_t> sourceIndices;
                bool hasEnabledClip = false;
                std::uint32_t maximumSourceIndex = 0;
                for (const auto* clip : group.clips)
                {
                    if (clip == nullptr ||
                        !sourceIndices.insert(clip->sourceAnimationIndex).second)
                    {
                        error =
                            "Character external animation recipe contains duplicate source-action provenance for " +
                            group.projectRelativePath + ".";
                        return false;
                    }
                    maximumSourceIndex = std::max(maximumSourceIndex, clip->sourceAnimationIndex);
                    hasEnabledClip = hasEnabledClip || clip->enabled;
                }
                if (!hasEnabledClip)
                    continue;

                std::string sourcePath;
                if (!ResolveExternalAnimationSource(
                        projectRoot, group.projectRelativePath, sourcePath, error))
                    return false;

                RetargetHumanoidAnimationsCommand command(
                    scene, destinationHumanoid, sourcePath);
                if (!command.Execute())
                {
                    error =
                        "Character external animation retarget failed for '" +
                        group.projectRelativePath + "': " + command.Result().error +
                        " Repair the Character/source humanoid mapping and reimport; Mixamo/VRM-compatible rigs are auto-mapped when possible.";
                    return false;
                }

                const auto& result = command.Result();
                if (result.createdAnimations.size() <= maximumSourceIndex)
                {
                    error =
                        "Character external animation retarget did not produce every source action referenced by the durable import recipe for '" +
                        group.projectRelativePath + "'.";
                    return false;
                }

                std::set<wi::ecs::Entity> keptAnimations;
                constexpr float RangeTolerance = 0.0001f;
                for (const auto* clip : group.clips)
                {
                    if (clip == nullptr || !clip->enabled)
                        continue;

                    const auto entity = result.createdAnimations[clip->sourceAnimationIndex];
                    auto* animation = scene.animations.GetComponent(entity);
                    if (animation == nullptr)
                    {
                        error =
                            "Retargeted Character animation disappeared before final asset preparation.";
                        return false;
                    }
                    if (clip->start < animation->start - RangeTolerance ||
                        clip->end > animation->end + RangeTolerance)
                    {
                        error =
                            "Character external animation range falls outside its retargeted source action for '" +
                            clip->name + "'.";
                        return false;
                    }

                    animation->start = clip->start;
                    animation->end = clip->end;
                    if (!clip->name.empty())
                    {
                        auto* name = scene.names.GetComponent(entity);
                        if (name == nullptr)
                            name = &scene.names.Create(entity);
                        name->name = clip->name;
                    }
                    keptAnimations.insert(entity);
                }

                for (const auto entity : result.createdAnimations)
                {
                    if (keptAnimations.count(entity) == 0)
                        scene.Entity_Remove(entity, true);
                }
                scene.ResetPose(destinationHumanoid);
            }

            error.clear();
            return true;
        }
    }

    bool ParseCreatorModelImportOptions(
        const std::string& optionsJson,
        CreatorModelImportRecipe& recipe,
        std::string& error)
    {
        recipe = {};
        try
        {
            const nlohmann::json root = nlohmann::json::parse(optionsJson);
            if (!root.is_object() || root.dump() != optionsJson)
            {
                error = "Creator model import options must be a canonical JSON object.";
                return false;
            }
            for (auto iterator = root.begin(); iterator != root.end(); ++iterator)
            {
                if (iterator.key() != "asset_kind" &&
                    iterator.key() != "transform" &&
                    iterator.key() != "materials" && iterator.key() != "animations" &&
                    iterator.key() != "external_animations")
                {
                    error = "Creator model import options contain an unsupported key: " +
                        iterator.key();
                    return false;
                }
            }

            if (root.contains("asset_kind"))
            {
                if (!root.at("asset_kind").is_string())
                {
                    error = "Creator model import asset_kind must be a string.";
                    return false;
                }
                const std::string kind = root.at("asset_kind").get<std::string>();
                if (kind == "model")
                    recipe.assetKind = CreatorAssetImportKind::Model;
                else if (kind == "character")
                    recipe.assetKind = CreatorAssetImportKind::Character;
                else
                {
                    error = "Creator model import asset_kind must be model or character.";
                    return false;
                }
            }

            if (root.contains("transform"))
            {
                const auto& transform = root.at("transform");
                if (!transform.is_object() || transform.size() != 3 ||
                    !transform.contains("position") ||
                    !transform.contains("rotation_degrees") ||
                    !transform.contains("scale"))
                {
                    error = "Creator model transform requires position, rotation_degrees and scale.";
                    return false;
                }
                recipe.transform.authored = true;
                if (!ReadVector3(transform, "position",
                        recipe.transform.positionX,
                        recipe.transform.positionY,
                        recipe.transform.positionZ,
                        -100.0f, 100.0f, error) ||
                    !ReadVector3(transform, "rotation_degrees",
                        recipe.transform.rotationXDegrees,
                        recipe.transform.rotationYDegrees,
                        recipe.transform.rotationZDegrees,
                        -180.0f, 180.0f, error) ||
                    !ReadVector3(transform, "scale",
                        recipe.transform.scaleX,
                        recipe.transform.scaleY,
                        recipe.transform.scaleZ,
                        0.000001f, 1000000.0f, error))
                    return false;
            }

            if (root.contains("materials"))
            {
                if (!root.at("materials").is_array())
                {
                    error = "Creator model recipe materials must be an array.";
                    return false;
                }
                std::set<std::uint32_t> materialIndices;
                for (const auto& item : root.at("materials"))
                {
                    if (!item.is_object() || !item.contains("material_index") ||
                        !item.at("material_index").is_number_unsigned())
                    {
                        error = "Each creator material recipe requires an unsigned material_index.";
                        return false;
                    }
                    CreatorMaterialImportRecipe material;
                    material.materialIndex = item.at("material_index").get<std::uint32_t>();
                    material.hasScalarSettings = item.contains("roughness") ||
                        item.contains("metalness") || item.contains("reflectance") ||
                        item.contains("normal_strength") || item.contains("ao_strength") ||
                        item.contains("emissive_strength");
                    if (!materialIndices.insert(material.materialIndex).second)
                    {
                        error = "Creator model recipe contains duplicate material_index values.";
                        return false;
                    }
                    for (auto iterator = item.begin(); iterator != item.end(); ++iterator)
                    {
                        const std::string& key = iterator.key();
                        if (key != "material_index" && key != "base_color_asset_id" &&
                            key != "normal_asset_id" && key != "surface_asset_id" &&
                            key != "emissive_asset_id" && key != "occlusion_asset_id" &&
                            key != "roughness" && key != "metalness" &&
                            key != "reflectance" && key != "normal_strength" &&
                            key != "ao_strength" && key != "emissive_strength")
                        {
                            error = "Creator material recipe contains an unsupported key: " + key;
                            return false;
                        }
                    }
                    if (!ReadOptionalStableId(item, "base_color_asset_id",
                            material.baseColorAssetId, error) ||
                        !ReadOptionalStableId(item, "normal_asset_id",
                            material.normalAssetId, error) ||
                        !ReadOptionalStableId(item, "surface_asset_id",
                            material.surfaceAssetId, error) ||
                        !ReadOptionalStableId(item, "emissive_asset_id",
                            material.emissiveAssetId, error) ||
                        !ReadOptionalStableId(item, "occlusion_asset_id",
                            material.occlusionAssetId, error) ||
                        !ReadScalar(item, "roughness", material.roughness, 0.0f, 1.0f, error) ||
                        !ReadScalar(item, "metalness", material.metalness, 0.0f, 1.0f, error) ||
                        !ReadScalar(item, "reflectance", material.reflectance, 0.0f, 1.0f, error) ||
                        !ReadScalar(item, "normal_strength", material.normalStrength, 0.0f, 4.0f, error) ||
                        !ReadScalar(item, "ao_strength", material.aoStrength, 0.0f, 1.0f, error) ||
                        !ReadScalar(item, "emissive_strength", material.emissiveStrength, 0.0f, 100.0f, error))
                        return false;
                    recipe.materials.push_back(std::move(material));
                }
            }

            if (root.contains("animations"))
            {
                if (!root.at("animations").is_array())
                {
                    error = "Creator model recipe animations must be an array.";
                    return false;
                }
                for (const auto& item : root.at("animations"))
                {
                    if (!item.is_object() ||
                        !item.contains("source_animation_index") ||
                        !item.at("source_animation_index").is_number_unsigned() ||
                        !item.contains("name") || !item.at("name").is_string() ||
                        !item.contains("start") || !item.at("start").is_number() ||
                        !item.contains("end") || !item.at("end").is_number() ||
                        !item.contains("enabled") || !item.at("enabled").is_boolean())
                    {
                        error = "Each creator animation recipe requires source index, name, start, end and enabled.";
                        return false;
                    }
                    if (item.size() != 5 + (item.contains("speed") ? 1 : 0) +
                        (item.contains("action") ? 1 : 0) ||
                        (item.contains("speed") && !item.at("speed").is_number()) ||
                        (item.contains("action") && !item.at("action").is_string()))
                    {
                        error = "Creator animation recipe contains unsupported fields or speed.";
                        return false;
                    }
                    CreatorAnimationImportRecipe animation;
                    animation.sourceAnimationIndex =
                        item.at("source_animation_index").get<std::uint32_t>();
                    animation.name = item.at("name").get<std::string>();
                    animation.start = item.at("start").get<float>();
                    animation.end = item.at("end").get<float>();
                    animation.speed = item.value("speed", 1.0f);
                    animation.action = item.value("action", std::string{});
                    if (animation.action.size() > 64 ||
                        std::any_of(animation.action.begin(), animation.action.end(),
                            [](unsigned char c) { return c < 32; }))
                    {
                        error = "Creator animation action label is invalid.";
                        return false;
                    }
                    animation.enabled = item.at("enabled").get<bool>();
                    if (!std::isfinite(animation.speed) ||
                        animation.speed < 0.1f || animation.speed > 4.0f)
                    {
                        error = "Creator animation clip speed must be between 0.1x and 4x.";
                        return false;
                    }
                    if (!std::isfinite(animation.start) ||
                        !std::isfinite(animation.end) ||
                        animation.end < animation.start)
                    {
                        error = "Creator animation clip has an invalid start/end range.";
                        return false;
                    }
                    recipe.animations.push_back(std::move(animation));
                }
            }

            if (root.contains("external_animations"))
            {
                if (!root.at("external_animations").is_array())
                {
                    error = "Creator external animation recipe must be an array.";
                    return false;
                }
                for (const auto& item : root.at("external_animations"))
                {
                    if (!item.is_object() || item.size() != 6 ||
                        !item.contains("source_project_relative_path") ||
                        !item.at("source_project_relative_path").is_string() ||
                        !item.contains("source_animation_index") ||
                        !item.at("source_animation_index").is_number_unsigned() ||
                        !item.contains("name") || !item.at("name").is_string() ||
                        !item.contains("start") || !item.at("start").is_number() ||
                        !item.contains("end") || !item.at("end").is_number() ||
                        !item.contains("enabled") || !item.at("enabled").is_boolean())
                    {
                        error = "Each external animation recipe requires governed source, index, name, range and enabled state.";
                        return false;
                    }
                    CreatorExternalAnimationImportRecipe animation;
                    animation.sourceProjectRelativePath =
                        item.at("source_project_relative_path").get<std::string>();
                    animation.sourceAnimationIndex =
                        item.at("source_animation_index").get<std::uint32_t>();
                    animation.name = item.at("name").get<std::string>();
                    animation.start = item.at("start").get<float>();
                    animation.end = item.at("end").get<float>();
                    animation.enabled = item.at("enabled").get<bool>();
                    if (animation.sourceProjectRelativePath.empty() ||
                        animation.sourceProjectRelativePath.find("..") != std::string::npos ||
                        !std::isfinite(animation.start) ||
                        !std::isfinite(animation.end) || animation.end < animation.start)
                    {
                        error = "External animation recipe has invalid governed source or range.";
                        return false;
                    }
                    recipe.externalAnimations.push_back(std::move(animation));
                }
            }
        }
        catch (const nlohmann::json::exception&)
        {
            error = "Creator model import options are malformed JSON.";
            recipe = {};
            return false;
        }
        error.clear();
        return true;
    }

    bool SerializeCreatorModelImportOptions(
        const CreatorModelImportRecipe& recipe,
        std::string& optionsJson,
        std::string& error)
    {
        nlohmann::json root = nlohmann::json::object();
        if (recipe.assetKind == CreatorAssetImportKind::Character)
            root["asset_kind"] = "character";
        if (recipe.transform.authored)
        {
            const auto& transform = recipe.transform;
            const auto valid = [](const float value, const float minimum,
                const float maximum)
            {
                return std::isfinite(value) && value >= minimum &&
                    value <= maximum;
            };
            if (!valid(transform.positionX, -100.0f, 100.0f) ||
                !valid(transform.positionY, -100.0f, 100.0f) ||
                !valid(transform.positionZ, -100.0f, 100.0f) ||
                !valid(transform.rotationXDegrees, -180.0f, 180.0f) ||
                !valid(transform.rotationYDegrees, -180.0f, 180.0f) ||
                !valid(transform.rotationZDegrees, -180.0f, 180.0f) ||
                !valid(transform.scaleX, 0.000001f, 1000000.0f) ||
                !valid(transform.scaleY, 0.000001f, 1000000.0f) ||
                !valid(transform.scaleZ, 0.000001f, 1000000.0f))
            {
                error = "Creator model transform contains an invalid authored value.";
                optionsJson.clear();
                return false;
            }
            root["transform"] = {
                {"position", {transform.positionX, transform.positionY,
                    transform.positionZ}},
                {"rotation_degrees", {transform.rotationXDegrees,
                    transform.rotationYDegrees,
                    transform.rotationZDegrees}},
                {"scale", {transform.scaleX, transform.scaleY,
                    transform.scaleZ}},
            };
        }
        if (!recipe.materials.empty())
        {
            nlohmann::json materials = nlohmann::json::array();
            for (const auto& material : recipe.materials)
            {
                nlohmann::json item;
                item["material_index"] = material.materialIndex;
                WriteOptionalStableId(item, "base_color_asset_id", material.baseColorAssetId);
                WriteOptionalStableId(item, "normal_asset_id", material.normalAssetId);
                WriteOptionalStableId(item, "surface_asset_id", material.surfaceAssetId);
                WriteOptionalStableId(item, "emissive_asset_id", material.emissiveAssetId);
                WriteOptionalStableId(item, "occlusion_asset_id", material.occlusionAssetId);
                if (material.hasScalarSettings)
                {
                    item["roughness"] = std::clamp(material.roughness, 0.0f, 1.0f);
                    item["metalness"] = std::clamp(material.metalness, 0.0f, 1.0f);
                    item["reflectance"] = std::clamp(material.reflectance, 0.0f, 1.0f);
                    item["normal_strength"] = std::clamp(material.normalStrength, 0.0f, 4.0f);
                    item["ao_strength"] = std::clamp(material.aoStrength, 0.0f, 1.0f);
                    item["emissive_strength"] = std::clamp(material.emissiveStrength, 0.0f, 100.0f);
                }
                materials.push_back(std::move(item));
            }
            root["materials"] = std::move(materials);
        }
        if (!recipe.animations.empty())
        {
            nlohmann::json animations = nlohmann::json::array();
            for (const auto& animation : recipe.animations)
            {
                if (!std::isfinite(animation.start) ||
                    !std::isfinite(animation.end) || animation.end < animation.start)
                {
                    error = "Creator animation clip has an invalid start/end range.";
                    optionsJson.clear();
                    return false;
                }
                if (!std::isfinite(animation.speed) ||
                    animation.speed < 0.1f || animation.speed > 4.0f)
                {
                    error = "Creator animation clip speed must be between 0.1x and 4x.";
                    optionsJson.clear();
                    return false;
                }
                nlohmann::json entry = {
                    {"enabled", animation.enabled},
                    {"end", animation.end},
                    {"name", animation.name},
                    {"source_animation_index", animation.sourceAnimationIndex},
                    {"start", animation.start},
                };
                if (animation.speed != 1.0f)
                    entry["speed"] = animation.speed;
                if (!animation.action.empty())
                    entry["action"] = animation.action;
                animations.push_back(std::move(entry));
            }
            root["animations"] = std::move(animations);
        }
        if (!recipe.externalAnimations.empty())
        {
            nlohmann::json animations = nlohmann::json::array();
            for (const auto& animation : recipe.externalAnimations)
            {
                if (animation.sourceProjectRelativePath.empty() ||
                    animation.sourceProjectRelativePath.find("..") != std::string::npos ||
                    !std::isfinite(animation.start) ||
                    !std::isfinite(animation.end) || animation.end < animation.start)
                {
                    error = "External animation recipe has invalid governed source or range.";
                    optionsJson.clear();
                    return false;
                }
                animations.push_back({
                    {"enabled", animation.enabled},
                    {"end", animation.end},
                    {"name", animation.name},
                    {"source_animation_index", animation.sourceAnimationIndex},
                    {"source_project_relative_path", animation.sourceProjectRelativePath},
                    {"start", animation.start},
                });
            }
            root["external_animations"] = std::move(animations);
        }
        optionsJson = root.dump();
        CreatorModelImportRecipe verified;
        if (!ParseCreatorModelImportOptions(optionsJson, verified, error))
        {
            optionsJson.clear();
            return false;
        }
        error.clear();
        return true;
    }

    bool ApplyCreatorModelImportRecipe(
        wi::scene::Scene& scene,
        const std::string& projectRoot,
        const StableId& projectId,
        const CreatorModelImportRecipe& recipe,
        std::string& error)
    {
        if (!IsValidStableId(projectId))
        {
            error = "Creator model recipe requires a valid project ID.";
            return false;
        }

        if (recipe.transform.authored)
        {
            std::vector<wi::ecs::Entity> roots;
            roots.reserve(scene.transforms.GetCount());
            for (std::size_t index = 0; index < scene.transforms.GetCount(); ++index)
            {
                const wi::ecs::Entity entity = scene.transforms.GetEntity(index);
                const auto* hierarchy = scene.hierarchy.GetComponent(entity);
                if (hierarchy == nullptr ||
                    hierarchy->parentID == wi::ecs::INVALID_ENTITY)
                    roots.push_back(entity);
            }

            const wi::ecs::Entity authoredRoot = wi::ecs::CreateEntity();
            scene.names.Create(authoredRoot).name =
                CreatorAuthoredTransformRootName;
            auto& transform = scene.transforms.Create(authoredRoot);
            transform.translation_local = XMFLOAT3(
                recipe.transform.positionX,
                recipe.transform.positionY,
                recipe.transform.positionZ);
            XMStoreFloat4(
                &transform.rotation_local,
                XMQuaternionRotationRollPitchYaw(
                    XMConvertToRadians(recipe.transform.rotationXDegrees),
                    XMConvertToRadians(recipe.transform.rotationYDegrees),
                    XMConvertToRadians(recipe.transform.rotationZDegrees)));
            transform.scale_local = XMFLOAT3(
                recipe.transform.scaleX,
                recipe.transform.scaleY,
                recipe.transform.scaleZ);
            transform.SetDirty();
            transform.UpdateTransform();
            for (const wi::ecs::Entity entity : roots)
                scene.Component_Attach(entity, authoredRoot, true);
            // Do not call Scene::Update() here. Creator recipes are also
            // applied by headless import/reimport paths where Wicked's global
            // graphics device can be null. The authored local transform and
            // hierarchy are fully serializable; renderer-owned scene updates
            // will propagate world transforms when a graphics context exists.
        }

        for (const auto& materialRecipe : recipe.materials)
        {
            if (materialRecipe.materialIndex >= scene.materials.GetCount())
            {
                error = "Creator material recipe refers to a material index that is absent after conversion.";
                return false;
            }
            const wi::ecs::Entity materialEntity =
                scene.materials.GetEntity(materialRecipe.materialIndex);
            auto* material = scene.materials.GetComponent(materialEntity);
            if (material == nullptr)
            {
                error = "Creator material recipe target disappeared during import.";
                return false;
            }
            if (materialRecipe.hasScalarSettings)
            {
                material->SetRoughness(materialRecipe.roughness);
                material->SetMetalness(materialRecipe.metalness);
                material->SetReflectance(materialRecipe.reflectance);
                material->SetNormalMapStrength(materialRecipe.normalStrength);
                material->SetEmissiveStrength(materialRecipe.emissiveStrength);
            }
            if (!ApplyTexture(scene, materialEntity, MaterialTextureSlot::BaseColor,
                    materialRecipe.baseColorAssetId, projectRoot, projectId, error) ||
                !ApplyTexture(scene, materialEntity, MaterialTextureSlot::Normal,
                    materialRecipe.normalAssetId, projectRoot, projectId, error) ||
                !ApplyTexture(scene, materialEntity, MaterialTextureSlot::Surface,
                    materialRecipe.surfaceAssetId, projectRoot, projectId, error) ||
                !ApplyTexture(scene, materialEntity, MaterialTextureSlot::Emissive,
                    materialRecipe.emissiveAssetId, projectRoot, projectId, error) ||
                !ApplyTexture(scene, materialEntity, MaterialTextureSlot::Occlusion,
                    materialRecipe.occlusionAssetId, projectRoot, projectId, error))
                return false;
        }

        if (!recipe.animations.empty())
        {
            std::vector<AnimationSnapshot> originals;
            originals.reserve(scene.animations.GetCount());
            for (std::size_t index = 0; index < scene.animations.GetCount(); ++index)
            {
                AnimationSnapshot snapshot;
                snapshot.entity = scene.animations.GetEntity(index);
                snapshot.animation = scene.animations[index];
                const auto* name = scene.names.GetComponent(snapshot.entity);
                if (name != nullptr)
                    snapshot.name = name->name;
                originals.push_back(std::move(snapshot));
            }

            for (const auto& clip : recipe.animations)
            {
                if (clip.sourceAnimationIndex >= originals.size())
                {
                    error = "Creator animation recipe refers to an animation index that is absent after conversion.";
                    return false;
                }
                const auto& source = originals[clip.sourceAnimationIndex].animation;
                if (clip.start < source.start || clip.end > source.end)
                {
                    error = "Creator animation clip range falls outside its source animation.";
                    return false;
                }
            }

            for (const auto& snapshot : originals)
                scene.animations.Remove(snapshot.entity);

            std::set<std::uint32_t> reusedSourceEntities;
            for (const auto& clip : recipe.animations)
            {
                if (!clip.enabled)
                    continue;
                const auto& source = originals[clip.sourceAnimationIndex];
                wi::ecs::Entity target = wi::ecs::INVALID_ENTITY;
                if (reusedSourceEntities.insert(clip.sourceAnimationIndex).second)
                    target = source.entity;
                else
                    target = wi::ecs::CreateEntity();

                auto& animation = scene.animations.Create(target);
                animation = source.animation;
                animation.start = clip.start;
                animation.end = clip.end;
                animation.speed = clip.speed;
                auto* name = scene.names.GetComponent(target);
                if (name == nullptr)
                    name = &scene.names.Create(target);
                name->name = clip.name.empty() ? source.name : clip.name;
                if (recipe.assetKind == CreatorAssetImportKind::Character &&
                    !clip.action.empty())
                {
                    auto* metadata = scene.metadatas.GetComponent(target);
                    if (metadata == nullptr)
                        metadata = &scene.metadatas.Create(target);
                    metadata->string_values.set(
                        CreatorCharacterAnimationActionMetadataKey, clip.action);
                }
            }
        }

        if (!recipe.externalAnimations.empty() &&
            recipe.assetKind != CreatorAssetImportKind::Character)
        {
            error =
                "External humanoid animations can only be committed by a Character import recipe.";
            return false;
        }

        if (recipe.assetKind == CreatorAssetImportKind::Character)
        {
            const bool hasEnabledExternalAnimation = std::any_of(
                recipe.externalAnimations.begin(), recipe.externalAnimations.end(),
                [](const CreatorExternalAnimationImportRecipe& clip)
                {
                    return clip.enabled;
                });

            wi::ecs::Entity destinationHumanoid = wi::ecs::INVALID_ENTITY;
            if (!PrepareCharacterHumanoid(
                    scene, hasEnabledExternalAnimation,
                    destinationHumanoid, error))
            {
                return false;
            }

            if (hasEnabledExternalAnimation)
            {
                if (destinationHumanoid == wi::ecs::INVALID_ENTITY)
                {
                    error =
                        "Character external animations require a prepared humanoid destination.";
                    return false;
                }
                if (!RetargetExternalAnimations(
                        scene, projectRoot, destinationHumanoid,
                        recipe.externalAnimations, error))
                {
                    return false;
                }
            }
        }

        error.clear();
        return true;
    }

    bool HasCreatorAuthoredTransform(
        const wi::scene::Scene& scene) noexcept
    {
        for (std::size_t index = 0; index < scene.names.GetCount(); ++index)
        {
            if (scene.names[index].name == CreatorAuthoredTransformRootName &&
                scene.transforms.Contains(scene.names.GetEntity(index)))
                return true;
        }
        return false;
    }
}
