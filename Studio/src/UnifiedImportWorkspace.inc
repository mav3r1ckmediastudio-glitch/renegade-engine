// Member implementations for the native Model Importer.
void StudioRenderPath::CreateImportWorkspace()
{
    importSide_.Create("Side view");
    importSide_.OnClick([this](const wi::gui::EventArgs&) {
        if (modelImportPreview_) modelImportPreview_->SetView(XM_PIDIV2, 0.f);
    });
    modelImportPanel_.AddWidget(&importSide_);
    for (int i = 0; i < 2; ++i) {
        auto& step = importStep_[i];
        step.Create(i ? "Next frame" : "Prev frame");
        step.SetTooltip("Step the preview by 1/30 second");
        step.OnClick([this, i](const wi::gui::EventArgs&) {
            if (modelImportPreview_ && modelImportPreview_->HasClip())
                modelImportPreview_->Scrub(modelImportPreview_->ClipTime() + (i ? 1.f : -1.f) / 30.f);
        });
        modelImportPanel_.AddWidget(&step);
    }
    const char* workflows[] = {"Static Model", "Animated Model", "Character", "Player Arms / Weapon"};
    for (int i = 0; i < 4; ++i) {
        auto& button = importWorkflowButtons_[i];
        button.Create(workflows[i]);
        button.OnClick([this, i](const wi::gui::EventArgs&) {
            SelectImportWorkflow(static_cast<ImportWorkflow>(i+1));
        });
        modelImportPanel_.AddWidget(&button);
    }
    CreateImportRigTools();
    CreateRetargetWorkspace();
    const char* pages[] = {"Model", "Materials", "Actions", "Advanced", "Edit Pose / IK", "Retargeting"};
    for (int i = 0; i < 6; ++i) {
        auto& button = importPageButtons_[i];
        button.Create(pages[i]);
        button.OnClick([this, i](const wi::gui::EventArgs&) {
            if ((importPage_ == ImportPage::Pose || importPage_ == ImportPage::Retarget) && i != int(importPage_)) {
                HideImportPoseMarker();
                modelImportClip_.SetSelected(modelImportClip_.GetSelected());
            }
            importPage_ = static_cast<ImportPage>(i);
            if (importPage_ == ImportPage::Pose || importPage_ == ImportPage::Retarget) EnterImportPose();
            if (importPage_ == ImportPage::Retarget) { SelectRetargetBone(); UpdateRetargetStatus(); }
            LayoutImportWorkspace();
        });
        modelImportPanel_.AddWidget(&button);
    }
    const char* labels[] = {"Rotate X", "Rotate Y", "Rotate Z", "Move X", "Move Y", "Move Z", "Scale"};
    for (int i = 0; i < 7; ++i) {
        auto& field = importTransformFields_[i];
        field.Create(labels[i]);
        field.SetDescription(std::string(labels[i]) + "  ");
        field.SetCancelInputEnabled(false);
        field.SetTooltip(i < 3 ? "Degrees" : i < 6 ? "Metres" : "Uniform scale");
        modelImportPanel_.AddWidget(&field);
    }
    importApplyTransform_.Create("Apply Transform");
    importApplyTransform_.OnClick([this](const wi::gui::EventArgs&) { ApplyImportTransform(); });
    modelImportPanel_.AddWidget(&importApplyTransform_);
}

