from pathlib import Path

path = Path('Studio/src/StudioApplication.cpp')
text = path.read_text(encoding='utf-8')


def replace_function(source: str, signature: str, replacement: str) -> str:
    start = source.find(signature)
    if start < 0:
        raise RuntimeError(f'missing function: {signature}')
    brace = source.find('{', start)
    if brace < 0:
        raise RuntimeError(f'missing function body: {signature}')
    depth = 0
    end = None
    for index in range(brace, len(source)):
        ch = source[index]
        if ch == '{':
            depth += 1
        elif ch == '}':
            depth -= 1
            if depth == 0:
                end = index + 1
                break
    if end is None:
        raise RuntimeError(f'unbalanced function: {signature}')
    return source[:start] + replacement.rstrip() + source[end:]


marker = '}\n\nnamespace renegade::studio\n{'
if 'struct CreatorModelImportWorkspaceState' not in text:
    state = r'''

    constexpr float CreatorImportStageHeight = 100000.0f;

    struct CreatorModelImportWorkspaceState
    {
        bool active = false;
        bool committing = false;
        std::string sourcePath;
        std::size_t undoBaseline = 0;
        wi::ecs::Entity previewRoot = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity previewLight = wi::ecs::INVALID_ENTITY;
        wi::scene::TransformComponent cameraBefore;
        bool cameraCaptured = false;
        float automaticScale = 1.0f;
        renegade::bridge::ImportedSceneSummary summary;
        renegade::bridge::ImportedModelEvidence evidence;
        std::vector<wi::ecs::Entity> materialEntities;
        std::vector<wi::ecs::Entity> animationEntities;
        std::size_t selectedMaterial = 0;
        std::size_t selectedAnimation = 0;
    };

    CreatorModelImportWorkspaceState creatorModelImporter;
    renegade::studio::RenegadeComboBox creatorImportMaterialCombo;
    renegade::studio::RenegadeComboBox creatorImportAnimationCombo;
    wi::gui::Label creatorImportMaterialLabel;
    wi::gui::Label creatorImportMaterialReadout;
    wi::gui::Label creatorImportAnimationLabel;
    wi::gui::Label creatorImportAnimationReadout;
    wi::gui::Label creatorImportTransformLabel;
    wi::gui::Label creatorImportHelpLabel;

    std::string CreatorImportEntityName(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const std::string& fallback)
    {
        const auto* name = scene.names.GetComponent(entity);
        return name != nullptr && !name->name.empty() ? name->name : fallback;
    }

    void RefreshCreatorImportMaterialReadout()
    {
        auto* session = renegade::bridge::StudioSession::Current();
        if (session == nullptr || creatorModelImporter.materialEntities.empty())
        {
            creatorImportMaterialReadout.SetText("No imported materials detected.");
            return;
        }
        creatorModelImporter.selectedMaterial = std::min(
            creatorModelImporter.selectedMaterial,
            creatorModelImporter.materialEntities.size() - 1);
        auto& scene = session->Scenes().GetScene();
        const auto entity = creatorModelImporter.materialEntities[
            creatorModelImporter.selectedMaterial];
        const auto* material = scene.materials.GetComponent(entity);
        if (material == nullptr)
        {
            creatorImportMaterialReadout.SetText("Selected material is unavailable.");
            return;
        }
        const auto textureName = [material](const int slot)
        {
            const auto& texture = material->textures[slot];
            if (texture.name.empty())
                return std::string("<none>");
            return fs::u8path(texture.name).filename().generic_u8string();
        };
        std::ostringstream out;
        out << "Base: " << textureName(wi::scene::MaterialComponent::BASECOLORMAP)
            << "\nNormal: " << textureName(wi::scene::MaterialComponent::NORMALMAP)
            << "\nSurface: " << textureName(wi::scene::MaterialComponent::SURFACEMAP)
            << "\nEmissive: " << textureName(wi::scene::MaterialComponent::EMISSIVEMAP)
            << "\nAO: " << textureName(wi::scene::MaterialComponent::OCCLUSIONMAP);
        creatorImportMaterialReadout.SetText(out.str());
    }

    void RefreshCreatorImportAnimationReadout()
    {
        auto* session = renegade::bridge::StudioSession::Current();
        if (session == nullptr || creatorModelImporter.animationEntities.empty())
        {
            creatorImportAnimationReadout.SetText("No animation actions detected.");
            return;
        }
        creatorModelImporter.selectedAnimation = std::min(
            creatorModelImporter.selectedAnimation,
            creatorModelImporter.animationEntities.size() - 1);
        auto& scene = session->Scenes().GetScene();
        const auto entity = creatorModelImporter.animationEntities[
            creatorModelImporter.selectedAnimation];
        const auto* animation = scene.animations.GetComponent(entity);
        if (animation == nullptr)
        {
            creatorImportAnimationReadout.SetText("Selected animation is unavailable.");
            return;
        }
        std::ostringstream out;
        out.precision(3);
        out << std::fixed
            << "Start: " << animation->start
            << "   End: " << animation->end
            << "   Duration: " << std::max(0.0f, animation->end - animation->start)
            << "\nChannels: " << animation->channels.size()
            << "   Samplers: " << animation->samplers.size();
        creatorImportAnimationReadout.SetText(out.str());
    }
'''
    if marker not in text:
        raise RuntimeError('namespace insertion marker missing')
    text = text.replace(marker, state + '\n' + marker, 1)

