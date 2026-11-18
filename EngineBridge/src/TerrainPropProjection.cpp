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

    Entity TerrainPropProjection::NewTransient()
    {
        const Entity entity = wi::ecs::CreateEntity();
        entities_.push_back(entity);
        auto& metadata = scene_.metadatas.Create(entity);
        metadata.runtimeProjectionOnly = true;
        if (!layerIdentity_.empty()) metadata.string_values.set("renegade.terrain_props.layer", layerIdentity_);
        return entity;
    }

    TerrainPropProjection::~TerrainPropProjection()
    {
        for (auto it = entities_.rbegin(); it != entities_.rend(); ++it)
            scene_.Entity_Remove(*it, false);
    }

    bool TerrainPropProjection::Register(const TerrainPropProfileId& id,
        const wi::scene::Scene& source, TerrainPropCollision collision, std::string& error)
    {
        error.clear();
        if (&source == &scene_ || id == TerrainPropProfileId{} || profiles_.count(id) ||
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
        const auto firstResource = entities_.size();
        prototype.collision = collision;
        std::unordered_map<Entity, Entity> meshes, materials;
        for (size_t i = 0; i < source.objects.GetCount(); ++i)
        {
            Entity modelMesh = source.objects[i].meshID;
            if (!meshes.count(modelMesh))
            {
                Entity target = NewTransient();
                meshes.emplace(modelMesh, target);
                auto& mesh = scene_.meshes.Create(target);
                mesh = *source.meshes.GetComponent(modelMesh);
                for (auto& subset : mesh.subsets)
                {
                    Entity material = subset.materialID;
                    if (!materials.count(material))
                    {
                        Entity targetMaterial = NewTransient();
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
        prototype.resources.assign(entities_.begin() + firstResource,entities_.end());
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
        const auto firstEntity = entities_.size();
        for (const auto& p : page.placements)
        {
            const auto& prototype = profiles_.at(page.profiles[p.profileIndex]);
            const auto world = XMMatrixScaling(p.scale, p.scale, p.scale) *
                XMMatrixRotationY(p.yawRadians) * XMMatrixTranslation(p.position[0], p.position[1], p.position[2]);
            for (const auto& part : prototype.parts)
            {
                const Entity entity = NewTransient();
                scene_.objects.Create(entity).meshID = part.mesh;
                auto& transform = scene_.transforms.Create(entity);
                transform.MatrixTransform(XMLoadFloat4x4(&part.local) * world);
                transform.UpdateTransform();
            }
            const auto& profile = prototype.collision;
            if (profile.shape != TerrainPropShape::None)
            {
                const Entity entity = NewTransient();
                auto& transform = scene_.transforms.Create(entity);
                transform.MatrixTransform(XMMatrixTranslation(profile.center.x, profile.center.y, profile.center.z) * world);
                transform.UpdateTransform();
                auto& body = scene_.rigidbodies.Create(entity);
                body.mass = 0;
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
        auto& owned = pageEntities_[cell];
        owned.insert(owned.end(), entities_.begin() + firstEntity, entities_.end());
        return true;
    }
}


namespace renegade::bridge
{
    void TerrainPropProjection::SetLayerIdentity(const std::string& id)
    {
        layerIdentity_ = id;
        for (auto entity : entities_)
            if (auto* metadata = scene_.metadatas.GetComponent(entity))
                metadata->string_values.set("renegade.terrain_props.layer", id);
    }

    void TerrainPropProjection::ReleaseOwnership() noexcept
    {
        entities_.clear();
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
        TerrainPropProjection staging(*pending->scene);
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

    void TerrainPropProjection::CommitPages(PendingPages& pending)
    {
        std::unordered_set<Entity> removed;
        // Remove old IDs in every changed cell before inserting moved IDs.
        for (const auto& pair : pending.entities)
        {
            const auto cell = pair.first;
            for (auto id : pageIds_[cell]) placementIds_.erase(id);
            for (auto entity : pageEntities_[cell])
            {
                scene_.Entity_Remove(entity, false);
                removed.insert(entity);
            }
        }
        entities_.erase(std::remove_if(entities_.begin(),entities_.end(),
            [&](Entity entity) { return removed.count(entity)!=0; }),entities_.end());
        for (const auto& pair : pending.entities)
        {
            const auto cell = pair.first;
            pageEntities_[cell] = pair.second;
            pageIds_[cell] = pending.ids.at(cell);
            for (auto id : pageIds_[cell]) placementIds_.insert(id);
            entities_.insert(entities_.end(), pair.second.begin(), pair.second.end());
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
        entities_.erase(std::remove_if(entities_.begin(),entities_.end(),
            [&](wi::ecs::Entity entity){return resources.count(entity)!=0;}),entities_.end());
        profiles_.erase(found);
    }
}
