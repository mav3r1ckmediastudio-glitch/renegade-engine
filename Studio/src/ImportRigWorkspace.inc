// Character import tools operate on the private preview scene.
void StudioRenderPath::CreateImportRigTools()
{
    const auto button = [this](wi::gui::Button& widget, const char* label, auto callback) {
        widget.Create(label);
        widget.OnClick([this,callback](const wi::gui::EventArgs&) {
            auto generation=modelImportGeneration_;
            wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[this,generation,callback](uint64_t){
                if(modelImportPanel_.IsVisible() && generation==modelImportGeneration_)callback();
            });
        });
        modelImportPanel_.AddWidget(&widget);
    };
    button(importFront_, "Front view", [this]{ if (modelImportPreview_) modelImportPreview_->SetView(0, 0); });
    button(importFit_, "Fit model", [this]{ if (modelImportPreview_) modelImportPreview_->FitModel(); });
    button(importZoomIn_, "Zoom in", [this]{ if (modelImportPreview_) modelImportPreview_->Zoom(0.8f); });
    button(importZoomOut_, "Zoom out", [this]{ if (modelImportPreview_) modelImportPreview_->Zoom(1.25f); });
    importRigInfo_.Create("Model measurements");
    modelImportPanel_.AddWidget(&importRigInfo_);
    importBone_.Create("Bone");
    importBone_.OnSelect([this](const wi::gui::EventArgs&) { RefreshPoseFields(); });
    modelImportPanel_.AddWidget(&importBone_);
    poseInfluence_.Create(0.f, 1.f, 1.f, 100, "IK influence");
    modelImportPanel_.AddWidget(&poseInfluence_);
    const char* fields[] = {"Turn X","Turn Y","Turn Z","Target X","Target Y","Target Z","Pole X","Pole Y","Pole Z"};
    for (int i = 0; i < 9; ++i) {
        auto& f = importPoseFields_[i];
        f.Create(fields[i]); f.SetDescription(std::string(fields[i]) + "  ");
        f.SetTooltip(i < 3 ? "Incremental local rotation in degrees" :
            i < 6 ? "End joint target in preview world metres" : "Point towards which the middle joint bends, in world metres");
        f.SetCancelInputEnabled(false);
        modelImportPanel_.AddWidget(&f);
    }
    button(poseRotate_, "Apply rotation", [this]{ ApplyPoseEdit(false); });
    button(poseSolve_, "Solve two-bone IK", [this]{ ApplyPoseEdit(true); });
    button(poseCompare_, "Show original", [this]{ ApplyReferencePose(!comparingPose_); RefreshPoseFields(); });
    button(poseReset_, "Reset selected bone", [this]{
        const int i = importBone_.GetSelected();
        if(i<0 || size_t(i)>=importBones_.size() || !importPoseRotations_.count(importBones_[i].path))return;
        auto old=importPoseRotations_;
        MapHistory([this,old]() mutable {importPoseRotations_.swap(old);if(importPage_==ImportPage::Pose || importPage_==ImportPage::Retarget)ApplyReferencePose(false);});
        mapDirty_=true;importPoseRotations_.erase(importBones_[i].path);
        ApplyReferencePose(false); RefreshPoseFields();
    });
    button(poseSave_, "Save reference pose...", [this]{ ImportPoseFile(true); });
    button(poseLoad_, "Load reference pose...", [this]{ ImportPoseFile(false); });
}

void StudioRenderPath::EnterImportPose()
{
    if (!modelImportPreview_) return;
    auto& scene = modelImportPreview_->InspectionScene();
    std::string error;
    importBone_.ClearItems();
    if (!bridge::CollectPlayerViewBones(scene, importBones_, error)) {
        ShowStudioMessageBox(error, "Character skeleton"); return;
    }
    int initial = 0;
    for (size_t i = 0; i < importBones_.size(); ++i) {
        const auto* name = scene.names.GetComponent(importBones_[i].entity);
        const std::string label = name ? name->name : importBones_[i].label;
        importBone_.AddItem(label);
        if (label == "hand_l" || label == "LeftHand") initial = int(i);
    }
    if(!mapEditingClip_)ApplyReferencePose(false);
    else {comparingPose_=false;modelImportPreview_->Scrub(modelImportPreview_->ClipTime());}
    if (!importBones_.empty()) importBone_.SetSelected(initial);
}

