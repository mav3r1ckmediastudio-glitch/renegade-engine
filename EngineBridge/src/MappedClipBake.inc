// Build a clip-only snapshot on the target skeleton. Entity IDs are local to
// each scene. The matching-rig importer resolves the retained snapshot by name.
static bool CopyBakeRig(const wi::scene::Scene& from, wi::scene::Scene& to, std::string& error)
{
    AnimationRigBones bones;
    if (!CollectAnimationRig(from,bones,error)) return false;
    const auto rig = from.armatures.GetEntity(0);
    auto& palette = to.armatures.Create(rig);
    palette.boneCollection = std::move(bones.boneCollection);
    palette.inverseBindMatrices = std::move(bones.inverseBindMatrices);
    const auto copyChain = [&](wi::ecs::Entity bone) {
        while (bone != wi::ecs::INVALID_ENTITY && !to.transforms.Contains(bone)) {
            const auto* t = from.transforms.GetComponent(bone);
            if (!t) break;
            to.transforms.Create(bone) = *t;
            if (const auto* n = from.names.GetComponent(bone)) to.names.Create(bone) = *n;
            const auto* parent = from.hierarchy.GetComponent(bone);
            if (!parent) break;
            to.hierarchy.Create(bone) = *parent;
            bone = parent->parentID;
        }
    };
    copyChain(rig);
    for (auto bone : palette.boneCollection) copyChain(bone);
    return true;
}

