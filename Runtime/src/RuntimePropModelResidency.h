#pragma once
#include "renegade/bridge/TerrainPropCollisionWorld.h"
#include "renegade/bridge/TerrainPropDetailProjection.h"
#include "RuntimePropRangeResidency.h"
#include <charconv>
#include <cmath>
#include <algorithm>
#include <deque>
#include <future>
#include <chrono>
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
        std::optional<Cell> center_, viewCell_;
        std::optional<uint64_t> revision_;
        RuntimePropRangeResidency ranges_;
        using ModelKey = std::pair<uint64_t, size_t>;
        using Preparation = std::future<bridge::PreparedReusableModelPlacement>;
        static constexpr size_t MaxPreparations = 2;
        std::map<ModelKey, Preparation> preparations_;
        std::set<size_t> wantedModels_;
        static bool Ready(const Preparation& job) {
            return job.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
        }
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
            auto bindCoarse = [&](wi::ecs::Entity e) {
                const auto* m = scene.metadatas.GetComponent(e);
                if (!m || !m->runtimeProjectionOnly || !m->string_values.has("renegade.terrain_props.coarse")) return;
                const auto id = m->string_values.get(bridge::TerrainPropPlacementMetadataKey);
                uint64_t number = 0;
                const auto parsed = std::from_chars(id.data(), id.data()+id.size(), number);
                if (parsed.ec != std::errc{} || parsed.ptr != id.data()+id.size()) return;
                const auto b = byPlacement.find({m->string_values.get("renegade.terrain_props.layer"), number});
                if (b != byPlacement.end()) b->second->coarse.push_back(e);
            };
            for (const auto& part : world.RangeParts()) {
                if (!part.coarse) continue;
                const auto batch=byPlacement.find({part.layer,part.placement});
                if (batch!=byPlacement.end()) batch->second->coarse.push_back(part.entity);
            }
            for (size_t i=0; i<scene.objects.GetCount(); ++i) {
                const auto e=scene.objects.GetEntity(i);
                const auto* m=scene.metadatas.GetComponent(e);
                if (m && !m->string_values.has("renegade.terrain_props.ranged")) bindCoarse(e);
            }
        }
    public:
        void Update(wi::scene::Scene& scene, uint64_t revision, const bridge::TerrainPropCollisionWorld* world,
            const std::string& root, const bridge::StableId& project, const DirectX::XMFLOAT3& pos,
            const DirectX::XMFLOAT3* camera = nullptr) {
            if (revision_ != revision) {
                // A revision can change while the same coarse scene objects still exist.
                for (const auto& cell : active_) for (const auto& entry : cells_.at(cell)) {
                    if (!entry.second.fine) continue;
                    for (auto entity : entry.second.coarse)
                        if (auto* object = scene.objects.GetComponent(entity)) object->SetRenderable(true);
                }
                cells_.clear(); models_.clear(); active_.clear(); pending_.clear(); center_.reset(); viewCell_.reset();
                revision_ = revision;
                if (world) { ranges_.Index(scene,world->RangeParts()); Index(scene,*world,root,project); }
            }
            if (!std::isfinite(pos.x) || !std::isfinite(pos.z)) return;
            const auto center = At(pos.x,pos.z);
            const auto& view = camera && std::isfinite(camera->x) && std::isfinite(camera->z) ? *camera : pos;
            const auto viewCell = At(view.x,view.z);
            if (world && viewCell_ != viewCell) {
                ranges_.Update(scene,world->RangeParts(),view);
                viewCell_ = viewCell;
            }
            if (center_ != center) {
                std::set<Cell> nearby;
                for (auto x=center.first-3; x<=center.first+3; ++x)
                    for (auto z=center.second-3; z<=center.second+3; ++z)
                        if (cells_.count({x,z})) nearby.emplace(x,z);
                wantedModels_.clear();
                pending_.clear();
                for (const auto& c : active_) if (!nearby.count(c)) pending_.push_back(c);
                std::vector<Cell> ordered(nearby.begin(),nearby.end());
                auto distance = [&](Cell c) {
                    auto x = c.first-center.first, z = c.second-center.second;
                    return x*x + z*z;
                };
                std::stable_sort(ordered.begin(),ordered.end(),[&](Cell a, Cell b){ return distance(a)<distance(b); });
                for (const auto& c : ordered) {
                    pending_.push_back(c);
                    for (const auto& entry : cells_.at(c)) wantedModels_.insert(entry.first);
                }
                center_=center;
            }
            for (auto job = preparations_.begin(); job != preparations_.end();) {
                const bool obsolete = job->first.first != revision || !wantedModels_.count(job->first.second);
                if (obsolete && Ready(job->second)) job = preparations_.erase(job);
                else ++job;
            }
            if (pending_.empty()) return;
            const auto cell = pending_.front();

            const bool wanted = std::abs(int64_t(cell.first)-center.first)<=3 && std::abs(int64_t(cell.second)-center.second)<=3;
            bool waiting = false;
            for (auto& pair : cells_.at(cell)) {
                auto& batch = pair.second;
                if (batch.fine == wanted || batch.coarse.empty()) continue;
                if (wanted) {
                    std::string error;
                    bridge::PreparedReusableModelPlacement prepared;
                    if (!batch.model->HasResidentModel()) {
                        const ModelKey key{revision,pair.first};
                        try {
                            auto job = preparations_.find(key);
                            if (job == preparations_.end() && preparations_.size() < MaxPreparations) {
                                auto worker = std::async(std::launch::async, [request=batch.model->Request()] {
                                    return bridge::ReusableAssetService().PrepareModelAssetPlacement(request);
                                });
                                job = preparations_.emplace(key,std::move(worker)).first;
                            }
                            if (job == preparations_.end() || !Ready(job->second)) {
                                waiting = true;
                                continue;
                            }
                            auto worker = std::move(job->second);
                            preparations_.erase(job);
                            prepared = worker.get();
                        } catch (const std::exception& e) { error = e.what(); }
                    }
                    if (!error.empty() || !batch.model->Acquire(batch.page,error,&prepared)) {
                        wi::backlog::post("Terrain prop detail: " + error,wi::backlog::LogLevel::Warning);
                        continue;
                    }
                }
                for (const auto e : batch.coarse)
                    ranges_.SetRenderable(scene,e,!wanted);
                if (!wanted) batch.model->Release(cell);
                batch.fine = wanted;
                if (wanted) active_.insert(cell);
            }
            pending_.pop_front();
            if (waiting) pending_.push_back(cell);
            if (!wanted) active_.erase(cell);
        }
        bool Settled() const { return pending_.empty() && preparations_.empty(); }
    };
}
