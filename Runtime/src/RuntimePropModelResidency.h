#pragma once
#include "renegade/bridge/TerrainPropCollisionWorld.h"
#include "renegade/bridge/TerrainPropDetailProjection.h"
#include <charconv>
#include <cmath>
#include <algorithm>
#include <deque>
#include <map>
#include <optional>
#include <set>

namespace renegade::runtime
{
    class RuntimePropModelResidency
    {
        using Cell = bridge::TerrainPropProjection::Cell;
        using Placement = std::pair<bridge::StableId, uint64_t>;
        struct Batch {
            bridge::TerrainPropDetailProjection* model = nullptr;
            bridge::TerrainPropPage page;
            std::vector<wi::ecs::Entity> coarse;
            bool fine = false;
        };
        std::vector<std::unique_ptr<bridge::TerrainPropDetailProjection>> models_;
        std::map<Cell, std::map<size_t,Batch>> cells_;
        std::set<Cell> active_;
        std::deque<Cell> pending_;
        std::optional<Cell> center_;
        std::optional<uint64_t> revision_;
        static Cell At(float x, float z) {
            const auto axis = [](float value) {
                return static_cast<int32_t>(std::clamp(std::floor(double(value)/32.0), -2147483640.0, 2147483640.0));
            };
            return {axis(x),axis(z)};
        }
        void Index(wi::scene::Scene& scene, const bridge::TerrainPropCollisionWorld& world,
            const std::string& root, const bridge::StableId& project) {
            std::map<Placement,Batch*> byPlacement;
            for (const auto& doc : world.Documents()) {
                std::map<bridge::TerrainPropProfileId,size_t> managed;
                for (const auto& p : doc.profiles) {
                    if (p.coarseModelAssetId.empty() || p.coarseModelAssetId == p.modelAssetId) continue;
                    managed.emplace(p.id,models_.size());
                    models_.push_back(std::make_unique<bridge::TerrainPropDetailProjection>(scene,doc.layerId,p,
                        bridge::ReusableModelPlacementRequest{root,project,p.modelAssetId}));
                }
                if (managed.empty()) continue;
                for (const auto& page : doc.pages) for (auto p : page.placements) {
                    const auto profile = page.profiles.at(p.profileIndex);
                    const auto model = managed.find(profile);
                    if (model == managed.end()) continue;
                    const auto cell = At(p.position[0],p.position[2]);
                    auto& batch = cells_[cell][model->second];
                    if (!batch.model) {
                        batch.model = models_[model->second].get();
                        batch.page.cellX = cell.first;
                        batch.page.cellZ = cell.second;
                        batch.page.profiles.push_back(profile);
                    }
                    p.profileIndex = 0;
                    batch.page.placements.push_back(p);
                    byPlacement.emplace(Placement{doc.layerId,p.id},&batch);
                }
            }
            if (byPlacement.empty()) return;
            for (size_t i=0; i<scene.objects.GetCount(); ++i) {
                const auto e = scene.objects.GetEntity(i);
                const auto* m = scene.metadatas.GetComponent(e);
                if (!m || !m->runtimeProjectionOnly || !m->string_values.has("renegade.terrain_props.coarse")) continue;
                const auto id = m->string_values.get(bridge::TerrainPropPlacementMetadataKey);
                uint64_t number = 0;
                if (std::from_chars(id.data(), id.data()+id.size(), number).ec != std::errc{}) continue;
                const auto b = byPlacement.find({m->string_values.get("renegade.terrain_props.layer"), number});
                if (b != byPlacement.end()) b->second->coarse.push_back(e);
            }
        }
    public:
        void Update(wi::scene::Scene& scene, uint64_t revision, const bridge::TerrainPropCollisionWorld* world,
            const std::string& root, const bridge::StableId& project, const DirectX::XMFLOAT3& pos) {
            if (revision_ != revision) {
                cells_.clear(); models_.clear(); active_.clear(); pending_.clear(); center_.reset();
                revision_ = revision;
                if (world) Index(scene,*world,root,project);
            }
            if (cells_.empty() || !std::isfinite(pos.x) || !std::isfinite(pos.z)) return;
            const auto center = At(pos.x,pos.z);
            if (center_ != center) {
                std::set<Cell> nearby;
                for (auto x=center.first-3; x<=center.first+3; ++x)
                    for (auto z=center.second-3; z<=center.second+3; ++z)
                        if (cells_.count({x,z})) nearby.emplace(x,z);
                pending_.clear();
                for (const auto& c : active_) if (!nearby.count(c)) pending_.push_back(c);
                for (const auto& c : nearby) if (!active_.count(c)) pending_.push_back(c);
                center_=center;
            }
            if (pending_.empty()) return;
            const auto cell = pending_.front();
            pending_.pop_front();
            const bool wanted = std::abs(int64_t(cell.first)-center.first)<=3 && std::abs(int64_t(cell.second)-center.second)<=3;
            for (auto& pair : cells_.at(cell)) {
                auto& batch = pair.second;
                if (batch.fine == wanted || batch.coarse.empty()) continue;
                if (wanted) {
                    std::string error;
                    if (!batch.model->Acquire(batch.page,error)) {
                        wi::backlog::post("Terrain prop detail: " + error,wi::backlog::LogLevel::Warning);
                        continue;
                    }
                }
                for (const auto e : batch.coarse)
                    if (auto* object=scene.objects.GetComponent(e)) object->SetRenderable(!wanted);
                if (!wanted) batch.model->Release(cell);
                batch.fine = wanted;
            }
            if (wanted) active_.insert(cell); else active_.erase(cell);
        }
        bool Settled() const { return pending_.empty(); }
    };
}
