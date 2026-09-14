#pragma once

#include "renegade/bridge/CharacterService.h"
#include "renegade/bridge/PatrolRouteService.h"

#include <memory>
#include <utility>

namespace renegade::bridge
{
    // One creator-facing transaction: create a governed Patrol Route and bind
    // it to an existing Character by persistent StableId. Undo restores the
    // previous Character settings and removes the command-owned route.
    class CreateCharacterPatrolRouteCommand final : public ICommand
    {
    public:
        CreateCharacterPatrolRouteCommand(
            wi::scene::Scene& scene,
            const wi::ecs::Entity characterEntity,
            const XMFLOAT3 position,
            PatrolRouteSettings settings = {})
            : scene_(&scene),
              characterEntity_(characterEntity),
              routeCommand_(scene, position, settings)
        {
        }

        bool Execute() override
        {
            if (scene_ == nullptr ||
                characterEntity_ == wi::ecs::INVALID_ENTITY ||
                !IsRenegadeCharacter(*scene_, characterEntity_))
            {
                return false;
            }

            if (!routeCommand_.Execute())
                return false;

            auto after = CaptureCharacterSettings(*scene_, characterEntity_);
            after.patrolRouteEntityId = routeCommand_.CreatedStableId();
            assignmentCommand_ = std::make_unique<SetCharacterSettingsCommand>(
                *scene_, characterEntity_, std::move(after));
            if (!assignmentCommand_->Execute())
            {
                assignmentCommand_.reset();
                routeCommand_.Undo();
                return false;
            }
            executed_ = true;
            return true;
        }

        void Undo() override
        {
            if (!executed_)
                return;
            if (assignmentCommand_ != nullptr)
                assignmentCommand_->Undo();
            routeCommand_.Undo();
            assignmentCommand_.reset();
            executed_ = false;
        }

        [[nodiscard]] wi::ecs::Entity CreatedRouteEntity() const noexcept
        {
            return routeCommand_.CreatedEntity();
        }

        [[nodiscard]] const StableId& CreatedRouteStableId() const noexcept
        {
            return routeCommand_.CreatedStableId();
        }

    private:
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity characterEntity_ = wi::ecs::INVALID_ENTITY;
        CreatePatrolRouteCommand routeCommand_;
        std::unique_ptr<SetCharacterSettingsCommand> assignmentCommand_;
        bool executed_ = false;
    };
}
