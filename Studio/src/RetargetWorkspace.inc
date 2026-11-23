#include "RetargetRestAlignment.inc"
#include "RetargetJointNames.inc"

// Animation-only FBX exports can have named transforms and tracks without a skin.
static bool BuildClipOnlyRig(wi::scene::Scene& scene)
{
    if(scene.armatures.GetCount() || !scene.animations.GetCount() || scene.objects.GetCount())return false;
    std::set<wi::ecs::Entity> joints;
    for(size_t i=0;i<scene.animations.GetCount();++i)
        for(const auto& channel:scene.animations[i].channels)
            if(scene.transforms.Contains(channel.target) && scene.names.Contains(channel.target))
                joints.insert(channel.target);
    if(joints.empty())return false;
    const auto root=wi::ecs::CreateEntity();
    scene.transforms.Create(root);
    auto& arm=scene.armatures.Create(root);
    for(size_t i=0;i<scene.transforms.GetCount();++i){
        const auto bone=scene.transforms.GetEntity(i);
        if(!joints.count(bone))continue;
        const auto world=scene.transforms[i].GetLocalMatrix()*scene.ComputeParentMatrixRecursive(bone);
        XMFLOAT4X4 inverse;XMStoreFloat4x4(&inverse,XMMatrixInverse(nullptr,world));
        arm.boneCollection.push_back(bone);arm.inverseBindMatrices.push_back(inverse);
    }
    return true;
}

void StudioRenderPath::CreateRetargetWorkspace()
{
    const auto button=[this](wi::gui::Button& b,const char* label,auto action) {
        b.Create(label);b.OnClick([action](const wi::gui::EventArgs&){action();});modelImportPanel_.AddWidget(&b);
    };
    for(size_t i=0;i<rtRigDegrees_.size();++i){
        auto& field=rtRigDegrees_[i];
        std::string axis="Rig ";axis+="XYZ"[i];
        field.Create(axis);field.SetDescription(axis+"  ");field.SetValue(0);
        field.SetTooltip("Whole source rig rotation in degrees about the viewport world axes");
        field.SetCancelInputEnabled(false);
        field.OnInputAccepted([this](const wi::gui::EventArgs&){ApplySourceRigRotation();});
        modelImportPanel_.AddWidget(&field);
    }
    button(rtRigApply_,"Apply source rig rotation",[this]{ApplySourceRigRotation();});
    button(rtRigReset_,"Reset source rig rotation",[this]{
        for(auto& field:rtRigDegrees_)field.SetValue(0);
        ApplySourceRigRotation();
    });
    button(rtOpen_,"Open source rig and clips...",[this]{OpenRetargetSource();});
    button(rtSourceTurn_,"Rotate source joint",[this]{RotateRetargetBone(true);});
    button(rtTargetTurn_,"Rotate target joint",[this]{RotateRetargetBone(false);});
    button(rtBake_,"Bake clip to Actions",[this]{
        if(std::find(rtPairs_.begin(),rtPairs_.end(),-2)!=rtPairs_.end()){
            rtSource_.SetSelected(int(std::find(rtPairs_.begin(),rtPairs_.end(),-2)-rtPairs_.begin()));
            SelectRetargetBone();UpdateRetargetStatus();return;
        }
        const auto generation=modelImportGeneration_;
        wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[this,generation](uint64_t){
            if(generation==modelImportGeneration_ && modelImportPanel_.IsVisible() && importPage_==ImportPage::Retarget)
                BakeRetargetClip();
        });
    });
    rtClip_.Create("Source clip");rtSource_.Create("Source joint");rtTarget_.Create("Target joint");
    for(auto* c:{&rtClip_,&rtSource_,&rtTarget_})modelImportPanel_.AddWidget(c);
    rtSource_.OnSelect([this](const wi::gui::EventArgs&){SelectRetargetBone();});
    rtTarget_.OnSelect([this](const wi::gui::EventArgs& a){
        const int i=rtSource_.GetSelected(), t=a.iValue-2;
        if(i<0 || size_t(i)>=rtPairs_.size())return;
        if(t>=0 && std::find(rtPairs_.begin(),rtPairs_.end(),t)!=rtPairs_.end() && rtPairs_[i]!=t) {
            ShowStudioMessageBox("This target is assigned to another source joint. Clear that pair first.","Bone mapping");
        } else rtPairs_[i]=t;
        SelectRetargetBone();UpdateRetargetStatus();
    });
    rtSummary_.Create("Retarget status");modelImportPanel_.AddWidget(&rtSummary_);
}