create_panel = r'''    void StudioRenderPath::CreateImportScalePanel()
    {
        importScalePanel_.Create(
            "Model Import Workspace",
            wi::gui::Window::WindowControls::DISABLE_TITLE_BAR);
        importScalePanel_.SetVisible(false);

        importScaleTitleLabel_.Create("MODEL IMPORTER // PREVIEW BEFORE COMMIT");
        importScaleReadoutLabel_.Create("");
        creatorImportHelpLabel.Create(
            "The model is temporary. The project is unchanged until IMPORT & PLACE is pressed.");
        creatorImportHelpLabel.SetFitTextEnabled(true);

        creatorImportTransformLabel.Create("TRANSFORM // PREVIEW");
        importScaleModeCombo_.Create("Scale Mode");
        importScaleModeCombo_.AddItem(
            "AUTOMATIC",
            static_cast<std::uint64_t>(bridge::ModelScaleMode::Automatic));
        importScaleModeCombo_.AddItem(
            "ORIGINAL / METRES",
            static_cast<std::uint64_t>(bridge::ModelScaleMode::Original));
        importScaleModeCombo_.AddItem(
            "CENTIMETRES",
            static_cast<std::uint64_t>(bridge::ModelScaleMode::Centimeters));
        importScaleModeCombo_.AddItem(
            "INCHES",
            static_cast<std::uint64_t>(bridge::ModelScaleMode::Inches));
        importScaleModeCombo_.SetSelectedWithoutCallback(0);
        importScaleModeCombo_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            if (session_ == nullptr || !creatorModelImporter.active)
                return;
            const auto mode = static_cast<bridge::ModelScaleMode>(args.userdata);
            pendingImportScaleMode_ = mode;
            auto& scene = session_->Scenes().GetScene();
            auto* transform = scene.transforms.GetComponent(
                creatorModelImporter.previewRoot);
            if (transform == nullptr)
                return;
            const float factor = mode == bridge::ModelScaleMode::Automatic
                ? creatorModelImporter.automaticScale
                : bridge::ImportService::ResolveScaleFactor(mode, scene);
            auto next = bridge::CaptureTransform(*transform);
            next.scale = XMFLOAT3(factor, factor, factor);
            session_->Commands().Execute(
                std::make_unique<bridge::SetTransformCommand>(
                    scene, creatorModelImporter.previewRoot, next));
            importScaleAppliedFactor_ = factor;
        });

        creatorImportMaterialLabel.Create("MATERIALS // DETECTED MAPS");
        creatorImportMaterialCombo.Create("Material Slot");
        creatorImportMaterialCombo.OnSelect([](const wi::gui::EventArgs& args)
        {
            creatorModelImporter.selectedMaterial =
                static_cast<std::size_t>(args.userdata);
            RefreshCreatorImportMaterialReadout();
        });
        creatorImportMaterialReadout.Create("");
        creatorImportMaterialReadout.SetFitTextEnabled(true);

        creatorImportAnimationLabel.Create("ANIMATIONS // SOURCE ACTIONS");
        creatorImportAnimationCombo.Create("Animation Action");
        creatorImportAnimationCombo.OnSelect([](const wi::gui::EventArgs& args)
        {
            creatorModelImporter.selectedAnimation =
                static_cast<std::size_t>(args.userdata);
            RefreshCreatorImportAnimationReadout();
        });
        creatorImportAnimationReadout.Create("");
        creatorImportAnimationReadout.SetFitTextEnabled(true);

        importScaleApplyButton_.Create("Import Model Commit");
        importScaleApplyButton_.SetText("IMPORT & PLACE");
        importScaleApplyButton_.SetTooltip(
            "Commit the governed reusable asset, then place the configured instance back in the real level.");
        importScaleApplyButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            pendingAction_ = EditorAction::ApplyImportScale;
        });

        importScaleDismissButton_.Create("Cancel Model Import");
        importScaleDismissButton_.SetText("CANCEL");
        importScaleDismissButton_.SetTooltip(
            "Discard the temporary preview and return to the level without importing anything.");
        importScaleDismissButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            pendingAction_ = EditorAction::DismissImportScale;
        });

        for (wi::gui::Widget* widget : {
            static_cast<wi::gui::Widget*>(&importScaleTitleLabel_),
            static_cast<wi::gui::Widget*>(&importScaleReadoutLabel_),
            static_cast<wi::gui::Widget*>(&creatorImportHelpLabel),
            static_cast<wi::gui::Widget*>(&creatorImportTransformLabel),
            static_cast<wi::gui::Widget*>(&importScaleModeCombo_),
            static_cast<wi::gui::Widget*>(&creatorImportMaterialLabel),
            static_cast<wi::gui::Widget*>(&creatorImportMaterialCombo),
            static_cast<wi::gui::Widget*>(&creatorImportMaterialReadout),
            static_cast<wi::gui::Widget*>(&creatorImportAnimationLabel),
            static_cast<wi::gui::Widget*>(&creatorImportAnimationCombo),
            static_cast<wi::gui::Widget*>(&creatorImportAnimationReadout),
            static_cast<wi::gui::Widget*>(&importScaleApplyButton_),
            static_cast<wi::gui::Widget*>(&importScaleDismissButton_)})
        {
            widget->SetShadowRadius(0.0f);
            importScalePanel_.AddWidget(widget);
        }
    }
'''
text = replace_function(text, '    void StudioRenderPath::CreateImportScalePanel()', create_panel)

