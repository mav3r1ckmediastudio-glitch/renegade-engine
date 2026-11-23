// Build a clip-only snapshot on the target skeleton. Entity IDs are local to
// each scene. The matching-rig importer resolves the retained snapshot by name.
static void CopyBakeRig(const wi::scene::Scene& from, wi::scene::Scene& to)
{
    const auto rig = from.armatures.GetEntity(0);
    to.armatures.Create(rig) = from.armatures[0];
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
    for (auto bone : from.armatures[0].boneCollection) copyChain(bone);
}

bool ModelImportCandidateService::AppendMappedClip(ModelImportCandidate& candidate,
    const wi::scene::Scene& source, size_t clipIndex, const std::vector<int>& targets,
    const std::map<std::string,XMFLOAT4>& sourcePose, const std::map<std::string,XMFLOAT4>& targetPose,
    const std::string& label, std::string& error) const
{
    using namespace wi::scene;
    using Entity = wi::ecs::Entity;
    if (!candidate.IsReady() || candidate.scene_->armatures.GetCount()!=1 ||
        source.armatures.GetCount()!=1 || clipIndex>=source.animations.GetCount()) {
        error="Select one source clip and a single target skeleton."; return false;
    }
    Scene src, dst;
    CopyBakeRig(source,src); CopyBakeRig(*candidate.scene_,dst);
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
    const auto rig=dst.armatures.GetEntity(0);
    auto& sh=src.humanoids.Create(src.armatures.GetEntity(0));
    dst.humanoids.Create(rig);
    AnimationComponent merged=clip;
    merged.channels.clear();merged.samplers.clear();merged.retargets.clear();
    // Wicked transfers between corresponding slots. Batch arbitrary named pairs,
    // including tails and wings, without installing a humanoid on the asset.
    for(size_t base=0;base<pairs.size();base+=HumanoidBoneCount) {
        auto& dh=*dst.humanoids.GetComponent(rig);
        std::fill(std::begin(sh.bones),std::end(sh.bones),wi::ecs::INVALID_ENTITY);
        std::fill(std::begin(dh.bones),std::end(dh.bones),wi::ecs::INVALID_ENTITY);
        for(size_t j=0;j<HumanoidBoneCount && base+j<pairs.size();++j) {
            sh.bones[j]=pairs[base+j].first; dh.bones[j]=pairs[base+j].second;
        }
        const Entity part=dst.RetargetAnimation(rig,original,true,&src);
        if(part==wi::ecs::INVALID_ENTITY) continue;
        const auto& baked=*dst.animations.GetComponent(part);
        const int offset=int(merged.samplers.size());
        for(auto channel:baked.channels) {channel.samplerIndex+=offset;merged.channels.push_back(channel);}
        for(const auto& sampler:baked.samplers) merged.samplers.push_back(sampler);
        dst.animations.Remove(part);dst.hierarchy.Remove(part);
    }
    if(merged.channels.empty()) {error="No mapped bone has an animated track in this clip.";return false;}
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
    dst.humanoids.Clear();
    const Entity result=wi::ecs::CreateEntity();
    dst.animations.Create(result)=std::move(merged);
    dst.names.Create(result).name=label;
    for(const auto& sampler:dst.animations.GetComponent(result)->samplers) dst.Component_Attach(sampler.data,result);
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
    candidate.dependencies_.reserve(candidate.dependencies_.size()+1);
    candidate.externalAnimations_.reserve(candidate.externalAnimations_.size()+1);
    candidate.externalAnimations_.push_back({candidate.dependencies_.size(),first,1,true});
    candidate.dependencies_.push_back(std::move(snapshot));
    candidate.scene_=std::move(prepared);
    candidate.summary_=ImportService::Summarize(*candidate.scene_);
    candidate.evidence_=ImportService::SummarizeModelEvidence(*candidate.scene_);
    return true;
}
