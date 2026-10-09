#pragma once

#include "renegade/bridge/MaterialImpactSurfaceService.h"

#include <string>

namespace renegade::bridge
{
    inline bool CanAuthorObjectImpactSurface(
        const wi::scene::Scene& scene, const wi::ecs::Entity entity) noexcept
    {
        if (entity == wi::ecs::INVALID_ENTITY)
            return false;
        if (scene.objects.Contains(entity) || scene.colliders.Contains(entity))
            return true;
        // Imported assets may be selected by their transform-only hierarchy root.
        if (!scene.transforms.Contains(entity))
            return false;
        for (std::size_t i = 0; i < scene.objects.GetCount(); ++i)
            if (scene.Entity_IsDescendant(scene.objects.GetEntity(i), entity))
                return true;
        for (std::size_t i = 0; i < scene.colliders.GetCount(); ++i)
            if (scene.Entity_IsDescendant(scene.colliders.GetEntity(i), entity))
                return true;
        return false;
    }

    inline bool HasObjectImpactSurface(
        const wi::scene::Scene& scene, const wi::ecs::Entity entity) noexcept
    {
        const auto* metadata = scene.metadatas.GetComponent(entity);
        return metadata != nullptr &&
            metadata->string_values.has(ImpactSurfaceMetadataKey);
    }

    inline ImpactSurfaceType CaptureObjectImpactSurface(
        const wi::scene::Scene& scene, const wi::ecs::Entity entity) noexcept
    {
        // The metadata belongs to this instance, never to its shared mesh.
        return CaptureMaterialImpactSurface(scene, entity);
    }

    inline bool ApplyObjectImpactSurface(
        wi::scene::Scene& scene, const wi::ecs::Entity entity,
        const ImpactSurfaceType type) noexcept
    {
        if (!CanAuthorObjectImpactSurface(scene, entity) ||
            static_cast<unsigned>(type) >=
                static_cast<unsigned>(ImpactSurfaceType::Character))
            return false;
        auto* metadata = scene.metadatas.GetComponent(entity);
        if (metadata == nullptr)
            metadata = &scene.metadatas.Create(entity);
        metadata->string_values.set(
            ImpactSurfaceMetadataKey, ImpactSurfaceTypeToken(type));
        return true;
    }

    inline ImpactSurfaceType ResolveObjectImpactSurface(
        const wi::scene::Scene& scene, const wi::ecs::Entity entity,
        const int subsetIndex = -1) noexcept
    {
        // Explicit Default is an authored response, not "inherit".
        auto current = entity;
        for (std::size_t remaining = scene.hierarchy.GetCount() + 1;
             current != wi::ecs::INVALID_ENTITY && remaining > 0; --remaining)
        {
            if (HasObjectImpactSurface(scene, current))
                return CaptureObjectImpactSurface(scene, current);
            const auto* hierarchy = scene.hierarchy.GetComponent(current);
            current = hierarchy == nullptr ? wi::ecs::INVALID_ENTITY : hierarchy->parentID;
        }
        const auto* object = scene.objects.GetComponent(entity);
        const auto* mesh = object == nullptr ? nullptr :
            scene.meshes.GetComponent(object->meshID);
        if (mesh == nullptr || subsetIndex < 0 ||
            static_cast<std::size_t>(subsetIndex) >= mesh->subsets.size())
            return ImpactSurfaceType::Default;
        return CaptureMaterialImpactSurface(
            scene, mesh->subsets[subsetIndex].materialID);
    }

    class SetObjectImpactSurfaceCommand final : public ICommand
    {
    public:
        SetObjectImpactSurfaceCommand(
            wi::scene::Scene& scene, const wi::ecs::Entity entity,
            const ImpactSurfaceType after)
            : scene_(&scene), entity_(entity), after_(after),
              hadBefore_(HasObjectImpactSurface(scene, entity))
        {
            if (hadBefore_)
                before_ = scene.metadatas.GetComponent(entity)->
                    string_values.get(ImpactSurfaceMetadataKey);
        }

        bool Execute() override
        {
            if (!CanAuthorObjectImpactSurface(*scene_, entity_) ||
                (hadBefore_ && before_ == ImpactSurfaceTypeToken(after_)))
                return false;
            return ApplyObjectImpactSurface(*scene_, entity_, after_);
        }

        void Undo() override
        {
            if (!CanAuthorObjectImpactSurface(*scene_, entity_))
                return;
            auto* metadata = scene_->metadatas.GetComponent(entity_);
            if (metadata == nullptr)
                return;
            if (hadBefore_)
                metadata->string_values.set(ImpactSurfaceMetadataKey, before_);
            else
                metadata->string_values.erase(ImpactSurfaceMetadataKey);
        }

    private:
        wi::scene::Scene* scene_;
        wi::ecs::Entity entity_;
        ImpactSurfaceType after_;
        bool hadBefore_;
        std::string before_;
    };
}
