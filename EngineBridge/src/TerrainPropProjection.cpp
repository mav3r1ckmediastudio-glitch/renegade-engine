#include "renegade/bridge/TerrainPropProjection.h"
#include <cmath>
#include <algorithm>
#include <exception>
#include <set>
#include <unordered_map>

namespace renegade::bridge
{
    using wi::ecs::Entity;
    using namespace DirectX;

    void TerrainPropRangePart::EnsureIdentityAndTransform(wi::scene::Scene& scene) const
    {
        if (!scene.metadatas.Contains(entity)) {
            auto& metadata = scene.metadatas.Create(entity);
            metadata.runtimeProjectionOnly = true;
            metadata.string_values.set("renegade.terrain_props.ranged","1");
            metadata.string_values.set("renegade.terrain_props.layer",layer);
            metadata.string_values.set("renegade.terrain_props.placement",std::to_string(placement));
            metadata.string_values.set(ImpactSurfaceMetadataKey,ImpactSurfaceTypeToken(surface));
            if (coarse) metadata.string_values.set("renegade.terrain_props.coarse","1");
        }
        if (!scene.transforms.Contains(entity)) {
            auto& transform = scene.transforms.Create(entity);
            transform.MatrixTransform(XMLoadFloat4x4(&matrix));
            transform.UpdateTransform();
        }
    }

    Entity TerrainPropProjection::NewTransient(std::vector<Entity>& owner)
    {
        const Entity entity = wi::ecs::CreateEntity();
        owner.push_back(entity);
        auto& metadata = scene_.metadatas.Create(entity);
        metadata.runtimeProjectionOnly = true;
        if (!layerIdentity_.empty()) metadata.string_values.set("renegade.terrain_props.layer", layerIdentity_);
        return entity;
    }

    TerrainPropProjection::~TerrainPropProjection()
    {
        for (const auto& page : pageEntities_)
            for (auto entity : page.second) scene_.Entity_Remove(entity, false);
        for (auto it = resources_.rbegin(); it != resources_.rend(); ++it)
            scene_.Entity_Remove(*it, false);
    }

    bool TerrainPropProjection::Register(const TerrainPropProfileId& id,
        const wi::scene::Scene& source, TerrainPropCollision collision, std::string& error, float drawDistance)
    {
        error.clear();
        if (!std::isfinite(drawDistance) || drawDistance < 0 || &source == &scene_ || id == TerrainPropProfileId{} || profiles_.count(id) ||
            source.objects.GetCount() == 0)
        {
            error = "Expected a new profile ID and a separate prepared static model.";
            return false;
        }
        const auto& size = collision.halfExtents;
        const auto& center = collision.center;
        if (!std::isfinite(size.x) || !std::isfinite(size.y) || !std::isfinite(size.z) ||
            size.x <= 0 || size.y <= 0 || size.z <= 0 || !std::isfinite(center.x) ||
            !std::isfinite(center.y) || !std::isfinite(center.z))
        {
            error = "Invalid collision profile dimensions.";
            return false;
        }
        for (size_t i = 0; i < source.objects.GetCount(); ++i)
        {
            const auto* mesh = source.meshes.GetComponent(source.objects[i].meshID);
            if (!mesh || mesh->IsSkinned() || !mesh->morph_targets.empty() ||
                !source.transforms.Contains(source.objects.GetEntity(i)))
            {
                error = "Terrain prop projection requires static mesh parts and transforms.";
                return false;
            }
            for (const auto& subset : mesh->subsets)
                if (!source.materials.Contains(subset.materialID))
                {
                    error = "Terrain prop mesh references a missing material.";
                    return false;
                }
        }
        Prototype prototype;
        const auto firstResource = resources_.size();
        prototype.collision = collision;
        prototype.drawDistance = drawDistance;
        std::unordered_map<Entity, Entity> meshes, materials;
        for (size_t i = 0; i < source.objects.GetCount(); ++i)
        {
            Entity modelMesh = source.objects[i].meshID;
            if (!meshes.count(modelMesh))
            {
                Entity target = NewTransient(resources_);
                meshes.emplace(modelMesh, target);
                auto& mesh = scene_.meshes.Create(target);
                mesh = *source.meshes.GetComponent(modelMesh);
                for (auto& subset : mesh.subsets)
                {
                    Entity material = subset.materialID;
                    if (!materials.count(material))
                    {
                        Entity targetMaterial = NewTransient(resources_);
                        scene_.materials.Create(targetMaterial) = *source.materials.GetComponent(material);
                        materials.emplace(material, targetMaterial);
                    }
                    subset.materialID = materials.at(material);
                }
            }
            Entity node = source.objects.GetEntity(i);
            Part part{meshes.at(modelMesh), {}};
            XMStoreFloat4x4(&part.local, source.transforms.GetComponent(node)->GetLocalMatrix() *
                source.ComputeParentMatrixRecursive(node));
            prototype.parts.push_back(part);
        }
        prototype.resources.assign(resources_.begin() + firstResource,resources_.end());
        profiles_.emplace(id, std::move(prototype));
        return true;
    }

