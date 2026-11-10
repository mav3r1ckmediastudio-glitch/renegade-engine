#pragma once
#include <WickedEngine.h>
#include <vector>
#include <cstdint>
namespace renegade::bridge {
class SelectionService {
public:
    void Select(wi::ecs::Entity entity);
    void Toggle(wi::ecs::Entity entity);
    void Set(std::vector<wi::ecs::Entity> entities);
    void Clear() noexcept;
    void Prune(const wi::scene::Scene& scene);
    bool HasSelection() const noexcept { return !entities_.empty(); }
    wi::ecs::Entity SelectedEntity() const noexcept { return entities_.empty()?wi::ecs::INVALID_ENTITY:entities_.back(); }
    bool Contains(wi::ecs::Entity entity) const noexcept;
    const std::vector<wi::ecs::Entity>& Entities() const noexcept { return entities_; }
    std::uint64_t Revision() const noexcept { return revision_; }
private:
    std::vector<wi::ecs::Entity> entities_;
    std::uint64_t revision_ = 0;
};
}
