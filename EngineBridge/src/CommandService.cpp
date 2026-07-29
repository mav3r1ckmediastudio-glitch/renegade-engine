#include "renegade/bridge/CommandService.h"

#include <utility>

namespace renegade::bridge
{
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

    bool SetTranslationCommand::Execute()
    {
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
}