void StudioRenderPath::ApplyReferencePose(bool original)
{
    if (!modelImportPreview_) return;
    auto& scene = modelImportPreview_->InspectionScene();
    modelImportPreview_->SelectClip(-1);
    if (!original) {
        for (const auto& bone : importBones_) {
            const auto found = importPoseRotations_.find(bone.path);
            if (found == importPoseRotations_.end()) continue;
            auto* t = scene.transforms.GetComponent(bone.entity);
            if (t) { t->rotation_local = found->second; t->SetDirty(); }
        }
    }
    modelImportPreview_->RefreshPose();
    comparingPose_ = original;
    poseCompare_.SetText(original ? "Show edited pose" : "Show original");
    poseRotate_.SetEnabled(!original); poseSolve_.SetEnabled(!original);
}

void StudioRenderPath::HideImportPoseMarker()
{
    rigDrag_ = RigHandle::None;
    dragOriginalPose_.clear();
}

void StudioRenderPath::RefreshPoseFields()
{
    const int i = importBone_.GetSelected();
    if (!modelImportPreview_ || i < 0 || size_t(i) >= importBones_.size()) return;
    auto& scene = modelImportPreview_->InspectionScene();
    const auto entity = importBones_[i].entity;
    const auto* t = scene.transforms.GetComponent(entity);
    if (!t) return;
    const XMFLOAT3 target = t->GetPosition();
    auto pole = target;
    const auto* parent = scene.hierarchy.GetComponent(entity);
    if (parent) {
        const auto* middle = scene.transforms.GetComponent(parent->parentID);
        if (middle) pole = middle->GetPosition();
    }
    const float values[] = {0,0,0,target.x,target.y,target.z,pole.x,pole.y,pole.z};
    for (int j = 0; j < 9; ++j) {
        std::ostringstream text;
        text << std::setprecision(5) << values[j];
        importPoseFields_[j].SetValue(text.str());
    }
    importBone_.SetTooltip(importBones_[i].label);
}

