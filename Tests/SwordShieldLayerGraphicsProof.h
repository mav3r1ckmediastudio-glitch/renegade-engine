#pragma once
#include "renegade/bridge/PlayerViewAnimationMask.h"

// Manual owner-asset proof. No generated clips or diagnostic materials are saved
// into the supplied arms asset, and no live gameplay capability is enabled here.
static bool SwordShieldLayerProof(const fs::path& input,const fs::path& output)
{
    wi::scene::Scene scene;
    ImportModel_FBX((input/"Idle.FBX").generic_u8string(),scene);
    if(scene.armatures.GetCount()!=1 || scene.animations.GetCount()!=1)return false;
    std::string error;
    std::vector<std::pair<std::string,wi::ecs::Entity>> sources={{"Idle",scene.animations.GetEntity(0)}};
    for(const char* action:{"BlockStart","BlockLoop","BlockEnd","AttackLeft","AttackRight","AttackDown","AttackStab"}) {
        wi::scene::Scene imported;
        ImportModel_FBX((input/"X-Forward"/(std::string(action)+".fbx")).generic_u8string(),imported);
        std::vector<wi::ecs::Entity> added;
        if(!AppendMatchingRigAnimationScene(scene,imported,added,error) || added.size()!=1) {
            std::cerr<<error<<"\n";return false;
        }
        sources.push_back({action,added[0]});
    }
    std::cout<<"PROCEDURAL humanoids="<<scene.humanoids.GetCount()<<" springs="<<scene.springs.GetCount()<<"\n";
    // Isolate native clip ownership from imported procedural character modifiers.
    scene.humanoids.Clear();scene.springs.Clear();
    auto bone=[&](const char* name) {
        for(const auto entity:scene.armatures[0].boneCollection) {
            const auto* n=scene.names.GetComponent(entity);
            if(n && n->name==name)return entity;
        }
        return wi::ecs::INVALID_ENTITY;
    };
    PlayerViewBonePartition partition;
    if(!CollectPlayerViewBonePartition(scene,scene.armatures.GetEntity(0),
        bone("clavicle_r"),bone("clavicle_l"),partition,error)) {
        std::cerr<<error<<"\n";return false;
    }
    std::vector<wi::ecs::Entity> generated;
    auto mask=[&](wi::ecs::Entity source,const std::vector<wi::ecs::Entity>& targets) {
        wi::ecs::Entity entity=wi::ecs::INVALID_ENTITY;
        if(!CreatePlayerViewMaskedAnimationClip(scene,source,targets,entity,error))return entity;
        generated.push_back(entity);return entity;
    };
    const auto base=mask(sources[0].second,partition.base);
    const auto leftIdle=mask(sources[0].second,partition.offHand);
    const auto rightIdle=mask(sources[0].second,partition.primary);
    std::vector<std::pair<std::string,wi::ecs::Entity>> layers;
    for(size_t i=1;i<sources.size();++i)
        layers.push_back({sources[i].first,mask(sources[i].second,
            i<4?partition.offHand:partition.primary)});
    if(base==wi::ecs::INVALID_ENTITY || leftIdle==wi::ecs::INVALID_ENTITY ||
        rightIdle==wi::ecs::INVALID_ENTITY ||
        std::any_of(layers.begin(),layers.end(),[](const auto& p){return p.second==wi::ecs::INVALID_ENTITY;})) {
        std::cerr<<error<<"\n";return false;
    }
    // Use matte diagnostic material to make pose inspection readable. This is
    // a private scene and is deliberately not a texture/material acceptance test.
    for(size_t i=0;i<scene.materials.GetCount();++i) {
        auto& material=scene.materials[i];
        for(auto& texture:material.textures){texture.name.clear();texture.resource=wi::Resource();}
        material.baseColor=XMFLOAT4(0.65f,0.75f,0.85f,1);
        material.roughness=0.8f;material.metalness=0;
    }
    auto disable=[&]() {
        for(size_t i=0;i<scene.animations.GetCount();++i) {
            auto& clip=scene.animations[i];clip.Pause();clip.RootMotionOff();clip.amount=0;
        }
    };
    auto select=[&](wi::ecs::Entity entity,float fraction) {
        auto& clip=*scene.animations.GetComponent(entity);
        clip.amount=1;clip.Pause();clip.RootMotionOff();
        clip.timer=clip.start+(clip.end-clip.start)*fraction;
        clip.last_update_time=-std::numeric_limits<float>::max();
    };
    auto pose=[&](wi::ecs::Entity right,float rightTime,wi::ecs::Entity left,float leftTime) {
        disable();select(base,0.35f);select(right,rightTime);select(left,leftTime);
        scene.Update(1.0f/60);
    };
    auto snapshot=[&](const std::vector<wi::ecs::Entity>& targets) {
        std::vector<XMFLOAT4X4> result;
        for(const auto entity:targets)result.push_back(scene.transforms.GetComponent(entity)->world);
        return result;
    };
    auto delta=[&](const std::vector<wi::ecs::Entity>& targets,const std::vector<XMFLOAT4X4>& expected) {
        float largest=0;
        for(size_t i=0;i<targets.size();++i) {
            const float* a=&scene.transforms.GetComponent(targets[i])->world._11;
            const float* b=&expected[i]._11;
            for(int k=0;k<16;++k)largest=std::max(largest,std::abs(a[k]-b[k]));
        }
        return largest;
    };
    pose(rightIdle,0.35f,layers[1].second,0.35f);
    const auto heldLeft=snapshot(partition.offHand);
    const auto heldBase=snapshot(partition.base);
    const auto idleRight=snapshot(partition.primary);
    if(!Capture(scene,output/"held-block.png"))return false;
    float maxLeftDelta=0,maxRightMotion=0;
    for(size_t action=3;action<layers.size();++action) {
        for(float time:{0.1f,0.35f,0.7f}) {
            pose(layers[action].second,time,layers[1].second,0.35f);
            maxLeftDelta=std::max(maxLeftDelta,delta(partition.offHand,heldLeft));
            maxRightMotion=std::max(maxRightMotion,delta(partition.primary,idleRight));
            const auto baseDelta=delta(partition.base,heldBase);
            if(maxLeftDelta>0.0001f || baseDelta>0.0001f)return false;
            if(!Capture(scene,output/(layers[action].first+"-"+std::to_string(time)+".png")))return false;
        }
    }
    if(maxRightMotion<0.01f)return false;
    // Shield windup/release may not overwrite an ongoing right-arm attack.
    pose(layers[3].second,0.35f,leftIdle,0.35f);
    const auto attackingRight=snapshot(partition.primary);
    float maxRightDelta=0;
    for(size_t action:{size_t(0),size_t(2)})for(float time:{0.0f,0.35f,0.95f}) {
        pose(layers[3].second,0.35f,layers[action].second,time);
        maxRightDelta=std::max(maxRightDelta,delta(partition.primary,attackingRight));
        if(maxRightDelta>0.0001f)return false;
        if(!Capture(scene,output/(layers[action].first+"-"+std::to_string(time)+".png")))return false;
    }
    // Let Wicked advance both native clocks. Restarting the right clip must not
    // restart the held left clip, and pausing must freeze both native timers.
    pose(layers[3].second,0.0f,layers[1].second,0.0f);
    auto play=[&](wi::ecs::Entity entity,bool loop) {
        auto& clip=*scene.animations.GetComponent(entity);
        clip.SetLooped(loop);clip.Play();
    };
    play(layers[3].second,false);play(layers[1].second,true);
    for(int frame=0;frame<120;++frame) {
        if(frame==40) {
            const auto leftTime=scene.animations.GetComponent(layers[1].second)->timer;
            scene.animations.GetComponent(layers[3].second)->amount=0;
            select(layers[4].second,0);play(layers[4].second,false);
            if(scene.animations.GetComponent(layers[1].second)->timer!=leftTime)return false;
        }
        if(frame==60) {
            auto* left=scene.animations.GetComponent(layers[1].second);
            auto* right=scene.animations.GetComponent(layers[4].second);
            left->Pause();right->Pause();
            const float lt=left->timer,rt=right->timer;
            scene.Update(1.0f/60);
            if(left->timer!=lt || right->timer!=rt)return false;
            left->Play();right->Play();
        }
        const auto* before=scene.animations.GetComponent(layers[1].second);
        const float expected=(before->timer>before->end?before->start:before->timer)+before->speed/60;
        scene.Update(1.0f/60);
        if(std::abs(scene.animations.GetComponent(layers[1].second)->timer-expected)>0.0001f)return false;
    }
    pose(rightIdle,0.35f,leftIdle,0.35f);
    if(!Capture(scene,output/"released-idle.png"))return false;
    // Generated masks share immutable native data; remove only transient clips.
    for(const auto entity:generated)scene.Entity_Remove(entity);
    if(scene.animations.GetCount()!=sources.size() || scene.armatures.GetCount()!=1)return false;
    std::cout<<"HAND MASK PASS // primary bones="<<partition.primary.size()
        <<" off-hand bones="<<partition.offHand.size()<<" base bones="<<partition.base.size()
        <<" left attack interference="<<maxLeftDelta<<" right block interference="<<maxRightDelta
        <<" right motion="<<maxRightMotion<<"; native clocks pause and restart independent\n";
    return true;
}
