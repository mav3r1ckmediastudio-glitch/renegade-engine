#include "renegade/bridge/CommandService.h"

#include <cmath>
#include <utility>

namespace
{
    constexpr float transformEpsilon = 0.00001f;

    bool NearlyEqual(const float before, const float after) noexcept
    {
        return std::abs(after - before) <= transformEpsilon;
    }

    bool IsMeaningfulTranslation(
        const XMFLOAT3& before,
        const XMFLOAT3& after) noexcept
    {
        return !NearlyEqual(after.x, before.x) ||
            !NearlyEqual(after.y, before.y) ||
            !NearlyEqual(after.z, before.z);
    }

    bool IsMeaningfulTransform(
        const renegade::bridge::TransformState& before,
        const renegade::bridge::TransformState& after) noexcept
    {
        return IsMeaningfulTranslation(
                before.translation,
                after.translation) ||
            !NearlyEqual(before.rotation.x, after.rotation.x) ||
            !NearlyEqual(before.rotation.y, after.rotation.y) ||
            !NearlyEqual(before.rotation.z, after.rotation.z) ||
            !NearlyEqual(before.rotation.w, after.rotation.w) ||
            !NearlyEqual(before.scale.x, after.scale.x) ||
            !NearlyEqual(before.scale.y, after.scale.y) ||
            !NearlyEqual(before.scale.z, after.scale.z);
    }

    bool EntityExists(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity)
    {
        if (entity == wi::ecs::INVALID_ENTITY)
        {
            return false;
        }

        wi::unordered_set<wi::ecs::Entity> entities;
        scene.FindAllEntities(entities);
        return entities.count(entity) != 0;
    }
}

namespace renegade::bridge
{
    TransformState CaptureTransform(
        const wi::scene::TransformComponent& transform) noexcept
    {
        TransformState state;
        state.translation = transform.translation_local;
        state.rotation = transform.rotation_local;
        state.scale = transform.scale_local;
        return state;
    }

    bool CommandService::Execute(std::unique_ptr<ICommand> command)
    {
        if (command == nullptr || !command->Execute())
        {
            return false;
        }

        undoStack_.push_back(std::move(command));
        redoStack_.clear();
        return true;
    }

    bool CommandService::Undo()
    {
        if (undoStack_.empty())
        {
            return false;
        }

        auto command = std::move(undoStack_.back());
        undoStack_.pop_back();
        command->Undo();
        redoStack_.push_back(std::move(command));
        return true;
    }

    bool CommandService::Redo()
    {
        if (redoStack_.empty())
        {
            return false;
        }

        auto command = std::move(redoStack_.back());
        redoStack_.pop_back();
        if (!command->Execute())
        {
            redoStack_.push_back(std::move(command));
            return false;
        }

        undoStack_.push_back(std::move(command));
        return true;
    }

    void CommandService::Clear() noexcept
    {
        undoStack_.clear();
        redoStack_.clear();
    }

    bool CommandService::CanUndo() const noexcept
    {
        return !undoStack_.empty();
    }

    bool CommandService::CanRedo() const noexcept
    {
        return !redoStack_.empty();
    }

    std::size_t CommandService::UndoCount() const noexcept
    {
        return undoStack_.size();
    }

    std::size_t CommandService::RedoCount() const noexcept
    {
        return redoStack_.size();
    }