run_preview = r'''    void StudioRenderPath::RunModelImportPlacement(
        const std::string& sourcePath)
    {
        if (session_ == nullptr || sourcePath.empty() ||
            !session_->Projects().HasProject() || creatorModelImporter.active ||
            creatorModelImporter.committing)
        {
            return;
        }

        struct PreviewPrepareState
        {
            std::string sourcePath;
            std::string projectRoot;
            bridge::PreparedModelImport prepared;
        };
        auto state = std::make_shared<PreviewPrepareState>();
        state->sourcePath = sourcePath;
        state->projectRoot = session_->Projects().CurrentProject().rootPath;

        wi::jobsystem::Execute(modelImportWorkload_,
            [this, state](wi::jobsystem::JobArgs)
            {
                const fs::path previewDirectory =
                    fs::u8path(state->projectRoot) / "Intermediate" / "Imports";
                std::error_code ec;
                fs::create_directories(previewDirectory, ec);
                bridge::ModelImportRequest request;
                request.sourcePath = state->sourcePath;
                request.assetPath = (previewDirectory / ".creator-preview.wiscene")
                    .generic_u8string();
                request.expectedFormat = bridge::ImportService::ClassifyModelSourceFormat(
                    state->sourcePath);
                state->prepared = bridge::ImportService().PrepareModelAsset(request);

                wi::eventhandler::Subscribe_Once(
                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [this, state](std::uint64_t)
                    {
                        if (session_ == nullptr || !state->prepared.IsReady())
                        {
                            const std::string error = state->prepared.Result().error.empty()
                                ? "Renegade could not prepare a visible preview."
                                : state->prepared.Result().error;
                            studioChrome_.SetStatusText("IMPORT MODEL // PREVIEW FAILED");
                            wi::helper::messageBox(error, "Import Model");
                            return;
                        }

                        auto& liveScene = session_->Scenes().GetScene();
                        const auto* isolated = state->prepared.PeekScene();
                        creatorModelImporter = {};
                        creatorModelImporter.active = true;
                        creatorModelImporter.sourcePath = state->sourcePath;
                        creatorModelImporter.undoBaseline = session_->Commands().UndoCount();
                        creatorModelImporter.cameraBefore = editorCameraTransform_;
                        creatorModelImporter.cameraCaptured = true;
                        creatorModelImporter.summary = bridge::ImportService::Summarize(*isolated);
                        creatorModelImporter.evidence = bridge::ImportService::SummarizeModelEvidence(*isolated);
                        creatorModelImporter.automaticScale = bridge::ImportService::ResolveScaleFactor(
                            bridge::ModelScaleMode::Automatic, *isolated);

                        const std::size_t materialStart = liveScene.materials.GetCount();
                        const std::size_t animationStart = liveScene.animations.GetCount();

                        auto place = std::make_unique<bridge::PlaceImportedModelCommand>(
                            liveScene,
                            state->prepared.ReleaseScene(),
                            XMFLOAT3(0.0f, CreatorImportStageHeight, 0.0f),
                            creatorModelImporter.automaticScale);
                        auto* placed = place.get();
                        if (!session_->Commands().Execute(std::move(place)))
                        {
                            creatorModelImporter = {};
                            studioChrome_.SetStatusText("IMPORT MODEL // PREVIEW PLACE FAILED");
                            return;
                        }
                        creatorModelImporter.previewRoot = placed->PlacedEntity();

                        for (std::size_t index = materialStart; index < liveScene.materials.GetCount(); ++index)
                            creatorModelImporter.materialEntities.push_back(liveScene.materials.GetEntity(index));
                        for (std::size_t index = animationStart; index < liveScene.animations.GetCount(); ++index)
                            creatorModelImporter.animationEntities.push_back(liveScene.animations.GetEntity(index));

                        auto light = std::make_unique<bridge::CreateLightCommand>(
                            liveScene,
                            wi::scene::LightComponent::POINT,
                            XMFLOAT3(3.0f, CreatorImportStageHeight + 4.0f, -3.0f));
                        auto* lightRaw = light.get();
                        if (session_->Commands().Execute(std::move(light)))
                        {
                            creatorModelImporter.previewLight = lightRaw->CreatedEntity();
                            auto lightState = bridge::MakeNewLightState(wi::scene::LightComponent::POINT);
                            lightState.intensity = 900.0f;
                            lightState.range = 18.0f;
                            session_->Commands().Execute(
                                std::make_unique<bridge::SetLightCommand>(
                                    liveScene, creatorModelImporter.previewLight, lightState));
                        }

                        const XMVECTOR eye = XMVectorSet(4.0f, CreatorImportStageHeight + 2.6f, -6.0f, 1.0f);
                        const XMVECTOR at = XMVectorSet(0.0f, CreatorImportStageHeight + 1.0f, 0.0f, 1.0f);
                        const XMMATRIX view = XMMatrixLookAtLH(eye, at, XMVectorSet(0, 1, 0, 0));
                        editorCameraTransform_.ClearTransform();
                        editorCameraTransform_.MatrixTransform(XMMatrixInverse(nullptr, view));
                        editorCameraTransform_.UpdateTransform();
                        camera->TransformCamera(editorCameraTransform_);
                        camera->UpdateCamera();

                        session_->Selection().Select(creatorModelImporter.previewRoot);
                        RefreshHierarchy();
                        RefreshInspector();
                        studioChrome_.SetVisible(false);
                        ShowImportScalePanel(
                            creatorModelImporter.previewRoot,
                            creatorModelImporter.automaticScale,
                            fs::u8path(state->sourcePath).filename().generic_u8string());
                    });
            });
    }
'''
text = replace_function(text, '    void StudioRenderPath::RunModelImportPlacement(', run_preview)