// Preserve the motion between a mapped joint and its nearest mapped ancestor.
// An unmatched parent may carry all pelvis translation and facing rotation.
static bool FoldAnimatedParents(wi::scene::Scene& scene, wi::scene::AnimationComponent& clip,
    const std::set<wi::ecs::Entity>& mapped, std::string& error)
{
    using namespace wi::scene;
    using Entity=wi::ecs::Entity;
    using Channel=AnimationComponent::AnimationChannel;
    using Path=Channel::Path;
    struct Chain {
        Entity bone;
        std::vector<Entity> parents;
        XMFLOAT4X4 inverseRest;
        AnimationDataComponent keys[3];
    };
    std::set<Entity> animated, affected, inputs;
    for(const auto& ch:clip.channels)animated.insert(ch.target);
    std::vector<Chain> chains;
    for(auto bone:mapped){
        Chain chain{};chain.bone=bone;
        auto rest=XMMatrixIdentity();
        bool moving=false;
        for(auto h=scene.hierarchy.GetComponent(bone);h;h=scene.hierarchy.GetComponent(h->parentID)){
            auto e=h->parentID;
            if(mapped.count(e))break;
            auto* t=scene.transforms.GetComponent(e);
            if(!t)break;
            chain.parents.push_back(e);rest=rest*t->GetLocalMatrix();
            moving=moving || animated.count(e);
        }
        if(!moving)continue;
        if(std::abs(XMVectorGetX(XMMatrixDeterminant(rest)))<1e-20f){error="Singular animated parent transform.";return false;}
        XMStoreFloat4x4(&chain.inverseRest,XMMatrixInverse(nullptr,rest));
        affected.insert(bone);inputs.insert(bone);
        inputs.insert(chain.parents.begin(),chain.parents.end());chains.push_back(std::move(chain));
    }
    if(chains.empty())return true;
    std::set<float> times{clip.start,clip.end};
    for(const auto& ch:clip.channels){
        if(!inputs.count(ch.target))continue;
        const auto& sampler=clip.samplers[ch.samplerIndex];
        for(auto t:scene.animation_datas.GetComponent(sampler.data)->keyframe_times){
            if(t<clip.start || t>clip.end)continue;
            times.insert(t);
            if(sampler.mode==AnimationComponent::AnimationSampler::STEP && t>clip.start)
                times.insert(std::nextafter(t,clip.start));
        }
    }
    std::vector<float> knots(times.begin(),times.end());
    for(size_t i=1;i<knots.size();++i){
        const int subdivisions=int(std::ceil((knots[i]-knots[i-1])*60.f));
        for(int j=1;j<subdivisions;++j)times.insert(knots[i-1]+(knots[i]-knots[i-1])*float(j)/subdivisions);
    }
    std::vector<TransformComponent> reference;
    for(size_t i=0;i<scene.transforms.GetCount();++i)reference.push_back(scene.transforms[i]);
    clip.Pause();clip.amount=1;clip.RootMotionOff();
    for(float time:times){
        clip.timer=time;clip.last_update_time=-std::numeric_limits<float>::max();scene.Update(0);
        for(auto& chain:chains){
            auto m=scene.transforms.GetComponent(chain.bone)->GetLocalMatrix();
            for(auto e:chain.parents)m=m*scene.transforms.GetComponent(e)->GetLocalMatrix();
            XMVECTOR scale,rotation,translation;
            if(!XMMatrixDecompose(&scale,&rotation,&translation,m*XMLoadFloat4x4(&chain.inverseRest))){
                error="Animated parent chain cannot be represented by local scale, rotation and translation.";return false;
            }
            const XMVECTOR values[]={translation,rotation,scale};
            for(int k=0;k<3;++k){
                auto& data=chain.keys[k];XMFLOAT4 value;XMStoreFloat4(&value,values[k]);
                if(k==1 && data.keyframe_data.size()>=4){
                    const auto* prev=data.keyframe_data.data()+data.keyframe_data.size()-4;
                    if(prev[0]*value.x+prev[1]*value.y+prev[2]*value.z+prev[3]*value.w<0)
                        XMStoreFloat4(&value,-values[k]);
                }
                data.keyframe_times.push_back(time);
                for(int j=0;j<(k==1?4:3);++j)data.keyframe_data.push_back((&value.x)[j]);
            }
        }
    }
    for(size_t i=0;i<reference.size();++i)scene.transforms[i]=reference[i];
    clip.channels.erase(std::remove_if(clip.channels.begin(),clip.channels.end(),
        [&](const Channel& ch){return affected.count(ch.target)>0;}),clip.channels.end());
    for(auto& chain:chains){
        const Path paths[]={Path::TRANSLATION,Path::ROTATION,Path::SCALE};
        for(int k=0;k<3;++k){
            const auto id=wi::ecs::CreateEntity();
            scene.animation_datas.Create(id)=std::move(chain.keys[k]);
            auto& ch=clip.channels.emplace_back();ch.target=chain.bone;ch.path=paths[k];
            ch.samplerIndex=int(clip.samplers.size());clip.samplers.emplace_back().data=id;
        }
    }
    return true;
}

bool ModelImportCandidateService::AppendMappedClip(ModelImportCandidate& candidate,
    const wi::scene::Scene& source, size_t clipIndex, const std::vector<int>& targets,
    const std::map<std::string,XMFLOAT4>& sourcePose, const std::map<std::string,XMFLOAT4>& targetPose,
    const std::string& label, std::string& error) const
{
    return EditMappedClip(candidate,source,clipIndex,targets,sourcePose,targetPose,label,error,-1,{});
}