struct ImportRetargetSource
{
    wi::allocator::shared_ptr<wi::scene::Scene> scene;
    std::vector<std::string> sourceNames, targetNames;
    std::vector<int> pairs;
    std::string status;
    bool bodyAligned=false;
};
static bool OptionalUnpairedTrack(const std::string& name) {
    const auto key=RetargetJointKey(name);
    if(key.rfind("ik",0)==0 || key.find("twist")!=std::string::npos)return true;
    for(const char* digit:{"thumb","index","middle","ring","pinky"})
        if(key.rfind(digit,0)==0)return true;
    return false;
}

static bool PrepareImportRetargetSource(const std::string& path,const wi::scene::Scene& target,ImportRetargetSource& out,std::string& error)
{
    auto next=wi::allocator::make_shared<wi::scene::Scene>();
    bridge::ClearWickedModelImporterFailureDiagnostic();
    ImportModel_FBX(path,*next);
    const auto failure=bridge::ConsumeWickedModelImporterFailureDiagnostic();

    const bool clipOnly=failure.empty() && BuildClipOnlyRig(*next);
    if(!failure.empty() || next->armatures.GetCount()!=1 || target.armatures.GetCount()!=1 ||
        next->animations.GetCount()==0) {
        error=failure.empty()?"Choose an FBX with one skeleton and at least one clip.":failure;return false;
    }
    auto names=[](const wi::scene::Scene& s,std::vector<std::string>& out) {
        std::set<std::string> unique;
        for(auto e:s.armatures[0].boneCollection) {
            const auto* n=s.names.GetComponent(e);
            if(!s.transforms.Contains(e) || !n || n->name.empty() || !unique.insert(n->name).second)return false;
            out.push_back(n->name);
        }
        return !out.empty();
    };
    std::vector<std::string> sn,tn;
    if(!names(*next,sn)||!names(target,tn)){
        error="Each rig needs uniquely named joints with transforms.";return false;
    }
    (void)bridge::DisableDefaultHumanoidLookAt(*next);
    bridge::PauseImportedModelAnimations(*next);
    for(size_t i=0;i<next->animations.GetCount();++i)next->animations[i].amount=0;
    const auto nativeRig=[](const wi::scene::Scene& scene) {
        for(size_t i=0;i<scene.humanoids.GetCount();++i)
            if(scene.humanoids[i].IsValid())return scene.humanoids.GetEntity(i);
        return wi::ecs::INVALID_ENTITY;
    };
    const auto nativeSource=nativeRig(*next),nativeTarget=nativeRig(target);
    if(nativeSource!=wi::ecs::INVALID_ENTITY && nativeTarget!=wi::ecs::INVALID_ENTITY){
        const auto& sb=next->armatures[0].boneCollection;
        const auto& tb=target.armatures[0].boneCollection;
        const auto& src=*next->humanoids.GetComponent(nativeSource);
        const auto& dst=*target.humanoids.GetComponent(nativeTarget);
        std::vector<int> slots(sb.size(),-2);std::set<int> used;
        for(size_t slot=0;slot<bridge::HumanoidBoneCount;++slot){
            auto a=std::find(sb.begin(),sb.end(),src.bones[slot]);
            auto b=std::find(tb.begin(),tb.end(),dst.bones[slot]);
            if(a!=sb.end() && b!=tb.end() && used.insert(int(b-tb.begin())).second)
                slots[a-sb.begin()]=int(b-tb.begin());
        }
        if(!used.empty()){
            SuggestNamedPairs(sn,tn,slots,used);
            for(size_t i=0;i<slots.size();++i)
                if(slots[i]==-2 && OptionalUnpairedTrack(sn[i]))slots[i]=-1;
            out.bodyAligned=true;out.status="Wicked humanoid reference poses";
            out.scene=std::move(next);out.sourceNames=std::move(sn);out.targetNames=std::move(tn);
            out.pairs=std::move(slots);return true;
        }
    }
    const bool companion=clipOnly && LoadCompanionRest(*next,fs::u8path(path).parent_path()/"T_pose.fbx");
    const bool avatar=!companion && LoadAvatarReference(*next,fs::u8path(path));
    if(!clipOnly && !avatar)next->ResetPose(next->armatures.GetEntity(0));
    next->Update(0);
    auto mapping=[](const wi::scene::Scene& scene){
        const auto rig=scene.armatures.GetEntity(0);
        for(size_t i=0;i<scene.humanoids.GetCount();++i)
            if(scene.humanoids[i].IsValid())return bridge::CaptureHumanoidMapping(scene,scene.humanoids.GetEntity(i));
        return bridge::BuildAutoHumanoidMapping(scene,rig).mapping;
    };
    auto srcMap=mapping(*next);
    const auto dstMap=mapping(target);
    const auto& srcBones=next->armatures[0].boneCollection;const auto& dstBones=target.armatures[0].boneCollection;
    std::vector<int> pairs(sn.size(),-2);std::set<int> used;
    for(size_t i=0;i<sn.size();++i){
        const auto it=std::find(tn.begin(),tn.end(),sn[i]);
        if(it!=tn.end()){pairs[i]=int(it-tn.begin());used.insert(pairs[i]);}
    }
    SuggestNamedPairs(sn,tn,pairs,used);
    for(size_t k=0;k<bridge::HumanoidBoneCount;++k) {
        auto a=std::find(srcBones.begin(),srcBones.end(),srcMap.bones[k]),b=std::find(dstBones.begin(),dstBones.end(),dstMap.bones[k]);
        if(a!=srcBones.end() && b!=dstBones.end() && pairs[a-srcBones.begin()]<0 && used.insert(int(b-dstBones.begin())).second)
            pairs[a-srcBones.begin()]=int(b-dstBones.begin());
    }
    // Derive alignment landmarks from the actual one-to-one suggestions.
    for(size_t k=0;k<bridge::HumanoidBoneCount;++k){
        auto b=std::find(dstBones.begin(),dstBones.end(),dstMap.bones[k]);
        if(b==dstBones.end())continue;
        auto a=std::find(pairs.begin(),pairs.end(),int(b-dstBones.begin()));
        if(a!=pairs.end())srcMap.bones[k]=srcBones[a-pairs.begin()];
    }

    // IK helpers, twist joints and absent finger segments can be reviewed
    // individually. They must not block a valid humanoid body transfer.
    if(bridge::IsHumanoidMappingValid(srcMap) && bridge::IsHumanoidMappingValid(dstMap)){
        for(size_t i=0;i<pairs.size();++i)
            if(pairs[i]==-2 && OptionalUnpairedTrack(sn[i]))pairs[i]=-1;
    }
    const auto result=AlignRestRigs(*next,target,srcMap,dstMap,pairs);
    out.bodyAligned=result.find("Source to target scale")==0;
    out.status=(companion?"T_pose.fbx":avatar?"Avatar reference pose":clipOnly?"FBX default pose":"Source bind pose")+std::string(" / ")+result;
    out.scene=std::move(next);out.sourceNames=std::move(sn);out.targetNames=std::move(tn);out.pairs=std::move(pairs);
    return true;
}

