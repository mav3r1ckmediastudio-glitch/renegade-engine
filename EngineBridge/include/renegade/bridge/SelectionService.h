#pragma once

#include <WickedEngine.h>

namespace renegade::bridge
{
    class SelectionService
    {
    public:
        void BindScene(const wi::scene::Scene& scene) noexcept { scene_ = &scene; }
        void Select(wi::ecs::Entity entity) noexcept;
        void Clear() noexcept;

        [[nodiscard]] bool HasSelection() const noexcept;
        [[nodiscard]] wi::ecs::Entity SelectedEntity() const noexcept;

    private:
        const wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity selectedEntity_ = wi::ecs::INVALID_ENTITY;
    };
}