show_panel = r'''    void StudioRenderPath::ShowImportScalePanel(
        const wi::ecs::Entity entity,
        const float appliedScaleFactor,
        const std::string& sourceFileName)
    {
        importScaleTargetEntity_ = entity;
        importScaleAppliedFactor_ = appliedScaleFactor;
        pendingImportScaleMode_ = bridge::ModelScaleMode::Automatic;

        std::ostringstream readout;
        readout << sourceFileName
            << "\nMeshes: " << creatorModelImporter.summary.meshes
            << "   Materials: " << creatorModelImporter.summary.materials
            << "   Textures: " << creatorModelImporter.summary.textureReferences
            << "\nAnimations: " << creatorModelImporter.summary.animations
            << "   Bones: " << creatorModelImporter.evidence.armatureBones;
        importScaleReadoutLabel_.SetText(readout.str());
        importScaleModeCombo_.SetSelectedWithoutCallback(0);

        creatorImportMaterialCombo.ClearItems();
        auto& scene = session_->Scenes().GetScene();
        for (std::size_t index = 0; index < creatorModelImporter.materialEntities.size(); ++index)
        {
            const auto entityId = creatorModelImporter.materialEntities[index];
            creatorImportMaterialCombo.AddItem(
                CreatorImportEntityName(scene, entityId, "Material " + std::to_string(index + 1)),
                static_cast<std::uint64_t>(index));
        }
        if (!creatorModelImporter.materialEntities.empty())
        {
            creatorImportMaterialCombo.SetSelectedWithoutCallback(0);
            creatorModelImporter.selectedMaterial = 0;
        }
        RefreshCreatorImportMaterialReadout();

        creatorImportAnimationCombo.ClearItems();
        for (std::size_t index = 0; index < creatorModelImporter.animationEntities.size(); ++index)
        {
            const auto entityId = creatorModelImporter.animationEntities[index];
            creatorImportAnimationCombo.AddItem(
                CreatorImportEntityName(scene, entityId, "Animation " + std::to_string(index + 1)),
                static_cast<std::uint64_t>(index));
        }
        if (!creatorModelImporter.animationEntities.empty())
        {
            creatorImportAnimationCombo.SetSelectedWithoutCallback(0);
            creatorModelImporter.selectedAnimation = 0;
        }
        RefreshCreatorImportAnimationReadout();
        importScalePanel_.SetVisible(true);
    }
'''
text = replace_function(text, '    void StudioRenderPath::ShowImportScalePanel(', show_panel)

