// Draw rig controls in the importer GUI pass so the character cannot occlude them.
bool StudioRenderPath::RigEditingVisible() const
{
    return modelImportPreview_ && modelImportPanel_.IsVisible() && (importPage_ == ImportPage::Pose || importPage_ == ImportPage::Retarget) && !importBones_.empty();
}

XMFLOAT3 StudioRenderPath::ProjectImportPoint(XMFLOAT3 world) const
{
    const auto pos = modelImportPreviewImage_.GetPos(), size = modelImportPreviewImage_.GetSize();
    const auto& camera = modelImportPreview_->InspectionCamera();
    XMFLOAT3 result;
    XMStoreFloat3(&result, XMVector3Project(XMLoadFloat3(&world), pos.x,pos.y,size.x,size.y,0,1,
        camera.GetProjection(),camera.GetView(),XMMatrixIdentity()));
    return result;
}

XMFLOAT3 StudioRenderPath::UnprojectImportPoint(XMFLOAT3 screen) const
{
    const auto pos = modelImportPreviewImage_.GetPos(), size = modelImportPreviewImage_.GetSize();
    const auto& camera = modelImportPreview_->InspectionCamera();
    XMFLOAT3 result;
    XMStoreFloat3(&result, XMVector3Unproject(XMLoadFloat3(&screen), pos.x,pos.y,size.x,size.y,0,1,
        camera.GetProjection(),camera.GetView(),XMMatrixIdentity()));
    return result;
}

XMFLOAT3 StudioRenderPath::ImportPosePoint(int field) const
{
    const auto value = [&](int offset) {
        return std::strtof(importPoseFields_[field+offset].font.GetTextA().c_str(), nullptr);
    };
    return {value(0),value(1),value(2)};
}

// Skinning writes vertices in armature space. The renderer then places the
// skinned mesh with the object transform, which can include FBX unit conversion.
static wi::vector<wi::ecs::Entity> ImportAnimationBones(const wi::scene::Scene& scene)
{
    if (scene.armatures.GetCount() == 1) return scene.armatures[0].boneCollection;
    bridge::AnimationRigBones rig;
    std::string error;
    if (!bridge::CollectAnimationRig(scene,rig,error)) return {};
    return std::move(rig.boneCollection);
}

static XMMATRIX ImportRigToObject(const wi::scene::Scene& scene) {
    if(!scene.armatures.GetCount())return XMMatrixIdentity();
    const auto rig=scene.armatures.GetEntity(0);
    const auto* arm=scene.transforms.GetComponent(rig);
    if(!arm)return XMMatrixIdentity();
    for(size_t i=0;i<scene.objects.GetCount();++i){
        const auto* mesh=scene.meshes.GetComponent(scene.objects[i].meshID);
        if(!mesh || mesh->armatureID!=rig)continue;
        const auto* obj=scene.transforms.GetComponent(scene.objects.GetEntity(i));
        if(obj)return XMMatrixInverse(nullptr,XMLoadFloat4x4(&arm->world))*XMLoadFloat4x4(&obj->world);
    }
    return XMMatrixIdentity();
}

