#include "renegade/bridge/SelectionService.h"
#include "renegade/bridge/ParticleEffectService.h"

namespace renegade::bridge
{
    void SelectionService::Select(const wi::ecs::Entity entity) noexcept
    {
        // Effect layers are implementation details of one placed authoring
        // object. Resolve at the common selection boundary, before shortcuts,
        // inspector transforms or the native gizmo can act on a child.
        const auto root = scene_ ? ParticleEffectRoot(*scene_, entity) : wi::ecs::INVALID_ENTITY;
        selectedEntity_ = root == wi::ecs::INVALID_ENTITY ? entity : root;
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
