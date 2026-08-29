#include "renegade/bridge/MaterialService.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float Epsilon = 0.00001f;
    constexpr float RadiansToDegrees = 180.0f / XM_PI;
    constexpr float DegreesToRadians = XM_PI / 180.0f;

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

    float NormalizeDegrees(const float value) noexcept
    {
        float result = std::fmod(value, 360.0f);
        if (result < 0.0f)
            result += 360.0f;
        return result;
    }

    wi::scene::MaterialComponent::SHADERTYPE SanitizeShaderType(
        const wi::scene::MaterialComponent::SHADERTYPE value) noexcept
    {
        using Material = wi::scene::MaterialComponent;
        switch (value)
        {
        case Material::SHADERTYPE_PBR:
        case Material::SHADERTYPE_PBR_PLANARREFLECTION:
        case Material::SHADERTYPE_PBR_PARALLAXOCCLUSIONMAPPING:
        case Material::SHADERTYPE_PBR_ANISOTROPIC:
        case Material::SHADERTYPE_WATER:
        case Material::SHADERTYPE_CARTOON:
        case Material::SHADERTYPE_UNLIT:
        case Material::SHADERTYPE_PBR_CLOTH:
        case Material::SHADERTYPE_PBR_CLEARCOAT:
        case Material::SHADERTYPE_PBR_CLOTH_CLEARCOAT:
        case Material::SHADERTYPE_PBR_TERRAINBLENDED:
        case Material::SHADERTYPE_INTERIORMAPPING:
            return value;
        default:
            return Material::SHADERTYPE_PBR;
        }
    }

    wi::enums::BLENDMODE SanitizeBlendMode(
        const wi::enums::BLENDMODE value) noexcept
    {
        switch (value)
        {
        case wi::enums::BLENDMODE_OPAQUE:
        case wi::enums::BLENDMODE_ALPHA:
        case wi::enums::BLENDMODE_PREMULTIPLIED:
        case wi::enums::BLENDMODE_ADDITIVE:
        case wi::enums::BLENDMODE_MULTIPLY:
        case wi::enums::BLENDMODE_INVERSE:
            return value;
        default:
            return wi::enums::BLENDMODE_OPAQUE;
        }
    }

    bool Same3(const XMFLOAT3& left, const XMFLOAT3& right) noexcept
    {
        return NearlyEqual(left.x, right.x) &&
            NearlyEqual(left.y, right.y) &&
            NearlyEqual(left.z, right.z);
    }

    bool Same4(const XMFLOAT4& left, const XMFLOAT4& right) noexcept
    {
        return NearlyEqual(left.x, right.x) &&
            NearlyEqual(left.y, right.y) &&
            NearlyEqual(left.z, right.z) &&
            NearlyEqual(left.w, right.w);
    }
}