commit = r'''    void StudioRenderPath::ApplyImportScaleMode(
        const bridge::ModelScaleMode mode)
    {
        if (session_ == nullptr || !creatorModelImporter.active || creatorModelImporter.committing)
            return;

        auto& liveScene = session_->Scenes().GetScene();
        auto* previewTransform = liveScene.transforms.GetComponent(creatorModelImporter.previewRoot);
        if (previewTransform == nullptr)
        {
            DismissImportScalePanel();
            return;
        }

        const bridge::TransformState preview = bridge::CaptureTransform(*previewTransform);
        const XMFLOAT3 previewOffset(
            preview.translation.x,
            preview.translation.y - CreatorImportStageHeight,
            preview.translation.z);
        const std::string sourcePath = creatorModelImporter.sourcePath;
        const auto cameraBefore = creatorModelImporter.cameraBefore;
        creatorModelImporter.committing = true;

        while (session_->Commands().UndoCount() > creatorModelImporter.undoBaseline)
        {
            if (!session_->Commands().Undo())
                break;
        }
        importScalePanel_.SetVisible(false);
        studioChrome_.SetVisible(true);
        editorCameraTransform_ = cameraBefore;
        editorCameraTransform_.UpdateTransform();
        camera->TransformCamera(editorCameraTransform_);
        camera->UpdateCamera();
        ClearSelectionOutline();
        session_->Selection().Clear();
        RefreshHierarchy();
        RefreshInspector();

        struct GovernedCommitState
        {
            std::string projectRoot;
            bridge::StableId projectId;
            std::string sourcePath;
            XMFLOAT3 position;
            XMFLOAT4 rotation;
            float scale = 1.0f;
            bridge::CreatorModelImportResult imported;
            bridge::PreparedReusableModelPlacement prepared;
        };
        auto state = std::make_shared<GovernedCommitState>();
        const auto& project = session_->Projects().CurrentProject();
        state->projectRoot = project.rootPath;
        state->projectId = project.projectId;
        state->sourcePath = sourcePath;
        const XMFLOAT3 cameraPosition = cameraBefore.GetPosition();
        const XMFLOAT3 cameraForward = cameraBefore.GetForward();
        state->position = XMFLOAT3(
            cameraPosition.x + cameraForward.x * 5.0f + previewOffset.x,
            cameraPosition.y + cameraForward.y * 5.0f + previewOffset.y,
            cameraPosition.z + cameraForward.z * 5.0f + previewOffset.z);
        state->rotation = preview.rotation;
        state->scale = preview.scale.x;
        studioChrome_.SetStatusText("IMPORT MODEL // COMMITTING GOVERNED ASSET");

        wi::jobsystem::Execute(modelImportWorkload_,
            [this, state](wi::jobsystem::JobArgs)
            {
                bridge::CreatorAssetWorkflowService workflow;
                state->imported = workflow.ImportModel(state->projectRoot, state->projectId, state->sourcePath);
                if (state->imported.succeeded)
                    state->prepared = workflow.PrepareModelPlacement(
                        state->projectRoot, state->projectId, state->imported.asset.assetId);
                wi::eventhandler::Subscribe_Once(
                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [this, state](std::uint64_t)
                    {
                        creatorModelImporter = {};
                        if (!state->imported.succeeded || !state->prepared.IsReady())
                        {
                            studioChrome_.SetStatusText("IMPORT MODEL // COMMIT FAILED");
                            const std::string reason = !state->imported.succeeded
                                ? state->imported.error : state->prepared.Result().error;
                            wi::helper::messageBox(
                                "The preview was discarded safely, but the governed asset could not be committed.\n\nReason: " + reason,
                                "Import Model");
                            return;
                        }

                        auto command = std::make_unique<bridge::PlaceReusableModelCommand>(
                            session_->Scenes().GetScene(), state->prepared.ReleaseScene(),
                            state->imported.asset.assetId, state->position, state->scale);
                        auto* placed = command.get();
                        if (!session_->Commands().Execute(std::move(command)))
                        {
                            studioChrome_.SetStatusText("IMPORT MODEL // ASSET COMMITTED // PLACE FAILED");
                            return;
                        }
                        auto& scene = session_->Scenes().GetScene();
                        auto* transform = scene.transforms.GetComponent(placed->PlacedEntity());
                        if (transform != nullptr)
                        {
                            auto finalTransform = bridge::CaptureTransform(*transform);
                            finalTransform.rotation = state->rotation;
                            session_->Commands().Execute(
                                std::make_unique<bridge::SetTransformCommand>(
                                    scene, placed->PlacedEntity(), finalTransform));
                        }
                        session_->Selection().Select(placed->PlacedEntity());
                        RefreshHierarchy();
                        RefreshInspector();
                        RefreshStatus();
                        assetBrowserCurrentFolder_ = "Content/Models";
                        RefreshAssetBrowser();
                        studioChrome_.SetStatusText(
                            "IMPORT MODEL // GOVERNED ASSET READY // " +
                            fs::u8path(state->imported.assetProjectRelativePath).filename().generic_u8string());
                    });
            });
    }
'''
text = replace_function(text, '    void StudioRenderPath::ApplyImportScaleMode(', commit)