bool ModelImportCandidateService::EditMappedClip(ModelImportCandidate& candidate,
    const wi::scene::Scene& source, size_t clipIndex, const std::vector<int>& targets,
    const std::map<std::string,XMFLOAT4>& sourcePose, const std::map<std::string,XMFLOAT4>& targetPose,
    const std::string& label, std::string& error, int replaceIndex, const std::vector<RetargetPoseKey>& edits,
    const std::set<int>& heldTargets) const
{
    using namespace wi::scene;
    using Entity = wi::ecs::Entity;
    if (!candidate.IsReady() || clipIndex>=source.animations.GetCount()) {
        error="Select one source clip and a single target skeleton."; return false;
    }
    Scene src, dst;
    if (!CopyBakeRig(source,src,error) || !CopyBakeRig(*candidate.scene_,dst,error)) return false;
    const auto& sb=src.armatures[0].boneCollection;
    const auto& tb=dst.armatures[0].boneCollection;
    if (targets.size()!=sb.size()) {error="The source skeleton changed."; return false;}
    const auto apply=[](Scene& scene,const auto& pose) {
        for (auto bone:scene.armatures[0].boneCollection) {
            auto* n=scene.names.GetComponent(bone);
            if (!n) continue;
            auto it=pose.find(n->name);
            if(it!=pose.end()) {
                auto* t=scene.transforms.GetComponent(bone);
                t->rotation_local=it->second; t->SetDirty();
            }
        }
    };
    apply(src,sourcePose); apply(dst,targetPose);
    const auto original=source.animations.GetEntity(clipIndex);
    const auto& clip=source.animations[clipIndex];
    auto& input=src.animations.Create(original); input=clip;
    for (const auto& sampler:clip.samplers) {
        if(sampler.scene || !source.animation_datas.Contains(sampler.data)) {error="Clip data is unavailable.";return false;}
        if(!src.animation_datas.Contains(sampler.data))
            src.animation_datas.Create(sampler.data)=*source.animation_datas.GetComponent(sampler.data);
    }
    using Channel=AnimationComponent::AnimationChannel;
    for(const auto& ch:clip.channels) {
        if(ch.samplerIndex<0 || size_t(ch.samplerIndex)>=clip.samplers.size() || ch.retargetIndex>=0) {
            error="Unsupported source sampler.";return false;
        }
        const auto& sampler=clip.samplers[ch.samplerIndex];
        const auto& data=*src.animation_datas.GetComponent(sampler.data);
        const size_t stride=ch.path==Channel::Path::ROTATION?4:3;
        if(ch.path!=Channel::Path::TRANSLATION && ch.path!=Channel::Path::SCALE && ch.path!=Channel::Path::ROTATION) {
            error="This source includes a non-skeletal track.";return false;
        }
        if(data.keyframe_times.empty() || data.keyframe_data.size()!=stride*data.keyframe_times.size()) {
            error="This clip requires linear or stepped skeletal keys.";return false;
        }
    }
    std::vector<std::pair<Entity,Entity>> pairs;
    std::set<int> used;
    for(size_t i=0;i<targets.size();++i) {
        const int t=targets[i];
        if(t==-1) continue;
        if(t<0 || size_t(t)>=tb.size() || !used.insert(t).second) {
            error="Resolve unmatched bones or duplicate target assignments before baking.";return false;
        }
        pairs.emplace_back(sb[i],tb[t]);
    }
    std::set<Entity> mapped;
    for(const auto& pair:pairs)mapped.insert(pair.first);
    if(!FoldAnimatedParents(src,input,mapped,error))return false;
    const auto rig=dst.armatures.GetEntity(0);
    auto& sh=src.humanoids.Create(src.armatures.GetEntity(0));
    dst.humanoids.Create(rig);
    AnimationComponent merged=clip;
    merged.channels.clear();merged.samplers.clear();merged.retargets.clear();
    auto& dh=*dst.humanoids.GetComponent(rig);
    auto bakeSlots=[&]() {
        const Entity part=dst.RetargetAnimation(rig,original,true,&src);
        if(part==wi::ecs::INVALID_ENTITY) return;
        const auto& baked=*dst.animations.GetComponent(part);
        const int offset=int(merged.samplers.size());
        for(auto channel:baked.channels) {channel.samplerIndex+=offset;merged.channels.push_back(channel);}
        for(const auto& sampler:baked.samplers) merged.samplers.push_back(sampler);
        dst.animations.Remove(part);dst.hierarchy.Remove(part);
    };
    // Keep the imported Wicked humanoid roles. An explicit mapping overrides
    // the corresponding target slot; additional joints are baked separately.
    auto roles=[](const Scene& scene, const auto& joints) {
        for(size_t i=0;i<scene.humanoids.GetCount();++i)
            if(scene.humanoids[i].IsValid())
                return CaptureHumanoidMapping(scene,scene.humanoids.GetEntity(i));
        return BuildAutoHumanoidMapping(scene,joints).mapping;
    };
    const auto sourceRoles=roles(source,sb),targetRoles=roles(*candidate.scene_,tb);
    std::set<Entity> assigned;
    for(size_t slot=0;slot<HumanoidBoneCount;++slot) {
        auto it=std::find_if(pairs.begin(),pairs.end(),[&](const auto& p){
            return p.first==sourceRoles.bones[slot];});
        if(it==pairs.end() && sourceRoles.bones[slot]==wi::ecs::INVALID_ENTITY)
            it=std::find_if(pairs.begin(),pairs.end(),[&](const auto& p){
                return p.second==targetRoles.bones[slot];});
        if(it!=pairs.end() && assigned.insert(it->first).second){
            sh.bones[slot]=it->first;dh.bones[slot]=it->second;
        }
    }
    if(!assigned.empty())bakeSlots();
    pairs.erase(std::remove_if(pairs.begin(),pairs.end(),[&](const auto& p){
        return assigned.count(p.first)>0;}),pairs.end());
    for(size_t base=0;base<pairs.size();base+=HumanoidBoneCount) {
        std::fill(std::begin(sh.bones),std::end(sh.bones),wi::ecs::INVALID_ENTITY);
        std::fill(std::begin(dh.bones),std::end(dh.bones),wi::ecs::INVALID_ENTITY);
        for(size_t j=0;j<HumanoidBoneCount && base+j<pairs.size();++j) {
            sh.bones[j]=pairs[base+j].first;dh.bones[j]=pairs[base+j].second;
        }
        bakeSlots();
    }
    if(merged.channels.empty()) {error="No mapped bone has an animated track in this clip.";return false;}
    // Explicit holds must overwrite the preceding clip's local pose on playback.
    for (int index : heldTargets) {
        if (index < 0 || size_t(index) >= tb.size() || used.count(index)) {
            error = "A held target is missing or also has a source assignment.";
            return false;
        }
        const auto bone = tb[index];
        const auto* transform = dst.transforms.GetComponent(bone);
        if (!transform) { error = "Held target has no reference transform."; return false; }
        const auto key = [&](Channel::Path path, std::initializer_list<float> values) {
            const auto id = wi::ecs::CreateEntity();
            auto& data = dst.animation_datas.Create(id);
            data.keyframe_times = {clip.start};
            data.keyframe_data.assign(values.begin(), values.end());
            auto& channel = merged.channels.emplace_back();
            channel.target = bone;
            channel.path = path;
            channel.samplerIndex = int(merged.samplers.size());
            merged.samplers.emplace_back().data = id;
        };
        const auto& p = transform->translation_local;
        const auto& q = transform->rotation_local;
        const auto& s = transform->scale_local;
        key(Channel::Path::TRANSLATION, {p.x,p.y,p.z});
        key(Channel::Path::ROTATION, {q.x,q.y,q.z,q.w});
        key(Channel::Path::SCALE, {s.x,s.y,s.z});
    }
    for(auto e:tb) {
        const auto* name=dst.names.GetComponent(e);
        if(!name || !targetPose.count(name->name))continue;
        if(std::any_of(merged.channels.begin(),merged.channels.end(),[e](const auto& c){return c.target==e && c.path==Channel::Path::ROTATION;}))continue;
        const auto dataEntity=wi::ecs::CreateEntity();
        auto& data=dst.animation_datas.Create(dataEntity);
        const auto& q=targetPose.at(name->name);
        data.keyframe_times.push_back(clip.start);data.keyframe_data={q.x,q.y,q.z,q.w};
        auto& ch=merged.channels.emplace_back();ch.target=e;ch.path=Channel::Path::ROTATION;
        ch.samplerIndex=int(merged.samplers.size());merged.samplers.emplace_back().data=dataEntity;
    }
    for(const auto& edit:edits){
        const auto it=std::find_if(tb.begin(),tb.end(),[&](Entity e){
            const auto* n=dst.names.GetComponent(e);return n && n->name==edit.bone;});
        if(it==tb.end()){error="Edited target bone is missing.";return false;}
        auto channel=std::find_if(merged.channels.begin(),merged.channels.end(),[&](const auto& c){
            return c.target==*it && c.path==Channel::Path::ROTATION;});
        if(channel==merged.channels.end()){
            const auto id=wi::ecs::CreateEntity();auto& data=dst.animation_datas.Create(id);
            const auto q=dst.transforms.GetComponent(*it)->rotation_local;
            data.keyframe_times={clip.start};data.keyframe_data={q.x,q.y,q.z,q.w};
            auto& c=merged.channels.emplace_back();c.path=Channel::Path::ROTATION;c.target=*it;
            c.samplerIndex=int(merged.samplers.size());merged.samplers.emplace_back().data=id;
            channel=std::prev(merged.channels.end());
        }
        auto& sampler=merged.samplers[channel->samplerIndex];
        auto& data=*dst.animation_datas.GetComponent(sampler.data);
        auto& times=data.keyframe_times;auto& values=data.keyframe_data;
        if(times.empty() || values.size()!=4*times.size()){error="Unsupported rotation curve.";return false;}
        const auto q=edit.rotation;
        if(edit.scope==RetargetPoseScope::Offset){
            const auto offset=XMLoadFloat4(&q);
            for(size_t j=0;j<values.size();j+=4){
                const auto local=XMVectorSet(values[j],values[j+1],values[j+2],values[j+3]);
                XMFLOAT4 result;
                XMStoreFloat4(&result,XMQuaternionNormalize(XMQuaternionMultiply(offset,local)));
                values[j]=result.x;values[j+1]=result.y;values[j+2]=result.z;values[j+3]=result.w;
            }
            continue;
        }
        if(edit.scope==RetargetPoseScope::Hold){times={clip.start};values={q.x,q.y,q.z,q.w};continue;}
        auto sample=[&](float t){
            size_t hi=std::lower_bound(times.begin(),times.end(),t)-times.begin();
            auto at=[&](size_t j){return XMVectorSet(values[4*j],values[4*j+1],values[4*j+2],values[4*j+3]);};
            if(hi==0)return at(0);
            if(hi==times.size())return at(hi-1);
            if(times[hi]==t)return at(hi);
            if(sampler.mode==AnimationComponent::AnimationSampler::STEP)return at(hi-1);
            return XMQuaternionSlerp(at(hi-1),at(hi),(t-times[hi-1])/(times[hi]-times[hi-1]));
        };
        auto put=[&](float t,FXMVECTOR value){
            auto at=std::lower_bound(times.begin(),times.end(),t);
            size_t j=at-times.begin();
            if(at==times.end() || *at!=t){times.insert(at,t);values.insert(values.begin()+4*j,4,0.f);}
            XMFLOAT4 v;XMStoreFloat4(&v,value);
            values[4*j]=v.x;values[4*j+1]=v.y;values[4*j+2]=v.z;values[4*j+3]=v.w;
        };
        float t=std::clamp(edit.time,clip.start,clip.end);
        float left=std::max(clip.start,t-1.f/30),right=std::min(clip.end,t+1.f/30);
        const auto l=sample(left),r=sample(right);
        if(left<t)put(left,l);
        if(right>t)put(right,r);
        put(t,XMLoadFloat4(&q));
    }
    dst.humanoids.Clear();
    const Entity result=wi::ecs::CreateEntity();
    dst.animations.Create(result)=std::move(merged);
    dst.names.Create(result).name=label;
    for(const auto& sampler:dst.animations.GetComponent(result)->samplers) dst.Component_Attach(sampler.data,result);
    return CommitBakedSkeleton(candidate,dst,replaceIndex,error);
}

