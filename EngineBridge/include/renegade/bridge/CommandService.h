#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include <WickedEngine.h>

namespace renegade::bridge
{
    struct TransformState
    {
        XMFLOAT3 translation = {};
        XMFLOAT4 rotation = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        XMFLOAT3 scale = XMFLOAT3(1.0f, 1.0f, 1.0f);
    };

    [[nodiscard]] TransformState CaptureTransform(
        const wi::scene::TransformComponent& transform) noexcept;

    class ICommand
    {
    public:
        virtual ~ICommand() = default;
        virtual bool Execute() = 0;
        virtual void Undo() = 0;
    };

    class CommandService
    {
    public:
        bool Execute(std::unique_ptr<ICommand> command);
        bool Undo();
        bool Redo();
        void Clear() noexcept;

        [[nodiscard]] bool CanUndo() const noexcept;
        [[nodiscard]] bool CanRedo() const noexcept;
        [[nodiscard]] std::size_t UndoCount() const noexcept;
        [[nodiscard]] std::size_t RedoCount() const noexcept;

    private:
        std::vector<std::unique_ptr<ICommand>> undoStack_;
        std::vector<std::unique_ptr<ICommand>> redoStack_;
    };

    class SetTranslationCommand final : public ICommand
    {
    public:
        SetTranslationCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            const XMFLOAT3& translation);
        SetTranslationCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            const XMFLOAT3& before,
            const XMFLOAT3& after);

        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const XMFLOAT3& translation);

        wi::scene::Scene* scene_;
        wi::ecs::Entity entity_;
        XMFLOAT3 before_;
        XMFLOAT3 after_;
    };

    class SetTransformCommand final : public ICommand
    {
    public:
        SetTransformCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            const TransformState& transform);
        SetTransformCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            const TransformState& before,
            const TransformState& after);

        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const TransformState& transform);

        wi::scene::Scene* scene_;
        wi::ecs::Entity entity_;
        TransformState before_;
        TransformState after_;
    };

    class DuplicateEntityCommand final : public ICommand
    {
    public:
        DuplicateEntityCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity source);

        bool Execute() override;
        void Undo() override;

        [[nodiscard]] wi::ecs::Entity DuplicatedEntity() const noexcept;

    private:
        wi::scene::Scene* scene_;
        wi::ecs::Entity source_;
        wi::ecs::Entity duplicate_ = wi::ecs::INVALID_ENTITY;
        wi::Archive snapshot_;
        bool hasSnapshot_ = false;
    };

    class DeleteEntityCommand final : public ICommand
    {
    public:
        DeleteEntityCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity);

        bool Execute() override;
        void Undo() override;

    private:
        wi::scene::Scene* scene_;
        wi::ecs::Entity entity_;
        wi::Archive snapshot_;
        bool hasSnapshot_ = false;
    };
}
