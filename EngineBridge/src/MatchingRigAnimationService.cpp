#include "renegade/bridge/MatchingRigAnimationService.h"
#include "renegade/bridge/ModelImporterFailureAdapter.h"
#include <ModelImporter.h>
#include <cmath>
#include <map>
#include <set>
namespace renegade::bridge
{
    bool CollectAnimationRig(const wi::scene::Scene& scene, AnimationRigBones& result, std::string& error)
    {
        using namespace wi::ecs;
        result = {};
        if (!scene.armatures.GetCount()) { error = "No skin skeleton was found."; return false; }
        std::set<Entity> joints;
        const auto first = scene.armatures.GetEntity(0);
        auto world = [&](Entity entity) {
            const auto* t = scene.transforms.GetComponent(entity);
            return t ? t->GetLocalMatrix() * scene.ComputeParentMatrixRecursive(entity) : XMMatrixIdentity();
        };
        const auto frame = world(first);
        for (size_t i = 0; i < scene.armatures.GetCount(); ++i) {
            const auto& skin = scene.armatures[i];
            if (skin.boneCollection.empty() || skin.boneCollection.size() != skin.inverseBindMatrices.size()) {
                error = "Skin has an incomplete bind pose."; return false;
            }
            const auto skinFrame = world(scene.armatures.GetEntity(i));
            if (i && std::abs(XMVectorGetX(XMMatrixDeterminant(skinFrame))) < 1e-20f) {
                error = "Skin has a singular transform."; return false;
            }
            const auto toSkin = i ? frame * XMMatrixInverse(nullptr, skinFrame) : XMMatrixIdentity();
            for (size_t j = 0; j < skin.boneCollection.size(); ++j) {
                const Entity bone = skin.boneCollection[j];
                if (!joints.insert(bone).second) continue;
                result.boneCollection.push_back(bone);
                auto bind = skin.inverseBindMatrices[j];
                if (i) XMStoreFloat4x4(&bind, toSkin * XMLoadFloat4x4(&skin.inverseBindMatrices[j]));
                result.inverseBindMatrices.push_back(bind);
            }
        }
        // Several skins may cover disjoint parts of one hierarchy. Equal names on
        // independent characters are never sufficient evidence of a shared skeleton.
        if (scene.armatures.GetCount() > 1) {
            Entity root = INVALID_ENTITY;
            for (auto bone : result.boneCollection) {
                Entity highest = bone;
                for (auto h = scene.hierarchy.GetComponent(bone); h; h = scene.hierarchy.GetComponent(h->parentID))
                    if (joints.count(h->parentID)) highest = h->parentID;
                if (root != INVALID_ENTITY && highest != root) {
                    error = "The model contains independent skeletons."; return false;
                }
                root = highest;
            }
        }
        return true;
    }

