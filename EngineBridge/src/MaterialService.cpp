#include "renegade/bridge/MaterialService.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float Epsilon = 0.00001f;

    bool NearlyEqual(const float left, const float right) noexcept
    {
        return std::abs(left - right) <= Epsilon;
    }

    bool Contains(
        const wi::vector<wi::ecs::Entity>& entities,
        const wi::ecs::Entity entity) noexcept
    {
        return std::find(entities.begin(), entities.end(), entity) !=
            entities.end();
    }
}

namespace renegade::bridge
{
    bool IsTerrainOwnedMaterial(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity materialEntity) noexcept
    {
        if (materialEntity == wi::ecs::INVALID_ENTITY)
        {
            return false;
        }
        for (std::size_t index = 0; index < scene.terrains.GetCount(); ++index)
        {
            const auto terrainEntity = scene.terrains.GetEntity(index);
            const auto& terrain = scene.terrains[index];
            if (Contains(terrain.materialEntities, materialEntity) ||
                terrain.grassEntity == materialEntity ||
                (terrainEntity != wi::ecs::INVALID_ENTITY &&
                    scene.Entity_IsDescendant(materialEntity, terrainEntity)))
            {
                return true;
            }
            for (const auto& [chunk, data] : terrain.chunks)
            {
                (void)chunk;
                if (data.entity == materialEntity ||
                    data.grass_entity == materialEntity)
                {
                    return true;
                }
            }
        }
        return false;
    }

    wi::ecs::Entity ResolveEditableMaterialEntity(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity selectedEntity) noexcept
    {
        if (selectedEntity == wi::ecs::INVALID_ENTITY)
        {
            return wi::ecs::INVALID_ENTITY;
        }
        if (scene.materials.Contains(selectedEntity))
        {
            return IsTerrainOwnedMaterial(scene, selectedEntity)
                ? wi::ecs::INVALID_ENTITY
                : selectedEntity;
        }

        const auto* object = scene.objects.GetComponent(selectedEntity);
        if (object == nullptr)
        {
            return wi::ecs::INVALID_ENTITY;
        }
        const auto* mesh = scene.meshes.GetComponent(object->meshID);
        if (mesh == nullptr || mesh->subsets.empty())
        {
            return wi::ecs::INVALID_ENTITY;
        }

        wi::ecs::Entity resolved = wi::ecs::INVALID_ENTITY;
        for (const auto& subset : mesh->subsets)
        {
            const auto candidate = subset.materialID;
            if (candidate == wi::ecs::INVALID_ENTITY ||
                !scene.materials.Contains(candidate) ||
                IsTerrainOwnedMaterial(scene, candidate))
            {
                return wi::ecs::INVALID_ENTITY;
            }
            if (resolved == wi::ecs::INVALID_ENTITY)
            {
                resolved = candidate;
            }
            else if (resolved != candidate)
            {
                return wi::ecs::INVALID_ENTITY;
            }
        }
        return resolved;
    }

    MaterialState CaptureMaterial(
        const wi::scene::MaterialComponent& material) noexcept
    {
        MaterialState state;
        state.baseColor = material.baseColor;
        state.metalness = material.metalness;
        state.roughness = material.roughness;
        state.reflectance = material.reflectance;
        state.emissiveColor = XMFLOAT3(
            material.emissiveColor.x,
            material.emissiveColor.y,
            material.emissiveColor.z);
        state.emissiveStrength = material.GetEmissiveStrength();
        return state;
    }

    MaterialState SanitizeMaterialState(const MaterialState& state) noexcept
    {
        MaterialState result = state;
        result.baseColor.x = std::clamp(result.baseColor.x, 0.0f, 1.0f);
        result.baseColor.y = std::clamp(result.baseColor.y, 0.0f, 1.0f);
        result.baseColor.z = std::clamp(result.baseColor.z, 0.0f, 1.0f);
        result.baseColor.w = std::clamp(result.baseColor.w, 0.0f, 1.0f);
        result.metalness = std::clamp(result.metalness, 0.0f, 1.0f);
        result.roughness = std::clamp(result.roughness, 0.0f, 1.0f);
        result.reflectance = std::clamp(result.reflectance, 0.0f, 1.0f);
        result.emissiveColor.x = std::clamp(
            result.emissiveColor.x,
            0.0f,
            1.0f);
        result.emissiveColor.y = std::clamp(
            result.emissiveColor.y,
            0.0f,
            1.0f);
        result.emissiveColor.z = std::clamp(
            result.emissiveColor.z,
            0.0f,
            1.0f);
        result.emissiveStrength = std::clamp(
            result.emissiveStrength,
            0.0f,
            100.0f);
        return result;
    }

    bool HasMaterialStateChange(
        const MaterialState& before,
        const MaterialState& after) noexcept
    {
        const auto left = SanitizeMaterialState(before);
        const auto right = SanitizeMaterialState(after);
        return !NearlyEqual(left.baseColor.x, right.baseColor.x) ||
            !NearlyEqual(left.baseColor.y, right.baseColor.y) ||
            !NearlyEqual(left.baseColor.z, right.baseColor.z) ||
            !NearlyEqual(left.baseColor.w, right.baseColor.w) ||
            !NearlyEqual(left.metalness, right.metalness) ||
            !NearlyEqual(left.roughness, right.roughness) ||
            !NearlyEqual(left.reflectance, right.reflectance) ||
            !NearlyEqual(left.emissiveColor.x, right.emissiveColor.x) ||
            !NearlyEqual(left.emissiveColor.y, right.emissiveColor.y) ||
            !NearlyEqual(left.emissiveColor.z, right.emissiveColor.z) ||
            !NearlyEqual(left.emissiveStrength, right.emissiveStrength);
    }

    void ApplyMaterial(
        wi::scene::MaterialComponent& material,
        const MaterialState& state) noexcept
    {
        const auto safe = SanitizeMaterialState(state);
        material.SetBaseColor(safe.baseColor);
        material.SetMetalness(safe.metalness);
        material.SetRoughness(safe.roughness);
        material.SetReflectance(safe.reflectance);
        material.SetEmissiveColor(XMFLOAT4(
            safe.emissiveColor.x,
            safe.emissiveColor.y,
            safe.emissiveColor.z,
            safe.emissiveStrength));
    }

    SetMaterialCommand::SetMaterialCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity materialEntity,
        const MaterialState& material)
        : scene_(&scene)
        , materialEntity_(materialEntity)
        , after_(SanitizeMaterialState(material))
    {
        if (const auto* existing = scene.materials.GetComponent(materialEntity))
        {
            before_ = CaptureMaterial(*existing);
        }
    }

    SetMaterialCommand::SetMaterialCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity materialEntity,
        const MaterialState& before,
        const MaterialState& after)
        : scene_(&scene)
        , materialEntity_(materialEntity)
        , before_(SanitizeMaterialState(before))
        , after_(SanitizeMaterialState(after))
    {
    }

    bool SetMaterialCommand::Execute()
    {
        return HasMaterialStateChange(before_, after_) && Apply(after_);
    }

    void SetMaterialCommand::Undo()
    {
        Apply(before_);
    }

    bool SetMaterialCommand::Apply(const MaterialState& state) noexcept
    {
        if (scene_ == nullptr ||
            IsTerrainOwnedMaterial(*scene_, materialEntity_))
        {
            return false;
        }
        auto* material = scene_->materials.GetComponent(materialEntity_);
        if (material == nullptr)
        {
            return false;
        }
        ApplyMaterial(*material, state);
        return true;
    }
}
