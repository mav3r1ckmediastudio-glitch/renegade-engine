#pragma once

#include <WickedEngine.h>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    struct MaterialState
    {
        XMFLOAT4 baseColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        float metalness = 0.0f;
        float roughness = 0.5f;
        float reflectance = 0.5f;
        XMFLOAT3 emissiveColor = XMFLOAT3(0.0f, 0.0f, 0.0f);
        float emissiveStrength = 0.0f;
    };

    [[nodiscard]] bool IsTerrainOwnedMaterial(
        const wi::scene::Scene& scene,
        wi::ecs::Entity materialEntity) noexcept;

    // Returns a material directly attached to the selected entity, or the one
    // unique material shared by all subsets of its referenced mesh. Ambiguous,
    // missing and terrain-owned targets are deliberately rejected.
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

    // Applies only the curated core PBR values. Textures, shader mode, blend
    // state, opacity semantics and every unexposed field remain untouched.
    // Wicked's setters intentionally mark an ordinary material dirty.
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
