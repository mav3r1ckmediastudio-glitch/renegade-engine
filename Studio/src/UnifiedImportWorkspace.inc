namespace {
constexpr float actionColumnEdges[]={0.f,.25f,.42f,.78f,.85f,.92f,1.f};
}
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
                if(mapDirty_ && mappedTakes_.count(rtClip_.GetSelected()) && !WriteMappedTake(false))return;
                bool keepClip=mapEditingClip_;mapEditingClip_=false;HideImportPoseMarker();
                if(!keepClip)
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
    constexpr float pad = 16, gap = 8;
    const bool editingEntries=importPage_==ImportPage::Actions && importWorkflow_==ImportWorkflow::Character;
    const float inspectorWidth=editingEntries ? std::clamp(width*.41f,430.f,800.f) : importPage_==ImportPage::Materials ? std::clamp(width*.32f,430.f,605.f) : 430.f;
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
    float half=(inspectorWidth-gap)/2;
    place(mapSearch_,right+38,198,half-38,24);
    place(mapPrev_,right+half+gap,198,half/2-2,24);
    place(mapNext_,right+half*1.5f+gap,198,half/2,24);
    place(mapSourceHeader_,right,226,half,22);
    place(mapTargetHeader_,right+half+gap,226,half,22);
    for(int row=0;row<MapRows;++row){
        place(mapSourceRows_[row],right,251+row*28,half-22,25);
        place(mapTargetRows_[row],right+half+gap,251+row*28,half,25);
    }
    place(mapSourceLabel_,right,394,half,24);
    place(mapTargetLabel_,right+half+gap,394,half,24);
    place(reviewSources_,right,394,half-22,24);
    place(reviewShowAll_,right+half+gap,394,half,24);
    place(mapReviewExtras_,right,424,half,25);
    place(reviewAssign_,right+half+gap,424,half,25);
    place(mapHoldBranch_,right+half+gap,424,half,25);
    place(rtSourceTurn_,right,492,half,26);place(rtTargetTurn_,right+half+gap,492,half,26);
    place(rtSummary_,right,524,inspectorWidth,73);place(rtBake_,right,602,inspectorWidth,29);
    for(int i=0;i<3;++i)place(rtRigDegrees_[i],right+74+140*i,642,65,25);
    place(rtRigApply_,right,673,half,26);place(rtRigReset_,right+half+gap,673,half,26);
    place(mapUndo_,right,714,half,26);place(mapRedo_,right+half+gap,714,half,26);
    place(clipPose_,right,535,inspectorWidth,28);place(clipEditScope_,right+92,457,inspectorWidth-120,27);
    place(importFront_, right, 133, 210, 28);
    place(importFit_, right + 219, 133, 210, 28);
    place(importZoomIn_, right, 170, 210, 28);
    place(importZoomOut_, right + 219, 170, 210, 28);
    place(importRigInfo_, right, 218, inspectorWidth, 90);
    place(importBone_, right + 64, 127, 366, 28);
    for (int i = 0; i < 9; ++i)
        place(importPoseFields_[i], right + 74 + (i % 3) * 140, (importPage_==ImportPage::Retarget ? 459 : 167) + (i / 3) * 39, 65, 27);
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
    if(importMaterials_) importMaterials_->layout(right,inspectorWidth,footer);
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
    place(sourceHeading_,right,125,half,26);
    place(referencePose_,right+half+gap,125,half,26);
    const float listHeight=std::clamp(footer*.21f,70.f,174.f);
    place(actionSources_,right,156,inspectorWidth,listHeight);
    float y=162+listHeight;
    place(modelImportAddAnimation_,right,y,inspectorWidth,28);
    y+=36;
    place(entriesHeading_,right,y,half,28);
    place(addEntry_,right+half+gap,y,half*.57f,28);
    place(removeEntry_,right+half+gap+half*.59f,y,half*.41f,28);
    y+=34;
    const float columns=inspectorWidth-24;
    for(int c=0;c<6;++c) place(entryHeaders_[c],right+5+columns*actionColumnEdges[c],y,
        columns*(actionColumnEdges[c+1]-actionColumnEdges[c])-3,23);
    y+=25;
    place(actionTable_,right,y,inspectorWidth,std::max(37.f,footer-88-y));
    for(size_t row=0;row<actionWidgets_.size();++row) PositionActionRow(*actionWidgets_[row],int(row));
    place(entryUndo_,right,footer-78,half,27);place(entryRedo_,right+half+gap,footer-78,half,27);
    place(clipPose_,pad+viewWidth-172,footer-42,172,26);
    if(editingEntries)place(modelImportSummary_,pad,footer-42,viewWidth-180,26);
    place(entryHint_,right,footer+65,inspectorWidth,19);
    place(modelImportRole_, right + 93, 295, 337, 28);
    place(modelImportName_, pad + 92, footer, viewWidth - 92, 27);
    place(modelImportFolder_, pad + 92, footer + 36, viewWidth - 92, 27);
    place(modelImportFolderChoices_, right + 86, footer, inspectorWidth-86, 27);
    place(modelImportTags_, right + 86, footer + 36, inspectorWidth-86, 27);
    place(modelImportCancel_, right, footer + (editingEntries?89:76), half, 32);
    place(modelImportCommit_, right + half+gap, footer + (editingEntries?89:76), half, 32);
}
bool StudioRenderPath::NavigateImportViewport()
{
    if(actionTable_.IsVisible() && wi::input::Press(wi::input::MOUSE_BUTTON_LEFT)) {
        const auto p=wi::input::GetPointer();
        const auto origin=actionTable_.GetPos(), size=actionTable_.GetSize();
        const bool inside=p.x>=origin.x && p.x<origin.x+size.x-18 && p.y>=origin.y && p.y<origin.y+size.y;
        const bool picking=std::any_of(actionWidgets_.begin(),actionWidgets_.end(),[](const auto& w){
            return w->source.GetState()==wi::gui::ACTIVE || w->type.GetState()==wi::gui::ACTIVE;
        });
        if(inside && !picking) for(int i=0;i<int(actionEntries_.rows.size());++i) {
            const auto& cell=actionWidgets_[i]->name;
            if(p.y>=cell.GetPos().y && p.y<cell.GetPos().y+cell.GetSize().y) {
                SelectActionEntry(i); break;
            }
        }
    }
    if (ScrollRetargetTable()) return true;
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
        if(mapDirty_ && mappedTakes_.count(rtClip_.GetSelected()) && !WriteMappedTake(false))return;
        mapEditingClip_=false;HideImportPoseMarker();
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
    ResizeImportWorkspace(modelImportPanel_.GetSize().x,modelImportPanel_.GetSize().y);
    const bool chosen = importWorkflow_ != ImportWorkflow::None;
    const bool character = importWorkflow_ == ImportWorkflow::Character;
    const bool model = chosen && importPage_ == ImportPage::Model;
    const bool materials = chosen && importPage_ == ImportPage::Materials;
    const bool actions = chosen && importPage_ == ImportPage::Actions;
    const auto& evidence = modelImportCandidate_->Evidence();
    for (int i=0;i<4;++i) {
        auto& b=importWorkflowButtons_[i];
        b.SetColor(int(importWorkflow_)==i+1 ? theme::Selected : theme::Control, wi::gui::IDLE);
        b.SetShadowRadius(1);b.SetShadowColor(int(importWorkflow_)==i+1 ? theme::Accent : theme::Border);
        b.SetEnabled(i==0 ? !evidence.HasRigOrAnimationPayload() : i==2 ? evidence.skinnedMeshes && evidence.armatureBones : true);
    }
    for (int i=0;i<6;++i) {
        importPageButtons_[i].SetVisible(chosen && (i!=2 || character || importWorkflow_==ImportWorkflow::Animated) && (i<4 || character));
        importPageButtons_[i].SetColor(int(importPage_)==i ? theme::Selected : theme::Control, wi::gui::IDLE);
        importPageButtons_[i].SetShadowRadius(1);importPageButtons_[i].SetShadowColor(int(importPage_)==i ? theme::Accent : theme::Border);
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
    if(mapEditingClip_ && pose)for(auto* b:{&poseCompare_,&poseReset_,&poseSave_,&poseLoad_})b->SetVisible(false);
    const bool rt = character && importPage_ == ImportPage::Retarget;
    for(auto& field:rtRigDegrees_)field.SetVisible(rt && rtScene_.IsValid());
    rtOpen_.SetVisible(rt); rtSummary_.SetVisible(rt);
    for (auto* b : {&rtSourceTurn_,&rtTargetTurn_,&rtRigApply_,&rtRigReset_,&rtBake_}) b->SetVisible(rt && rtScene_.IsValid());
    rtSource_.SetVisible(false);rtTarget_.SetVisible(false);
    bool ready=rt && rtScene_.IsValid();
    rtClip_.SetVisible(ready);
    mapSearch_.SetVisible(ready);mapPrev_.SetVisible(ready);mapNext_.SetVisible(ready);
    mapSourceHeader_.SetVisible(ready);mapTargetHeader_.SetVisible(ready);
    mapTargetLabel_.SetVisible(ready && !reviewingPairs_);mapSourceLabel_.SetVisible(ready && !reviewingPairs_);
    reviewSources_.SetVisible(ready && reviewingPairs_);reviewShowAll_.SetVisible(ready && reviewingPairs_);
    for(int j=0;j<MapRows;++j){mapSourceRows_[j].SetVisible(ready && mapFirst_+j<int(mapFiltered_.size()));mapTargetRows_[j].SetVisible(mapSourceRows_[j].IsVisible());}
    mapHoldBranch_.SetVisible(ready && !reviewingPairs_);
    reviewAssign_.SetVisible(ready && reviewingPairs_);
    mapReviewExtras_.SetVisible(ready && std::find(rtPairs_.begin(),rtPairs_.end(),-2)!=rtPairs_.end());
    mapUndo_.SetVisible(character && (rt||pose));mapRedo_.SetVisible(mapUndo_.IsVisible());
    mapUndo_.SetEnabled(mapCursor_>0);mapRedo_.SetEnabled(mapCursor_<mapHistory_.size());
    clipPose_.SetVisible(actions && std::any_of(mappedTakes_.begin(),mappedTakes_.end(),[this](const auto& t){return t.second.clip>=0 && t.second.clip+1==modelImportClip_.GetSelected();}));
    clipEditScope_.SetVisible(pose && mapEditingClip_);

    const float right = std::max(252.f, modelImportPanel_.GetSize().x-462.f);
    for (int j=0;j<9;++j) {
        importPoseFields_[j].SetVisible(pose || (rt && rtScene_.IsValid() && j<3));
        if(j<3) importPoseFields_[j].SetPos({right+74+140.f*j,rt?459.f:167.f});
    }
    for (auto* button : {&poseRotate_, &poseSolve_, &poseCompare_, &poseReset_, &poseSave_, &poseLoad_}) button->SetVisible(pose);
    if(pose && mapEditingClip_)for(auto* b:{&poseCompare_,&poseReset_,&poseSave_,&poseLoad_})b->SetVisible(false);
    for (auto& field : importTransformFields_) field.SetVisible(model);
    importApplyTransform_.SetVisible(model);
    modelImportRole_.SetVisible(model && importWorkflow_==ImportWorkflow::Player);
    modelImportMaterial_.SetVisible(materials); modelImportMapSlot_.SetVisible(false);
    modelImportAssignMap_.SetVisible(false); modelImportMapInfo_.SetVisible(false);
    modelImportWarnings_.SetVisible(false);
    importMaterials_->show(materials);
    modelImportClip_.SetVisible(actions && !character); const bool timeline = !rt && (!pose || mapEditingClip_) && (character || importWorkflow_ == ImportWorkflow::Animated);
    modelImportPlay_.SetVisible(timeline);
    for (auto& step : importStep_) step.SetVisible(timeline);
    modelImportRestart_.SetVisible(timeline); modelImportTime_.SetVisible(timeline);
    modelImportSpeed_.SetVisible(actions && !character);
    actionSources_.SetVisible(actions && character);
    sourceHeading_.SetVisible(actions && character);
    entriesHeading_.SetVisible(actions && character);
    removeEntry_.SetVisible(actions && character);
    for(auto& label:entryHeaders_)label.SetVisible(actions && character);
    actionTable_.SetVisible(actions && character);
    entryHint_.SetVisible(actions && character);
    for(auto* button:{&referencePose_, &addEntry_, &entryUndo_, &entryRedo_}) button->SetVisible(actions && character);
    if (actions && character) RefreshCharacterClips();
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

#include "CharacterActionEntries.inc"
