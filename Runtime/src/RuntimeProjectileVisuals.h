#pragma once
#include "RuntimeProjectileSession.h"
#include "renegade/bridge/ProjectileEffectRuntime.h"
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
        struct Retained { wi::ecs::Entity root; float seconds; };
        struct Effect { wi::ecs::Entity entity; float seconds; };
        std::vector<Retained> retained;
        std::vector<Effect> dyingEffects;
        std::map<std::uint64_t,std::vector<wi::ecs::Entity>> flightEffects;
        std::string error;

        void Reset(wi::scene::Scene& scene)
        {
            for (const auto& [id, root] : instances) scene.Entity_Remove(root);
            for(const auto& r:retained)scene.Entity_Remove(r.root);
            for(const auto& e:dyingEffects)scene.Entity_Remove(e.entity);
            retained.clear();dyingEffects.clear();flightEffects.clear();
            instances.clear(); instanceAssets.clear(); roots.clear(); templates.clear(); error.clear();
        }

        bool Prepare(const std::string& root, const std::string& packageRoot,
                     const bridge::StableId& project,
                     const bridge::ProjectileAssetDocument& definition)
        {
            if (templates.count(definition.assetId)) return true;
            if(definition.meshAssetId.empty()) {
                templates.emplace(definition.assetId,Template{wi::allocator::make_shared<wi::scene::Scene>(),definition});
                return true;
            }
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
            model->scripts.Clear(); model->characters.Clear(); model->metadatas.Clear();model->animations.Clear();
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
            const auto root=found->second.scene->objects.GetCount()==0
                ? scene.Entity_CreateTransform("Projectile appearance")
                : scene.Instantiate(*found->second.scene,true);
            if (root==wi::ecs::INVALID_ENTITY) { error="Could not create projectile appearance."; return false; }
            instances.emplace(projectile,root);instanceAssets.emplace(projectile,asset); roots.push_back(root);
            for(const auto& layer:found->second.definition.flightEffects) {
                if(EffectCount()>=128) {error="Projectile effect budget reached.";break;}
                const auto e=bridge::CreateProjectileEffectEmitter(scene,layer,{0,0,0});
                scene.Component_Attach(e,root,true);
                auto& t=*scene.transforms.GetComponent(e);t.ClearTransform();
                t.Translate(XMFLOAT3{layer.offset[0],layer.offset[1],layer.offset[2]});t.UpdateTransform();
                flightEffects[projectile].push_back(e);
            }
            return true;
        }

        static XMMATRIX Pose(const bridge::ProjectileAssetDocument& d,
            const bridge::ProjectileVector& velocity, const bridge::ProjectileVector& position) {
            auto direction=XMVectorSet(velocity.x,velocity.y,velocity.z,0);
            direction=XMVectorGetX(XMVector3LengthSq(direction))>.00000001f?
                XMVector3Normalize(direction):XMVectorSet(0,0,1,0);
            const auto up=std::abs(XMVectorGetY(direction))>.99f?XMVectorSet(1,0,0,0):XMVectorSet(0,1,0,0);
            const auto facing=XMMatrixInverse(nullptr,XMMatrixLookToLH(XMVectorZero(),direction,up));
            return XMMatrixScaling(d.visualScale,d.visualScale,d.visualScale) *
                XMMatrixRotationRollPitchYaw(XMConvertToRadians(d.visualRotationDegrees[0]),
                    XMConvertToRadians(d.visualRotationDegrees[1]),XMConvertToRadians(d.visualRotationDegrees[2])) *
                facing * XMMatrixTranslation(position.x,position.y,position.z);
        }
        size_t EffectCount() const {
            size_t count=dyingEffects.size();for(const auto& [id,list]:flightEffects)count+=list.size();return count;
        }
        void StopEffects(wi::scene::Scene& scene,std::uint64_t id) {
            const auto effects=flightEffects.find(id);if(effects==flightEffects.end())return;
            for(const auto e:effects->second) {
                auto* emitter=scene.emitters.GetComponent(e);if(!emitter)continue;
                emitter->count=0;scene.Component_Detach(e);
                dyingEffects.push_back({e,emitter->life*(1+emitter->random_life)+.1f});
            }
            flightEffects.erase(effects);
        }
        void Impact(wi::scene::Scene& scene,const bridge::ProjectileImpact& impact,wi::ecs::Entity parent) {
            const auto instance=instances.find(impact.projectileId);
            const auto association=instanceAssets.find(impact.projectileId);
            if(instance==instances.end()||association==instanceAssets.end())return;
            const auto cached=templates.find(association->second);if(cached==templates.end())return;
            const auto& d=cached->second.definition;
            StopEffects(scene,impact.projectileId);
            if(d.impactEffect!=bridge::ProjectileEffectKind::None && EffectCount()<128) {
                bridge::ProjectileEffectLayer layer;layer.kind=d.impactEffect;
                layer.sizeMetres=.12f;layer.particleLifeSeconds=.65f;
                const auto e=bridge::CreateProjectileEffectEmitter(scene,layer,ProjectileNativeVector(impact.contact.position),true);
                dyingEffects.push_back({e,1.1f});
            }
            if(!d.stickOnImpact)return;
            if(retained.size()>=64) {scene.Entity_Remove(retained.front().root);retained.erase(retained.begin());}
            // Align the forwardmost model point with contact, then embed by authored depth.
            float tip=0;
            const auto appearance=Pose(d,{0,0,1},{0,0,0});
            const auto& model=*cached->second.scene;
            for(size_t i=0;i<model.objects.GetCount();++i) {
                const auto* mesh=model.meshes.GetComponent(model.objects[i].meshID);
                const auto* transform=model.transforms.GetComponent(model.objects.GetEntity(i));if(!mesh||!transform)continue;
                const auto matrix=XMLoadFloat4x4(&transform->world)*appearance;
                for(const auto& vertex:mesh->vertex_positions)
                    tip=std::max(tip,XMVectorGetZ(XMVector3TransformCoord(XMLoadFloat3(&vertex),matrix)));
            }
            const auto v=impact.incomingVelocity;
            const float length=std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);
            const float offset=tip-d.embedDepthMetres;
            auto p=impact.contact.position;
            if(length>.000001f) {p.x-=v.x/length*offset;p.y-=v.y/length*offset;p.z-=v.z/length*offset;}
            if(auto* t=scene.transforms.GetComponent(instance->second)) {
                t->ClearTransform();t->MatrixTransform(Pose(d,v,p));t->UpdateTransform();
                if(parent!=wi::ecs::INVALID_ENTITY && scene.transforms.Contains(parent))
                    scene.Component_Attach(instance->second,parent);
            }
            retained.push_back({instance->second,d.stuckLifetimeSeconds});
            instances.erase(instance);instanceAssets.erase(association);
        }
        void Sync(wi::scene::Scene& scene, const bridge::ProjectileSimulation& simulation,float dt=0)
        {
            for (auto it=instances.begin();it!=instances.end();) {
                const auto found=std::find_if(simulation.Records().begin(),simulation.Records().end(),
                    [&](const auto& record){return record.id==it->first;});
                if (found==simulation.Records().end()) {
                    StopEffects(scene,it->first);
                    scene.Entity_Remove(it->second);it=instances.erase(it);continue;
                }
                // Source definition is stable for this session and never read from disk per shot.
                const auto& record=*found;
                const auto association=instanceAssets.find(it->first);
                const auto cached=association==instanceAssets.end() ? templates.end() :
                    templates.find(association->second);
                const Template* visual=cached==templates.end()?nullptr:&cached->second;
                if (visual) {
                    const auto matrix=Pose(visual->definition,record.launch.velocity,record.launch.position);
                    if(auto* transform=scene.transforms.GetComponent(it->second)) {
                        transform->ClearTransform();transform->MatrixTransform(matrix);transform->UpdateTransform();
                    }
                }
                ++it;
            }
            for(auto it=retained.begin();it!=retained.end();) {
                it->seconds-=dt;
                if(it->seconds<=0 || !scene.transforms.Contains(it->root)) {
                    scene.Entity_Remove(it->root);it=retained.erase(it);
                } else ++it;
            }
            for(auto it=dyingEffects.begin();it!=dyingEffects.end();) {
                it->seconds-=dt;
                if(it->seconds<=0) {scene.Entity_Remove(it->entity);it=dyingEffects.erase(it);}
                else ++it;
            }
            roots.clear();
            for(const auto& [id,root]:instances)roots.push_back(root);
            for(const auto& r:retained)roots.push_back(r.root);
            for(auto it=instanceAssets.begin();it!=instanceAssets.end();)
                if(!instances.count(it->first))it=instanceAssets.erase(it);else ++it;
        }
        std::map<std::uint64_t,bridge::StableId> instanceAssets;
    };
}