void StudioRenderPath::OpenRetargetSource()
{
    const auto generation=modelImportGeneration_;
    wi::helper::FileDialogParams params;
    params.type=wi::helper::FileDialogParams::OPEN;params.description="Source skeleton and animation";params.extensions={"fbx"};
    wi::helper::FileDialog(params,[this,generation](const std::string& path){
        wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[this,generation,path](uint64_t){
            if(path.empty() || generation!=modelImportGeneration_ || !modelImportPanel_.IsVisible() ||
                importPage_!=ImportPage::Retarget || !modelImportPreview_)return;
            ImportRetargetSource source;
            std::string error;
            if(!PrepareImportRetargetSource(path,*modelImportCandidate_->PeekMutableScene(),source,error)){
                ShowStudioMessageBox(error,"Retarget source");return;
            }
            rtSummary_.SetTooltip(source.status);
            rtScene_=std::move(source.scene);rtFile_=fs::u8path(path).stem().generic_u8string();
            rtSourceNames_=std::move(source.sourceNames);rtTargetNames_=std::move(source.targetNames);rtPairs_=std::move(source.pairs);rtSourcePose_.clear();
            rtOrientationRoot_=wi::ecs::INVALID_ENTITY;
            for(auto& field:rtRigDegrees_)field.SetValue(0);
            rtClip_.ClearItems();rtSource_.ClearItems();rtTarget_.ClearItems();
            rtTarget_.AddItem("Unresolved");rtTarget_.AddItem("Ignore source motion");
            for(const auto& n:rtTargetNames_)rtTarget_.AddItem(n);
            for(const auto& n:rtSourceNames_)rtSource_.AddItem(n);
            for(size_t i=0;i<rtScene_->animations.GetCount();++i){
                auto* n=rtScene_->names.GetComponent(rtScene_->animations.GetEntity(i));
                rtClip_.AddItem(rtFile_+(n?" / "+n->name:""));
            }
            rtClip_.SetSelected(0);
            auto first=std::find(rtPairs_.begin(),rtPairs_.end(),-2);
            auto hand=std::find(rtSourceNames_.begin(),rtSourceNames_.end(),"hand_l");
            rtSource_.SetSelected(first!=rtPairs_.end()?int(first-rtPairs_.begin()):hand!=rtSourceNames_.end()?int(hand-rtSourceNames_.begin()):0);
            UpdateRetargetStatus();LayoutImportWorkspace();
        });
    });
}

