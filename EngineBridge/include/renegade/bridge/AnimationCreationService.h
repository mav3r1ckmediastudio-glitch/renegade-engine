#pragma once

#include <string>

#include <WickedEngine.h>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    class CreateNativeAnimationCommand final : public ICommand
    {
    public:
        CreateNativeAnimationCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity owner,
            std::string name);

        bool Execute() override;
        void Undo() override;

        [[nodiscard]] wi::ecs::Entity CreatedEntity() const noexcept;

    private:
        bool Restore();

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity owner_ = wi::ecs::INVALID_ENTITY;
        std::string name_;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        wi::Archive snapshot_;
        bool snapshotReady_ = false;
    };

    class EnsureExpressionComponentCommand final : public ICommand
    {
    public:
        EnsureExpressionComponentCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity);

        bool Execute() override;
        void Undo() override;

    private:
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        bool created_ = false;
    };
}
