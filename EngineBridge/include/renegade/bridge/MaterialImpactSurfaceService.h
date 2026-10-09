#pragma once

#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/ImpactSurface.h"

#include <WickedEngine.h>

namespace renegade::bridge
{
    inline ImpactSurfaceType CaptureMaterialImpactSurface(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity materialEntity) noexcept
    {
        const auto* metadata = scene.metadatas.GetComponent(materialEntity);
        if (metadata == nullptr ||
            !metadata->string_values.has(ImpactSurfaceMetadataKey))
            return ImpactSurfaceType::Default;

        ImpactSurfaceType type = ImpactSurfaceType::Default;
        if (!ParseImpactSurfaceType(
                metadata->string_values.get(ImpactSurfaceMetadataKey), type))
            return ImpactSurfaceType::Default;
        return type == ImpactSurfaceType::Character
            ? ImpactSurfaceType::Default : type;
    }

    inline bool ApplyMaterialImpactSurface(
        wi::scene::Scene& scene,
        const wi::ecs::Entity materialEntity,
        const ImpactSurfaceType type) noexcept
    {
        if (!scene.materials.Contains(materialEntity) ||
            type == ImpactSurfaceType::Character)
            return false;
        auto* metadata = scene.metadatas.GetComponent(materialEntity);
        if (metadata == nullptr)
            metadata = &scene.metadatas.Create(materialEntity);
        metadata->string_values.set(
            ImpactSurfaceMetadataKey, ImpactSurfaceTypeToken(type));
        return true;
    }

    class SetMaterialImpactSurfaceCommand final : public ICommand
    {
    public:
        SetMaterialImpactSurfaceCommand(
            wi::scene::Scene& scene,
            const wi::ecs::Entity materialEntity,
            const ImpactSurfaceType after) noexcept
            : scene_(&scene)
            , materialEntity_(materialEntity)
            , before_(CaptureMaterialImpactSurface(scene, materialEntity))
            , after_(after)
        {
        }

        bool Execute() override
        {
            if (scene_ == nullptr || before_ == after_ ||
                after_ == ImpactSurfaceType::Character)
                return false;
            return ApplyMaterialImpactSurface(*scene_, materialEntity_, after_);
        }

        void Undo() override
        {
            if (scene_ != nullptr)
                (void)ApplyMaterialImpactSurface(
                    *scene_, materialEntity_, before_);
        }

    private:
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity materialEntity_ = wi::ecs::INVALID_ENTITY;
        ImpactSurfaceType before_ = ImpactSurfaceType::Default;
        ImpactSurfaceType after_ = ImpactSurfaceType::Default;
    };
}