#include "ClipJointPin.inc"
#include "ClipFloor.inc"

bool ModelImportCandidateService::PinBakedClip(ModelImportCandidate& candidate,int clipIndex,
    const ClipJointPin& pin,std::string& error) const
{
    for(const auto& entry:candidate.externalAnimations_){
        if(entry.firstAnimationIndex!=size_t(clipIndex) || entry.animationCount!=1)continue;
        const auto& snapshot=candidate.dependencies_.at(entry.dependencyIndex);
        if(fs::path(snapshot.retainedRelativePath).extension()!=".wiscene")continue;
        wi::scene::Scene scene;
        wi::Archive input(snapshot.bytes.data(),snapshot.bytes.size());
        scene.Serialize(input);
        if(scene.animations.GetCount()!=1){error="Invalid baked skeleton.";return false;}
        auto transforms=scene.transforms.GetComponentArray();
        const auto original=scene.animations[0];
        if(pin.floorOffsets.empty()){if(!BakeJointPin(scene,pin,error))return false;}
        else if(!ApplyClipFloor(scene,pin,error))return false;
        for(size_t i=0;i<transforms.size();++i)scene.transforms[i]=transforms[i];
        auto channels=std::move(scene.animations[0].channels);
        auto samplers=std::move(scene.animations[0].samplers);
        scene.animations[0]=original;
        scene.animations[0].channels=std::move(channels);
        scene.animations[0].samplers=std::move(samplers);
        return CommitBakedSkeleton(candidate,scene,clipIndex,error);
    }
    error="Select a baked retargeted clip.";return false;
}