cancel = r'''    void StudioRenderPath::DismissImportScalePanel()
    {
        importScalePanel_.SetVisible(false);
        if (session_ != nullptr && creatorModelImporter.active)
        {
            while (session_->Commands().UndoCount() > creatorModelImporter.undoBaseline)
            {
                if (!session_->Commands().Undo())
                    break;
            }
            if (creatorModelImporter.cameraCaptured)
            {
                editorCameraTransform_ = creatorModelImporter.cameraBefore;
                editorCameraTransform_.UpdateTransform();
                camera->TransformCamera(editorCameraTransform_);
                camera->UpdateCamera();
            }
            session_->Selection().Clear();
            ClearSelectionOutline();
            RefreshHierarchy();
            RefreshInspector();
            RefreshStatus();
        }
        creatorModelImporter = {};
        importScaleTargetEntity_ = wi::ecs::INVALID_ENTITY;
        studioChrome_.SetVisible(true);
        studioChrome_.SetStatusText("IMPORT MODEL // CANCELLED // PROJECT UNCHANGED");
    }
'''
text = replace_function(text, '    void StudioRenderPath::DismissImportScalePanel()', cancel)

text = text.replace('const float importScalePanelWidth = 320.0f;', 'const float importScalePanelWidth = 390.0f;')
text = text.replace('const float importScalePanelHeight = 288.0f;', 'const float importScalePanelHeight = 690.0f;')
text = text.replace(
    '            0.02f);',
    '            creatorModelImporter.active ? CreatorImportStageHeight + 0.02f : 0.02f);',
    1)