void StudioRenderPath::ResizeImportWorkspace(float width, float height)
{
    constexpr float pad = 16, gap = 8, inspectorWidth = 430;
    modelImportPanel_.SetPos({0, 0});
    modelImportPanel_.SetSize({width, height});
    const float right = std::max(252.f, width - inspectorWidth - 32);
    const float viewWidth = right - 2 * pad;
    const float footer = std::max(430.f, height - 147);
    const float viewHeight = footer - 225;
    const float third = (viewWidth - 2 * gap) / 3;
    const auto place = [](auto& widget, float x, float y, float w, float h) {
        widget.SetPos({x, y});
        widget.SetSize({w, h});
    };
    const float workflowWidth = (width - 2 * pad - 3 * gap) / 4;
    for (int i = 0; i < 4; ++i)
        place(importWorkflowButtons_[i], pad + (workflowWidth + gap) * i, 8, workflowWidth, 30);
    const float tabWidth = (inspectorWidth - 2 * gap) / 3;
    for (int i = 0; i < 3; ++i)
        place(importPageButtons_[i], right + (tabWidth + gap) * i, 52, tabWidth, 28);
    place(importPageButtons_[3], right, 86, tabWidth, 28);
    place(importPageButtons_[4], right + tabWidth + gap, 86, tabWidth, 28);
    place(importPageButtons_[5],right+2*(tabWidth+gap),86,tabWidth,28);
    place(rtOpen_,right,128,inspectorWidth,28);
    place(rtClip_,right+96,165,inspectorWidth-96,26);
    place(rtSource_,right+96,201,inspectorWidth-96,26);
    place(rtTarget_,right+96,237,inspectorWidth-96,26);
    place(rtSourceTurn_,right,313,(inspectorWidth-gap)/2,28);
    place(rtTargetTurn_,right+(inspectorWidth+gap)/2,313,(inspectorWidth-gap)/2,28);
    place(rtSummary_,right,352,inspectorWidth,86);
    place(rtBake_,right,446,inspectorWidth,30);
    for(int i=0;i<3;++i)place(rtRigDegrees_[i],right+74+140*i,491,65,27);
    place(rtRigApply_,right,529,(inspectorWidth-gap)/2,28);
    place(rtRigReset_,right+(inspectorWidth+gap)/2,529,(inspectorWidth-gap)/2,28);
    place(importFront_, right, 133, 210, 28);
    place(importFit_, right + 219, 133, 210, 28);
    place(importZoomIn_, right, 170, 210, 28);
    place(importZoomOut_, right + 219, 170, 210, 28);
    place(importRigInfo_, right, 218, inspectorWidth, 90);
    place(importBone_, right + 64, 127, 366, 28);
    for (int i = 0; i < 9; ++i)
        place(importPoseFields_[i], right + 74 + (i % 3) * 140, (importPage_==ImportPage::Retarget ? 276 : 167) + (i / 3) * 39, 65, 27);
    place(poseInfluence_, right + 98, 392, 272, 23);
    place(poseRotate_, right, 286, 211, 28);
    place(poseSolve_, right + 219, 286, 211, 28);
    place(poseCompare_, right, 322, 211, 28);
    place(poseReset_, right + 219, 322, 211, 28);
    place(poseSave_, right, 358, 211, 28);
    place(poseLoad_, right + 219, 358, 211, 28);
    for (int i = 0; i < 7; ++i)
        place(importTransformFields_[i], right + 73 + 141 * (i % 3), 140 + 48 * (i / 3), 63, 28);
    place(importApplyTransform_, right + 183, 236, 247, 28);
    place(modelImportPreviewImage_, pad, 52, viewWidth, viewHeight);
    const float presetWidth = std::min(105.f, viewWidth * 0.14f);
    const float hintWidth = viewWidth - 3 * (presetWidth + gap);
    place(importNavigationHint_, pad, 60 + viewHeight, hintWidth, 28);
    place(modelImportRotateLeft_, pad + hintWidth + gap, 60 + viewHeight, presetWidth, 28);
    place(modelImportRotateRight_, pad + hintWidth + 2 * gap + presetWidth, 60 + viewHeight, presetWidth, 28);
    place(importSide_, pad + viewWidth - presetWidth, 60 + viewHeight, presetWidth, 28);
    place(modelImportSummary_, pad, 174 + viewHeight, viewWidth, 42);
    place(modelImportMaterial_, right + 92, 134, 338, 28);
    place(modelImportMapSlot_, right + 92, 173, 338, 28);
    place(modelImportAssignMap_, right, 213, inspectorWidth, 32);
    place(modelImportMapInfo_, right, 257, inspectorWidth, 99);
    place(modelImportWarnings_, right, 364, inspectorWidth, std::min(180.f, footer - 372));
    place(modelImportClip_, right + 86, 139, 344, 30);
    const float transport = (viewWidth - 3 * gap) / 4;
    place(modelImportPlay_, pad, 96 + viewHeight, transport, 28);
    place(modelImportRestart_, pad + transport + gap, 96 + viewHeight, transport, 28);
    place(importStep_[0], pad + 2 * (transport + gap), 96 + viewHeight, transport, 28);
    place(importStep_[1], pad + 3 * (transport + gap), 96 + viewHeight, transport, 28);
    place(modelImportTime_, pad + 150, 137 + viewHeight, std::max(30.f, viewWidth - 225), 23);
    place(modelImportSpeed_, right + 155, 273, 210, 24);
    place(modelImportAction_, right + 138, 318, 292, 30);
    place(modelImportAddAnimation_, right, 367, inspectorWidth, 32);
    place(modelImportRole_, right + 93, 295, 337, 28);
    place(modelImportName_, pad + 92, footer, viewWidth - 92, 27);
    place(modelImportFolder_, pad + 92, footer + 36, viewWidth - 92, 27);
    place(modelImportFolderChoices_, right + 86, footer, 344, 27);
    place(modelImportTags_, right + 86, footer + 36, 344, 27);
    place(modelImportCancel_, right, footer + 76, 207, 32);
    place(modelImportCommit_, right + 215, footer + 76, 215, 32);
}
bool StudioRenderPath::NavigateImportViewport()
{
    if (rigDrag_ != RigHandle::None) return false;
    const auto pointer = wi::input::GetPointer();
    const auto pos = modelImportPreviewImage_.GetPos();
    const auto size = modelImportPreviewImage_.GetSize();
    const bool inside = pointer.x >= pos.x && pointer.x < pos.x + size.x &&
        pointer.y >= pos.y && pointer.y < pos.y + size.y;
    const float speed = wi::input::Down(wi::input::KEYBOARD_BUTTON_LSHIFT) ||
        wi::input::Down(wi::input::KEYBOARD_BUTTON_RSHIFT) ? 0.2f : 1.f;

    if (inside && pointer.z != 0)
        modelImportPreview_->Zoom(std::exp(-std::clamp(pointer.z, -10.f, 10.f) * 0.14f * speed));

    if (importCameraDrag_ == ImportCameraDrag::None)
    {
        if (!inside) return false;
        if (wi::input::Press(wi::input::MOUSE_BUTTON_RIGHT))
            importCameraDrag_ = ImportCameraDrag::Orbit;
        else if (wi::input::Press(wi::input::MOUSE_BUTTON_MIDDLE))
            importCameraDrag_ = ImportCameraDrag::Pan;
        else return false;
        importCameraPointer_ = {pointer.x, pointer.y};
    }
    const bool orbit = importCameraDrag_ == ImportCameraDrag::Orbit;
    if (!wi::input::Down(orbit ? wi::input::MOUSE_BUTTON_RIGHT : wi::input::MOUSE_BUTTON_MIDDLE))
    {
        importCameraDrag_ = ImportCameraDrag::None;
        return false;
    }
    const float dx = (pointer.x - importCameraPointer_.x) * speed;
    const float dy = (pointer.y - importCameraPointer_.y) * speed;
    importCameraPointer_ = {pointer.x, pointer.y};
    if (dx != 0 || dy != 0)
    {
        if (orbit) modelImportPreview_->Orbit(dx * 0.006f, dy * 0.006f);
        else modelImportPreview_->Pan(dx / size.y, dy / size.y);
    }
    return true;
}