bool ModelImportCandidateService::CommitBakedSkeleton(ModelImportCandidate& candidate,wi::scene::Scene& dst,int replaceIndex,std::string& error) const
{
    using namespace wi::scene;
    using wi::ecs::Entity;
    wi::Archive retained; dst.Serialize(retained);
    wi::vector<uint8_t> bytes;retained.WriteData(bytes);
    ModelImportDependency snapshot;
    snapshot.bytes.assign(bytes.begin(),bytes.end());
    snapshot.retainedRelativePath=GenerateStableId()+"/Retargeted.wiscene";
    auto prepared=wi::allocator::make_shared_single<Scene>();
    wi::Archive archive;candidate.scene_->Serialize(archive);archive.SetReadModeAndResetPos(true);prepared->Serialize(archive);
    const size_t first=prepared->animations.GetCount();
    std::vector<Entity> created;
    if(!AppendMatchingRigAnimationScene(*prepared,dst,created,error))return false;
    if(replaceIndex>=0){
        auto existing=std::find_if(candidate.externalAnimations_.begin(),candidate.externalAnimations_.end(),
            [&](const auto& entry){return entry.firstAnimationIndex==size_t(replaceIndex) && entry.animationCount==1;});
        if(existing==candidate.externalAnimations_.end() || size_t(replaceIndex)>=first){
            error="The original baked clip is unavailable.";return false;
        }
        auto& original=prepared->animations[replaceIndex];
        // Matching-rig import allocates independent sampler data for each baked clip.
        std::set<Entity> oldKeys;
        for(const auto& sampler:original.samplers)oldKeys.insert(sampler.data);
        original=std::move(*prepared->animations.GetComponent(created.front()));
        prepared->Entity_Remove(created.front());
        for(auto e:oldKeys)prepared->animation_datas.Remove(e);
        snapshot.retainedRelativePath=candidate.dependencies_[existing->dependencyIndex].retainedRelativePath;
        candidate.dependencies_[existing->dependencyIndex]=std::move(snapshot);
    }else{
        candidate.dependencies_.reserve(candidate.dependencies_.size()+1);
        candidate.externalAnimations_.reserve(candidate.externalAnimations_.size()+1);
        candidate.externalAnimations_.push_back({candidate.dependencies_.size(),first,1,true});
        candidate.dependencies_.push_back(std::move(snapshot));
    }
    candidate.scene_=std::move(prepared);
    candidate.summary_=ImportService::Summarize(*candidate.scene_);
    candidate.evidence_=ImportService::SummarizeModelEvidence(*candidate.scene_);
    return true;
}
