#pragma once
#include "RuntimeProjectileSession.h"
#include "renegade/bridge/ReusableAssetService.h"
#include "renegade/bridge/ReusableAssetRuntimeService.h"
#include <map>

namespace renegade::runtime
{
    // Render-only instances. Simulation remains the sole movement/collision authority.
    struct RuntimeProjectileVisuals
    {
        struct Template {
            wi::allocator::shared_ptr<wi::scene::Scene> scene;
            bridge::ProjectileAssetDocument definition;
        };
        std::map<bridge::StableId, Template> templates;
        std::map<std::uint64_t, wi::ecs::Entity> instances;
        std::vector<wi::ecs::Entity> roots;
        std::string error;

        void Reset(wi::scene::Scene& scene)
        {
            for (const auto& [id, root] : instances) scene.Entity_Remove(root);
            instances.clear(); instanceAssets.clear(); roots.clear(); templates.clear(); error.clear();
        }

        bool Prepare(const std::string& root, const std::string& packageRoot,
                     const bridge::StableId& project,
                     const bridge::ProjectileAssetDocument& definition)
        {
            if (definition.meshAssetId.empty() || templates.count(definition.assetId)) return true;
            wi::allocator::shared_ptr<wi::scene::Scene> model;
            if (!packageRoot.empty()) {
                bridge::PreparedPackagedReusableAsset prepared;
                if (!bridge::PreparePackagedReusableAsset(packageRoot,project,
                        definition.meshAssetId,prepared,error)) return false;
                model=std::move(prepared.scene);
            } else {
                auto prepared=bridge::ReusableAssetService().PrepareModelAssetPlacement(
                    {root,project,definition.meshAssetId});
                if (!prepared.IsReady()) { error=prepared.Result().error; return false; }
                model=prepared.ReleaseScene();
            }
            // Model appearance does not bring gameplay/physics controllers with it.
            model->rigidbodies.Clear(); model->softbodies.Clear(); model->colliders.Clear();
            model->scripts.Clear(); model->characters.Clear(); model->metadatas.Clear();
            if (model->objects.GetCount()==0) {
                error="Projectile appearance has no renderable model objects."; return false;
            }
            // Instantiate attaches transforms only. Give resource entities a
            // transform too so recursive retirement owns meshes/materials as
            // well as visible objects; no orphan resources accumulate per shot.
            wi::unordered_set<wi::ecs::Entity> entities;
            model->FindAllEntities(entities);
            for(const auto entity:entities)
                if(!model->transforms.Contains(entity))model->transforms.Create(entity);
            templates.emplace(definition.assetId,Template{std::move(model),definition});
            error.clear(); return true;
        }

        bool Spawn(wi::scene::Scene& scene, const bridge::StableId& asset,
                   std::uint64_t projectile, bool requiresMesh = false)
        {
            const auto found=templates.find(asset);
            if (found==templates.end()) {
                if (requiresMesh) { error="Projectile model could not be prepared."; return false; }
                return true; // Meshless definition.
            }
            if (instances.size()>=bridge::ProjectileSimulation::MaxProjectiles) {
                error="Projectile visual capacity exceeded.";return false;
            }
            const auto root=scene.Instantiate(*found->second.scene,true);
            if (root==wi::ecs::INVALID_ENTITY) { error="Could not create projectile appearance."; return false; }
            instances.emplace(projectile,root);instanceAssets.emplace(projectile,asset); roots.push_back(root); return true;
        }

        void Sync(wi::scene::Scene& scene, const bridge::ProjectileSimulation& simulation)
        {
            for (auto it=instances.begin();it!=instances.end();) {
                const auto found=std::find_if(simulation.Records().begin(),simulation.Records().end(),
                    [&](const auto& record){return record.id==it->first;});
                if (found==simulation.Records().end()) {
                    scene.Entity_Remove(it->second);it=instances.erase(it);continue;
                }
                // Source definition is stable for this session and never read from disk per shot.
                const auto& record=*found;
                const auto association=instanceAssets.find(it->first);
                const auto cached=association==instanceAssets.end() ? templates.end() :
                    templates.find(association->second);
                const Template* visual=cached==templates.end()?nullptr:&cached->second;
                if (visual) {
                    const auto& v=record.launch.velocity;
                    const auto& p=record.launch.position;
                    const float lengthSquared=v.x*v.x+v.y*v.y+v.z*v.z;
                    XMVECTOR direction=lengthSquared>0.00000001f
                        ? XMVector3Normalize(XMVectorSet(v.x,v.y,v.z,0))
                        : XMVectorSet(0,0,1,0);
                    XMVECTOR up=XMVectorSet(0,1,0,0);
                    if(std::abs(XMVectorGetY(direction))>0.99f)up=XMVectorSet(1,0,0,0);
                    const auto facing=XMMatrixInverse(nullptr,
                        XMMatrixLookToLH(XMVectorZero(),direction,up));
                    const auto& d=visual->definition;
                    const auto matrix=XMMatrixScaling(d.visualScale,d.visualScale,d.visualScale) *
                        XMMatrixRotationRollPitchYaw(XMConvertToRadians(d.visualRotationDegrees[0]),
                            XMConvertToRadians(d.visualRotationDegrees[1]),
                            XMConvertToRadians(d.visualRotationDegrees[2])) * facing *
                        XMMatrixTranslation(p.x,p.y,p.z);
                    if(auto* transform=scene.transforms.GetComponent(it->second)) {
                        transform->ClearTransform();transform->MatrixTransform(matrix);transform->UpdateTransform();
                    }
                }
                ++it;
            }
            roots.clear();
            for(const auto& [id,root]:instances)roots.push_back(root);
            for(auto it=instanceAssets.begin();it!=instanceAssets.end();)
                if(!instances.count(it->first))it=instanceAssets.erase(it);else ++it;
        }
        std::map<std::uint64_t,bridge::StableId> instanceAssets;
    };
}
