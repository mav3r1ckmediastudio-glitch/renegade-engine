#pragma once

#include <vector>

#include <WickedEngine.h>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    struct MaterialState
    {
        wi::scene::MaterialComponent::SHADERTYPE shaderType =
            wi::scene::MaterialComponent::SHADERTYPE_PBR;
        wi::enums::BLENDMODE blendMode = wi::enums::BLENDMODE_OPAQUE;

        XMFLOAT4 baseColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        float metalness = 0.0f;
        float roughness = 0.5f;
        float reflectance = 0.5f;
        float normalMapStrength = 1.0f;
        float alphaRef = 1.0f;
        XMFLOAT3 emissiveColor = XMFLOAT3(0.0f, 0.0f, 0.0f);
        float emissiveStrength = 0.0f;

        bool receiveShadow = true;
        bool castShadow = true;
        bool useVertexColors = false;
        bool doubleSided = false;

        // Native Wicked UV multiplier/addition: XY = tiling, ZW = offset.
        XMFLOAT4 texMulAdd = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);

        float parallaxOcclusionMapping = 0.0f;
        float anisotropyStrength = 0.0f;
        float anisotropyRotationDegrees = 0.0f;
        XMFLOAT3 sheenColor = XMFLOAT3(1.0f, 1.0f, 1.0f);
        float sheenRoughness = 0.0f;
        float clearcoat = 0.0f;
        float clearcoatRoughness = 0.0f;
        float transmission = 0.0f;
        float refraction = 0.0f;
        float blendWithTerrainHeight = 0.0f;
        float meshBlend = 0.0f;
        XMFLOAT3 interiorMappingScale = XMFLOAT3(1.0f, 1.0f, 1.0f);
        XMFLOAT3 interiorMappingOffset = XMFLOAT3(0.0f, 0.0f, 0.0f);
        float interiorMappingRotationDegrees = 0.0f;
    };

    [[nodiscard]] bool IsTerrainOwnedMaterial(
        const wi::scene::Scene& scene,
        wi::ecs::Entity materialEntity) noexcept;

    // Returns every distinct editable material referenced by the selected
    // material, mesh/object, or descendant render objects of a reusable/root
    // selection. Terrain-owned materials are excluded and mesh subset bindings
    // are never rewritten or flattened.
    [[nodiscard]] std::vector<wi::ecs::Entity> CollectEditableMaterialEntities(
        const wi::scene::Scene& scene,
        wi::ecs::Entity selectedEntity);

    // Compatibility/single-target helper: returns the only editable material
    // when selection resolves unambiguously, otherwise INVALID_ENTITY.
    [[nodiscard]] wi::ecs::Entity ResolveEditableMaterialEntity(
        const wi::scene::Scene& scene,
        wi::ecs::Entity selectedEntity) noexcept;

    [[nodiscard]] MaterialState CaptureMaterial(
        const wi::scene::MaterialComponent& material) noexcept;
    [[nodiscard]] MaterialState SanitizeMaterialState(
        const MaterialState& state) noexcept;
    [[nodiscard]] bool HasMaterialStateChange(
        const MaterialState& before,
        const MaterialState& after) noexcept;

    // Applies only the Gate 4 creator-facing native material surface. Texture
    // resources/bindings remain owned by MaterialTextureAssetService and all
    // unexposed renderer/internal fields remain untouched.
    void ApplyMaterial(
        wi::scene::MaterialComponent& material,
        const MaterialState& state) noexcept;

    class SetMaterialCommand final : public ICommand
    {
    public:
        SetMaterialCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity materialEntity,
            const MaterialState& material);
        SetMaterialCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity materialEntity,
            const MaterialState& before,
            const MaterialState& after);

        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const MaterialState& state) noexcept;

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity materialEntity_ = wi::ecs::INVALID_ENTITY;
        MaterialState before_;
        MaterialState after_;
    };
}