void StudioRenderPath::ApplyPoseEdit(bool solve)
{
    const int i = importBone_.GetSelected();
    if (!modelImportPreview_ || comparingPose_ || i < 0 || size_t(i) >= importBones_.size()) return;
    float v[9]{};
    for (int j = solve ? 3 : 0; j < (solve ? 9 : 3); ++j) {
        const auto value = importPoseFields_[j].GetValue();
        char* end = nullptr;
        v[j] = std::strtof(value.c_str(), &end);
        if (value.empty() || *end || !std::isfinite(v[j]) || std::abs(v[j]) > 100000.f) {
            ShowStudioMessageBox("Enter a finite pose value within +/-100000.", "Reference pose"); return;
        }
    }
    auto& scene = modelImportPreview_->InspectionScene();
    auto previousPose=importPoseRotations_;
    const auto& tip = importBones_[i];
    if (!solve) {
        auto& t = *scene.transforms.GetComponent(tip.entity);
        const auto increment = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(v[0]), XMConvertToRadians(v[1]), XMConvertToRadians(v[2]));
        XMStoreFloat4(&t.rotation_local, XMQuaternionNormalize(XMQuaternionMultiply(increment, XMLoadFloat4(&t.rotation_local))));
        importPoseRotations_[tip.path] = t.rotation_local;
    } else {
        const auto parent = [&scene](wi::ecs::Entity e) {
            auto* h = scene.hierarchy.GetComponent(e); return h ? h->parentID : wi::ecs::INVALID_ENTITY;
        };
        const auto mid = parent(tip.entity), root = parent(mid);
        const auto bone = [this](wi::ecs::Entity e) { return std::find_if(importBones_.begin(), importBones_.end(),
            [e](const auto& b){ return b.entity == e; }); };
        if (bone(mid) == importBones_.end() || bone(root) == importBones_.end()) {
            ShowStudioMessageBox("Select an end joint with two parent bones, such as a hand or foot.", "Two-bone IK"); return;
        }
        const auto point = [&scene](wi::ecs::Entity e) {
            const auto p = scene.transforms.GetComponent(e)->GetPosition(); return XMLoadFloat3(&p);
        };
        const auto length = [](FXMVECTOR d){ return XMVectorGetX(XMVector3Length(d)); };
        const auto a = point(root), b = point(mid), c = point(tip.entity);
        const auto goal = XMVectorSet(v[3],v[4],v[5],0), pole = XMVectorSet(v[6],v[7],v[8],0);
        const float l1 = length(b-a), l2 = length(c-b), distance = length(goal-a);
        if (std::min({l1,l2,distance}) < 1e-6f) {
            if (rigDrag_ == RigHandle::None) ShowStudioMessageBox("The target and both limb segments must have a nonzero length.", "Two-bone IK"); return;
        }
        const auto axis = XMVector3Normalize(goal-a);
        const auto bend = pole-a-axis*XMVectorGetX(XMVector3Dot(pole-a,axis));
        if (length(bend) < 1e-6f) {
            if (rigDrag_ == RigHandle::None) ShowStudioMessageBox("Move the pole off the line between the chain root and target.", "Two-bone IK"); return;
        }
        const float d = std::clamp(distance, std::max(1e-6f,std::abs(l1-l2)), l1+l2);
        const float along = (l1*l1-l2*l2+d*d)/(2*d);
        const auto elbow = a+axis*along+XMVector3Normalize(bend)*std::sqrt(std::max(0.f,l1*l1-along*along));
        const auto aim = [&](wi::ecs::Entity joint, wi::ecs::Entity child, FXMVECTOR target) {
            const auto* pt = scene.transforms.GetComponent(parent(joint));
            const auto inverse = pt ? XMMatrixInverse(nullptr, XMLoadFloat4x4(&pt->world)) : XMMatrixIdentity();
            const auto from = XMVector3Normalize(XMVector3TransformNormal(point(child)-point(joint),inverse));
            const auto to = XMVector3Normalize(XMVector3TransformNormal(target-point(joint),inverse));
            const float dot = std::clamp(XMVectorGetX(XMVector3Dot(from,to)),-1.f,1.f);
            if (dot < 0.999999f) {
                auto n = XMVector3Cross(from,to);
                if (length(n)<1e-6f) n=XMVector3Cross(from,std::abs(XMVectorGetX(from))<0.8f ? XMVectorSet(1,0,0,0) : XMVectorSet(0,1,0,0));
                auto& t = *scene.transforms.GetComponent(joint);
                t.Rotate(XMQuaternionRotationAxis(n,std::acos(dot)));
                importPoseRotations_[bone(joint)->path] = t.rotation_local;
                modelImportPreview_->RefreshPose();
            }
        };
        const auto originalRoot = scene.transforms.GetComponent(root)->rotation_local;
        const auto originalMiddle = scene.transforms.GetComponent(mid)->rotation_local;
        aim(root, mid, elbow);
        aim(mid, tip.entity, a+axis*d);
        const auto mix = [&](wi::ecs::Entity joint, const XMFLOAT4& before) {
            const auto& after = scene.transforms.GetComponent(joint)->rotation_local;
            XMStoreFloat4(&importPoseRotations_[bone(joint)->path],
                XMQuaternionSlerp(XMLoadFloat4(&before), XMLoadFloat4(&after), poseInfluence_.GetValue()));
        };
        mix(root, originalRoot);
        mix(mid, originalMiddle);
    }
    if(mapEditingClip_){
        const int take=rtClip_.GetSelected();auto& keys=mappedTakes_.at(take).keys;auto old=keys;
        for(const auto& b:importBones_){
            auto edited=importPoseRotations_.find(b.path);
            if(edited==importPoseRotations_.end())continue;
            auto prior=previousPose.find(b.path);
            if(prior!=previousPose.end() && std::memcmp(&prior->second,&edited->second,sizeof(XMFLOAT4))==0)continue;
            auto* name=scene.names.GetComponent(b.entity);
            if(name)keys.push_back({name->name,modelImportPreview_->ClipTime(),edited->second,clipEditScope_.GetSelected()==1});
        }
        importPoseRotations_.swap(previousPose);
        if(!WriteMappedTake(false)){keys.swap(old);return;}
        MapHistory([this,take,old]() mutable {mappedTakes_.at(take).keys.swap(old);});
        if(size_t(i)<importBones_.size())importBone_.SetSelected(i);
        LayoutImportWorkspace();
    }else{
        MapHistory([this,previousPose]() mutable {importPoseRotations_.swap(previousPose);if(importPage_==ImportPage::Pose || importPage_==ImportPage::Retarget)ApplyReferencePose(false);});
        mapDirty_=true;ApplyReferencePose(false);
    }
    RefreshPoseFields();
}

