#include "renegade/bridge/CreatorModelImportRecipe.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <utility>

#include "json.hpp"

namespace renegade::bridge
{
    namespace
    {
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
                if (iterator.key() != "transform" &&
                    iterator.key() != "materials" && iterator.key() != "animations")
                {
                    error = "Creator model import options contain an unsupported key: " +
                        iterator.key();
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
                    if (item.size() != 5)
                    {
                        error = "Creator animation recipe contains unsupported fields.";
                        return false;
                    }
                    CreatorAnimationImportRecipe animation;
                    animation.sourceAnimationIndex =
                        item.at("source_animation_index").get<std::uint32_t>();
                    animation.name = item.at("name").get<std::string>();
                    animation.start = item.at("start").get<float>();
                    animation.end = item.at("end").get<float>();
                    animation.enabled = item.at("enabled").get<bool>();
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
                animations.push_back({
                    {"enabled", animation.enabled},
                    {"end", animation.end},
                    {"name", animation.name},
                    {"source_animation_index", animation.sourceAnimationIndex},
                    {"start", animation.start},
                });
            }
            root["animations"] = std::move(animations);
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
            scene.Update(0.0f);
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
                auto* name = scene.names.GetComponent(target);
                if (name == nullptr)
                    name = &scene.names.Create(target);
                name->name = clip.name.empty() ? source.name : clip.name;
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