    bool TerrainPropProjection::Place(const TerrainPropPage& page, std::string& error)
    {
        if (!ValidateTerrainPropPage(page, error)) return false;
        for (const auto& p : page.placements)
            if (!profiles_.count(page.profiles[p.profileIndex]) || placementIds_.count(p.id))
            {
                error = "Placement references an unknown profile or an already resident ID.";
                return false;
            }

        const Cell cell{page.cellX, page.cellZ};
        auto& owned = pageEntities_[cell];
        for (const auto& p : page.placements)
        {
            const auto& prototype = profiles_.at(page.profiles[p.profileIndex]);
            const auto world = XMMatrixScaling(p.scale, p.scale, p.scale) *
                XMMatrixRotationY(p.yawRadians) * XMMatrixTranslation(p.position[0], p.position[1], p.position[2]);
            for (const auto& part : prototype.parts)
            {
                const auto matrix = XMLoadFloat4x4(&part.local) * world;
                const float distance = static_cast<float>(std::min(double(prototype.drawDistance) * p.scale,
                    double(std::numeric_limits<float>::max())));
                if (rangeParts_ && distance > 0) {
                    const auto bounds = scene_.meshes.GetComponent(part.mesh)->aabb.transform(matrix);
                    const auto center = bounds.getCenter();
                    const double reach = double(distance) + bounds.getRadius() +
                        std::hypot(double(center.x)-p.position[0], double(center.z)-p.position[2]);
                    if (std::isfinite(reach)) {
                        TerrainPropRangePart record{wi::ecs::CreateEntity(),part.mesh,{},
                            {p.position[0],p.position[1],p.position[2]},reach,distance,
                            prototype.collision.shape != TerrainPropShape::None && &part == &prototype.parts.front(),
                            prototype.representation == Representation::Coarse,layerIdentity_,p.id,TerrainPropSurface(prototype.collision)};
                        XMStoreFloat4x4(&record.matrix,matrix);
                        rangeParts_->push_back(std::move(record));
                        continue;
                    }
                }
                const Entity entity = NewTransient(owned);
                if (prototype.representation != Representation::Default)
                    scene_.metadatas.GetComponent(entity)->string_values.set(prototype.representation == Representation::Coarse
                        ? "renegade.terrain_props.coarse" : "renegade.terrain_props.fine","1");
                scene_.metadatas.GetComponent(entity)->string_values.set(
                    "renegade.terrain_props.placement", std::to_string(p.id));
                scene_.metadatas.GetComponent(entity)->string_values.set(ImpactSurfaceMetadataKey,
                    ImpactSurfaceTypeToken(TerrainPropSurface(prototype.collision)));
                auto& object = scene_.objects.Create(entity);
                object.meshID = part.mesh;
                if (distance > 0) object.draw_distance = distance;
                auto& transform = scene_.transforms.Create(entity);
                transform.MatrixTransform(matrix);
                transform.UpdateTransform();
            }
            const auto& profile = prototype.collision;
            if (createCollision_ && profile.shape != TerrainPropShape::None)
            {
                const Entity entity = NewTransient(owned);
                auto& metadata = *scene_.metadatas.GetComponent(entity);
                metadata.string_values.set("renegade.terrain_props.placement", std::to_string(p.id));
                metadata.string_values.set("renegade.terrain_props.collision", "1");
                auto& transform = scene_.transforms.Create(entity);
                transform.MatrixTransform(XMMatrixTranslation(profile.center.x, profile.center.y, profile.center.z) * world);
                transform.UpdateTransform();
                auto& body = scene_.rigidbodies.Create(entity);
                body.mass = 0;
                body.friction = profile.friction;
                if (profile.shape == TerrainPropShape::TrunkCapsule)
                {
                    body.shape = body.CAPSULE;
                    body.capsule.radius = profile.halfExtents.x;
                    body.capsule.height = profile.halfExtents.y;
                }
                else
                {
                    body.shape = body.BOX;
                    body.box.halfextents = profile.halfExtents;
                }
            }
            placementIds_.insert(p.id);
            pageIds_[cell].push_back(p.id);
        }
        return true;
    }
}