void StudioRenderPath::SelectRetargetBone()
{
    int i=rtSource_.GetSelected();
    if(i<0 || size_t(i)>=rtPairs_.size())return;
    int t=rtPairs_[i];rtTarget_.SetSelectedWithoutCallback(t+2);
    rtTargetTurn_.SetEnabled(t>=0);
    if(t>=0 && modelImportPreview_) {
        auto& scene=modelImportPreview_->InspectionScene();
        auto e=scene.armatures[0].boneCollection[t];
        for(size_t j=0;j<importBones_.size();++j)if(importBones_[j].entity==e){importBone_.SetSelected(int(j));break;}
    }
}

void StudioRenderPath::UpdateRetargetStatus()
{
    if(!rtScene_.IsValid()){rtSummary_.SetText("Open an animation FBX to compare its skeleton with this character.");return;}
    const int unresolved=int(std::count(rtPairs_.begin(),rtPairs_.end(),-2));
    const int ignored=int(std::count(rtPairs_.begin(),rtPairs_.end(),-1));
    const int matched=int(rtPairs_.size())-ignored-unresolved;
    rtSummary_.SetText("Source: "+rtFile_+"\n"+std::to_string(matched)+" matched   /   "+std::to_string(ignored)+" ignored   /   "+
        std::to_string(unresolved)+" unresolved\n"+std::to_string(rtTargetNames_.size()-matched)+" target-only joints keep their reference pose.\n"+rtSummary_.GetTooltip());
    rtBake_.SetText(unresolved ? "Review "+std::to_string(unresolved)+" unresolved joints" : "Bake clip to Actions");
    std::string extras;
    for(size_t i=0;i<rtPairs_.size();++i)if(rtPairs_[i]==-2)extras+=(extras.empty()?"":", ")+rtSourceNames_[i];
    rtBake_.SetTooltip(unresolved ? extras : "Bake the selected clip with this mapping.");
    rtBake_.SetEnabled(matched>0);
}

void StudioRenderPath::ApplySourceRigRotation()
{
    if(!rtScene_.IsValid() || !modelImportPreview_ || !modelImportCandidate_)return;
    float angles[3];
    for(size_t i=0;i<3;++i){
        const auto value=rtRigDegrees_[i].GetValue();char* end=nullptr;
        angles[i]=std::strtof(value.c_str(),&end);
        if(value.empty() || *end || !std::isfinite(angles[i])){
            ShowStudioMessageBox("Enter three finite angles in degrees.","Source rig rotation");return;
        }
    }
    if(rtOrientationRoot_==wi::ecs::INVALID_ENTITY){
        std::vector<wi::ecs::Entity> roots;
        for(size_t i=0;i<rtScene_->transforms.GetCount();++i){
            const auto e=rtScene_->transforms.GetEntity(i);
            if(!rtScene_->hierarchy.Contains(e))roots.push_back(e);
        }
        rtOrientationRoot_=rtScene_->Entity_CreateTransform("Source rig orientation");
        for(auto e:roots)rtScene_->Component_Attach(e,rtOrientationRoot_,true);
    }
    const auto raw=modelImportCandidate_->PeekScene();
    const auto& preview=modelImportPreview_->InspectionScene();
    const auto display=XMMatrixInverse(nullptr,XMLoadFloat4x4(&raw->transforms.GetComponent(raw->armatures.GetEntity(0))->world))
        *XMLoadFloat4x4(&preview.transforms.GetComponent(preview.armatures.GetEntity(0))->world)*ImportRigToObject(preview);
    XMVECTOR scale,rotation,pos;
    if(!XMMatrixDecompose(&scale,&rotation,&pos,display))return;
    const auto basis=XMMatrixRotationQuaternion(rotation);
    auto* transform=rtScene_->transforms.GetComponent(rtOrientationRoot_);
    transform->ClearTransform();
    transform->MatrixTransform(basis * XMMatrixRotationRollPitchYaw(XMConvertToRadians(angles[0]),
        XMConvertToRadians(angles[1]),XMConvertToRadians(angles[2])) * XMMatrixTranspose(basis));
    rtScene_->Update(0);
}