void StudioRenderPath::ImportPoseFile(bool save)
{
    if (importBones_.empty()) return;
    const auto generation = modelImportGeneration_;
    wi::helper::FileDialogParams params;
    params.type = save ? wi::helper::FileDialogParams::SAVE : wi::helper::FileDialogParams::OPEN;
    params.description = "Renegade reference pose";
    params.extensions = {"rpose"};
    wi::helper::FileDialog(params, [this, save, generation](const std::string& path) {
        wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,
            [this, save, generation, path](std::uint64_t) {
                if (path.empty() || generation != modelImportGeneration_ || !modelImportPreview_ ||
                    !modelImportPanel_.IsVisible() || importPage_ != ImportPage::Pose || importBones_.empty()) return;
                if (save) {
                    std::ostringstream out;
                    out << "RenegadeReferencePose 1 " << importBones_.size() << '\n' << std::setprecision(9);
                    for (const auto& b : importBones_) {
                        const auto it = importPoseRotations_.find(b.path);
                        out << std::quoted(b.path) << ' ' << (it != importPoseRotations_.end());
                        if (it != importPoseRotations_.end()) {
                            const auto& q = it->second;
                            out << ' ' << q.x << ' ' << q.y << ' ' << q.z << ' ' << q.w;
                        }
                        out << '\n';
                    }
                    const auto body = out.str();
                    bridge::ProjectDocumentWrite document;
                    document.destinationPath = path;
                    document.content.assign(body.begin(),body.end());
                    const auto result = bridge::ProjectDocumentTransaction().Execute({document});
                    if (!result.success) { ShowStudioMessageBox(result.message,"Save reference pose"); return; }
                } else {
                    std::ifstream file(fs::u8path(path), std::ios::binary | std::ios::ate);
                    if (!file || file.tellg() > 8 * 1024 * 1024) {
                        ShowStudioMessageBox("Cannot read the selected pose file.","Load reference pose"); return;
                    }
                    file.seekg(0);
                    std::string header;
                    int version = 0;
                    size_t count = 0;
                    bool valid = (file >> header >> version >> count) && header == "RenegadeReferencePose" &&
                        version == 1 && count == importBones_.size();
                    decltype(importPoseRotations_) loaded;
                    for (const auto& b : importBones_) {
                        std::string pathKey;
                        int changed = 0;
                        if (!valid || !(file >> std::quoted(pathKey) >> changed) || pathKey != b.path ||
                            (changed != 0 && changed != 1)) { valid = false; break; }
                        if (changed) {
                            XMFLOAT4 q;
                            if (!(file >> q.x >> q.y >> q.z >> q.w)) { valid = false; break; }
                            const auto rot = XMLoadFloat4(&q);
                            const float length = XMVectorGetX(XMQuaternionLength(rot));
                            if (!std::isfinite(length) || std::abs(length - 1.f) > 0.001f) { valid = false; break; }
                            XMStoreFloat4(&loaded[b.path], XMQuaternionNormalize(rot));
                        }
                    }
                    if (!valid || (file >> std::ws).peek() != std::char_traits<char>::eof()) {
                        ShowStudioMessageBox("The pose is damaged or belongs to a different bone hierarchy.","Load reference pose"); return;
                    }
                    importPoseRotations_ = std::move(loaded);
                    ApplyReferencePose(false);
                    RefreshPoseFields();
                }
                modelImportSummary_.SetText((save ? "Saved reference pose   /   " : "Loaded reference pose   /   ") +
                    fs::u8path(path).filename().generic_u8string());
            });
    });
}
