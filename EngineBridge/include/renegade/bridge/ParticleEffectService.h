#pragma once

#include <string>
#include <vector>

#include <WickedEngine.h>

#include "renegade/bridge/ParticleEmitterService.h"

namespace renegade::bridge
{
    inline constexpr const char* ParticleEffectRootMetadataKey =
        "renegade.particle_effect.root";
    inline constexpr const char* ParticleEffectLayerMetadataKey =
        "renegade.particle_effect.layer";
    inline constexpr const char* ParticleEffectMetadataVersion = "1";

    struct ParticleEffectLayer
    {
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        std::string name;
    };

    [[nodiscard]] bool IsParticleEffectRoot(
        const wi::scene::Scene& scene,
        wi::ecs::Entity entity) noexcept;

    [[nodiscard]] bool IsParticleEffectLayer(
        const wi::scene::Scene& scene,
        wi::ecs::Entity entity) noexcept;

    [[nodiscard]] wi::ecs::Entity ParticleEffectRootForLayer(
        const wi::scene::Scene& scene,
        wi::ecs::Entity layer) noexcept;

    [[nodiscard]] std::vector<ParticleEffectLayer> CollectParticleEffectLayers(
        const wi::scene::Scene& scene,
        wi::ecs::Entity effectRoot);

    class CreateParticleEffectCommand final : public ICommand
    {
    public:
        CreateParticleEffectCommand(
            wi::scene::Scene& scene,
            const XMFLOAT3& position,
            std::string name = "Particle Effect");

        bool Execute() override;
        void Undo() override;

        [[nodiscard]] wi::ecs::Entity RootEntity() const noexcept
        {
            return root_;
        }
        [[nodiscard]] wi::ecs::Entity FirstLayerEntity() const noexcept
        {
            return firstLayer_;
        }

    private:
        wi::scene::Scene* scene_ = nullptr;
        XMFLOAT3 position_ = {};
        std::string requestedName_;
        wi::ecs::Entity root_ = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity firstLayer_ = wi::ecs::INVALID_ENTITY;
        wi::Archive rootSnapshot_;
        wi::Archive layerSnapshot_;
        bool hasSnapshot_ = false;
    };

    class AddParticleEffectLayerCommand final : public ICommand
    {
    public:
        AddParticleEffectLayerCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity effectRoot,
            ParticleEmitterState state = MakeNewParticleEmitterState(),
            TransformState localTransform = {});

        bool Execute() override;
        void Undo() override;

        [[nodiscard]] wi::ecs::Entity CreatedLayer() const noexcept
        {
            return layer_;
        }

    private:
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity root_ = wi::ecs::INVALID_ENTITY;
        ParticleEmitterState state_;
        TransformState localTransform_;
        wi::ecs::Entity layer_ = wi::ecs::INVALID_ENTITY;
        wi::Archive snapshot_;
        bool hasSnapshot_ = false;
    };
}