namespace renegade::bridge
{
    bool IsTerrainOwnedMaterial(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity materialEntity) noexcept
    {
        if (materialEntity == wi::ecs::INVALID_ENTITY)
            return false;

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

    std::vector<wi::ecs::Entity> CollectEditableMaterialEntities(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity selectedEntity)
    {
        std::vector<wi::ecs::Entity> result;
        if (selectedEntity == wi::ecs::INVALID_ENTITY)
            return result;

        const auto addMaterial = [&](const wi::ecs::Entity materialEntity)
        {
            if (materialEntity == wi::ecs::INVALID_ENTITY ||
                !scene.materials.Contains(materialEntity) ||
                IsTerrainOwnedMaterial(scene, materialEntity) ||
                std::find(result.begin(), result.end(), materialEntity) != result.end())
            {
                return;
            }
            result.push_back(materialEntity);
        };

        if (scene.materials.Contains(selectedEntity))
        {
            addMaterial(selectedEntity);
            return result;
        }

        const auto collectMesh = [&](const wi::ecs::Entity meshEntity)
        {
            const auto* mesh = scene.meshes.GetComponent(meshEntity);
            if (mesh == nullptr)
                return;
            for (const auto& subset : mesh->subsets)
                addMaterial(subset.materialID);
        };

        if (scene.meshes.Contains(selectedEntity))
            collectMesh(selectedEntity);

        for (std::size_t index = 0; index < scene.objects.GetCount(); ++index)
        {
            const auto objectEntity = scene.objects.GetEntity(index);
            if (objectEntity != selectedEntity &&
                !scene.Entity_IsDescendant(objectEntity, selectedEntity))
            {
                continue;
            }
            collectMesh(scene.objects[index].meshID);
        }

        return result;
    }

    wi::ecs::Entity ResolveEditableMaterialEntity(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity selectedEntity) noexcept
    {
        const auto materials = CollectEditableMaterialEntities(scene, selectedEntity);
        return materials.size() == 1 ? materials.front() : wi::ecs::INVALID_ENTITY;
    }

    MaterialState CaptureMaterial(
        const wi::scene::MaterialComponent& material) noexcept
    {
        MaterialState state;
        state.shaderType = material.shaderType;
        state.blendMode = material.userBlendMode;
        state.baseColor = material.baseColor;
        state.metalness = material.metalness;
        state.roughness = material.roughness;
        state.reflectance = material.reflectance;
        state.normalMapStrength = material.normalMapStrength;
        state.alphaRef = material.alphaRef;
        state.emissiveColor = XMFLOAT3(
            material.emissiveColor.x,
            material.emissiveColor.y,
            material.emissiveColor.z);
        state.emissiveStrength = material.GetEmissiveStrength();
        state.receiveShadow = material.IsReceiveShadow();
        state.castShadow = material.IsCastingShadow();
        state.useVertexColors = material.IsUsingVertexColors();
        state.doubleSided = material.IsDoubleSided();
        state.texMulAdd = material.texMulAdd;
        state.parallaxOcclusionMapping = material.parallaxOcclusionMapping;
        state.anisotropyStrength = material.anisotropy_strength;
        state.anisotropyRotationDegrees =
            material.anisotropy_rotation * RadiansToDegrees;
        state.sheenColor = XMFLOAT3(
            material.sheenColor.x,
            material.sheenColor.y,
            material.sheenColor.z);
        state.sheenRoughness = material.sheenRoughness;
        state.clearcoat = material.clearcoat;
        state.clearcoatRoughness = material.clearcoatRoughness;
        state.transmission = material.transmission;
        state.refraction = material.refraction;
        state.blendWithTerrainHeight = material.blend_with_terrain_height;
        state.meshBlend = material.mesh_blend;
        state.interiorMappingScale = material.interiorMappingScale;
        state.interiorMappingOffset = material.interiorMappingOffset;
        state.interiorMappingRotationDegrees =
            material.interiorMappingRotation * RadiansToDegrees;
        return state;
    }

    MaterialState SanitizeMaterialState(const MaterialState& state) noexcept
    {
        MaterialState result = state;
        result.shaderType = SanitizeShaderType(result.shaderType);
        result.blendMode = SanitizeBlendMode(result.blendMode);

        result.baseColor.x = std::clamp(result.baseColor.x, 0.0f, 1.0f);
        result.baseColor.y = std::clamp(result.baseColor.y, 0.0f, 1.0f);
        result.baseColor.z = std::clamp(result.baseColor.z, 0.0f, 1.0f);
        result.baseColor.w = std::clamp(result.baseColor.w, 0.0f, 1.0f);
        result.metalness = std::clamp(result.metalness, 0.0f, 1.0f);
        result.roughness = std::clamp(result.roughness, 0.0f, 1.0f);
        result.reflectance = std::clamp(result.reflectance, 0.0f, 1.0f);
        result.normalMapStrength = std::clamp(result.normalMapStrength, 0.0f, 4.0f);
        result.alphaRef = std::clamp(result.alphaRef, 0.0f, 1.0f);
        result.emissiveColor.x = std::clamp(result.emissiveColor.x, 0.0f, 1.0f);
        result.emissiveColor.y = std::clamp(result.emissiveColor.y, 0.0f, 1.0f);
        result.emissiveColor.z = std::clamp(result.emissiveColor.z, 0.0f, 1.0f);
        result.emissiveStrength = std::clamp(result.emissiveStrength, 0.0f, 100.0f);

        result.texMulAdd.x = std::clamp(result.texMulAdd.x, 0.01f, 10.0f);
        result.texMulAdd.y = std::clamp(result.texMulAdd.y, 0.01f, 10.0f);
        result.texMulAdd.z = std::clamp(result.texMulAdd.z, -1000.0f, 1000.0f);
        result.texMulAdd.w = std::clamp(result.texMulAdd.w, -1000.0f, 1000.0f);

        result.parallaxOcclusionMapping = std::clamp(
            result.parallaxOcclusionMapping, 0.0f, 1.0f);
        result.anisotropyStrength = std::clamp(result.anisotropyStrength, 0.0f, 1.0f);
        result.anisotropyRotationDegrees = NormalizeDegrees(
            result.anisotropyRotationDegrees);
        result.sheenColor.x = std::clamp(result.sheenColor.x, 0.0f, 1.0f);
        result.sheenColor.y = std::clamp(result.sheenColor.y, 0.0f, 1.0f);
        result.sheenColor.z = std::clamp(result.sheenColor.z, 0.0f, 1.0f);
        result.sheenRoughness = std::clamp(result.sheenRoughness, 0.0f, 1.0f);
        result.clearcoat = std::clamp(result.clearcoat, 0.0f, 1.0f);
        result.clearcoatRoughness = std::clamp(result.clearcoatRoughness, 0.0f, 1.0f);
        result.transmission = std::clamp(result.transmission, 0.0f, 1.0f);
        result.refraction = std::clamp(result.refraction, 0.0f, 1.0f);
        result.blendWithTerrainHeight = std::clamp(
            result.blendWithTerrainHeight, 0.0f, 2.0f);
        result.meshBlend = std::clamp(result.meshBlend, 0.0f, 2.0f);

        result.interiorMappingScale.x = std::clamp(
            result.interiorMappingScale.x, 0.001f, 1000.0f);
        result.interiorMappingScale.y = std::clamp(
            result.interiorMappingScale.y, 0.001f, 1000.0f);
        result.interiorMappingScale.z = std::clamp(
            result.interiorMappingScale.z, 0.001f, 1000.0f);
        result.interiorMappingOffset.x = std::clamp(
            result.interiorMappingOffset.x, -1000.0f, 1000.0f);
        result.interiorMappingOffset.y = std::clamp(
            result.interiorMappingOffset.y, -1000.0f, 1000.0f);
        result.interiorMappingOffset.z = std::clamp(
            result.interiorMappingOffset.z, -1000.0f, 1000.0f);
        result.interiorMappingRotationDegrees = NormalizeDegrees(
            result.interiorMappingRotationDegrees);
        return result;
    }

    bool HasMaterialStateChange(
        const MaterialState& before,
        const MaterialState& after) noexcept
    {
        const auto left = SanitizeMaterialState(before);
        const auto right = SanitizeMaterialState(after);
        return left.shaderType != right.shaderType ||
            left.blendMode != right.blendMode ||
            !Same4(left.baseColor, right.baseColor) ||
            !NearlyEqual(left.metalness, right.metalness) ||
            !NearlyEqual(left.roughness, right.roughness) ||
            !NearlyEqual(left.reflectance, right.reflectance) ||
            !NearlyEqual(left.normalMapStrength, right.normalMapStrength) ||
            !NearlyEqual(left.alphaRef, right.alphaRef) ||
            !Same3(left.emissiveColor, right.emissiveColor) ||
            !NearlyEqual(left.emissiveStrength, right.emissiveStrength) ||
            left.receiveShadow != right.receiveShadow ||
            left.castShadow != right.castShadow ||
            left.useVertexColors != right.useVertexColors ||
            left.doubleSided != right.doubleSided ||
            !Same4(left.texMulAdd, right.texMulAdd) ||
            !NearlyEqual(left.parallaxOcclusionMapping, right.parallaxOcclusionMapping) ||
            !NearlyEqual(left.anisotropyStrength, right.anisotropyStrength) ||
            !NearlyEqual(left.anisotropyRotationDegrees, right.anisotropyRotationDegrees) ||
            !Same3(left.sheenColor, right.sheenColor) ||
            !NearlyEqual(left.sheenRoughness, right.sheenRoughness) ||
            !NearlyEqual(left.clearcoat, right.clearcoat) ||
            !NearlyEqual(left.clearcoatRoughness, right.clearcoatRoughness) ||
            !NearlyEqual(left.transmission, right.transmission) ||
            !NearlyEqual(left.refraction, right.refraction) ||
            !NearlyEqual(left.blendWithTerrainHeight, right.blendWithTerrainHeight) ||
            !NearlyEqual(left.meshBlend, right.meshBlend) ||
            !Same3(left.interiorMappingScale, right.interiorMappingScale) ||
            !Same3(left.interiorMappingOffset, right.interiorMappingOffset) ||
            !NearlyEqual(
                left.interiorMappingRotationDegrees,
                right.interiorMappingRotationDegrees);
    }

    void ApplyMaterial(
        wi::scene::MaterialComponent& material,
        const MaterialState& state) noexcept
    {
        const auto safe = SanitizeMaterialState(state);

        // Custom shader ownership is deliberately outside Gate 4. Preserve an
        // existing customShaderID while editing all other creator-facing state.
        material.shaderType = safe.shaderType;
        material.userBlendMode = safe.blendMode;
        material.SetBaseColor(safe.baseColor);
        material.SetMetalness(safe.metalness);
        material.SetRoughness(safe.roughness);
        material.SetReflectance(safe.reflectance);
        material.SetNormalMapStrength(safe.normalMapStrength);
        material.SetAlphaRef(safe.alphaRef);
        material.SetEmissiveColor(XMFLOAT4(
            safe.emissiveColor.x,
            safe.emissiveColor.y,
            safe.emissiveColor.z,
            safe.emissiveStrength));
        material.SetReceiveShadow(safe.receiveShadow);
        material.SetCastShadow(safe.castShadow);
        material.SetUseVertexColors(safe.useVertexColors);
        material.SetDoubleSided(safe.doubleSided);

        material.texMulAdd = safe.texMulAdd;
        material.SetParallaxOcclusionMapping(safe.parallaxOcclusionMapping);
        material.anisotropy_strength = safe.anisotropyStrength;
        material.anisotropy_rotation =
            safe.anisotropyRotationDegrees * DegreesToRadians;
        material.sheenColor.x = safe.sheenColor.x;
        material.sheenColor.y = safe.sheenColor.y;
        material.sheenColor.z = safe.sheenColor.z;
        material.sheenRoughness = safe.sheenRoughness;
        material.clearcoat = safe.clearcoat;
        material.clearcoatRoughness = safe.clearcoatRoughness;
        material.SetTransmissionAmount(safe.transmission);
        material.SetRefractionAmount(safe.refraction);
        material.blend_with_terrain_height = safe.blendWithTerrainHeight;
        material.mesh_blend = safe.meshBlend;
        material.SetInteriorMappingScale(safe.interiorMappingScale);
        material.SetInteriorMappingOffset(safe.interiorMappingOffset);
        material.SetInteriorMappingRotation(
            safe.interiorMappingRotationDegrees * DegreesToRadians);
        material.SetDirty();
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
            before_ = CaptureMaterial(*existing);
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
        if (scene_ == nullptr || IsTerrainOwnedMaterial(*scene_, materialEntity_))
            return false;
        auto* material = scene_->materials.GetComponent(materialEntity_);
        if (material == nullptr)
            return false;
        ApplyMaterial(*material, state);
        return true;
    }
}