static XMFLOAT3 TransformRigPoint(XMFLOAT3 point,FXMMATRIX matrix){
    XMFLOAT3 result;XMStoreFloat3(&result,XMVector3TransformCoord(XMLoadFloat3(&point),matrix));return result;
}
void StudioRenderPath::DrawImportRig(wi::graphics::CommandList cmd) const
{
    const auto pos=modelImportPreviewImage_.GetPos(), size=modelImportPreviewImage_.GetSize();
    const float dpi=GetPhysicalWidth()/std::max(1.f,GetLogicalWidth());
    wi::graphics::Rect clip={long(pos.x*dpi),long(pos.y*dpi),long((pos.x+size.x)*dpi),long((pos.y+size.y)*dpi)};
    auto* device=wi::graphics::GetDevice();
    device->BindScissorRects(1,&clip,cmd);
    const auto line = [&](XMFLOAT2 a,XMFLOAT2 b,float width,wi::Color color) {
        const float dx=b.x-a.x,dy=b.y-a.y;
        wi::image::Params p;
        p.pos={(a.x+b.x)*0.5f,(a.y+b.y)*0.5f,0};
        p.siz={std::hypot(dx,dy),width};p.pivot={0.5f,0.5f};p.rotation=std::atan2(dy,dx);
        p.color=color;p.blendFlag=wi::enums::BLENDMODE_ALPHA;
        wi::image::Draw(nullptr,p,cmd);
    };
    const auto visible=[](XMFLOAT3 p){return p.z>=0 && p.z<=1 && std::isfinite(p.x) && std::isfinite(p.y);};
    auto& scene=modelImportPreview_->InspectionScene();
    const int selected=importBone_.GetSelected();
    const auto tip=selected>=0 && size_t(selected)<importBones_.size()?importBones_[selected].entity:wi::ecs::INVALID_ENTITY;
    const auto parent=[&](wi::ecs::Entity e){const auto* h=scene.hierarchy.GetComponent(e);return h?h->parentID:wi::ecs::INVALID_ENTITY;};
    const auto mid=parent(tip);
    std::map<wi::ecs::Entity,XMFLOAT3> points;
    const auto rigToObject=ImportRigToObject(scene);
    for(const auto& bone:importBones_) {
        const auto* t=scene.transforms.GetComponent(bone.entity);
        if(t){XMFLOAT3 world;XMStoreFloat3(&world,XMVector3TransformCoord(XMLoadFloat4x4(&t->world).r[3],rigToObject));points.emplace(bone.entity,ProjectImportPoint(world));}
    }
    for(const auto& [entity,p]:points) {
        auto it=points.find(parent(entity));
        if(it==points.end() || !visible(p) || !visible(it->second))continue;
        const XMFLOAT2 a={it->second.x,it->second.y}, b={p.x,p.y};
        const bool chain=entity==tip || entity==mid;
        line(a,b,chain?5.f:3.5f,wi::Color(16,22,28,220));
        line(a,b,chain?3.f:1.65f,chain?wi::Color(255,160,57,255):wi::Color(98,217,231,245));
    }
    for(const auto& [entity,p]:points) if(visible(p)) {
        const float r=entity==tip?3.8f:2.f;
        wi::image::Draw(nullptr,wi::image::Params(p.x-r,p.y-r,2*r,2*r,wi::Color(209,245,248,255)),cmd);
    }
    if(importPage_==ImportPage::Pose && !comparingPose_ && points.count(tip) && points.count(mid) && points.count(parent(mid))) {
        for(int i=0;i<2;++i) {
            const auto handle=ProjectImportPoint(TransformRigPoint(ImportPosePoint(i?6:3),rigToObject));
            if(!visible(handle))continue;
            const auto color=i?wi::Color(116,195,255,255):wi::Color(255,155,55,255);
            const auto j=points.at(i?mid:tip);
            if(visible(j))line({j.x,j.y},{handle.x,handle.y},2,color);
            const int sides=i?4:24;
            const float r=i?10.f:12.f;
            for(int k=0;k<sides;++k) {
                const float angle=XM_2PI*k/sides, next=XM_2PI*(k+1)/sides;
                XMFLOAT2 a={handle.x+r*std::cos(angle),handle.y+r*std::sin(angle)};
                XMFLOAT2 b={handle.x+r*std::cos(next),handle.y+r*std::sin(next)};
                line(a,b,5,wi::Color(20,23,27,245));line(a,b,2.5f,color);
            }
            const float labelX=i?handle.x-60:handle.x+16;
            wi::font::Params fp(int(labelX),int(handle.y)-8,14,wi::font::WIFALIGN_LEFT,wi::font::WIFALIGN_TOP,color);
            fp.shadowColor=wi::Color::Black();
            wi::font::Draw(i?"POLE":"TARGET",fp,cmd);
        }
    }
    const wi::graphics::Rect full={0,0,long(GetPhysicalWidth()),long(GetPhysicalHeight())};
    device->BindScissorRects(1,&full,cmd);
}

