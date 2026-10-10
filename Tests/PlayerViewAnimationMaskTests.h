#pragma once
#include "renegade/bridge/PlayerViewAnimationMask.h"

static bool TestPlayerViewAnimationMasks(std::string& error)
{
    using namespace renegade::bridge;
    wi::scene::Scene scene;
    const auto root=scene.Entity_CreateTransform("Root");
    const auto right=scene.Entity_CreateTransform("Right");
    const auto left=scene.Entity_CreateTransform("Left");
    const auto finger=scene.Entity_CreateTransform("Left finger");
    scene.Component_Attach(right,root,true);
    scene.Component_Attach(left,root,true);
    scene.Component_Attach(finger,left,true);
    const auto armature=wi::ecs::CreateEntity();
    scene.armatures.Create(armature).boneCollection={root,right,left,finger};
    PlayerViewBonePartition partition;
    if(!CollectPlayerViewBonePartition(scene,armature,right,left,partition,error) ||
        partition.base!=std::vector<wi::ecs::Entity>{root} ||
        partition.primary!=std::vector<wi::ecs::Entity>{right} ||
        partition.offHand!=std::vector<wi::ecs::Entity>{left,finger})return false;
    const auto saved=partition;
    if(CollectPlayerViewBonePartition(scene,armature,root,left,partition,error) ||
        partition.primary!=saved.primary || partition.offHand!=saved.offHand)return false;
    const auto foreign=scene.Entity_CreateTransform("Foreign");
    if(CollectPlayerViewBonePartition(scene,armature,right,foreign,partition,error))return false;
    // Reject malformed cycles without hanging, leaving the output untouched.
    scene.hierarchy.Create(root).parentID=root;
    if(CollectPlayerViewBonePartition(scene,armature,right,left,partition,error))return false;
    scene.hierarchy.Remove(root);

    const auto sourceEntity=wi::ecs::CreateEntity();
    auto& source=scene.animations.Create(sourceEntity);
    source.start=0;source.end=1;source.speed=1;source.Play();source.RootMotionOn();
    for(const auto bone:{root,right,left,finger})
    {
        const auto dataEntity=wi::ecs::CreateEntity();
        auto& data=scene.animation_datas.Create(dataEntity);
        data.keyframe_times={0,1};data.keyframe_data={0,0,0,1,2,3};
        wi::scene::AnimationComponent::AnimationSampler sampler;sampler.data=dataEntity;
        auto* clip=scene.animations.GetComponent(sourceEntity);
        const auto index=int(clip->samplers.size());clip->samplers.push_back(sampler);
        wi::scene::AnimationComponent::AnimationChannel channel;
        channel.target=bone;channel.samplerIndex=index;
        channel.path=wi::scene::AnimationComponent::AnimationChannel::Path::TRANSLATION;
        clip->channels.push_back(channel);
    }
    const auto dataCount=scene.animation_datas.GetCount();
    wi::ecs::Entity masked=wi::ecs::INVALID_ENTITY;
    if(!CreatePlayerViewMaskedAnimationClip(scene,sourceEntity,saved.offHand,masked,error))return false;
    const auto* clipped=scene.animations.GetComponent(masked);
    if(!clipped || clipped->channels.size()!=2 || clipped->samplers.size()!=2 ||
        clipped->IsPlaying() || clipped->IsRootMotion() || clipped->amount!=0 ||
        clipped->channels[0].target!=left || clipped->channels[1].target!=finger ||
        clipped->channels[0].samplerIndex!=0 || clipped->channels[1].samplerIndex!=1 ||
        scene.animation_datas.GetCount()!=dataCount)return false;
    const auto* original=scene.animations.GetComponent(sourceEntity);
    if(original->channels.size()!=4 || !original->IsPlaying() || !original->IsRootMotion() ||
        clipped->samplers[0].data!=original->samplers[2].data)return false;
    wi::ecs::Entity primaryMask=wi::ecs::INVALID_ENTITY;
    if(!CreatePlayerViewMaskedAnimationClip(scene,sourceEntity,saved.primary,primaryMask,error))return false;
    scene.animations.GetComponent(sourceEntity)->Pause();
    scene.animations.GetComponent(sourceEntity)->amount=0;
    auto select=[&](wi::ecs::Entity entity,float time) {
        auto& clip=*scene.animations.GetComponent(entity);
        clip.amount=1;clip.timer=time;clip.last_update_time=-std::numeric_limits<float>::max();
    };
    auto evaluate=[&]() {
        scene.dt=1.0f/60;scene.ScanAnimationDependencies();
        wi::jobsystem::context context;scene.RunAnimationUpdateSystem(context);wi::jobsystem::Wait(context);
    };
    wi::jobsystem::Initialize();
    select(masked,0.25f);select(primaryMask,0.75f);evaluate();
    if(std::abs(scene.transforms.GetComponent(left)->translation_local.x-0.25f)>0.0001f ||
        std::abs(scene.transforms.GetComponent(finger)->translation_local.z-0.75f)>0.0001f ||
        std::abs(scene.transforms.GetComponent(right)->translation_local.x-0.75f)>0.0001f)return false;
    select(primaryMask,0.5f);evaluate();
    if(std::abs(scene.transforms.GetComponent(left)->translation_local.x-0.25f)>0.0001f ||
        std::abs(scene.transforms.GetComponent(right)->translation_local.x-0.5f)>0.0001f)return false;
    select(masked,0.9f);evaluate();
    if(std::abs(scene.transforms.GetComponent(right)->translation_local.x-0.5f)>0.0001f)return false;
    scene.Entity_Remove(primaryMask);
    const auto count=scene.animations.GetCount();
    const auto unchanged=masked;
    if(CreatePlayerViewMaskedAnimationClip(scene,sourceEntity,{},masked,error) ||
        masked!=unchanged || scene.animations.GetCount()!=count)return false;
    scene.animations.GetComponent(sourceEntity)->channels[2].retargetIndex=0;
    if(CreatePlayerViewMaskedAnimationClip(scene,sourceEntity,saved.offHand,masked,error) ||
        masked!=unchanged || scene.animations.GetCount()!=count)return false;
    scene.animations.GetComponent(sourceEntity)->channels[2].retargetIndex=-1;
    scene.animations.GetComponent(sourceEntity)->samplers[2].scene=&scene;
    if(CreatePlayerViewMaskedAnimationClip(scene,sourceEntity,saved.offHand,masked,error))return false;
    scene.Entity_Remove(masked);
    if(!scene.animations.Contains(sourceEntity) || scene.animation_datas.GetCount()!=dataCount)return false;
    error.clear();
    return true;
}
