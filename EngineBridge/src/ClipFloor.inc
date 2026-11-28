// SkinVertex produces armature-local coordinates. Wicked renders each instance
// through its object matrix, which can differ from the armature's own matrix.
bool ModelImportCandidateService::MeasureClipFloor(wi::scene::Scene& model,int index,
    ClipJointPin& edit,std::string& error) const
{
    using namespace wi::scene;
    using Entity=wi::ecs::Entity;
    Scene scene;wi::Archive copy;model.Serialize(copy);copy.SetReadModeAndResetPos(true);scene.Serialize(copy);
    ModelAnimationPreviewService preview;preview.Prepare(scene);
    if(!preview.Select(index)){error="Select a baked clip.";return false;}
    const auto range=preview.Clips().at(index);
    struct Skin {size_t object;std::vector<Entity> roots;};
    std::vector<Skin> skins;
    auto parent=[&](Entity e){auto h=scene.hierarchy.GetComponent(e);return h?h->parentID:wi::ecs::INVALID_ENTITY;};
    for(size_t o=0;o<scene.objects.GetCount();++o){
        const auto* mesh=scene.meshes.GetComponent(scene.objects[o].meshID);
        if(!mesh || !mesh->IsSkinned() || mesh->vertex_positions.empty())continue;
        const auto* arm=scene.armatures.GetComponent(mesh->armatureID);
        if(!arm || arm->boneCollection.empty())continue;
        Skin skin{o,{}};
        std::set<Entity> bones(arm->boneCollection.begin(),arm->boneCollection.end());
        for(auto joint:bones){
            auto ancestor=parent(joint);
            while(ancestor!=wi::ecs::INVALID_ENTITY && !bones.count(ancestor))ancestor=parent(ancestor);
            if(ancestor==wi::ecs::INVALID_ENTITY)skin.roots.push_back(joint);
        }
        skins.push_back(std::move(skin));
    }
    if(skins.empty()){error="This clip has no skinned character mesh.";return false;}
    float bottom=FLT_MAX;
    std::map<std::string,XMFLOAT3> directions;
    const int steps=std::max(1,int(std::ceil((range.end-range.start)*60.f)));
    for(int frame=0;frame<=steps;++frame){
        preview.Scrub(range.start+(range.end-range.start)*float(frame)/steps);scene.Update(0);
        for(const auto& skin:skins){
            const auto& mesh=*scene.meshes.GetComponent(scene.objects[skin.object].meshID);
            const auto& arm=*scene.armatures.GetComponent(mesh.armatureID);
            const auto objectWorld=XMLoadFloat4x4(&scene.matrix_objects[skin.object]);
            const auto armWorld=XMLoadFloat4x4(&scene.transforms.GetComponent(mesh.armatureID)->world);
            for(auto root:skin.roots){
                const auto* p=scene.transforms.GetComponent(parent(root));const auto* name=scene.names.GetComponent(root);
                if(!name){error="The skeleton has an unnamed root.";return false;}
                auto basis=(p?XMLoadFloat4x4(&p->world):XMMatrixIdentity())*XMMatrixInverse(nullptr,armWorld)*objectWorld;
                auto dir=XMVector3TransformNormal(XMVectorSet(0,1,0,0),XMMatrixInverse(nullptr,basis));
                if(XMVector3IsNaN(dir)||XMVector3IsInfinite(dir)){error="Cannot ground a skeleton with singular transforms.";return false;}
                XMFLOAT3 local;XMStoreFloat3(&local,dir);
                auto it=directions.emplace(name->name,local);
                if(!it.second && XMVectorGetX(XMVector3Length(dir-XMLoadFloat3(&it.first->second)))>
                    0.0001f*XMVectorGetX(XMVector3Length(dir))){
                    error="This skeleton changes its root axes during the clip. A constant floor lift cannot be applied.";return false;
                }
            }
            for(unsigned v=0;v<mesh.vertex_positions.size();++v){
                const float y=XMVectorGetY(XMVector3TransformCoord(wi::scene::SkinVertex(mesh,arm,v),objectWorld));
                if(!std::isfinite(y)){error="A skinned vertex has an invalid position.";return false;}
                bottom=std::min(bottom,y);
            }
        }
    }
    if(bottom>=0){error="The sampled mesh already clears the floor throughout this clip.";return false;}
    edit.floorOffsets.clear();
    for(auto& [name,dir]:directions)edit.floorOffsets[name]={-bottom*dir.x,-bottom*dir.y,-bottom*dir.z};
    return true;
}

static bool ApplyClipFloor(wi::scene::Scene& scene,const ClipJointPin& edit,std::string& error)
{
    using namespace wi::scene;
    using Path=AnimationComponent::AnimationChannel::Path;
    auto& clip=scene.animations[0];
    for(const auto& [name,offset]:edit.floorOffsets){
        const auto root=scene.Entity_FindByName(name);
        const auto* transform=scene.transforms.GetComponent(root);
        if(!transform){error="The baked clip is missing its floor-correction root.";return false;}
        auto it=std::find_if(clip.channels.begin(),clip.channels.end(),[&](const auto& ch){return ch.target==root && ch.path==Path::TRANSLATION;});
        AnimationDataComponent keys;
        AnimationComponent::AnimationSampler sampler;
        if(it==clip.channels.end()){
            const auto& p=transform->translation_local;keys.keyframe_times={clip.start};keys.keyframe_data={p.x,p.y,p.z};
            it=clip.channels.emplace(clip.channels.end());it->target=root;it->path=Path::TRANSLATION;
        }else{
            sampler=clip.samplers[it->samplerIndex];keys=*scene.animation_datas.GetComponent(sampler.data);
        }
        if(keys.keyframe_data.size()!=3*keys.keyframe_times.size()){error="Unsupported baked translation curve.";return false;}
        for(size_t i=0;i<keys.keyframe_data.size();++i)keys.keyframe_data[i]+=offset[i%3];
        sampler.data=wi::ecs::CreateEntity();scene.animation_datas.Create(sampler.data)=std::move(keys);
        scene.Component_Attach(sampler.data,scene.animations.GetEntity(0));
        it->samplerIndex=int(clip.samplers.size());clip.samplers.push_back(sampler);
    }
    return true;
}