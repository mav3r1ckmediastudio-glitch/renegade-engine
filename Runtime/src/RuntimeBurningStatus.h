#pragma once
#include "RuntimeCombatDamage.h"
#include "RuntimeFireballEffect.h"
#include "RuntimeImpactMarks.h"
#include <cctype>

namespace renegade::runtime {
// Transient status ownership. All health mutation goes through attributed combat damage.
struct RuntimeBurningStatus {
    static constexpr size_t MaxTargets=64;
    static constexpr unsigned FlameCount=14, EmitterCount=17;
    using Bone=wi::scene::HumanoidComponent::HumanoidBone;
    static constexpr const char* ImmuneKey="renegade.status.fire_immune";
    struct Entry {
        bridge::StableId target;
        wi::ecs::Entity receiver=wi::ecs::INVALID_ENTITY,root=wi::ecs::INVALID_ENTITY;
        bridge::ProjectileSource source;
        float remaining=0,damagePerSecond=0,tickSeconds=0,pendingDamage=0;
        std::array<wi::ecs::Entity,EmitterCount> emitters{},bones{},ends{};
        std::array<RuntimeImpactMarks::Anchor,FlameCount> surfaces{};
        std::array<unsigned char,FlameCount> surfaceAttempts{};
    };
    std::vector<Entry> entries;
    static bool Immune(const wi::scene::Scene& scene,wi::ecs::Entity entity) {
        const auto* metadata=scene.metadatas.GetComponent(entity);
        return metadata && metadata->bool_values.has(ImmuneKey) && metadata->bool_values.get(ImmuneKey);
    }
    void Reset(wi::scene::Scene& scene) {
        for(const auto& e:entries)scene.Entity_Remove(e.root);
        entries.clear();
    }
    bool Extinguish(wi::scene::Scene& scene,const bridge::StableId& target) {
        const auto it=std::find_if(entries.begin(),entries.end(),[&](const auto& e){return e.target==target;});
        if(it==entries.end())return false;
        scene.Entity_Remove(it->root);entries.erase(it);return true;
    }
    static wi::ecs::Entity BodyBone(const wi::scene::Scene& scene,wi::ecs::Entity receiver,Bone wanted) {
        // Prefer the live skinned skeleton, not retained retarget-source rigs.
        bool hasSkin=false;
        const auto renderedBone=[&](wi::ecs::Entity bone) {
            for(size_t i=0;i<scene.objects.GetCount();++i) {
                const auto object=scene.objects.GetEntity(i);
                if(object!=receiver && !scene.Entity_IsDescendant(object,receiver))continue;
                const auto* mesh=scene.meshes.GetComponent(scene.objects[i].meshID);
                const auto* armature=mesh?scene.armatures.GetComponent(mesh->armatureID):nullptr;
                if(!armature)continue;
                if(std::find(armature->boneCollection.begin(),armature->boneCollection.end(),bone)!=armature->boneCollection.end())return true;
            }
            return false;
        };
        for(size_t i=0;i<scene.objects.GetCount();++i) {
            const auto object=scene.objects.GetEntity(i);
            if(object!=receiver && !scene.Entity_IsDescendant(object,receiver))continue;
            const auto* mesh=scene.meshes.GetComponent(scene.objects[i].meshID);
            hasSkin=hasSkin || (mesh && scene.armatures.Contains(mesh->armatureID));
        }
        for(size_t i=0;i<scene.humanoids.GetCount();++i) {
            const auto owner=scene.humanoids.GetEntity(i);
            if(owner!=receiver && !scene.Entity_IsDescendant(owner,receiver))continue;
            const auto entity=scene.humanoids[i].bones[size_t(wanted)];
            if(scene.transforms.Contains(entity) && (!hasSkin || renderedBone(entity)) &&
                (entity==receiver || scene.Entity_IsDescendant(entity,receiver)))return entity;
        }
        for(size_t i=0;i<scene.names.GetCount();++i) {
            const auto entity=scene.names.GetEntity(i);
            if(!scene.transforms.Contains(entity) || !scene.Entity_IsDescendant(entity,receiver) ||
                (hasSkin && !renderedBone(entity)))continue;
            auto name=scene.names[i].name;
            std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return char(std::tolower(c));});
            const auto has=[&](const char* part){return name.find(part)!=std::string::npos;};
            bool match=false;
            switch(wanted) {
            case Bone::Hips:match=has("pelvis")||has("hips");break;
            case Bone::Chest:match=has("spine2")||has("spine_03")||has("chest");break;
            case Bone::Head:match=has("head")&&!has("end");break;
            case Bone::LeftUpperArm:match=has("leftarm")||has("upperarm_l");break;
            case Bone::RightUpperArm:match=has("rightarm")||has("upperarm_r");break;
            case Bone::LeftLowerArm:match=has("leftforearm")||has("lowerarm_l");break;
            case Bone::RightLowerArm:match=has("rightforearm")||has("lowerarm_r");break;
            case Bone::LeftHand:match=has("lefthand")||has("hand_l");break;
            case Bone::RightHand:match=has("righthand")||has("hand_r");break;
            default:break;
            }
            if(match)return entity;
        }
        return receiver;
    }
    static Bone Region(unsigned i) {
        if(i<4)return Bone::Hips;
        if(i<8 || i==14)return Bone::Chest;
        if(i==8)return Bone::LeftUpperArm;
        if(i==9)return Bone::LeftLowerArm;
        if(i==10)return Bone::RightUpperArm;
        if(i==11)return Bone::RightLowerArm;
        if(i==15)return Bone::LeftUpperArm;
        if(i==16)return Bone::RightUpperArm;
        return Bone::Head;
    }
    static void CreateAppearance(wi::scene::Scene& scene,Entry& e) {
        e.root=scene.Entity_CreateTransform("Burning status");scene.Component_Attach(e.root,e.receiver,true);
        for(unsigned i=0;i<EmitterCount;++i) {
            const bool smoke=i>=FlameCount;
            bridge::ProjectileEffectLayer layer;layer.kind=smoke?bridge::ProjectileEffectKind::Smoke:bridge::ProjectileEffectKind::Flame;
            layer.sizeMetres=(i>=8 && i<12)?.24f:.18f;layer.particlesPerSecond=smoke?9.f:28.f;
            layer.particleLifeSeconds=smoke?1.2f:.75f;
            const auto entity=bridge::CreateProjectileEffectEmitter(scene,layer,{},false);e.emitters[i]=entity;
            scene.Component_Attach(entity,e.root,true);
            e.bones[i]=BodyBone(scene,e.receiver,Region(i));
            e.ends[i]=e.bones[i];
            if(i>=8 && i<=11)e.ends[i]=BodyBone(scene,e.receiver,
                i==8?Bone::LeftLowerArm:i==9?Bone::LeftHand:i==10?Bone::RightLowerArm:Bone::RightHand);
            auto* p=scene.emitters.GetComponent(entity);if(!p)continue;
            p->SetMaxParticleCount(64);p->velocity={0,smoke?.45f:.50f,0};p->gravity={0,smoke?.25f:.35f,0};
            p->normal_factor=0;p->random_factor=.22f;p->random_life=.25f;
            p->scaleX=smoke?1.7f:.8f;p->scaleY=smoke?2.f:2.f;
            p->framesX=p->framesY=4;p->frameCount=16;p->frameRate=24;p->SetFrameBlendingEnabled(true);
            // Keep the textured native shader; adequate surface clearance avoids fading inside skin.
            p->shaderType=smoke?wi::EmittedParticleSystem::SOFT_LIGHTING:wi::EmittedParticleSystem::SOFT;
            auto* m=scene.materials.GetComponent(entity);if(!m)continue;
            m->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.SetTexture(RuntimeFireballEffect::Texture(smoke));
            m->baseColor=smoke?XMFLOAT4{.20f,.18f,.16f,.24f}:XMFLOAT4{1,.48f,.12f,.40f};
            m->userBlendMode=smoke?wi::enums::BLENDMODE_ALPHA:wi::enums::BLENDMODE_ADDITIVE;
            m->emissiveColor=smoke?XMFLOAT4{0,0,0,0}:XMFLOAT4{1,.30f,.04f,.16f};m->SetDirty();
        }
    }
    bool Ignite(wi::scene::Scene& scene,const RuntimeCharacterSystemState& characters,
        RuntimeCharacterPerceptionState& perception,RuntimeCombatState& combat,
        const bridge::ProjectileImpact& impact,bool acceptedDamage,const CombatEventEmitter& emit) {
        const auto* character=FindRuntimeCharacter(characters,impact.contact.targetSubjectId);
        const auto* health=FindCharacterCombat(combat,impact.contact.targetSubjectId);
        if(!acceptedDamage || !character || !health || health->dead || !scene.transforms.Contains(character->entity) ||
           Immune(scene,character->entity) || !bridge::ValidBurningPayload(impact.burning) || impact.burning.durationSeconds<=0 ||
           !bridge::IsValidStableId(impact.source.sourceAssetId) || impact.source.ownerSubjectId.empty() ||
           impact.source.ownerSubjectId==impact.contact.targetSubjectId)return false;
        auto found=std::find_if(entries.begin(),entries.end(),[&](const auto& e){return e.target==impact.contact.targetSubjectId;});
        if(found!=entries.end()) {
            // Refresh duration and retain strongest dose; never stack independent health drains.
            if(impact.burning.damagePerSecond>=found->damagePerSecond) {
                // Settle the previous dose before replacing its attribution.
                if(found->pendingDamage>0) {
                    std::string error;
                    if(!ApplyAttributedCombatDamage(scene,characters,perception,combat,found->target,
                        found->source.ownerSubjectId,found->source.factionId,ProjectilePosition(found->source.knownPosition),
                        ProjectilePosition(found->source.knownVelocity),found->pendingDamage,emit,error))return false;
                    found->pendingDamage=found->tickSeconds=0;
                    const auto* current=FindCharacterCombat(combat,found->target);
                    if(!current || current->dead){Extinguish(scene,found->target);return false;}
                }
                found->damagePerSecond=impact.burning.damagePerSecond;found->source=impact.source;
            }
            found->remaining=std::max(found->remaining,impact.burning.durationSeconds);
            return true;
        }
        if(entries.size()>=MaxTargets)return false;
        Entry e;e.target=impact.contact.targetSubjectId;e.receiver=character->entity;e.source=impact.source;
        e.remaining=impact.burning.durationSeconds;e.damagePerSecond=impact.burning.damagePerSecond;
        CreateAppearance(scene,e);entries.push_back(std::move(e));return true;
    }
    void Update(wi::scene::Scene& scene,const RuntimeCharacterSystemState& characters,
        RuntimeCharacterPerceptionState& perception,RuntimeCombatState& combat,float dt,const CombatEventEmitter& emit) {
        if(!std::isfinite(dt) || dt<0 || dt>10)return;
        for(auto it=entries.begin();it!=entries.end();) {
            const auto* target=FindRuntimeCharacter(characters,it->target);
            const auto* health=FindCharacterCombat(combat,it->target);
            bool remove=!target || target->entity!=it->receiver || !health || health->dead ||
                !scene.transforms.Contains(it->receiver) || Immune(scene,it->receiver);
            for(const auto entity:it->emitters)if(auto* p=scene.emitters.GetComponent(entity))p->SetPaused(dt==0);
            if(!remove && dt>0) {
                const float elapsed=std::min(dt,it->remaining);
                it->remaining=std::max(0.f,it->remaining-elapsed);
                it->tickSeconds+=elapsed;it->pendingDamage+=elapsed*it->damagePerSecond;
                if(it->tickSeconds>=.25f || it->remaining<=.00001f) {
                    std::string error;
                    if(!ApplyAttributedCombatDamage(scene,characters,perception,combat,it->target,
                        it->source.ownerSubjectId,it->source.factionId,ProjectilePosition(it->source.knownPosition),
                        ProjectilePosition(it->source.knownVelocity),it->pendingDamage,emit,error))remove=true;
                    it->pendingDamage=it->tickSeconds=0;
                }
                health=FindCharacterCombat(combat,it->target);
                remove=remove || !health || health->dead || it->remaining<=.00001f;
            }
            if(remove){scene.Entity_Remove(it->root);it=entries.erase(it);}else ++it;
        }
    }
    static XMFLOAT3 ProjectilePosition(const bridge::ProjectileVector& v){return {v.x,v.y,v.z};}
    void RefreshAppearance(wi::scene::Scene& scene) {
        for(auto& e:entries)for(unsigned i=0;i<EmitterCount;++i) {
            const auto entity=e.emitters[i];const auto bone=e.bones[i];
            auto* p=scene.emitters.GetComponent(entity);const auto* t=scene.transforms.GetComponent(bone);
            if(!p || !t)continue;
            auto position=t->GetPosition();
            if(const auto* end=scene.transforms.GetComponent(e.ends[i]);end && e.ends[i]!=bone && e.ends[i]!=e.receiver)
                { const auto endpoint=end->GetPosition();
                  // Lower-arm flames start nearer the wrist so upward rise covers the forearm.
                  const float along=(i==9 || i==11)?.75f:.5f;
                  XMStoreFloat3(&position,XMLoadFloat3(&position)*(1-along)+XMLoadFloat3(&endpoint)*along); }
            const auto* receiver=scene.transforms.GetComponent(e.receiver);
            const XMFLOAT3 rightVector=receiver?receiver->GetRight():XMFLOAT3{1,0,0};
            const XMFLOAT3 forwardVector=receiver?receiver->GetForward():XMFLOAT3{0,0,1};
            const auto right=XMVector3Normalize(XMLoadFloat3(&rightVector));
            const auto forward=XMVector3Normalize(XMLoadFloat3(&forwardVector));
            XMVECTOR offset=XMVectorZero();
            if(bone==e.receiver) {
                const float height=i<4?.85f:(i<8||i==14)?1.30f:(i==12||i==13)?1.73f:1.15f;
                position.y+=height;
                if(i==8||i==9||i==15)offset-=right*.39f;
                if(i==10||i==11||i==16)offset+=right*.39f;
            }
            if(i<8) {
                // Emit around the trunk surface, not inside its opaque centre.
                const unsigned side=i%4;
                offset+=side==0?right*-.22f:side==1?right*.22f:side==2?forward*-.18f:forward*.18f;
                if(i>=4)position.y+=.08f;
            } else if(i<12)offset-=forward*.12f;
            else if(i<14)offset+=right*(i==12?-.12f:.12f);
            if(i<FlameCount) {
                auto& anchor=e.surfaces[i];
                if(anchor.receiver==wi::ecs::INVALID_ENTITY && e.surfaceAttempts[i]<4) {
                    bool geometry=false;
                    for(size_t o=0;o<scene.objects.GetCount();++o) {
                        const auto entity=scene.objects.GetEntity(o);
                        if(!scene.Entity_IsDescendant(entity,e.receiver))continue;
                        const auto* mesh=scene.meshes.GetComponent(scene.objects[o].meshID);
                        geometry=geometry || (mesh && !mesh->vertex_positions.empty() && !mesh->indices.empty());
                    }
                    if(geometry) {
                        ++e.surfaceAttempts[i];
                        auto direction=i<8?XMVector3Normalize(offset):i<12?-forward:right*(i==12?-1.f:1.f);
                        XMFLOAT3 origin,rayDirection;
                        XMStoreFloat3(&origin,XMLoadFloat3(&position)+direction*.9f);
                        XMStoreFloat3(&rayDirection,-direction);
                        wi::vector<wi::scene::Scene::RayIntersectionResult> hits;
                        scene.IntersectsAll(hits,wi::primitive::Ray(origin,rayDirection,0,1.8f),
                            wi::enums::FILTER_OPAQUE|wi::enums::FILTER_TRANSPARENT);
                        // IntersectsAll returns traversal order, not nearest surface order.
                        // Choose the skin facing this region's probe, not a farther back face.
                        std::sort(hits.begin(),hits.end(),[&](const auto& a,const auto& b) {
                            return XMVectorGetX(XMVector3LengthSq(XMLoadFloat3(&a.position)-XMLoadFloat3(&origin))) <
                                XMVectorGetX(XMVector3LengthSq(XMLoadFloat3(&b.position)-XMLoadFloat3(&origin)));
                        });
                        for(const auto& hit:hits) {
                            if(!scene.Entity_IsDescendant(hit.entity,e.receiver) || hit.vertexID0<0)continue;
                            auto normal=hit.normal;
                            if(XMVectorGetX(XMVector3Dot(XMLoadFloat3(&normal),direction))<0)
                                XMStoreFloat3(&normal,-XMLoadFloat3(&normal));
                            if(RuntimeImpactMarks::Capture(scene,hit.position,normal,hit.entity,anchor))break;
                        }
                    }
                }
                XMFLOAT4X4 surface;
                if(anchor.receiver!=wi::ecs::INVALID_ENTITY && RuntimeImpactMarks::SurfacePose(scene,anchor,surface)) {
                    // Native triangle/barycentric follow keeps fire on animated skin.
                    position={surface._41+surface._31*.14f,surface._42+surface._32*.14f,surface._43+surface._33*.14f};
                    offset=XMVectorZero();
                }
            }
            XMStoreFloat3(&position,XMLoadFloat3(&position)+offset);
            if(auto* transform=scene.transforms.GetComponent(entity)) {
                const auto* parent=scene.transforms.GetComponent(e.root);
                const auto local=parent?XMVector3TransformCoord(XMLoadFloat3(&position),XMMatrixInverse(nullptr,parent->GetWorldMatrix())):XMLoadFloat3(&position);
                XMStoreFloat3(&transform->translation_local,local);transform->SetDirty();
                XMStoreFloat4x4(&transform->world,XMMatrixTranslation(position.x,position.y,position.z));
            }
            p->center=position;XMStoreFloat4x4(&p->worldMatrix,XMMatrixTranslation(position.x,position.y,position.z));
        }
    }
};
}
