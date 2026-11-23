// Member implementations for the native Model Importer.
void StudioRenderPath::CreateImportWorkspace()
{
    const char* workflows[] = {"Static Model", "Animated Model", "Character", "Player Arms / Weapon"};
    for (int i = 0; i < 4; ++i) {
        auto& button = importWorkflowButtons_[i];
        button.Create(workflows[i]);
        button.OnClick([this, i](const wi::gui::EventArgs&) {
            SelectImportWorkflow(static_cast<ImportWorkflow>(i+1));
        });
        modelImportPanel_.AddWidget(&button);
    }
    const char* pages[] = {"Model", "Materials", "Actions"};
    for (int i = 0; i < 3; ++i) {
        auto& button = importPageButtons_[i];
        button.Create(pages[i]);
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
    const float viewHeight = footer - 148;
    const float half = (viewWidth - gap) / 2;
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
    for (int i = 0; i < 7; ++i)
        place(importTransformFields_[i], right + 73 + 141 * (i % 3), 108 + 48 * (i / 3), 63, 28);
    place(importApplyTransform_, right + 183, 204, 247, 28);
    place(modelImportPreviewImage_, pad, 52, viewWidth, viewHeight);
    place(modelImportRotateLeft_, pad, 60 + viewHeight, half, 28);
    place(modelImportRotateRight_, pad + half + gap, 60 + viewHeight, half, 28);
    place(modelImportSummary_, pad, 96 + viewHeight, viewWidth, 42);
    place(modelImportMaterial_, right + 92, 102, 338, 28);
    place(modelImportMapSlot_, right + 92, 141, 338, 28);
    place(modelImportAssignMap_, right, 181, inspectorWidth, 32);
    place(modelImportMapInfo_, right, 225, inspectorWidth, 99);
    place(modelImportWarnings_, right, 332, inspectorWidth, std::min(180.f, footer - 340));
    place(modelImportClip_, right + 86, 107, 344, 30);
    place(modelImportPlay_, right, 151, 211, 30);
    place(modelImportRestart_, right + 219, 151, 211, 30);
    place(modelImportTime_, right + 155, 198, 210, 24);
    place(modelImportSpeed_, right + 155, 241, 210, 24);
    place(modelImportAction_, right + 138, 286, 292, 30);
    place(modelImportAddAnimation_, right, 335, inspectorWidth, 32);
    place(modelImportRole_, right + 93, 263, 337, 28);
    place(modelImportName_, pad + 92, footer, viewWidth - 92, 27);
    place(modelImportFolder_, pad + 92, footer + 36, viewWidth - 92, 27);
    place(modelImportFolderChoices_, right + 86, footer, 344, 27);
    place(modelImportTags_, right + 86, footer + 36, 344, 27);
    place(modelImportCancel_, right, footer + 76, 207, 32);
    place(modelImportCommit_, right + 215, footer + 76, 215, 32);
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
