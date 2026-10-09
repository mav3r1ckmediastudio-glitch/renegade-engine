#pragma once
#include "RuntimeProjectileSession.h"
#include "RuntimeImpactTextures.h"
#include "RuntimeImpactGeometry.h"
#include "RuntimeImpactMarks.h"
#include "RuntimeBloodEffects.h"
#include "renegade/bridge/ProjectileEffectRuntime.h"
#include "renegade/bridge/ReusableAssetService.h"
#include "renegade/bridge/ReusableAssetRuntimeService.h"
#include <array>
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
        struct Retained {
            wi::ecs::Entity root; float seconds;
            wi::ecs::Entity receiver=wi::ecs::INVALID_ENTITY;
            XMFLOAT3 localContact{};
            XMFLOAT4X4 relativePose=wi::math::IDENTITY_MATRIX;
            RuntimeImpactMarks::Anchor skinAnchor;
            XMFLOAT4X4 surfaceRelativePose=wi::math::IDENTITY_MATRIX;
        };
        static XMMATRIX ReceiverRigidPose(const wi::scene::TransformComponent& t) {
            return XMMatrixRotationQuaternion(t.GetRotationV())*XMMatrixTranslationFromVector(t.GetPositionV());
        }
        static bool FollowReceiver(wi::scene::Scene& scene,const Retained& r) {
            auto* t=scene.transforms.GetComponent(r.root);
            const auto* receiver=scene.transforms.GetComponent(r.receiver);
            if(!t)return false;
            if(r.skinAnchor.receiver!=wi::ecs::INVALID_ENTITY) {
                XMFLOAT4X4 surface;
                if(!RuntimeImpactMarks::SurfacePose(scene,r.skinAnchor,surface))return false;
                t->ClearTransform();t->MatrixTransform(XMLoadFloat4x4(&r.surfaceRelativePose)*XMLoadFloat4x4(&surface));
                t->UpdateTransform();return true;
            }
            if(!receiver)return r.receiver==wi::ecs::INVALID_ENTITY;
            auto pose=XMLoadFloat4x4(&r.relativePose)*ReceiverRigidPose(*receiver);
            // Contact follows receiver geometry; arrow orientation and scale remain rigid.
            pose.r[3]=XMVectorSetW(XMVector3TransformCoord(XMLoadFloat3(&r.localContact),
                receiver->GetWorldMatrix()),1);
            t->ClearTransform();t->MatrixTransform(pose);t->UpdateTransform();return true;
        }
        struct Effect { wi::ecs::Entity entity; float seconds; };
        struct ImpactDecal { wi::ecs::Entity entity; float seconds; RuntimeImpactMarks::Anchor anchor; float bleedAge=-1; };
        std::vector<Retained> retained;
        std::vector<Effect> dyingEffects;
        std::vector<ImpactDecal> impactDecals;
        unsigned markSerial=0;
        std::map<std::uint64_t,std::vector<wi::ecs::Entity>> flightEffects;
        RuntimeImpactGeometry impactGeometry;
        RuntimeBloodEffects blood;
        std::string error;

        void Reset(wi::scene::Scene& scene)
        {
            for (const auto& [id, root] : instances) scene.Entity_Remove(root);
            for(const auto& r:retained)scene.Entity_Remove(r.root);
            for(const auto& e:dyingEffects)scene.Entity_Remove(e.entity);
            for(const auto& d:impactDecals)scene.Entity_Remove(d.entity);
            impactGeometry.Reset(scene);blood.Reset(scene);
            blood.directionalPrototype=std::getenv("RENEGADE_DIRECTIONAL_BLOOD")!=nullptr;
            retained.clear();dyingEffects.clear();impactDecals.clear();flightEffects.clear();
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
        static XMFLOAT4 SurfaceEffectColor(const bridge::ImpactSurfaceType type)
        {
            using Surface = bridge::ImpactSurfaceType;
            switch (type)
            {
            case Surface::Metal: return {1.0f, .52f, .08f, 1.0f};
            case Surface::Wood: return {.36f, .21f, .09f, .72f};
            case Surface::Concrete: return {.58f, .57f, .54f, .68f};
            case Surface::Stone: return {.42f, .44f, .46f, .68f};
            case Surface::Dirt: return {.34f, .23f, .12f, .70f};
            case Surface::Glass: return {.64f, .84f, 1.0f, .92f};
            case Surface::Water: return {.24f, .58f, .98f, .58f};
            case Surface::Character: return {.48f, .10f, .07f, .62f};
            default: return {.95f, .62f, .22f, .92f};
            }
        }

        void PresentSurfaceImpact(
            wi::scene::Scene& scene,
            const bridge::ProjectileImpact& impact,
            const wi::ecs::Entity parent = wi::ecs::INVALID_ENTITY)
        {
            using Surface = bridge::ImpactSurfaceType;
            const auto surface = impact.contact.surfaceType;
            for(size_t i=0;i<scene.metadatas.GetCount();++i)
                if(scene.metadatas[i].bool_values.has("renegade.impact.defaults.directional_blood") &&
                    scene.metadatas[i].bool_values.get("renegade.impact.defaults.directional_blood"))
                    {blood.directionalPrototype=true;break;}


            const bool polished=surface==Surface::Metal || surface==Surface::Wood || surface==Surface::Concrete || surface==Surface::Glass;
            bool embedded=false;
            const auto association=instanceAssets.find(impact.projectileId);
            if(association!=instanceAssets.end())
                if(const auto cached=templates.find(association->second);cached!=templates.end())
                    embedded=cached->second.definition.stickOnImpact;
            const float strength=embedded?.55f:1.f;
            auto normal=XMVectorSet(impact.contact.normal.x,impact.contact.normal.y,impact.contact.normal.z,0);
            if(XMVectorGetX(XMVector3LengthSq(normal))<.000001f)normal=XMVectorSet(0,0,-1,0);
            normal=XMVector3Normalize(normal);
            XMFLOAT3 direction;XMStoreFloat3(&direction,normal);
            const auto addBurst=[&](ImpactTexture texture,bridge::ProjectileEffectKind kind,
                float size,float life,unsigned count,float speed,float spread,
                const XMFLOAT4& color,bool luminous)
            {
                if(EffectCount()>=128)return;
                bridge::ProjectileEffectLayer layer;layer.kind=kind;
                layer.sizeMetres=size;layer.particleLifeSeconds=life;
                auto position=ProjectileNativeVector(impact.contact.position);
                position.x+=direction.x*.025f;position.y+=direction.y*.025f;position.z+=direction.z*.025f;
                const auto entity=bridge::CreateProjectileEffectEmitter(scene,layer,position,true);
                if(entity==wi::ecs::INVALID_ENTITY)return;
                if(auto* emitter=scene.emitters.GetComponent(entity))
                {
                    emitter->burst_on_create=std::max(texture==ImpactTexture::BloodSpray?1u:2u,unsigned(count*strength));
                    emitter->SetMaxParticleCount(texture==ImpactTexture::WaterDrop?192:64);
                    emitter->velocity={direction.x*speed,direction.y*speed,direction.z*speed};
                    emitter->random_factor=1.f;emitter->normal_factor=spread;emitter->random_life=.25f;
                    const bool cloud=texture==ImpactTexture::Dust || texture==ImpactTexture::ConcreteDust;
                    emitter->gravity=cloud?XMFLOAT3{0,.12f,0}:XMFLOAT3{0,-7.5f,0};
                    emitter->scaleX=cloud?1.9f:.65f;
                    emitter->scaleY=emitter->scaleX;
                    emitter->rotation=texture==ImpactTexture::Spark?0:2.f;
                    emitter->motionBlurAmount=texture==ImpactTexture::Spark?.12f:0;
                    emitter->opacityCurveControlPeakStart=0;
                    emitter->opacityCurveControlPeakEnd=cloud?.08f:.35f;
                    emitter->random_color=.12f;
                    if(texture==ImpactTexture::WaterDrop) {
                        emitter->shaderType=wi::EmittedParticleSystem::SOFT_LIGHTING;
                        emitter->random_factor=.65f;emitter->rotation=0;
                        emitter->gravity={0,-9.81f,0};emitter->motionBlurAmount=.06f;
                        emitter->scaleX=.65f;emitter->scaleY=1.7f;emitter->random_color=.16f;
                        emitter->random_life=.35f;
                    }
                    if(texture==ImpactTexture::BloodDrop)
                    {
                        emitter->shaderType=wi::EmittedParticleSystem::SOFT_LIGHTING;
                        emitter->random_factor=.35f;emitter->rotation=0;
                        emitter->gravity={0,-9.81f,0};emitter->motionBlurAmount=.06f;
                        emitter->random_color=.06f;
                    }
                    if(texture==ImpactTexture::BloodSpray)
                    {emitter->shaderType=wi::EmittedParticleSystem::SOFT_LIGHTING;emitter->random_color=0;
                        emitter->framesX=8;emitter->framesY=8;emitter->frameCount=64;emitter->frameStart=0;
                        emitter->frameRate=0;emitter->SetFrameBlendingEnabled(true);
                        emitter->random_life=0;emitter->random_factor=0;emitter->normal_factor=0;
                        emitter->rotation=0;emitter->gravity={0,0,0};emitter->scaleX=1;emitter->scaleY=1;}
                }
                if(auto* material=scene.materials.GetComponent(entity))
                {
                    const auto& mask=GetImpactTexture(texture);
                    if(mask.IsValid())material->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.SetTexture(mask);
                    material->baseColor=color;
                    material->userBlendMode=luminous?wi::enums::BLENDMODE_ADDITIVE:wi::enums::BLENDMODE_ALPHA;
                    material->emissiveColor=luminous?XMFLOAT4{color.x,color.y,color.z,.7f}:XMFLOAT4{0,0,0,0};
                    material->SetDirty();
                }
                dyingEffects.push_back({entity,life+.65f});
            };
            if(surface==Surface::Metal)
            {
                addBurst(ImpactTexture::Spark,bridge::ProjectileEffectKind::Sparks,.06f,.19f,10,2.4f,1.7f,{1,.87f,.55f,.85f},true);
            }
            else if(surface==Surface::Wood)
            {
                if(!impactGeometry.Debris(scene,RuntimeImpactGeometry::Kind::Wood,
                    ProjectileNativeVector(impact.contact.position),direction,strength,static_cast<unsigned>(impact.projectileId)))
                addBurst(ImpactTexture::Splinter,bridge::ProjectileEffectKind::Smoke,.065f,.65f,14,.9f,1.4f,{.49f,.28f,.10f,.95f},false);
                addBurst(ImpactTexture::ConcreteDust,bridge::ProjectileEffectKind::Smoke,.12f,.45f,4,.3f,.4f,{.52f,.36f,.20f,.22f},false);
            }
            else if(surface==Surface::Concrete || surface==Surface::Stone)
            {
                if(!impactGeometry.Debris(scene,RuntimeImpactGeometry::Kind::Rock,
                    ProjectileNativeVector(impact.contact.position),direction,strength,static_cast<unsigned>(impact.projectileId)))
                addBurst(ImpactTexture::Chip,bridge::ProjectileEffectKind::Smoke,.05f,.65f,12,1.f,1.5f,{.72f,.71f,.68f,.95f},false);
                addBurst(ImpactTexture::ConcreteDust,bridge::ProjectileEffectKind::Smoke,.32f,1.05f,6,.45f,.5f,{.78f,.77f,.74f,.32f},false);
            }
            else if(surface==Surface::Character)
            {
                auto incoming=XMVectorSet(impact.incomingVelocity.x,impact.incomingVelocity.y,impact.incomingVelocity.z,0);
                const float incomingLength=XMVectorGetX(XMVector3LengthSq(incoming));
                if(std::isfinite(incomingLength) && incomingLength>.000001f)
                    XMStoreFloat3(&direction,XMVector3Normalize(normal*.65f-XMVector3Normalize(incoming)*.35f));
                // Mesh normals can point with the incoming shot (backface contact).
                // Present blood on the struck, incoming-facing side.
                if(incomingLength>.000001f && XMVectorGetX(XMVector3Dot(XMLoadFloat3(&direction),incoming))>0)
                {direction.x=-direction.x;direction.y=-direction.y;direction.z=-direction.z;}
                const unsigned bloodSeed=static_cast<unsigned>(impact.projectileId)+
                    (blood.directionalPrototype?++blood.hitSerial*101u:0u);
                if(blood.directionalPrototype && std::isfinite(incomingLength) && incomingLength>.000001f)
                {
                    const auto incident=XMVector3Normalize(incoming);
                    auto outward=normal;
                    if(XMVectorGetX(XMVector3Dot(incident,outward))>0)outward=-outward;
                    const auto reflected=incident-outward*(2*XMVectorGetX(XMVector3Dot(incident,outward)));
                    XMStoreFloat3(&direction,XMVector3Normalize(reflected*.7f+outward*.3f));
                }
                XMFLOAT3 sheetFacing=direction,sheetOutward;
                auto outward=normal;
                if(incomingLength>.000001f) {
                    XMStoreFloat3(&sheetFacing,-XMVector3Normalize(incoming));
                    if(XMVectorGetX(XMVector3Dot(incoming,outward))>0)outward=-outward;
                }
                XMStoreFloat3(&sheetOutward,outward);
                if(!blood.liquidSheets.Spawn(scene,ProjectileNativeVector(impact.contact.position),
                    direction,bloodSeed,blood.directionalPrototype,
                    blood.directionalPrototype?&sheetFacing:nullptr,
                    blood.directionalPrototype?&sheetOutward:nullptr))
                    addBurst(ImpactTexture::BloodSpray,bridge::ProjectileEffectKind::Smoke,
                        embedded?.70f:1.0f,1.25f,1,.45f,0,{.70f,.70f,.70f,.9f},false);
                auto source=parent;
                for(size_t i=0;i<=scene.hierarchy.GetCount() && source!=wi::ecs::INVALID_ENTITY;++i)
                {
                    if(scene.characters.Contains(source))break;
                    const auto* h=scene.hierarchy.GetComponent(source);
                    if(!h) {source=parent;break;}
                    source=h->parentID;
                }
                blood.Spawn(scene,ProjectileNativeVector(impact.contact.position),direction,
                    source,strength,bloodSeed);
                if(blood.directionalPrototype)
                    addBurst(ImpactTexture::BloodDrop,bridge::ProjectileEffectKind::Smoke,
                        .018f,.45f,28,3.7f,.35f,{.23f,.004f,.009f,.85f},false);
            }
            else if(surface==Surface::Glass)
            {
                impactGeometry.Glass(scene,ProjectileNativeVector(impact.contact.position),direction,
                    strength,static_cast<unsigned>(impact.projectileId));
            }
            else if(surface==Surface::Water)
            {
                impactGeometry.Water(scene,ProjectileNativeVector(impact.contact.position),direction,strength);
                addBurst(ImpactTexture::WaterDrop,bridge::ProjectileEffectKind::Smoke,
                    .014f,.58f,160,3.4f,.22f,{1,1,1,.6f},false);
                addBurst(ImpactTexture::WaterDrop,bridge::ProjectileEffectKind::Smoke,
                    .011f,.44f,96,1.85f,1.f,{1,1,1,.5f},false);
            }
            else
            {
            if (EffectCount() < 128)
            {
                bridge::ProjectileEffectLayer layer;
                const bool dusty = surface == Surface::Wood ||
                    surface == Surface::Concrete || surface == Surface::Stone ||
                    surface == Surface::Dirt || surface == Surface::Water ||
                    surface == Surface::Character;
                layer.kind = dusty ? bridge::ProjectileEffectKind::Smoke :
                    bridge::ProjectileEffectKind::Sparks;
                layer.sizeMetres =
                    surface == Surface::Dirt ? .12f :
                    surface == Surface::Concrete || surface == Surface::Stone ? .10f :
                    surface == Surface::Wood || surface == Surface::Water ? .085f :
                    surface == Surface::Character ? .055f : .05f;
                layer.particleLifeSeconds =
                    surface == Surface::Dirt ? .48f :
                    surface == Surface::Concrete || surface == Surface::Stone ? .40f :
                    surface == Surface::Wood ? .34f :
                    surface == Surface::Water ? .30f : .24f;
                const auto e = bridge::CreateProjectileEffectEmitter(
                    scene, layer, ProjectileNativeVector(impact.contact.position), true);
                if (e != wi::ecs::INVALID_ENTITY)
                {
                    if (auto* emitter = scene.emitters.GetComponent(e))
                    {
                        emitter->burst_on_create =
                            surface == Surface::Dirt ? 58 :
                            surface == Surface::Concrete || surface == Surface::Stone ? 52 :
                            surface == Surface::Water ? 44 : 34;
                        if (surface == Surface::Water)
                        {
                            emitter->velocity = {0, .65f, 0};
                            emitter->gravity = {0, -3.0f, 0};
                        }
                        else if (surface == Surface::Metal || surface == Surface::Glass)
                        {
                            emitter->velocity = {0, .28f, 0};
                            emitter->gravity = {0, -4.5f, 0};
                        }
                    }
                    if (auto* material = scene.materials.GetComponent(e))
                    {
                        material->baseColor = SurfaceEffectColor(surface);
                        if (layer.kind == bridge::ProjectileEffectKind::Sparks)
                            material->emissiveColor = {
                                material->baseColor.x, material->baseColor.y,
                                material->baseColor.z, 2.0f};
                        else
                            material->emissiveColor = {0,0,0,0};
                        material->SetDirty();
                    }
                    dyingEffects.push_back({e, layer.particleLifeSeconds + .65f});
                }
            }

            }

            const auto markDonor=RuntimeImpactMarks::FindMaterial(scene,surface);
            // The 2x2 original marks are static variations, not animation frames.
            const wi::graphics::Texture* authoredMark=nullptr;
            switch(surface) {
            case Surface::Metal: authoredMark=&GetBuiltinImpactAtlas(BuiltinImpactAtlas::MetalMarks);break;
            case Surface::Wood: authoredMark=&GetBuiltinImpactAtlas(BuiltinImpactAtlas::WoodMarks);break;
            case Surface::Concrete: authoredMark=&GetBuiltinImpactAtlas(BuiltinImpactAtlas::ConcreteMarks);break;
            case Surface::Stone: authoredMark=&GetBuiltinImpactAtlas(BuiltinImpactAtlas::StoneMarks);break;
            case Surface::Dirt: authoredMark=&GetBuiltinImpactAtlas(BuiltinImpactAtlas::DirtMarks);break;
            case Surface::Glass: authoredMark=&GetBuiltinImpactAtlas(BuiltinImpactAtlas::GlassMarks);break;
            case Surface::Character: authoredMark=&GetBuiltinImpactAtlas(
                embedded?BuiltinImpactAtlas::SkinEntryMarks:BuiltinImpactAtlas::SkinMarks);break;
            default:break;
            }
            const bool builtInMark=authoredMark && authoredMark->IsValid();
            const bool detailedMark=builtInMark || markDonor!=wi::ecs::INVALID_ENTITY;
            RuntimeImpactMarks::Anchor skinAnchor;
            if(surface==Surface::Character && (!detailedMark ||
                !RuntimeImpactMarks::Capture(scene,ProjectileNativeVector(impact.contact.position),direction,parent,skinAnchor)))
                return;
            if(surface==Surface::Water)return;
            if(surface!=Surface::Character && markDonor!=wi::ecs::INVALID_ENTITY)
                RuntimeImpactMarks::Capture(scene,ProjectileNativeVector(impact.contact.position),direction,parent,skinAnchor);
            if (impactDecals.size() >= 64)
            {
                scene.Entity_Remove(impactDecals.front().entity);
                impactDecals.erase(impactDecals.begin());
            }

            const auto entity = scene.Entity_CreateDecal("Impact mark", "", "");
            auto* transform = scene.transforms.GetComponent(entity);
            auto* material = scene.materials.GetComponent(entity);
            if (entity == wi::ecs::INVALID_ENTITY || transform == nullptr ||
                material == nullptr)
            {
                if (entity != wi::ecs::INVALID_ENTITY)
                    scene.Entity_Remove(entity);
                return;
            }

            static wi::graphics::Texture mark;
            if (auto* device = wi::graphics::GetDevice())
            {
                if (!mark.IsValid())
                {
                    std::array<unsigned char, 32 * 32 * 4> pixels{};
                    for (unsigned y = 0; y < 32; ++y)
                    for (unsigned x = 0; x < 32; ++x)
                    {
                        const float dx = (float(x) + .5f) / 16.0f - 1.0f;
                        const float dy = (float(y) + .5f) / 16.0f - 1.0f;
                        const float r2 = dx * dx + dy * dy;
                        const float edge = std::max(0.0f, 1.0f - r2);
                        const float core = std::max(0.0f, 1.0f - r2 * 3.2f);
                        const auto i = (y * 32 + x) * 4;
                        pixels[i] = pixels[i + 1] = pixels[i + 2] = 255;
                        pixels[i + 3] = static_cast<unsigned char>(
                            std::min(1.0f, edge * .55f + core * .75f) * 255.0f);
                    }
                    wi::graphics::TextureDesc desc;
                    desc.width = desc.height = 32;
                    desc.format = wi::graphics::Format::R8G8B8A8_UNORM;
                    desc.bind_flags = wi::graphics::BindFlag::SHADER_RESOURCE;
                    wi::graphics::SubresourceData data;
                    data.data_ptr = pixels.data();
                    data.row_pitch = 32 * 4;
                    data.slice_pitch = pixels.size();
                    device->CreateTexture(&desc, &data, &mark);
                }
                if (mark.IsValid())
                    material->textures[
                        wi::scene::MaterialComponent::BASECOLORMAP].resource.SetTexture(mark);
            }


            if(polished)
            {
                const auto type=surface==Surface::Glass?ImpactTexture::GlassMark:
                    surface==Surface::Wood?ImpactTexture::WoodMark:
                    surface==Surface::Concrete?ImpactTexture::ConcreteMark:ImpactTexture::MetalMark;
                const auto& texture=GetImpactTexture(type);
                if(texture.IsValid())material->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.SetTexture(texture);
            }
            material->userBlendMode = wi::enums::BLENDMODE_ALPHA;
            material->baseColor =
                surface == Surface::Glass?XMFLOAT4{.86f,.93f,.97f,.75f}:
                surface == Surface::Wood || surface == Surface::Dirt
                    ? XMFLOAT4{.28f, .16f, .07f, .90f}
                    : surface==Surface::Concrete?XMFLOAT4{.58f,.56f,.52f,.95f}:XMFLOAT4{.16f,.16f,.15f,.95f};
            material->SetRoughness(1.0f);
            const unsigned markSeed=static_cast<unsigned>(impact.projectileId)+ ++markSerial*101u;
            if(builtInMark) {
                material->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.SetTexture(*authoredMark);
                const auto variant=markSeed%4;
                material->texMulAdd={.5f,.5f,float(variant%2)*.5f,float(variant/2)*.5f};
                material->baseColor={1,1,1,1};
                material->SetRoughness(surface==Surface::Glass?.28f:.92f);
                material->SetReflectance(surface==Surface::Glass?.08f:.02f);
                material->SetAlphaRef(1.f);material->emissiveColor={0,0,0,0};
            }
            else if(markDonor!=wi::ecs::INVALID_ENTITY) {
                *material=*scene.materials.GetComponent(markDonor);
                const auto variant=markSeed%4;
                material->texMulAdd={.5f,.5f,float(variant%2)*.5f,float(variant/2)*.5f};
                if(surface==Surface::Glass) {
                    material->baseColor={3.f,3.f,3.f,1};material->SetNormalMapStrength(.25f);
                }
            }
            material->SetDirty();


            const auto up = std::abs(XMVectorGetY(normal)) > .95f
                ? XMVectorSet(1,0,0,0) : XMVectorSet(0,1,0,0);
            const auto facing = XMMatrixInverse(
                nullptr, XMMatrixLookToLH(XMVectorZero(), normal, up));
            const float size =
                surface == Surface::Glass ? .16f :
                surface == Surface::Concrete || surface == Surface::Stone
                    ? .075f : .065f;
            const float variedSize=size*(embedded?.65f:1.f)*
                (detailedMark?(.85f+float(markSeed%31)*.01f):1.f);
            const float roll=detailedMark?float(markSeed%628)*.01f:0;
            const auto n = direction;
            const auto p = impact.contact.position;
            transform->ClearTransform();
            transform->MatrixTransform(
                XMMatrixScaling(variedSize,variedSize,.012f)*XMMatrixRotationZ(roll) * facing *
                XMMatrixTranslation(
                    p.x + n.x * .003f,
                    p.y + n.y * .003f,
                    p.z + n.z * .003f));
            transform->UpdateTransform();
            if(skinAnchor.receiver!=wi::ecs::INVALID_ENTITY) {
                skinAnchor.size=variedSize;skinAnchor.roll=roll;
                if(!RuntimeImpactMarks::Follow(scene,entity,skinAnchor)){scene.Entity_Remove(entity);return;}
            } else if (parent != wi::ecs::INVALID_ENTITY && scene.transforms.Contains(parent))
                scene.Component_Attach(entity, parent);
            impactDecals.push_back({entity,detailedMark?120.f:18.f,skinAnchor});
            if(surface==Surface::Character && embedded && skinAnchor.receiver!=wi::ecs::INVALID_ENTITY &&
                RuntimeImpactMarks::BloodTrickle(markSeed%4).IsValid()) {
                if(impactDecals.size()>=64){scene.Entity_Remove(impactDecals.front().entity);impactDecals.erase(impactDecals.begin());}
                const auto trail=scene.Entity_CreateDecal("Arrow wound blood trickle","","");
                auto* m=scene.materials.GetComponent(trail);
                m->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.SetTexture(RuntimeImpactMarks::BloodTrickle(markSeed%4));
                m->textures[wi::scene::MaterialComponent::SURFACEMAP].resource.SetTexture(RuntimeBloodEffects::StainSurface(0));
                m->baseColor={.38f,.008f,.016f,.95f};m->SetDirty();
                auto trailAnchor=skinAnchor;trailAnchor.width=.009f;trailAnchor.height=.002f;
                trailAnchor.offsetY=.002f;trailAnchor.gravityAligned=true;trailAnchor.roll=0;
                if(RuntimeImpactMarks::Follow(scene,trail,trailAnchor))impactDecals.push_back({trail,120.f,trailAnchor,0});
                else scene.Entity_Remove(trail);
            }
            if(surface==Surface::Glass && markDonor!=wi::ecs::INVALID_ENTITY &&
                skinAnchor.receiver!=wi::ecs::INVALID_ENTITY && RuntimeImpactMarks::GlassPuncture().IsValid()) {
                if(impactDecals.size()>=64){scene.Entity_Remove(impactDecals.front().entity);impactDecals.erase(impactDecals.begin());}
                const auto core=scene.Entity_CreateDecal("Glass impact puncture","","");
                auto* m=scene.materials.GetComponent(core);
                m->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.SetTexture(RuntimeImpactMarks::GlassPuncture());
                m->baseColor={1,1,1,1};m->SetRoughness(.8f);m->SetReflectance(.02f);m->SetDirty();
                auto coreAnchor=skinAnchor;coreAnchor.size*=.14f;
                if(RuntimeImpactMarks::Follow(scene,core,coreAnchor))impactDecals.push_back({core,120.f,coreAnchor});
                else scene.Entity_Remove(core);
            }
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
            if(!d.stickOnImpact || impact.contact.surfaceType==bridge::ImpactSurfaceType::Water)return;
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
            Retained retainedArrow{instance->second,d.stuckLifetimeSeconds};
            if(auto* t=scene.transforms.GetComponent(instance->second)) {
                // A rotated child under nonuniform scale cannot be represented as local TRS
                // without shear. Track the receiver explicitly instead of inheriting scale.
                scene.Component_Detach(instance->second);
                t=scene.transforms.GetComponent(instance->second);
                const auto pose=Pose(d,v,p);
                t->ClearTransform();t->MatrixTransform(pose);t->UpdateTransform();
                if(const auto* receiver=scene.transforms.GetComponent(parent)) {
                    retainedArrow.receiver=parent;
                    XMStoreFloat3(&retainedArrow.localContact,XMVector3TransformCoord(
                        XMVectorSet(p.x,p.y,p.z,1),XMMatrixInverse(nullptr,receiver->GetWorldMatrix())));
                    XMStoreFloat4x4(&retainedArrow.relativePose,
                        pose*XMMatrixInverse(nullptr,ReceiverRigidPose(*receiver)));
                }
            }
            if(impact.contact.surfaceType==bridge::ImpactSurfaceType::Character &&
                RuntimeImpactMarks::Capture(scene,ProjectileNativeVector(impact.contact.position),
                    ProjectileNativeVector(impact.contact.normal),parent,retainedArrow.skinAnchor)) {
                XMFLOAT4X4 surface;
                if(RuntimeImpactMarks::SurfacePose(scene,retainedArrow.skinAnchor,surface))
                    XMStoreFloat4x4(&retainedArrow.surfaceRelativePose,
                        scene.transforms.GetComponent(retainedArrow.root)->GetWorldMatrix()*XMMatrixInverse(nullptr,XMLoadFloat4x4(&surface)));
                else retainedArrow.skinAnchor={};
            }
            retained.push_back(retainedArrow);
            instances.erase(instance);instanceAssets.erase(association);
        }
        static void RefreshArrowObject(wi::scene::Scene& scene,wi::ecs::Entity entity)
        {
            const auto index=scene.objects.GetIndex(entity);
            auto* object=scene.objects.GetComponent(entity);const auto* transform=scene.transforms.GetComponent(entity);
            const auto* mesh=object?scene.meshes.GetComponent(object->meshID):nullptr;
            if(!mesh || !transform || !scene.instanceArrayMapped || index>=scene.matrix_objects.size())return;
            const auto world=transform->GetWorldMatrix();
            auto bounds=mesh->aabb.transform(transform->world);bounds.layerMask=scene.aabb_objects[index].layerMask;
            scene.aabb_objects[index]=bounds;scene.matrix_objects[index]=transform->world;
            object->center=bounds.getCenter();object->radius=bounds.getRadius();
            scene.bounds=wi::primitive::AABB::Merge(scene.bounds,bounds);
            // Only current pose fields change; native instance identity, meshlet
            // allocation and previous-frame transform remain untouched.
            ShaderMeshInstance instance;
            std::memcpy(&instance,scene.instanceArrayMapped+index,sizeof(instance));
            instance.transformRaw.Create(transform->world);
            XMFLOAT4X4 gpuWorld=transform->world;
            if(wi::graphics::IsFormatUnorm(mesh->position_format) && !mesh->so_pos.IsValid())
                XMStoreFloat4x4(&gpuWorld,mesh->aabb.getUnormRemapMatrix()*world);
            instance.transform.Create(gpuWorld);instance.center=object->center;instance.radius=object->radius;
            std::memcpy(scene.instanceArrayMapped+index,&instance,sizeof(instance));
            if(scene.TLAS_instancesMapped && object->lod<mesh->BLASes.size()) {
                using Accel=wi::graphics::RaytracingAccelerationStructureDesc;
                Accel::TopLevel::Instance rayInstance;
                for(int i=0;i<3;++i)for(int j=0;j<4;++j)rayInstance.transform[i][j]=gpuWorld.m[j][i];
                rayInstance.instance_id=uint32_t(index);rayInstance.instance_mask=bounds.layerMask?0xFF:0;
                if(!object->IsRenderable() || !mesh->IsRenderable())rayInstance.instance_mask=0;
                if(!object->IsCastingShadow())rayInstance.instance_mask&=~wi::renderer::raytracing_inclusion_mask_shadow;
                if(object->IsNotVisibleInReflections())rayInstance.instance_mask&=~wi::renderer::raytracing_inclusion_mask_reflection;
                rayInstance.bottom_level=&mesh->BLASes[object->lod];rayInstance.instance_contribution_to_hit_group_index=0;
                rayInstance.flags=0;
                if(mesh->IsDoubleSided() || mesh->_flags & wi::scene::MeshComponent::TLAS_FORCE_DOUBLE_SIDED)
                    rayInstance.flags|=Accel::TopLevel::Instance::FLAG_TRIANGLE_CULL_DISABLE;
                if(XMVectorGetX(XMMatrixDeterminant(world))>0)
                    rayInstance.flags|=Accel::TopLevel::Instance::FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;
                auto* device=wi::graphics::GetDevice();
                auto* dest=static_cast<unsigned char*>(scene.TLAS_instancesMapped)+index*device->GetTopLevelAccelerationStructureInstanceSize();
                device->WriteTopLevelAccelerationStructureInstance(&rayInstance,dest);
            }
        }
        // Only refresh transforms and decal render data after completed skinning.
        // Entity creation, expiry and removal stay in the pre-upload Sync phase.
        void RefreshImpactMarkPose(wi::scene::Scene& scene)
        {
            for(const auto& arrow:retained)if(arrow.skinAnchor.receiver!=wi::ecs::INVALID_ENTITY) {
                const auto* oldTransform=scene.transforms.GetComponent(arrow.root);if(!oldTransform)continue;
                const auto oldWorld=oldTransform->GetWorldMatrix();
                if(!FollowReceiver(scene,arrow))continue;
                const auto delta=XMMatrixInverse(nullptr,oldWorld)*scene.transforms.GetComponent(arrow.root)->GetWorldMatrix();
                // Preserve child local transforms. Only completed-frame world
                // matrices and their existing render slots are refreshed.
                for(size_t i=0;i<scene.transforms.GetCount();++i) {
                    const auto entity=scene.transforms.GetEntity(i);
                    if(entity!=arrow.root && scene.Entity_IsDescendant(entity,arrow.root))
                        XMStoreFloat4x4(&scene.transforms[i].world,scene.transforms[i].GetWorldMatrix()*delta);
                }
                for(size_t i=0;i<scene.objects.GetCount();++i) {
                    const auto entity=scene.objects.GetEntity(i);
                    if(entity==arrow.root || scene.Entity_IsDescendant(entity,arrow.root))RefreshArrowObject(scene,entity);
                }
            }
            bool changed=false;
            for(auto& mark:impactDecals)
                if(mark.anchor.receiver!=wi::ecs::INVALID_ENTITY &&
                    RuntimeImpactMarks::Follow(scene,mark.entity,mark.anchor))changed=true;
            if(changed) {
                wi::jobsystem::context ctx;
                scene.RunDecalUpdateSystem(ctx);
                wi::jobsystem::Wait(ctx);
            }
        }
        void Sync(wi::scene::Scene& scene, const bridge::ProjectileSimulation& simulation,float dt=0)
        {
            blood.Update(scene,dt,roots);
            impactGeometry.Update(scene,dt);
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
                } else if(!FollowReceiver(scene,*it)) {
                    scene.Entity_Remove(it->root);it=retained.erase(it);
                } else ++it;
            }
            for(auto it=dyingEffects.begin();it!=dyingEffects.end();) {
                it->seconds-=dt;
                if(it->seconds<=0) {scene.Entity_Remove(it->entity);it=dyingEffects.erase(it);}
                else ++it;
            }
            for(auto it=impactDecals.begin();it!=impactDecals.end();) {
                if(it->bleedAge>=0) {
                    it->bleedAge+=std::max(0.f,dt);
                    const float growth=std::clamp((it->bleedAge-.2f)/4.f,0.f,1.f);
                    it->anchor.height=.002f+.058f*growth;it->anchor.offsetY=it->anchor.height;
                    // Once flow stops, the stain rotates with its receiver instead of reorienting to gravity.
                    it->anchor.gravityAligned=it->bleedAge<4.2f;
                    if(auto* m=scene.materials.GetComponent(it->entity)) {
                        m->textures[wi::scene::MaterialComponent::SURFACEMAP].resource.SetTexture(
                            RuntimeBloodEffects::StainSurface(std::clamp(it->bleedAge/90.f,0.f,1.f)));
                        m->SetDirty();
                    }
                }
                it->seconds-=dt;
                if(it->seconds<=0 || !scene.transforms.Contains(it->entity) ||
                    (it->anchor.receiver!=wi::ecs::INVALID_ENTITY && !RuntimeImpactMarks::Follow(scene,it->entity,it->anchor))) {
                    scene.Entity_Remove(it->entity);it=impactDecals.erase(it);
                } else ++it;
            }
            roots.clear();
            for(const auto& [id,root]:instances)roots.push_back(root);
            for(const auto& r:retained)roots.push_back(r.root);
            for(const auto& piece:impactGeometry.pieces)roots.push_back(piece.entity);
            for(const auto& drop:blood.drops)roots.push_back(drop.entity);
            for(const auto& sheet:blood.liquidSheets.sheets)roots.push_back(sheet.entity);
            for(auto it=instanceAssets.begin();it!=instanceAssets.end();)
                if(!instances.count(it->first))it=instanceAssets.erase(it);else ++it;
        }
        std::map<std::uint64_t,bridge::StableId> instanceAssets;
    };
}
