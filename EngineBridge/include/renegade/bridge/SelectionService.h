#pragma once

#include <WickedEngine.h>

namespace renegade::bridge
{
    class SelectionService
    {
    public:
        void Select(wi::ecs::Entity entity) noexcept;
        void Clear() noexcept;

        [[nodiscard]] bool HasSelection() const noexcept;
        [[nodiscard]] wi::ecs::Entity SelectedEntity() const noexcept;

    private:
        wi::ecs::Entity selectedEntity_ = wi::ecs::INVALID_ENTITY;
    };
}
