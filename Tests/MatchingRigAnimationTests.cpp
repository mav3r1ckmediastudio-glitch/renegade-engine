#include "renegade/bridge/MatchingRigAnimationService.h"
#include "renegade/bridge/CreatorModelImportRecipe.h"
#include <iostream>
using namespace renegade::bridge;
using namespace wi::scene;
static void Rig(Scene& scene,bool animation)
{
    const auto root=wi::ecs::CreateEntity(),child=wi::ecs::CreateEntity(),armature=wi::ecs::CreateEntity();
    scene.names.Create(root).name="root";scene.transforms.Create(root);
    scene.names.Create(child).name="grip";scene.transforms.Create(child);
    scene.Component_Attach(child,root,true);
    auto& rig=scene.armatures.Create(armature);
    rig.boneCollection={root,child};
    rig.inverseBindMatrices={wi::math::IDENTITY_MATRIX,wi::math::IDENTITY_MATRIX};
    if(animation) {
        const auto entity=wi::ecs::CreateEntity(),data=wi::ecs::CreateEntity();
        auto& keys=scene.animation_datas.Create(data);keys.keyframe_times={0,1};
        keys.keyframe_data={0,0,0,1,2,3};
        auto& clip=scene.animations.Create(entity);clip.end=1;
        AnimationComponent::AnimationSampler sampler;sampler.data=data;clip.samplers.push_back(sampler);
        AnimationComponent::AnimationChannel channel;channel.target=child;channel.samplerIndex=0;
        channel.path=AnimationComponent::AnimationChannel::Path::TRANSLATION;clip.channels.push_back(channel);
        scene.names.Create(entity).name="Reload";
    }
}
static bool Check(bool value,const char* message)
{if(!value)std::cerr<<message<<"\n";return value;}
int main()
{
    Scene source,destination;Rig(source,true);Rig(destination,false);
    std::vector<wi::ecs::Entity> created;std::string error;
    if(!Check(AppendMatchingRigAnimationScene(destination,source,created,error),"Matching skeleton rejected") ||
        !Check(created.size()==1 && destination.animations.GetCount()==1 &&
            destination.animation_datas.GetCount()==1,"Native clips/data missing"))return 1;
    const auto& clip=destination.animations[0];
    if(!Check(clip.channels[0].target==destination.armatures[0].boneCollection[1] &&
        !clip.IsPlaying() && clip.retargets.empty() &&
        destination.animation_datas.GetComponent(clip.samplers[0].data)->keyframe_data==
        source.animation_datas[0].keyframe_data,"Track changed or target not remapped"))return 2;
    for(int failure=0;failure<5;++failure) {
        Scene bad,unchanged;Rig(bad,true);Rig(unchanged,false);
        if(failure==0)bad.names.GetComponent(bad.armatures[0].boneCollection[1])->name="wrong";
        if(failure==1)bad.hierarchy.Remove(bad.armatures[0].boneCollection[1]);
        if(failure==2)bad.armatures[0].inverseBindMatrices[1]._41=0.1f;
        if(failure==3)bad.animations[0].channels[0].target=wi::ecs::INVALID_ENTITY;
        if(failure==4)bad.names.GetComponent(bad.armatures[0].boneCollection[1])->name="root";
        if(!Check(!AppendMatchingRigAnimationScene(unchanged,bad,created,error) &&
            unchanged.animations.GetCount()==0 && unchanged.animation_datas.GetCount()==0,
            "Invalid rig accepted or failed copy mutated destination"))return 3;
    }
    CreatorModelImportRecipe recipe;
    recipe.assetKind=CreatorAssetImportKind::Character;
    CreatorExternalAnimationImportRecipe external;
    external.sourceProjectRelativePath="SourceAssets/Animations/Snapshots/clip.fbx";
    external.matchingRig=true;external.name="Reload";external.end=1;
    recipe.externalAnimations.push_back(external);
    recipe.textureRelinks.push_back({0,0,"SourceAssets/Models/Arms/relinked/albedo.png"});
    std::string json;
    if(!Check(SerializeCreatorModelImportOptions(recipe,json,error),"Recipe serialization failed"))return 4;
    CreatorModelImportRecipe parsed;
    if(!Check(ParseCreatorModelImportOptions(json,parsed,error) &&
        parsed.externalAnimations[0].matchingRig && parsed.textureRelinks.size()==1,
        "Recipe lost matching rig or texture relink"))return 5;
    recipe.textureRelinks[0].sourceProjectRelativePath="../outside.png";
    if(!Check(!SerializeCreatorModelImportOptions(recipe,json,error),"Escaping relink recipe accepted"))return 6;
    std::cout<<"MATCHING RIG TRACKS, REJECTION ATOMICITY AND DURABLE RECIPE PASS\n";
    return 0;
}