    SetTranslationCommand::SetTranslationCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const XMFLOAT3& translation)
        : scene_(&scene)
        , entity_(entity)
        , after_(translation)
    {
        const auto* transform = scene.transforms.GetComponent(entity);
        before_ = transform == nullptr ? XMFLOAT3{} : transform->translation_local;
    }

    SetTranslationCommand::SetTranslationCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const XMFLOAT3& before,
        const XMFLOAT3& after)
        : scene_(&scene)
        , entity_(entity)
        , before_(before)
        , after_(after)
    {
    }

    bool SetTranslationCommand::Execute()
    {
        if (!IsMeaningfulTranslation(before_, after_))
        {
            return false;
        }
        return Apply(after_);
    }

    void SetTranslationCommand::Undo()
    {
        Apply(before_);
    }

    bool SetTranslationCommand::Apply(const XMFLOAT3& translation)
    {
        auto* transform = scene_->transforms.GetComponent(entity_);
        if (transform == nullptr)
        {
            return false;
        }

        transform->translation_local = translation;
        transform->SetDirty();
        transform->UpdateTransform();
        return true;
    }

    SetTransformCommand::SetTransformCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const TransformState& transform)
        : scene_(&scene)
        , entity_(entity)
        , after_(transform)
    {
        const auto* existing = scene.transforms.GetComponent(entity);
        before_ = existing == nullptr
            ? TransformState{}
            : CaptureTransform(*existing);
    }

    SetTransformCommand::SetTransformCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const TransformState& before,
        const TransformState& after)
        : scene_(&scene)
        , entity_(entity)
        , before_(before)
        , after_(after)
    {
    }

    bool SetTransformCommand::Execute()
    {
        if (!IsMeaningfulTransform(before_, after_))
        {
            return false;
        }
        return Apply(after_);
    }

    void SetTransformCommand::Undo()
    {
        Apply(before_);
    }

    bool SetTransformCommand::Apply(const TransformState& transformState)
    {
        auto* transform = scene_->transforms.GetComponent(entity_);
        if (transform == nullptr)
        {
            return false;
        }

        transform->translation_local = transformState.translation;
        transform->rotation_local = transformState.rotation;
        transform->scale_local = transformState.scale;
        transform->SetDirty();
        transform->UpdateTransform();
        return true;
    }

    DuplicateEntityCommand::DuplicateEntityCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity source)
        : scene_(&scene)
        , source_(source)
    {
    }

    bool DuplicateEntityCommand::Execute()
    {
        if (!hasSnapshot_)
        {
            if (!EntityExists(*scene_, source_))
            {
                return false;
            }

            duplicate_ = scene_->Entity_Duplicate(source_);
            if (duplicate_ == wi::ecs::INVALID_ENTITY)
            {
                return false;
            }

            if (auto* name = scene_->names.GetComponent(duplicate_))
            {
                name->name += " Copy";
            }

            snapshot_.SetReadModeAndResetPos(false);
            wi::ecs::EntitySerializer serializer;
            scene_->Entity_Serialize(snapshot_, serializer, duplicate_);
            hasSnapshot_ = true;
            scene_->Update(0.0f);
            return true;
        }

        if (EntityExists(*scene_, duplicate_))
        {
            return false;
        }

        snapshot_.SetReadModeAndResetPos(true);
        wi::ecs::EntitySerializer serializer;
        serializer.allow_remap = false;
        const auto restored =
            scene_->Entity_Serialize(snapshot_, serializer);
        scene_->Update(0.0f);
        return restored == duplicate_;
    }

    void DuplicateEntityCommand::Undo()
    {
        if (EntityExists(*scene_, duplicate_))
        {
            scene_->Entity_Remove(duplicate_);
            scene_->Update(0.0f);
        }
    }

    wi::ecs::Entity DuplicateEntityCommand::DuplicatedEntity() const noexcept
    {
        return duplicate_;
    }

    DeleteEntityCommand::DeleteEntityCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity)
        : scene_(&scene)
        , entity_(entity)
    {
    }

    bool DeleteEntityCommand::Execute()
    {
        if (!EntityExists(*scene_, entity_))
        {
            return false;
        }

        if (!hasSnapshot_)
        {
            snapshot_.SetReadModeAndResetPos(false);
            wi::ecs::EntitySerializer serializer;
            scene_->Entity_Serialize(snapshot_, serializer, entity_);
            hasSnapshot_ = true;
        }

        scene_->Entity_Remove(entity_);
        scene_->Update(0.0f);
        return true;
    }

    void DeleteEntityCommand::Undo()
    {
        if (!hasSnapshot_ || EntityExists(*scene_, entity_))
        {
            return;
        }

        snapshot_.SetReadModeAndResetPos(true);
        wi::ecs::EntitySerializer serializer;
        serializer.allow_remap = false;
        scene_->Entity_Serialize(snapshot_, serializer);
        scene_->Update(0.0f);
    }
}
