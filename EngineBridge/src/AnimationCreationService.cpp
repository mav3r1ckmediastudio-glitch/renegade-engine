#include "renegade/bridge/AnimationCreationService.h"

#include "renegade/bridge/AnimationService.h"

namespace renegade::bridge
{
    namespace
    {
        bool EntityExists(
            const wi::scene::Scene& scene,
            const wi::ecs::Entity entity)
        {
            if (entity == wi::ecs::INVALID_ENTITY)
                return false;
            wi::unordered_set<wi::ecs::Entity> entities;
            scene.FindAllEntities(entities);
            return entities.count(entity) != 0;
        }
    }

    CreateNativeAnimationCommand::CreateNativeAnimationCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity owner,
        std::string name)
        : scene_(&scene)
        , owner_(owner)
        , name_(std::move(name))
    {
    }

    bool CreateNativeAnimationCommand::Execute()
    {
        if (scene_ == nullptr)
            return false;
        if (entity_ == wi::ecs::INVALID_ENTITY)
        {
            entity_ = CreateNativeAnimation(*scene_, owner_, name_);
            return entity_ != wi::ecs::INVALID_ENTITY;
        }
        return Restore();
    }

    void CreateNativeAnimationCommand::Undo()
    {
        if (scene_ == nullptr || entity_ == wi::ecs::INVALID_ENTITY ||
            !EntityExists(*scene_, entity_))
        {
            return;
        }
        if (!snapshotReady_)
        {
            snapshot_.SetReadModeAndResetPos(false);
            wi::ecs::EntitySerializer serializer;
            scene_->Entity_Serialize(snapshot_, serializer, entity_);
            snapshotReady_ = true;
        }
        scene_->Entity_Remove(entity_);
    }

    wi::ecs::Entity CreateNativeAnimationCommand::CreatedEntity() const noexcept
    {
        return entity_;
    }

    bool CreateNativeAnimationCommand::Restore()
    {
        if (!snapshotReady_ || EntityExists(*scene_, entity_))
            return false;
        snapshot_.SetReadModeAndResetPos(true);
        wi::ecs::EntitySerializer serializer;
        serializer.allow_remap = false;
        return scene_->Entity_Serialize(snapshot_, serializer) == entity_;
    }

    EnsureExpressionComponentCommand::EnsureExpressionComponentCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity)
        : scene_(&scene)
        , entity_(entity)
    {
    }

    bool EnsureExpressionComponentCommand::Execute()
    {
        if (scene_ == nullptr || entity_ == wi::ecs::INVALID_ENTITY ||
            scene_->expressions.Contains(entity_) || !EntityExists(*scene_, entity_))
        {
            return false;
        }
        scene_->expressions.Create(entity_);
        created_ = true;
        return true;
    }

    void EnsureExpressionComponentCommand::Undo()
    {
        if (scene_ != nullptr && created_)
            scene_->expressions.Remove(entity_);
    }
}
