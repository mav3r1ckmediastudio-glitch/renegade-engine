#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include <WickedEngine.h>

namespace renegade::bridge
{
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
}
