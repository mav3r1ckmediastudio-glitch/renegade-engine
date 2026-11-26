// The scene is an isolated, single-clip target skeleton. Publish its keys only
// after every sample has succeeded, so undo and failed pins retain the original.
static bool BakeJointPin(wi::scene::Scene& scene, const ClipJointPin& pin, std::string& error)
{
    using namespace wi::scene;
    using Entity = wi::ecs::Entity;
    using Channel = AnimationComponent::AnimationChannel;
    if (scene.animations.GetCount()!=1) {error="Select a single baked clip.";return false;}
    auto& clip=scene.animations[0];
    if ((!pin.position && !pin.rotation) || pin.parents<1 || pin.parents>32 ||
        !std::isfinite(pin.start) || !std::isfinite(pin.end) || !std::isfinite(pin.captureTime) ||
        !std::isfinite(pin.blend) || pin.blend<0 || pin.start<clip.start || pin.end>clip.end ||
        pin.start>pin.end || pin.captureTime<clip.start || pin.captureTime>clip.end) {
        error="Check the pin range, blend and chain length.";return false;
    }
    const Entity tip=scene.Entity_FindByName(pin.joint);
    const Entity anchor=pin.relativeTo.empty()?wi::ecs::INVALID_ENTITY:scene.Entity_FindByName(pin.relativeTo);
    if (!scene.transforms.Contains(tip) || (!pin.relativeTo.empty() && !scene.transforms.Contains(anchor))) {
        error="The selected joint or anchor is missing.";return false;
    }
    auto parent=[&](Entity joint) {auto h=scene.hierarchy.GetComponent(joint);return h?h->parentID:wi::ecs::INVALID_ENTITY;};
    std::vector<Entity> chain;
    if (pin.position) {
        const auto& joints=scene.armatures[0].boneCollection;
        Entity joint=tip;
        for(int i=0;i<pin.parents;++i) {
            joint=parent(joint);
            if(std::find(joints.begin(),joints.end(),joint)==joints.end()){error="Choose fewer parent bones for this joint.";return false;}
            chain.push_back(joint);
        }
    }
    if(pin.rotation)chain.push_back(tip);
    for(Entity joint=anchor;joint!=wi::ecs::INVALID_ENTITY;joint=parent(joint))
        if(joint==tip || std::find(chain.begin(),chain.end(),joint)!=chain.end()) {
            error="Choose an anchor outside the chain being solved.";return false;
        }
    auto world=[&](Entity joint){auto t=scene.transforms.GetComponent(joint);return t?XMLoadFloat4x4(&t->world):XMMatrixIdentity();};
    auto sample=[&](float time){
        clip.Pause();clip.RootMotionOff();clip.timer=time;clip.amount=1;
        clip.last_update_time=-std::numeric_limits<float>::max();scene.Update(0);
    };
    sample(pin.captureTime);
    if(std::abs(XMVectorGetX(XMMatrixDeterminant(world(anchor))))<1e-15f){error="The anchor has zero scale.";return false;}
    const auto relative=XMMatrixTranslation(pin.offsetX,pin.offsetY,pin.offsetZ)*world(tip)*XMMatrixInverse(nullptr,world(anchor));
    std::set<float> times{clip.start,clip.end,pin.start,pin.end};
    if(pin.start>clip.start)times.insert(std::nextafter(pin.start,clip.start));
    if(pin.end<clip.end)times.insert(std::nextafter(pin.end,clip.end));
    for(const auto& sampler:clip.samplers){
        const auto* data=scene.animation_datas.GetComponent(sampler.data);
        if(!data){error="The baked clip is missing animation samples.";return false;}
        for(float t:data->keyframe_times)if(t>=clip.start && t<=clip.end){
            times.insert(t);
            if(t>clip.start && sampler.mode==AnimationComponent::AnimationSampler::STEP)times.insert(std::nextafter(t,clip.start));
        }
    }
    const int steps=int(std::ceil((pin.end-pin.start)*60.f));
    for(int i=1;i<steps;++i)times.insert(pin.start+(pin.end-pin.start)*i/steps);
    if(pin.blend>0){times.insert(std::min(pin.end,pin.start+pin.blend));times.insert(std::max(pin.start,pin.end-pin.blend));}
    auto* ik=pin.position?&scene.inverse_kinematics.Create(tip):nullptr;
    if(ik){ik->chain_length=pin.parents;ik->iteration_count=32;ik->use_target_position=true;}
    std::vector<AnimationDataComponent> curves(chain.size());
    for(float t:times){
        if(ik)ik->SetDisabled(true);
        sample(t);
        const auto goal=relative*world(anchor);
        float weight=t>=pin.start && t<=pin.end?1.f:0.f;
        if(pin.blend>0)weight*=std::clamp(std::min(t-pin.start,pin.end-t)/pin.blend,0.f,1.f);
        std::vector<XMFLOAT4> original;
        for(auto joint:chain)original.push_back(scene.transforms.GetComponent(joint)->rotation_local);
        if(ik && weight>0){ik->SetDisabled(false);XMStoreFloat3(&ik->target_position,goal.r[3]);scene.Update(0);}
        if(ik && weight>=0.9999f){
            float reach=0;auto child=tip;
            for(int i=0;i<pin.parents;++i){const auto ancestor=parent(child);
                reach+=XMVectorGetX(XMVector3Length(world(child).r[3]-world(ancestor).r[3]));child=ancestor;}
            if(XMVectorGetX(XMVector3Length(world(tip).r[3]-goal.r[3]))>std::max(1e-6f,reach*.02f)){
                error="The selected chain cannot reach the pin at frame "+std::to_string(int(std::lround(30*t)))+
                    ". Adjust the target, range or parent count.";return false;
            }
        }
        for(size_t i=0;i<chain.size();++i){
            auto joint=chain[i];auto q=XMLoadFloat4(&original[i]);
            if(weight>0){
                const auto wanted=joint==tip && pin.rotation?goal:world(joint);
                auto local=wanted*XMMatrixInverse(nullptr,world(parent(joint)));
                XMVECTOR scale,rot,translation;
                if(!XMMatrixDecompose(&scale,&rot,&translation,local)){error="The joint transform cannot be decomposed.";return false;}
                q=XMQuaternionNormalize(XMQuaternionSlerp(q,rot,weight));
            }
            auto& curve=curves[i];auto& values=curve.keyframe_data;
            if(values.size()>=4 && XMVectorGetX(XMVector4Dot(q,XMVectorSet(values[values.size()-4],values[values.size()-3],values[values.size()-2],values.back())))<0)q=-q;
            XMFLOAT4 value;XMStoreFloat4(&value,q);
            curve.keyframe_times.push_back(t);values.insert(values.end(),{value.x,value.y,value.z,value.w});
        }
    }
    if(ik)scene.inverse_kinematics.Remove(tip);
    for(size_t i=0;i<chain.size();++i){
        auto it=std::find_if(clip.channels.begin(),clip.channels.end(),[&](const auto& c){return c.target==chain[i] && c.path==Channel::Path::ROTATION;});
        int index=-1;
        if(it==clip.channels.end()){
            it=clip.channels.emplace(clip.channels.end());it->target=chain[i];it->path=Channel::Path::ROTATION;
        }else{
            const int existing=it->samplerIndex;
            if(std::count_if(clip.channels.begin(),clip.channels.end(),[&](const auto& ch){return ch.samplerIndex==existing;})==1)index=existing;
        }
        if(index<0){index=int(clip.samplers.size());clip.samplers.emplace_back();}
        auto& sampler=clip.samplers[index];auto old=sampler.data;
        const auto data=wi::ecs::CreateEntity();sampler={};sampler.data=data;it->samplerIndex=index;
        if(old!=wi::ecs::INVALID_ENTITY && std::none_of(clip.samplers.begin(),clip.samplers.end(),[&](const auto& other){return other.data==old;}))
            scene.Entity_Remove(old);
        scene.animation_datas.Create(data)=std::move(curves[i]);
        scene.Component_Attach(data,scene.animations.GetEntity(0));
    }
    return true;
}