old_layout_start = text.find('        importScaleTitleLabel_.SetPos(XMFLOAT2(12.0f, 8.0f));')
old_layout_end_marker = '        projectHubPanel_.SetPos(XMFLOAT2(12.0f, 12.0f));'
old_layout_end = text.find(old_layout_end_marker, old_layout_start)
if old_layout_start < 0 or old_layout_end < 0:
    raise RuntimeError('could not locate import panel layout block')
new_layout = r'''        importScaleTitleLabel_.SetPos(XMFLOAT2(12.0f, 8.0f));
        importScaleTitleLabel_.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 24.0f));
        importScaleReadoutLabel_.SetPos(XMFLOAT2(12.0f, 36.0f));
        importScaleReadoutLabel_.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 64.0f));
        creatorImportHelpLabel.SetPos(XMFLOAT2(12.0f, 104.0f));
        creatorImportHelpLabel.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 50.0f));
        creatorImportTransformLabel.SetPos(XMFLOAT2(12.0f, 160.0f));
        creatorImportTransformLabel.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 22.0f));
        importScaleModeCombo_.SetPos(XMFLOAT2(12.0f, 186.0f));
        importScaleModeCombo_.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 28.0f));
        creatorImportMaterialLabel.SetPos(XMFLOAT2(12.0f, 232.0f));
        creatorImportMaterialLabel.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 22.0f));
        creatorImportMaterialCombo.SetPos(XMFLOAT2(12.0f, 258.0f));
        creatorImportMaterialCombo.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 28.0f));
        creatorImportMaterialReadout.SetPos(XMFLOAT2(12.0f, 292.0f));
        creatorImportMaterialReadout.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 112.0f));
        creatorImportAnimationLabel.SetPos(XMFLOAT2(12.0f, 420.0f));
        creatorImportAnimationLabel.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 22.0f));
        creatorImportAnimationCombo.SetPos(XMFLOAT2(12.0f, 446.0f));
        creatorImportAnimationCombo.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 28.0f));
        creatorImportAnimationReadout.SetPos(XMFLOAT2(12.0f, 480.0f));
        creatorImportAnimationReadout.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 76.0f));
        importScaleApplyButton_.SetPos(XMFLOAT2(12.0f, 624.0f));
        importScaleApplyButton_.SetSize(XMFLOAT2((importScalePanelWidth - 32.0f) * 0.64f, 34.0f));
        importScaleDismissButton_.SetPos(XMFLOAT2(20.0f + (importScalePanelWidth - 32.0f) * 0.64f, 624.0f));
        importScaleDismissButton_.SetSize(XMFLOAT2((importScalePanelWidth - 32.0f) * 0.36f, 34.0f));

'''
text = text[:old_layout_start] + new_layout + text[old_layout_end:]

path.write_text(text, encoding='utf-8')
