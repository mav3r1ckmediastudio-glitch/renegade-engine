#include "renegade/bridge/TerrainPropProjection.h"
#include <cmath>
#include <unordered_map>

namespace renegade::bridge
{
    using wi::ecs::Entity;
    using namespace DirectX;

    Entity TerrainPropProjection::NewTransient()
    {
        const Entity entity = wi::ecs::CreateEntity();
        entities_.push_back(entity);
        scene_.metadatas.Create(entity).runtimeProjectionOnly = true;
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
        }
        return true;
    }
}