void StudioRenderPath::RotateRetargetBone(bool source)
{
    if(!source){ApplyPoseEdit(false);return;}
    int i=rtSource_.GetSelected();
    if(!rtScene_.IsValid() || i<0 || size_t(i)>=rtPairs_.size())return;
    float angles[3]{};
    for(int k=0;k<3;++k){
        auto text=importPoseFields_[k].GetValue();char* end=nullptr;
        angles[k]=std::strtof(text.c_str(),&end);
        if(text.empty() || *end || !std::isfinite(angles[k]) || std::abs(angles[k])>3600){
            ShowStudioMessageBox("Enter a finite joint rotation in degrees.","Source pose");return;
        }
    }
    auto* t=rtScene_->transforms.GetComponent(rtScene_->armatures[0].boneCollection[i]);
    t->Rotate(XMQuaternionRotationRollPitchYaw(XMConvertToRadians(angles[0]),XMConvertToRadians(angles[1]),XMConvertToRadians(angles[2])));
    rtSourcePose_[rtSourceNames_[i]]=t->rotation_local;rtScene_->Update(0);
    for(int k=0;k<3;++k)importPoseFields_[k].SetValue(0);
}

void StudioRenderPath::BakeRetargetClip()
{
    if(!rtScene_.IsValid() || rtClip_.GetSelected()<0 || !modelImportCandidate_)return;
    std::map<std::string,XMFLOAT4> targetPose;
    auto& scene=modelImportPreview_->InspectionScene();
    for(const auto& b:importBones_){
        auto it=importPoseRotations_.find(b.path);
        if(it!=importPoseRotations_.end())targetPose[scene.names.GetComponent(b.entity)->name]=it->second;
    }
    const int index=rtClip_.GetSelected();
    const std::string label=rtScene_->animations.GetCount()==1?rtFile_:rtClip_.GetItemText(index);
    std::string error;
    if(!bridge::ModelImportCandidateService().AppendMappedClip(*modelImportCandidate_,*rtScene_,index,rtPairs_,rtSourcePose_,targetPose,label,error)){
        ShowStudioMessageBox(error,"Bake retargeted clip");return;
    }
    auto next=std::make_unique<ModelImportPreview>();
    if(!next->Prepare(*modelImportCandidate_->PeekMutableScene(),error,importTransform_,modelImportProjectId_)){
        ShowStudioMessageBox(error,"Retarget preview");return;
    }
    next->AddImportFloor();modelImportPreviewImage_.SetImage({});modelImportPreview_=std::move(next);
    modelImportActions_.resize(modelImportCandidate_->Summary().animations,"Unassigned");
    modelImportActions_.back()=SuggestImportedCharacterAction(label);
    modelImportClip_.ClearItems();modelImportClip_.AddItem("REFERENCE POSE");
    for(const auto& clip:modelImportPreview_->Clips())modelImportClip_.AddItem(clip.name+" ("+std::to_string(clip.end-clip.start)+" s)");
    importPage_=ImportPage::Actions;LayoutImportWorkspace();
    modelImportClip_.SetSelected(int(modelImportClip_.GetItemCount())-1);
    modelImportSummary_.SetText("Retargeted "+label+"\n"+CharacterIdleHint(modelImportActions_));
}