namespace renegade::bridge
{
    void TerrainPropProjection::SetLayerIdentity(const std::string& id)
    {
        layerIdentity_ = id;
        const auto setIdentity = [&](Entity entity) {
            if (auto* metadata = scene_.metadatas.GetComponent(entity))
                metadata->string_values.set("renegade.terrain_props.layer", id);
        };
        for (auto entity : resources_) setIdentity(entity);
        for (const auto& page : pageEntities_)
            for (auto entity : page.second) setIdentity(entity);
    }

    void TerrainPropProjection::ReleaseOwnership() noexcept
    {
        resources_.clear();
        pageEntities_.clear();
        pageIds_.clear();
        placementIds_.clear();
    }

    TerrainPropProjection::PendingPages::~PendingPages()
    {
        if (owner && !committed)
            for (auto entity : installed) owner->scene_.Entity_Remove(entity, false);
    }

    std::unique_ptr<TerrainPropProjection::PendingPages> TerrainPropProjection::PreparePages(
        const std::vector<TerrainPropPage>& pages, std::string& error)
    {
        auto pending = std::unique_ptr<PendingPages>(new PendingPages);
        pending->owner = this;
        pending->scene = std::make_unique<wi::scene::Scene>();
        TerrainPropProjection staging(*pending->scene, createCollision_);
        staging.profiles_ = profiles_;
        staging.layerIdentity_ = layerIdentity_;
        std::unordered_set<std::uint64_t> replaced;
        std::set<Cell> changed;
        for (const auto& page : pages)
        {
            const Cell cell{page.cellX, page.cellZ};
            if (!changed.insert(cell).second) { error = "Duplicate changed terrain cell."; return {}; }
            auto found = pageIds_.find(cell);
            if (found != pageIds_.end())
                replaced.insert(found->second.begin(), found->second.end());
        }
        for (const auto& page : pages)
        {
            for (const auto& p : page.placements)
                if (placementIds_.count(p.id) && !replaced.count(p.id))
                { error = "Placement ID already belongs to an unchanged terrain cell."; return {}; }
            if (!staging.Place(page, error)) return {};
            const Cell cell{page.cellX, page.cellZ};
            pending->entities[cell] = staging.pageEntities_[cell];
            pending->ids[cell] = staging.pageIds_[cell];
        }
        staging.ReleaseOwnership();
        return pending;
    }

    bool TerrainPropProjection::InstallPages(PendingPages& pending, std::string& error)
    {
        if (pending.owner != this || pending.committed || pending.installedDone)
        { error = "Invalid terrain page staging operation."; return false; }
        for (const auto& pair : pending.entities)
            pending.installed.insert(pending.installed.end(), pair.second.begin(), pair.second.end());
        pending.installedDone = true;
        try { scene_.Merge(*pending.scene); }
        catch (const std::exception& e) { error = e.what(); return false; }
        return true;
    }

    void TerrainPropProjection::EvictPage(Cell cell)
    {
        const auto found = pageEntities_.find(cell);
        if (found == pageEntities_.end()) return;
        for (auto entity : found->second) scene_.Entity_Remove(entity, false);
        pageEntities_.erase(found);
        const auto ids = pageIds_.find(cell);
        if (ids != pageIds_.end())
        {
            for (auto id : ids->second) placementIds_.erase(id);
            pageIds_.erase(ids);
        }
    }

    void TerrainPropProjection::CommitPages(PendingPages& pending)
    {
        // Release all old IDs before installing placements that moved between cells.
        for (const auto& page : pending.entities) EvictPage(page.first);
        for (const auto& pair : pending.entities)
        {
            const auto cell = pair.first;
            pageEntities_[cell] = pair.second;
            pageIds_[cell] = pending.ids.at(cell);
            for (auto id : pageIds_[cell]) placementIds_.insert(id);

        }
        pending.committed = true;
    }
}
namespace renegade::bridge
{
    void TerrainPropProjection::UnregisterUnused(const TerrainPropProfileId& id)
    {
        const auto found=profiles_.find(id);
        if(found==profiles_.end()) return;
        const std::unordered_set<wi::ecs::Entity> resources(found->second.resources.begin(),
            found->second.resources.end());
        for(auto entity:resources) scene_.Entity_Remove(entity,false);
        resources_.erase(std::remove_if(resources_.begin(),resources_.end(),
            [&](wi::ecs::Entity entity){return resources.count(entity)!=0;}),resources_.end());
        profiles_.erase(found);
    }
}
