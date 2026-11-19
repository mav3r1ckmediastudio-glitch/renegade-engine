#pragma once
#include <WickedEngine.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace renegade::runtime
{
    // Runtime experiment. Keep a complete placement for every mesh in each
    // distant cell. Render parts of a multipart placement always move together.
    class RuntimePropCellResidency
    {
        using Entity = wi::ecs::Entity;
        using Cell = std::pair<int64_t, int64_t>;
        struct Part { Entity entity, mesh; float drawDistance; };
        struct Page {
            std::vector<Part> detail;
            size_t visible = 0;
            bool wanted = true;
            std::optional<std::list<Cell>::iterator> queued;
        };
        std::map<Cell, Page> pages_;
        std::set<Cell> near_;
        std::list<Cell> pending_;
        std::optional<Cell> center_;
        std::optional<uint64_t> revision_;
        static Cell CellAt(const DirectX::XMFLOAT3& p) {
            // Bound the conversion and leave room for neighbourhood offsets.
            const auto axis = [](float v) {
                return int64_t(std::clamp(std::floor(double(v) / 32.0), -1e15, 1e15));
            };
            return {axis(p.x), axis(p.z)};
        }
        void Schedule(Cell cell, Page& page, bool wanted) {
            if (page.queued) pending_.erase(*page.queued);
            page.wanted = wanted;
            page.queued.reset();
            if (page.visible == (wanted ? page.detail.size() : 0)) return;
            page.queued = pending_.insert(wanted ? pending_.begin() : pending_.end(), cell);
        }
        void Index(wi::scene::Scene& scene) {
            using Placement = std::pair<std::string,std::string>;
            std::map<Placement, std::vector<Part>> placements;
            for (size_t i = 0; i < scene.objects.GetCount(); ++i) {
                const auto entity = scene.objects.GetEntity(i);
                const auto* m = scene.metadatas.GetComponent(entity);
                if (!m || !m->runtimeProjectionOnly || m->string_values.has("renegade.terrain_props.coarse") ||
                    m->string_values.has("renegade.terrain_props.fine") || !m->string_values.has("renegade.terrain_props.placement") ||
                    !m->string_values.has("renegade.terrain_props.layer")) continue;
                placements[{m->string_values.get("renegade.terrain_props.layer"), m->string_values.get("renegade.terrain_props.placement")}]
                    .push_back({entity, scene.objects[i].meshID, scene.objects[i].draw_distance});
            }
            std::map<Cell, std::set<Entity>> represented;
            for (const auto& entry : placements) {
                const auto& parts = entry.second;
                const auto* transform = scene.transforms.GetComponent(parts.front().entity);
                if (!transform) continue;
                const auto p = transform->GetPosition();
                if (!std::isfinite(p.x) || !std::isfinite(p.z)) continue;
                const Cell cell = CellAt(p);
                auto& meshes = represented[cell];
                bool retain = false;
                for (const auto& part : parts) retain |= meshes.insert(part.mesh).second;
                if (retain) continue;
                auto& page = pages_[cell];
                page.detail.insert(page.detail.end(), parts.begin(), parts.end());
            }
            for (auto& entry : pages_) {
                entry.second.visible = entry.second.detail.size();
                Schedule(entry.first, entry.second, false);

            }
        }
    public:
        void Update(wi::scene::Scene& scene, uint64_t revision, const DirectX::XMFLOAT3& position) {
            if (!std::isfinite(position.x) || !std::isfinite(position.z)) return;
            if (revision_ != revision) {
                pages_.clear(); pending_.clear(); near_.clear(); center_.reset();
                revision_ = revision;
                Index(scene);
            }
            const Cell center = CellAt(position);
            if (center_ != center) {
                std::set<Cell> next;
                for (auto x = center.first - 3; x <= center.first + 3; ++x)
                    for (auto z = center.second - 3; z <= center.second + 3; ++z)
                        if (pages_.count({x,z})) next.emplace(x,z);
                for (const auto& c : near_) if (!next.count(c)) {
                    Schedule(c, pages_.at(c), false);
                }
                for (const auto& c : next) if (!near_.count(c)) {
                    Schedule(c, pages_.at(c), true);
                }
                near_.swap(next); center_ = center;
            }
            size_t budget = 128;
            while (budget && !pending_.empty()) {
                auto& page = pages_.at(pending_.front());
                const size_t target = page.wanted ? page.detail.size() : 0;
                if (page.visible == target) { page.queued.reset(); pending_.pop_front(); --budget; continue; }
                if (page.wanted) {
                    const auto& part = page.detail[page.visible++];
                    if (scene.transforms.Contains(part.entity) && scene.meshes.Contains(part.mesh) && !scene.objects.Contains(part.entity)) {
                        auto& object = scene.objects.Create(part.entity);
                        object.meshID = part.mesh;
                        object.draw_distance = part.drawDistance;
                    }
                } else {
                    scene.objects.Remove(page.detail[--page.visible].entity);
                }
                --budget;
            }
        }
        bool Settled() const { return pending_.empty(); }
    };
}
