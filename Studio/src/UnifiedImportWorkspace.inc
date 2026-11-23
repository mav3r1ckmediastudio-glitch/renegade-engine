// Member implementations for the native Model Importer.
void StudioRenderPath::CreateImportWorkspace()
{
    const char* workflows[] = {"Static Model", "Animated Model", "Character", "Player Arms / Weapon"};
    for (int i = 0; i < 4; ++i) {
        auto& button = importWorkflowButtons_[i];
        button.Create(workflows[i]);
        button.SetPos({16.f + 246.f*i, 10});
        button.SetSize({238, 30});
        button.OnClick([this, i](const wi::gui::EventArgs&) {
            SelectImportWorkflow(static_cast<ImportWorkflow>(i+1));
        });
        modelImportPanel_.AddWidget(&button);
    }
    const char* pages[] = {"Model", "Materials", "Actions"};
    for (int i = 0; i < 3; ++i) {
        auto& button = importPageButtons_[i];
        button.Create(pages[i]);
        button.SetPos({552.f + 145.f*i, 54});
        button.SetSize({137, 28});
        button.OnClick([this, i](const wi::gui::EventArgs&) {
            importPage_ = static_cast<ImportPage>(i);
            LayoutImportWorkspace();
        });
        modelImportPanel_.AddWidget(&button);
    }
    const char* labels[] = {"Rotate X", "Rotate Y", "Rotate Z", "Move X", "Move Y", "Move Z", "Scale"};
    for (int i = 0; i < 7; ++i) {
        auto& field = importTransformFields_[i];
        field.Create(labels[i]);
        field.SetDescription(std::string(labels[i]) + "  ");
        field.SetPos({625.f+141.f*(i%3), 108.f+48.f*(i/3)});
        field.SetSize({63,28});
        field.SetCancelInputEnabled(false);
        field.SetTooltip(i < 3 ? "Degrees" : i < 6 ? "Metres" : "Uniform scale");
        modelImportPanel_.AddWidget(&field);
    }
    importApplyTransform_.Create("Apply Transform");
    importApplyTransform_.SetPos({735,204});
    importApplyTransform_.SetSize({246,28});
    importApplyTransform_.OnClick([this](const wi::gui::EventArgs&) { ApplyImportTransform(); });
    modelImportPanel_.AddWidget(&importApplyTransform_);
    modelImportPanel_.SetSize({1010,635});
    modelImportPreviewImage_.SetPos({16,54});
    modelImportRotateLeft_.SetPos({16,381}); modelImportRotateRight_.SetPos({282,381});
    modelImportSummary_.SetPos({16,419}); modelImportSummary_.SetSize({512,66});
    modelImportMaterial_.SetPos({640,102}); modelImportMaterial_.SetSize({340,28});
    modelImportMapSlot_.SetPos({640,141}); modelImportMapSlot_.SetSize({340,28});
    modelImportAssignMap_.SetPos({552,181}); modelImportMapInfo_.SetPos({552,225});
    modelImportWarnings_.SetPos({552,340}); modelImportWarnings_.SetSize({430,138});
    modelImportClip_.SetPos({628,107}); modelImportClip_.SetSize({353,30});
    modelImportPlay_.SetPos({552,151}); modelImportPlay_.SetSize({209,30});
    modelImportRestart_.SetPos({772,151}); modelImportRestart_.SetSize({209,30});
    modelImportTime_.SetPos({706,198}); modelImportTime_.SetSize({210,24});
    modelImportSpeed_.SetPos({706,241}); modelImportSpeed_.SetSize({210,24});
    modelImportAction_.SetPos({690,286}); modelImportAction_.SetSize({291,30});
    modelImportAddAnimation_.SetPos({552,335}); modelImportAddAnimation_.SetSize({429,32});
    modelImportRole_.SetPos({645,263}); modelImportRole_.SetSize({336,28});
    modelImportName_.SetPos({107,497}); modelImportName_.SetSize({420,27});
    modelImportFolderChoices_.SetPos({638,497}); modelImportFolderChoices_.SetSize({344,27});
    modelImportFolder_.SetPos({107,534}); modelImportFolder_.SetSize({420,27});
    modelImportTags_.SetPos({638,534}); modelImportTags_.SetSize({344,27});
    modelImportCancel_.SetPos({552,574}); modelImportCancel_.SetSize({200,32});
    modelImportCommit_.SetPos({764,574}); modelImportCommit_.SetSize({218,32});
}
void StudioRenderPath::SelectImportWorkflow(ImportWorkflow workflow)
{
    if (!modelImportCandidate_) return;
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
    for (int i=0;i<3;++i) {
        importPageButtons_[i].SetVisible(chosen && (i!=2 || character || importWorkflow_==ImportWorkflow::Animated));
        importPageButtons_[i].SetColor(int(importPage_)==i ? theme::Accent : theme::Control, wi::gui::IDLE);
    }
    for (auto& field : importTransformFields_) field.SetVisible(model);
    importApplyTransform_.SetVisible(model);
    modelImportRole_.SetVisible(model && importWorkflow_==ImportWorkflow::Player);
    modelImportMaterial_.SetVisible(materials); modelImportMapSlot_.SetVisible(materials);
    modelImportAssignMap_.SetVisible(materials); modelImportMapInfo_.SetVisible(materials);
    modelImportWarnings_.SetVisible(materials);
    modelImportClip_.SetVisible(actions); modelImportPlay_.SetVisible(actions);
    modelImportRestart_.SetVisible(actions); modelImportTime_.SetVisible(actions);
    modelImportSpeed_.SetVisible(actions); modelImportAction_.SetVisible(actions && character);
    modelImportAddAnimation_.SetVisible(actions && character);
    modelImportName_.SetVisible(chosen); modelImportTags_.SetVisible(chosen);
    modelImportFolder_.SetVisible(chosen); modelImportFolderChoices_.SetVisible(chosen);
    modelImportCommit_.SetVisible(chosen);
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
    modelImportPreview_=std::move(preview);
    importTransform_=correction;
    modelImportClip_.SetSelected(clip);
    if (clip>0) modelImportPreview_->Scrub(time);
}