void StudioRenderPath::SelectImportWorkflow(ImportWorkflow workflow)
{
    if (!modelImportCandidate_) return;
    if ((importPage_ == ImportPage::Pose || importPage_ == ImportPage::Retarget) && modelImportPreview_) {
        HideImportPoseMarker();
        modelImportClip_.SetSelected(modelImportClip_.GetSelected());
    }
    importWorkflow_ = workflow;
    importPage_ = ImportPage::Model;
    if (!modelImportFromProjectile_) {
        const char* folder = workflow == ImportWorkflow::Character ? "Content/Characters" :
            workflow == ImportWorkflow::Player ? "Content/Player/Arms" : "Content/Models";
        modelImportFolder_.SetValue(folder);
        for (int i=0; i<int(modelImportFolderChoices_.GetItemCount()); ++i)
            if (modelImportFolderChoices_.GetItemText(i)==folder) modelImportFolderChoices_.SetSelectedWithoutCallback(i);
    }
    if (workflow == ImportWorkflow::Player) modelImportRole_.SetSelected(1);
    LayoutImportWorkspace();
}
void StudioRenderPath::LayoutImportWorkspace()
{
    const bool chosen = importWorkflow_ != ImportWorkflow::None;
    const bool character = importWorkflow_ == ImportWorkflow::Character;
    const bool model = chosen && importPage_ == ImportPage::Model;
    const bool materials = chosen && importPage_ == ImportPage::Materials;
    const bool actions = chosen && importPage_ == ImportPage::Actions;
    const auto& evidence = modelImportCandidate_->Evidence();
    for (int i=0;i<4;++i) {
        auto& b=importWorkflowButtons_[i];
        b.SetColor(int(importWorkflow_)==i+1 ? theme::Accent : theme::Control, wi::gui::IDLE);
        b.SetEnabled(i==0 ? !evidence.HasRigOrAnimationPayload() : i==2 ? evidence.skinnedMeshes && evidence.armatureBones : true);
    }
    for (int i=0;i<6;++i) {
        importPageButtons_[i].SetVisible(chosen && (i!=2 || character || importWorkflow_==ImportWorkflow::Animated) && (i<4 || character));
        importPageButtons_[i].SetColor(int(importPage_)==i ? theme::Accent : theme::Control, wi::gui::IDLE);
    }
    const bool advanced = chosen && importPage_ == ImportPage::Advanced;
    const bool pose = character && importPage_ == ImportPage::Pose;
    for (auto* button : {&importFront_, &importFit_, &importZoomIn_, &importZoomOut_}) button->SetVisible(advanced);
    importRigInfo_.SetVisible(advanced);
    if (advanced && modelImportPreview_) {
        const auto size = modelImportPreview_->ModelSize();
        std::ostringstream info;
        info << std::fixed << std::setprecision(3) << "Reference size (m)   " << size.x << " x " << size.y << " x " << size.z
             << "\nArmature bones   " << evidence.armatureBones << "\nFloor height   0.000 m";
        importRigInfo_.SetText(info.str());
    }
    importBone_.SetVisible(pose);
    poseInfluence_.SetVisible(pose);
    const bool rt = character && importPage_ == ImportPage::Retarget;
    for(auto& field:rtRigDegrees_)field.SetVisible(rt && rtScene_.IsValid());
    rtOpen_.SetVisible(rt); rtSummary_.SetVisible(rt);
    for (auto* b : {&rtSourceTurn_,&rtTargetTurn_,&rtRigApply_,&rtRigReset_,&rtBake_}) b->SetVisible(rt && rtScene_.IsValid());
    for (auto* c : {&rtSource_,&rtTarget_,&rtClip_}) c->SetVisible(rt && rtScene_.IsValid());
    const float right = std::max(252.f, modelImportPanel_.GetSize().x-462.f);
    for (int j=0;j<9;++j) {
        importPoseFields_[j].SetVisible(pose || (rt && rtScene_.IsValid() && j<3));
        if(j<3) importPoseFields_[j].SetPos({right+74+140.f*j,rt?276.f:167.f});
    }
    for (auto* button : {&poseRotate_, &poseSolve_, &poseCompare_, &poseReset_, &poseSave_, &poseLoad_}) button->SetVisible(pose);
    for (auto& field : importTransformFields_) field.SetVisible(model);
    importApplyTransform_.SetVisible(model);
    modelImportRole_.SetVisible(model && importWorkflow_==ImportWorkflow::Player);
    modelImportMaterial_.SetVisible(materials); modelImportMapSlot_.SetVisible(materials);
    modelImportAssignMap_.SetVisible(materials); modelImportMapInfo_.SetVisible(materials);
    modelImportWarnings_.SetVisible(materials);
    modelImportClip_.SetVisible(actions); const bool timeline = !rt && !pose && (character || importWorkflow_ == ImportWorkflow::Animated);
    modelImportPlay_.SetVisible(timeline);
    for (auto& step : importStep_) step.SetVisible(timeline);
    modelImportRestart_.SetVisible(timeline); modelImportTime_.SetVisible(timeline);
    modelImportSpeed_.SetVisible(actions); modelImportAction_.SetVisible(actions && character);
    modelImportAddAnimation_.SetVisible(actions && character);
    modelImportName_.SetVisible(chosen); modelImportTags_.SetVisible(chosen);
    modelImportFolder_.SetVisible(chosen); modelImportFolderChoices_.SetVisible(chosen);
    modelImportCommit_.SetVisible(chosen && !pose && !rt);
    modelImportCommit_.SetColor(theme::Accent, wi::gui::IDLE);
}
void StudioRenderPath::ApplyImportTransform()
{
    if (!modelImportCandidate_ || !modelImportPreview_) return;
    float values[7];
    for (int i=0;i<7;++i) {
        const auto input=importTransformFields_[i].GetValue();
        char* end=nullptr;
        values[i]=std::strtof(input.c_str(), &end);
        if (input.empty() || *end || !std::isfinite(values[i]) || (i==6 && values[i]<=0)) {
            ShowStudioMessageBox("Enter finite numbers and a positive scale.", "Import Transform");
            return;
        }
    }
    bridge::CreatorModelTransformRecipe correction;
    correction.authored=true;
    correction.rotationXDegrees=values[0]; correction.rotationYDegrees=values[1]; correction.rotationZDegrees=values[2];
    correction.positionX=values[3]; correction.positionY=values[4]; correction.positionZ=values[5];
    correction.scaleX=correction.scaleY=correction.scaleZ=values[6];
    auto preview=std::make_unique<ModelImportPreview>();
    std::string error;
    if (!preview->Prepare(*modelImportCandidate_->PeekMutableScene(), error, correction, modelImportProjectId_)) {
        ShowStudioMessageBox(error, "Import Transform"); return;
    }
    const int clip=modelImportClip_.GetSelected();
    const float time=modelImportPreview_->ClipTime();
    modelImportPreviewImage_.SetImage({});
    preview->AddImportFloor();
    modelImportPreview_=std::move(preview);
    importTransform_=correction;
    modelImportClip_.SetSelected(clip);
    if (clip>0) modelImportPreview_->Scrub(time);
}