void StudioRenderPath::UpdateImportRigInput()
{
    const auto mouse=wi::input::GetPointer();
    const auto pos=modelImportPreviewImage_.GetPos(), size=modelImportPreviewImage_.GetSize();
    const auto mapping=ImportRigToObject(modelImportPreview_->InspectionScene());
    if(rigDrag_!=RigHandle::None) {
        if(!wi::input::Down(wi::input::MOUSE_BUTTON_LEFT)) {
            rigDrag_=RigHandle::None;dragOriginalPose_.clear();ApplyPoseEdit(true);return;
        }
        const auto anchor=ProjectImportPoint(TransformRigPoint(dragAnchor_,mapping));
        const float dx=std::clamp(mouse.x,pos.x,pos.x+size.x)-dragStart_.x;
        const float dy=std::clamp(mouse.y,pos.y,pos.y+size.y)-dragStart_.y;
        const auto moved=TransformRigPoint(UnprojectImportPoint({anchor.x+dx,anchor.y+dy,anchor.z}),XMMatrixInverse(nullptr,mapping));
        if(!std::isfinite(moved.x)||!std::isfinite(moved.y)||!std::isfinite(moved.z))return;
        auto target=dragTarget_,pole=dragPole_;
        (rigDrag_==RigHandle::Target?target:pole)=moved;
        const auto fields=[&]{
            const float v[]={target.x,target.y,target.z,pole.x,pole.y,pole.z};
            for(int i=0;i<6;++i)importPoseFields_[3+i].SetValue(v[i]);
        };
        fields();return;
    }
    if(mouse.x<pos.x || mouse.x>pos.x+size.x || mouse.y<pos.y || mouse.y>pos.y+size.y ||
        !wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))return;
    auto& scene=modelImportPreview_->InspectionScene();
    const auto distance=[&](XMFLOAT3 world) {
        const auto p=ProjectImportPoint(TransformRigPoint(world,mapping));
        return p.z<0||p.z>1?1e10f:std::hypot(mouse.x-p.x,mouse.y-p.y);
    };
    const int selected=importBone_.GetSelected();
    if(importPage_==ImportPage::Pose && !comparingPose_ && selected>=0 && size_t(selected)<importBones_.size()) {
        const auto parent=[&](wi::ecs::Entity e){auto* h=scene.hierarchy.GetComponent(e);return h?h->parentID:wi::ecs::INVALID_ENTITY;};
        const auto mid=parent(importBones_[selected].entity), root=parent(mid);
        const auto isBone=[&](wi::ecs::Entity e){return std::any_of(importBones_.begin(),importBones_.end(),[e](const auto& b){return b.entity==e;});};
        dragTarget_=ImportPosePoint(3);dragPole_=ImportPosePoint(6);
        const float dt=distance(dragTarget_),dp=distance(dragPole_);
        if(std::min(dt,dp)<=15.f && isBone(mid) && isBone(root)) {
            rigDrag_=dt<=dp?RigHandle::Target:RigHandle::Pole;
            dragAnchor_=dt<=dp?dragTarget_:dragPole_;dragStart_={mouse.x,mouse.y};
            dragOriginalPose_=importPoseRotations_;return;
        }
    }
    int closest=-1;float best=10.f;
    for(size_t i=0;i<importBones_.size();++i) {
        const auto* t=scene.transforms.GetComponent(importBones_[i].entity);if(!t)continue;
        const float d=distance(t->GetPosition());
        if(d<best){closest=int(i);best=d;}
    }
    if(importPage_==ImportPage::Retarget && rtScene_.IsValid()){
        int src=-1;
        for(int j=0;j<int(sourceScreenPoints_.size());++j){
            auto p=sourceScreenPoints_[j];float d=std::hypot(p.x-mouse.x,p.y-mouse.y);
            if(p.z>=0 && p.z<=1 && d<best){best=d;src=j;}
        }
        if (src >= 0) {
            if (reviewingPairs_) {
                if (rtPairs_[src] == -2) { reviewSource_ = src; RefreshMapRows(); }
            } else AssignMapSource(mapTargetIndex_,src);
            return;
        }
        if(closest>=0){
            const auto bones=ImportAnimationBones(scene);
            auto it=std::find(bones.begin(),bones.end(),importBones_[closest].entity);
            if (it != bones.end()) {
                const int target = int(it - bones.begin());
                mapSearch_.SetValue("");
                RefreshMapRows();
                const auto row = std::find(mapFiltered_.begin(), mapFiltered_.end(), target);
                if (row != mapFiltered_.end()) {
                    mapFirst_ = int(row - mapFiltered_.begin()) / MapRows * MapRows;
                    SelectMapTarget(target);
                }
            }
        }
    }else if(closest>=0)importBone_.SetSelected(closest);
}
