#include "renegade/bridge/SelectionService.h"

namespace renegade::bridge
{
    void SelectionService::Select(const wi::ecs::Entity entity) noexcept
    {
        selectedEntity_ = entity;
    }

    void SelectionService::Clear() noexcept
    {
        selectedEntity_ = wi::ecs::INVALID_ENTITY;
    }

    bool SelectionService::HasSelection() const noexcept
    {
        return selectedEntity_ != wi::ecs::INVALID_ENTITY;
    }

    wi::ecs::Entity SelectionService::SelectedEntity() const noexcept
    {
        return selectedEntity_;
    }
}
