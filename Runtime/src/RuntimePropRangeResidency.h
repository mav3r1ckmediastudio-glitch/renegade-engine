#pragma once
#include "renegade/bridge/TerrainPropProjection.h"
#include <wiBVH.h>
#include <map>
#include <set>
#include <cmath>

namespace renegade::runtime
{
    // One BVH leaf per authored spatial cell. Its box includes the furthest
    // draw distance and transformed mesh extent of every part in that cell.
    class RuntimePropRangeResidency
    {
        using Records = std::vector<bridge::TerrainPropRangePart>;
        struct Page {
            wi::primitive::AABB interest;
            std::vector<size_t> parts;
        };
        wi::BVH index_;
        std::vector<Page> pages_;
        std::set<uint32_t> resident_;
        std::set<wi::ecs::Entity> hidden_;

        static float Coordinate(double v) {
            constexpr double limit = std::numeric_limits<float>::max();
            return static_cast<float>(std::clamp(v,-limit,limit));
        }

    public:
        void Index(wi::scene::Scene& scene, const Records& records) {
            pages_.clear();
            resident_.clear();
            hidden_.clear();
            std::map<std::pair<double,double>,size_t> lookup;
            for (size_t i=0; i<records.size(); ++i) {
                const auto& part = records[i];
                const double x = part.origin.x, z = part.origin.z;
                const auto key = std::make_pair(std::floor(x/32),std::floor(z/32));
                const auto slot = lookup.emplace(key,pages_.size());
                if (slot.second) pages_.emplace_back();
                auto& page = pages_[slot.first->second];
                const wi::primitive::AABB area(
                    XMFLOAT3{Coordinate(x-part.range),-1,Coordinate(z-part.range)},
                    XMFLOAT3{Coordinate(x+part.range),1,Coordinate(z+part.range)});
                page.interest = wi::primitive::AABB::Merge(page.interest,area);
                page.parts.push_back(i);
                if (scene.objects.Contains(part.entity)) resident_.insert(static_cast<uint32_t>(slot.first->second));
            }
            std::vector<wi::primitive::AABB> boxes;
            boxes.reserve(pages_.size());
            for (const auto& page : pages_) boxes.push_back(page.interest);
            index_.Build(boxes.data(),static_cast<uint32_t>(boxes.size()));
        }

        void SetRenderable(wi::scene::Scene& scene, wi::ecs::Entity entity, bool value) {
            if (value) hidden_.erase(entity); else hidden_.insert(entity);
            if (auto* object=scene.objects.GetComponent(entity)) object->SetRenderable(value);
        }

        void Update(wi::scene::Scene& scene, const Records& records, const XMFLOAT3& position) {
            if (!index_.IsValid()) return;
            // Cover the entire current cell and the fine-model neighbourhood.
            const double x=std::floor(double(position.x)/32)*32, z=std::floor(double(position.z)/32)*32;
            const wi::primitive::AABB query(XMFLOAT3{Coordinate(x-96),-1,Coordinate(z-96)},
                XMFLOAT3{Coordinate(x+128),1,Coordinate(z+128)});
            std::set<uint32_t> wanted;
            index_.Intersects(query,0,[&](uint32_t i){
                if (pages_[i].interest.intersects(query)) wanted.insert(i);
            });
            for (auto i : wanted) if (!resident_.count(i)) {
                for (auto id : pages_[i].parts) {
                    const auto& part = records[id];
                    part.EnsureIdentityAndTransform(scene);
                    auto& object=scene.objects.Create(part.entity);
                    object.meshID=part.mesh;
                    object.draw_distance=part.drawDistance;
                    object.SetRenderable(!hidden_.count(part.entity));
                }
            }
            for (auto i : resident_) if (!wanted.count(i)) {
                for (auto id : pages_[i].parts) {
                    const auto& part=records[id];
                    scene.objects.Remove(part.entity);
                    const auto* metadata=scene.metadatas.GetComponent(part.entity);
                    if (!metadata || !metadata->string_values.has("renegade.terrain_props.hit_anchor")) {
                        scene.transforms.Remove(part.entity);
                        scene.metadatas.Remove(part.entity);
                    }
                }
            }
            resident_.swap(wanted);
        }
    };
}