    bool AppendMatchingRigAnimationScene(wi::scene::Scene& destination,
        const wi::scene::Scene& source, std::vector<wi::ecs::Entity>& created,
        std::string& error)
    {
        created.clear(); error.clear();
        auto fail=[&](const char* message){error=message;return false;};
        AnimationRigBones src, dst;
        if (!CollectAnimationRig(source,src,error) || !CollectAnimationRig(destination,dst,error)) return false;
        if(src.boneCollection.empty() || src.boneCollection.size()!=dst.boneCollection.size() ||
            src.inverseBindMatrices.size()!=src.boneCollection.size() ||
            dst.inverseBindMatrices.size()!=dst.boneCollection.size())
            return fail("Matching-rig skeleton bone counts differ.");
        std::map<std::string,size_t> destinationBones;
        std::set<wi::ecs::Entity> srcBones(src.boneCollection.begin(),src.boneCollection.end());
        std::set<wi::ecs::Entity> dstBones(dst.boneCollection.begin(),dst.boneCollection.end());
        for(size_t i=0;i<dst.boneCollection.size();++i) {
            const auto* name=destination.names.GetComponent(dst.boneCollection[i]);
            if(!name || name->name.empty() || !destinationBones.emplace(name->name,i).second)
                return fail("Matching-rig destination has unnamed or duplicate bones.");
        }
        auto parentName=[](const wi::scene::Scene& scene,wi::ecs::Entity bone,
            const std::set<wi::ecs::Entity>& bones) {
            std::set<wi::ecs::Entity> visited;
            while(visited.insert(bone).second) {
                const auto* parent=scene.hierarchy.GetComponent(bone);
                if(!parent)break;
                bone=parent->parentID;
                if(bones.count(bone)) {
                    const auto* name=scene.names.GetComponent(bone);
                    return name?name->name:std::string{};
                }
            }
            return std::string{};
        };
        std::map<wi::ecs::Entity,wi::ecs::Entity> targets;
        std::set<std::string> sourceNames;
        for(size_t i=0;i<src.boneCollection.size();++i) {
            const auto bone=src.boneCollection[i];const auto* name=source.names.GetComponent(bone);
            if(!name || !sourceNames.insert(name->name).second || !destinationBones.count(name->name))
                return fail("Matching-rig source bone names differ.");
            const auto j=destinationBones.at(name->name);const auto target=dst.boneCollection[j];
            if(!targets.emplace(bone,target).second ||
                parentName(source,bone,srcBones)!=parentName(destination,target,dstBones))
                return fail("Matching-rig bone parent hierarchy differs.");
            const float* a=&src.inverseBindMatrices[i]._11;
            const float* b=&dst.inverseBindMatrices[j]._11;
            for(int k=0;k<16;++k)
                if(!std::isfinite(a[k]) || !std::isfinite(b[k]) || std::abs(a[k]-b[k])>0.0002f)
                    return fail("Matching-rig inverse bind poses differ; retargeting is required.");
        }
        if(source.animations.GetCount()==0)return fail("Source contains no animation clips.");
        using Channel=wi::scene::AnimationComponent::AnimationChannel;
        for(size_t i=0;i<source.animations.GetCount();++i) {
            const auto& clip=source.animations[i];
            if(!clip.retargets.empty() || clip.channels.empty() ||
                !std::isfinite(clip.start) || !std::isfinite(clip.end) || clip.end<clip.start)
                return fail("Source contains unsupported or invalid animation tracks.");
            for(const auto& channel:clip.channels) {
                if(!targets.count(channel.target) || channel.retargetIndex>=0 ||
                    (channel.path!=Channel::Path::TRANSLATION && channel.path!=Channel::Path::ROTATION &&
                     channel.path!=Channel::Path::SCALE) || channel.samplerIndex<0 ||
                    size_t(channel.samplerIndex)>=clip.samplers.size())
                    return fail("Matching-rig clip has a missing bone or unsupported track.");
            }
            for(const auto& sampler:clip.samplers)
                if(sampler.scene || !source.animation_datas.Contains(sampler.data))
                    return fail("Matching-rig clip keyframe data is unavailable.");
        }
        std::map<wi::ecs::Entity,wi::ecs::Entity> data;
        for(size_t i=0;i<source.animations.GetCount();++i) {
            auto clip=source.animations[i];
            for(auto& channel:clip.channels)channel.target=targets.at(channel.target);
            for(auto& sampler:clip.samplers) {
                auto found=data.find(sampler.data);
                if(found==data.end()) {
                    const auto entity=wi::ecs::CreateEntity();
                    destination.animation_datas.Create(entity)=*source.animation_datas.GetComponent(sampler.data);
                    found=data.emplace(sampler.data,entity).first;
                }
                sampler.data=found->second;sampler.scene=nullptr;
            }
            clip.Pause();clip.RootMotionOff();clip.timer=clip.start;clip.amount=0;
            const auto entity=wi::ecs::CreateEntity();destination.animations.Create(entity)=std::move(clip);
            const auto* name=source.names.GetComponent(source.animations.GetEntity(i));
            destination.names.Create(entity).name=name?name->name:"Matching rig clip";
            created.push_back(entity);
        }
        return true;
    }
    bool ImportMatchingRigAnimations(wi::scene::Scene& destination,
        const std::string& sourcePath,std::vector<wi::ecs::Entity>& created,std::string& error)
    {
        auto source=wi::allocator::make_shared<wi::scene::Scene>();
        if (wi::helper::toUpper(wi::helper::GetExtensionFromFileName(sourcePath)) == "WISCENE") {
            wi::Archive archive(sourcePath,true,false);
            if(!archive.IsOpen()){error="Retained rig clip could not be opened.";return false;}
            source->Serialize(archive);
            return AppendMatchingRigAnimationScene(destination,*source,created,error);
        }
        ClearWickedModelImporterFailureDiagnostic();
        ImportModel_FBX(sourcePath,*source);
        const auto failure=ConsumeWickedModelImporterFailureDiagnostic();
        if(!failure.empty()){error=failure;return false;}
        return AppendMatchingRigAnimationScene(destination,*source,created,error);
    }
}
