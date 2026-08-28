from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected exactly one match, found {count}: {old[:120]!r}")
    p.write_text(text.replace(old, new, 1), encoding="utf-8")


# Build graph -----------------------------------------------------------------
replace_once(
    "EngineBridge/CMakeLists.txt",
    "    include/renegade/bridge/CommandService.h\n",
    "    include/renegade/bridge/CameraService.h\n"
    "    include/renegade/bridge/CommandService.h\n",
)
replace_once(
    "EngineBridge/CMakeLists.txt",
    "    src/BuildStageService.cpp\n    src/CommandService.cpp\n",
    "    src/BuildStageService.cpp\n    src/CameraService.cpp\n    src/CommandService.cpp\n",
)
replace_once(
    "CMakeLists.txt",
    "    include(Tests/Phase5Gate1.cmake)\n",
    "    include(Tests/Phase5Gate1.cmake)\n    include(Tests/Phase5Gate2.cmake)\n",
)

# Creator ADD menu ------------------------------------------------------------
replace_once(
    "Studio/src/RenegadeStudioChrome.h",
    "            CreateRectangleLight,\n            ImportModel,\n",
    "            CreateRectangleLight,\n            CreateCamera,\n            ImportModel,\n",
)
replace_once(
    "Studio/src/RenegadeStudioChrome.cpp",
    "                    else if (activeMenu_ == 2 && item >= 0 && item < 5)\n"
    "                    {\n"
    "                        constexpr std::array<Action, 5> actions = {\n"
    "                            Action::CreatePointLight,\n"
    "                            Action::CreateSpotLight,\n"
    "                            Action::CreateDirectionalLight,\n"
    "                            Action::CreateRectangleLight,\n"
    "                            Action::ImportModel};\n"
    "                        invoke(actions[item]);\n"
    "                    }\n",
    "                    else if (activeMenu_ == 2 && item >= 0 && item < 6)\n"
    "                    {\n"
    "                        constexpr std::array<Action, 6> actions = {\n"
    "                            Action::CreatePointLight,\n"
    "                            Action::CreateSpotLight,\n"
    "                            Action::CreateDirectionalLight,\n"
    "                            Action::CreateRectangleLight,\n"
    "                            Action::CreateCamera,\n"
    "                            Action::ImportModel};\n"
    "                        invoke(actions[item]);\n"
    "                    }\n",
)
replace_once(
    "Studio/src/RenegadeStudioChrome.cpp",
    "                items = {{\"POINT LIGHT\", true}, {\"SPOT LIGHT\", true},\n"
    "                    {\"DIRECTIONAL LIGHT\", true}, {\"RECTANGLE LIGHT\", true},\n"
    "                    {\"IMPORT MODEL...\", true}};\n",
    "                items = {{\"POINT LIGHT\", true}, {\"SPOT LIGHT\", true},\n"
    "                    {\"DIRECTIONAL LIGHT\", true}, {\"RECTANGLE LIGHT\", true},\n"
    "                    {\"CAMERA\", true}, {\"IMPORT MODEL...\", true}};\n",
)

# Studio header ---------------------------------------------------------------
replace_once(
    "Studio/src/StudioApplication.h",
    "#include \"renegade/bridge/LightService.h\"\n",
    "#include \"renegade/bridge/LightService.h\"\n#include \"renegade/bridge/CameraService.h\"\n",
)
replace_once(
    "Studio/src/StudioApplication.h",
    "            CreateLight,\n            OpenScene,\n",
    "            CreateLight,\n            CreateCamera,\n            OpenScene,\n",
)
replace_once(
    "Studio/src/StudioApplication.h",
    "        enum class LightToggle\n"
    "        {\n"
    "            CastShadow,\n"
    "            Volumetrics,\n"
    "        };\n",
    "        enum class LightToggle\n"
    "        {\n"
    "            CastShadow,\n"
    "            Volumetrics,\n"
    "        };\n\n"
    "        enum class CameraField\n"
    "        {\n"
    "            FieldOfView,\n"
    "            NearPlane,\n"
    "            FarPlane,\n"
    "            FocalLength,\n"
    "            ApertureSize,\n"
    "            OrthoVerticalSize,\n"
    "        };\n",
)
replace_once(
    "Studio/src/StudioApplication.h",
    "        bool CommitSelectedLight(const bridge::LightState& light);\n"
    "        void CreateLight(\n",
    "        bool CommitSelectedLight(const bridge::LightState& light);\n"
    "        bool CommitSelectedCamera(const bridge::CameraState& cameraState);\n"
    "        void ApplySelectedCameraProjection(bool orthographic);\n"
    "        void BeginCameraSlider(CameraField field);\n"
    "        void PreviewCameraSlider(CameraField field, float value);\n"
    "        void CommitCameraSlider(CameraField field, float value);\n"
    "        static void SetCameraFieldValue(\n"
    "            bridge::CameraState& cameraState,\n"
    "            CameraField field,\n"
    "            float value) noexcept;\n"
    "        [[nodiscard]] bridge::TransformState CaptureEditorCameraTransform() const;\n"
    "        void CreateCameraFromView();\n"
    "        void AlignSelectedCameraToView();\n"
    "        void ViewFromSelectedCamera();\n"
    "        void CreateLight(\n",
)
replace_once(
    "Studio/src/StudioApplication.h",
    "        void LayoutInspectorActions(\n"
    "            bool environment,\n"
    "            bool terrain = false,\n"
    "            bool light = false);\n",
    "        void LayoutInspectorActions(\n"
    "            bool environment,\n"
    "            bool terrain = false,\n"
    "            bool light = false,\n"
    "            bool sceneCamera = false);\n",
)
replace_once(
    "Studio/src/StudioApplication.h",
    "        SceneInspectorCheckBox sceneObjectReflections_;\n"
    "        SceneInspectorCheckBox sceneObjectWetmap_;\n"
    "        wi::gui::Label lightLabel_;\n",
    "        SceneInspectorCheckBox sceneObjectReflections_;\n"
    "        SceneInspectorCheckBox sceneObjectWetmap_;\n"
    "        wi::gui::Label cameraLabel_;\n"
    "        SceneInspectorComboBox cameraProjection_;\n"
    "        SceneInspectorSlider cameraFieldOfView_;\n"
    "        SceneInspectorSlider cameraNearPlane_;\n"
    "        SceneInspectorSlider cameraFarPlane_;\n"
    "        SceneInspectorSlider cameraFocalLength_;\n"
    "        SceneInspectorSlider cameraApertureSize_;\n"
    "        SceneInspectorSlider cameraOrthoVerticalSize_;\n"
    "        SceneInspectorButton cameraAlignToView_;\n"
    "        SceneInspectorButton cameraViewFrom_;\n"
    "        wi::gui::Label lightLabel_;\n",
)
replace_once(
    "Studio/src/StudioApplication.h",
    "        bridge::LightState lightSliderBefore_;\n"
    "        bridge::LightState lightSliderAfter_;\n"
    "        wi::scene::LightComponent::LightType pendingLightType_ =\n",
    "        bridge::LightState lightSliderBefore_;\n"
    "        bridge::LightState lightSliderAfter_;\n"
    "        bool cameraSliderActive_ = false;\n"
    "        CameraField cameraSliderField_ = CameraField::FieldOfView;\n"
    "        wi::ecs::Entity cameraSliderEntity_ = wi::ecs::INVALID_ENTITY;\n"
    "        bridge::CameraState cameraSliderBefore_;\n"
    "        bridge::CameraState cameraSliderAfter_;\n"
    "        wi::scene::LightComponent::LightType pendingLightType_ =\n",
)

# Studio action routing --------------------------------------------------------
replace_once(
    "Studio/src/StudioApplication.cpp",
    "            case RenegadeStudioChrome::Action::CreateRectangleLight:\n"
    "                pendingLightType_ = wi::scene::LightComponent::RECTANGLE;\n"
    "                pendingAction_ = EditorAction::CreateLight;\n"
    "                break;\n"
    "            case RenegadeStudioChrome::Action::Focus:\n",
    "            case RenegadeStudioChrome::Action::CreateRectangleLight:\n"
    "                pendingLightType_ = wi::scene::LightComponent::RECTANGLE;\n"
    "                pendingAction_ = EditorAction::CreateLight;\n"
    "                break;\n"
    "            case RenegadeStudioChrome::Action::CreateCamera:\n"
    "                pendingAction_ = EditorAction::CreateCamera;\n"
    "                break;\n"
    "            case RenegadeStudioChrome::Action::Focus:\n",
)
replace_once(
    "Studio/src/StudioApplication.cpp",
    "        case EditorAction::CreateLight:\n"
    "            CreateLight(pendingLightType_);\n"
    "            break;\n"
    "        case EditorAction::OpenScene:\n",
    "        case EditorAction::CreateLight:\n"
    "            CreateLight(pendingLightType_);\n"
    "            break;\n"
    "        case EditorAction::CreateCamera:\n"
    "            CreateCameraFromView();\n"
    "            break;\n"
    "        case EditorAction::OpenScene:\n",
)

# Studio Inspector creation ---------------------------------------------------
replace_once(
    "Studio/src/StudioApplication.cpp",
    "        createObjectToggle(sceneObjectWetmap_, \"Wetmap: \",\n"
    "            \"Enable native Wicked wetmap participation.\",\n"
    "            bridge::ObjectParticipationProperty::Wetmap);\n\n"
    "        createSectionLabel(\n"
    "            lightLabel_,\n",
    "        createObjectToggle(sceneObjectWetmap_, \"Wetmap: \",\n"
    "            \"Enable native Wicked wetmap participation.\",\n"
    "            bridge::ObjectParticipationProperty::Wetmap);\n\n"
    "        createSectionLabel(\n"
    "            cameraLabel_,\n"
    "            \"Camera Section\",\n"
    "            \"CAMERA // NATIVE WICKED\");\n"
    "        cameraProjection_.Create(\"Camera Projection\");\n"
    "        cameraProjection_.AddItem(\"PERSPECTIVE\", 0);\n"
    "        cameraProjection_.AddItem(\"ORTHOGRAPHIC\", 1);\n"
    "        cameraProjection_.SetTooltip(\n"
    "            \"Choose the selected scene camera's native projection mode.\");\n"
    "        cameraProjection_.OnSelect([this](const wi::gui::EventArgs& args)\n"
    "        {\n"
    "            ApplySelectedCameraProjection(args.userdata == 1);\n"
    "        });\n"
    "        inspectorPanel_.AddWidget(&cameraProjection_);\n\n"
    "        const auto createCameraSlider = [this](\n"
    "            SceneInspectorSlider& input,\n"
    "            const char* name,\n"
    "            const char* label,\n"
    "            const char* tooltip,\n"
    "            const CameraField field,\n"
    "            const float minimum,\n"
    "            const float maximum,\n"
    "            const float steps)\n"
    "        {\n"
    "            input.Create(minimum, maximum, 0.0f, steps, name, label);\n"
    "            input.SetTooltip(tooltip);\n"
    "            input.OnDragStarted([this, field](const float)\n"
    "            {\n"
    "                BeginCameraSlider(field);\n"
    "            });\n"
    "            input.OnValuePreview([this, field](const float value)\n"
    "            {\n"
    "                PreviewCameraSlider(field, value);\n"
    "            });\n"
    "            input.OnValueCommitted([this, field](const float value)\n"
    "            {\n"
    "                CommitCameraSlider(field, value);\n"
    "            });\n"
    "            inspectorPanel_.AddWidget(&input);\n"
    "        };\n"
    "        createCameraSlider(cameraFieldOfView_, \"Camera FOV\", \"FIELD OF VIEW\",\n"
    "            \"Perspective field of view in degrees.\",\n"
    "            CameraField::FieldOfView, 1.0f, 179.0f, 1780.0f);\n"
    "        createCameraSlider(cameraNearPlane_, \"Camera Near Plane\", \"NEAR CLIP\",\n"
    "            \"Geometry nearer than this distance is clipped.\",\n"
    "            CameraField::NearPlane, 0.001f, 10.0f, 10000.0f);\n"
    "        createCameraSlider(cameraFarPlane_, \"Camera Far Plane\", \"FAR CLIP\",\n"
    "            \"Geometry farther than this distance is clipped.\",\n"
    "            CameraField::FarPlane, 10.0f, 100000.0f, 100000.0f);\n"
    "        createCameraSlider(cameraFocalLength_, \"Camera Focal Length\", \"FOCAL DISTANCE\",\n"
    "            \"Depth-of-field focus distance.\",\n"
    "            CameraField::FocalLength, 0.001f, 1000.0f, 10000.0f);\n"
    "        createCameraSlider(cameraApertureSize_, \"Camera Aperture\", \"APERTURE\",\n"
    "            \"Depth-of-field aperture strength.\",\n"
    "            CameraField::ApertureSize, 0.0f, 1.0f, 1000.0f);\n"
    "        createCameraSlider(cameraOrthoVerticalSize_, \"Camera Ortho Size\", \"ORTHO // VERTICAL SIZE\",\n"
    "            \"Vertical size of the orthographic camera volume.\",\n"
    "            CameraField::OrthoVerticalSize, 0.01f, 10000.0f, 100000.0f);\n\n"
    "        cameraAlignToView_.Create(\"Align Camera To View\");\n"
    "        cameraAlignToView_.SetText(\"ALIGN CAMERA TO VIEW\");\n"
    "        cameraAlignToView_.SetTooltip(\n"
    "            \"Move the selected scene camera to the current editor viewpoint.\");\n"
    "        cameraAlignToView_.OnClick([this](const wi::gui::EventArgs&)\n"
    "        {\n"
    "            AlignSelectedCameraToView();\n"
    "        });\n"
    "        inspectorPanel_.AddWidget(&cameraAlignToView_);\n"
    "        cameraViewFrom_.Create(\"View From Camera\");\n"
    "        cameraViewFrom_.SetText(\"VIEW FROM CAMERA\");\n"
    "        cameraViewFrom_.SetTooltip(\n"
    "            \"Move the transient editor view to the selected scene camera without changing the scene.\");\n"
    "        cameraViewFrom_.OnClick([this](const wi::gui::EventArgs&)\n"
    "        {\n"
    "            ViewFromSelectedCamera();\n"
    "        });\n"
    "        inspectorPanel_.AddWidget(&cameraViewFrom_);\n\n"
    "        createSectionLabel(\n"
    "            lightLabel_,\n",
)

# Theme camera section label --------------------------------------------------
replace_once(
    "Studio/src/StudioApplication.cpp",
    "        ownLabel(scaleLabel_);\n        ownLabel(lightLabel_);\n",
    "        ownLabel(scaleLabel_);\n        ownLabel(cameraLabel_);\n        ownLabel(lightLabel_);\n",
)

# Inspector layout ------------------------------------------------------------
replace_once(
    "Studio/src/StudioApplication.cpp",
    "        layoutObjectToggle(sceneObjectReflections_, 0, 590.0f);\n"
    "        layoutObjectToggle(sceneObjectWetmap_, 1, 590.0f);\n\n"
    "        positionEnvironmentWidget(lightLabel_, 630.0f, 20.0f);\n",
    "        layoutObjectToggle(sceneObjectReflections_, 0, 590.0f);\n"
    "        layoutObjectToggle(sceneObjectWetmap_, 1, 590.0f);\n\n"
    "        positionEnvironmentWidget(cameraLabel_, 506.0f, 20.0f);\n"
    "        positionEnvironmentWidget(cameraProjection_, 526.0f);\n"
    "        positionEnvironmentWidget(cameraFieldOfView_, 560.0f);\n"
    "        positionEnvironmentWidget(cameraNearPlane_, 594.0f);\n"
    "        positionEnvironmentWidget(cameraFarPlane_, 628.0f);\n"
    "        positionEnvironmentWidget(cameraFocalLength_, 662.0f);\n"
    "        positionEnvironmentWidget(cameraApertureSize_, 696.0f);\n"
    "        positionEnvironmentWidget(cameraOrthoVerticalSize_, 730.0f);\n"
    "        const float cameraActionWidth = (environmentFieldWidth - 8.0f) * 0.5f;\n"
    "        cameraAlignToView_.SetPos(XMFLOAT2(12.0f, 764.0f));\n"
    "        cameraViewFrom_.SetPos(XMFLOAT2(20.0f + cameraActionWidth, 764.0f));\n"
    "        cameraAlignToView_.SetSize(XMFLOAT2(cameraActionWidth, 28.0f));\n"
    "        cameraViewFrom_.SetSize(XMFLOAT2(cameraActionWidth, 28.0f));\n\n"
    "        positionEnvironmentWidget(lightLabel_, 630.0f, 20.0f);\n",
)

# Inspector action row accounts for camera controls --------------------------
replace_once(
    "Studio/src/StudioApplication.cpp",
    "    void StudioRenderPath::LayoutInspectorActions(\n"
    "        const bool environment,\n"
    "        const bool terrain,\n"
    "        const bool light)\n",
    "    void StudioRenderPath::LayoutInspectorActions(\n"
    "        const bool environment,\n"
    "        const bool terrain,\n"
    "        const bool light,\n"
    "        const bool sceneCamera)\n",
)
replace_once(
    "Studio/src/StudioApplication.cpp",
    "                : light\n"
    "                    ? 1130.0f\n"
    "                    : 630.0f;\n",
    "                : light\n"
    "                    ? 1130.0f\n"
    "                    : sceneCamera\n"
    "                        ? 804.0f\n"
    "                        : 630.0f;\n",
)

# RefreshInspector camera discovery and presentation --------------------------
replace_once(
    "Studio/src/StudioApplication.cpp",
    "        SyncSelectionOutline();\n\n"
    "        const bool hasTransform = transform != nullptr;\n"
    "        const bool hasWeather = weather != nullptr;\n"
    "        const bool hasTerrain = terrain != nullptr;\n"
    "        const bool hasLight = light != nullptr;\n",
    "        auto* authoredCamera = hasSession && !environmentWorkspaceActive_ &&\n"
    "            !terrainWorkspaceActive_\n"
    "            ? session_->Scenes().GetScene().cameras.GetComponent(entity)\n"
    "            : nullptr;\n"
    "        SyncSelectionOutline();\n\n"
    "        const bool hasTransform = transform != nullptr;\n"
    "        const bool hasWeather = weather != nullptr;\n"
    "        const bool hasTerrain = terrain != nullptr;\n"
    "        const bool hasLight = light != nullptr;\n"
    "        const bool hasCamera = authoredCamera != nullptr;\n",
)
replace_once(
    "Studio/src/StudioApplication.cpp",
    "        LayoutInspectorActions(hasWeather, hasTerrain, hasLight);\n",
    "        LayoutInspectorActions(hasWeather, hasTerrain, hasLight, hasCamera);\n",
)
replace_once(
    "Studio/src/StudioApplication.cpp",
    "        sceneObjectWetmap_.SetVisible(sceneComponentsVisible && hasObjectTargets);\n\n"
    "        if (sceneComponentsVisible && sceneAuthoringRoot != wi::ecs::INVALID_ENTITY)\n",
    "        sceneObjectWetmap_.SetVisible(sceneComponentsVisible && hasObjectTargets);\n\n"
    "        cameraLabel_.SetVisible(hasCamera);\n"
    "        cameraProjection_.SetVisible(hasCamera);\n"
    "        cameraFieldOfView_.SetVisible(hasCamera);\n"
    "        cameraNearPlane_.SetVisible(hasCamera);\n"
    "        cameraFarPlane_.SetVisible(hasCamera);\n"
    "        cameraFocalLength_.SetVisible(hasCamera);\n"
    "        cameraApertureSize_.SetVisible(hasCamera);\n"
    "        cameraOrthoVerticalSize_.SetVisible(hasCamera);\n"
    "        cameraAlignToView_.SetVisible(hasCamera);\n"
    "        cameraViewFrom_.SetVisible(hasCamera);\n"
    "        if (hasCamera)\n"
    "        {\n"
    "            const auto cameraState = bridge::CaptureCamera(*authoredCamera);\n"
    "            cameraProjection_.SetSelectedByUserdataWithoutCallback(\n"
    "                cameraState.orthographic ? 1u : 0u);\n"
    "            cameraFieldOfView_.SetValue(cameraState.fieldOfViewDegrees);\n"
    "            cameraNearPlane_.SetValue(cameraState.nearPlane);\n"
    "            cameraFarPlane_.SetValue(cameraState.farPlane);\n"
    "            cameraFocalLength_.SetValue(cameraState.focalLength);\n"
    "            cameraApertureSize_.SetValue(cameraState.apertureSize);\n"
    "            cameraOrthoVerticalSize_.SetValue(cameraState.orthoVerticalSize);\n"
    "            cameraFieldOfView_.SetEnabled(!cameraState.orthographic);\n"
    "            cameraOrthoVerticalSize_.SetEnabled(cameraState.orthographic);\n"
    "        }\n\n"
    "        if (sceneComponentsVisible && sceneAuthoringRoot != wi::ecs::INVALID_ENTITY)\n",
)

# Camera implementation before existing light commit path --------------------
camera_methods = r'''
    bridge::TransformState StudioRenderPath::CaptureEditorCameraTransform() const
    {
        bridge::TransformState state;
        if (camera == nullptr)
            return state;
        wi::scene::TransformComponent transform;
        transform.MatrixTransform(camera->GetInvView());
        transform.UpdateTransform();
        return bridge::CaptureTransform(transform);
    }

    void StudioRenderPath::CreateCameraFromView()
    {
        if (session_ == nullptr || camera == nullptr)
            return;
        auto command = std::make_unique<bridge::CreateCameraCommand>(
            session_->Scenes().GetScene(),
            bridge::CaptureCamera(*camera),
            CaptureEditorCameraTransform(),
            camera->width,
            camera->height);
        auto* created = command.get();
        if (session_->Commands().Execute(std::move(command)))
        {
            session_->Selection().Select(created->CreatedEntity());
            SetEnvironmentWorkspaceActive(false);
            SetTerrainWorkspaceActive(false);
            RefreshHierarchy();
            RefreshInspector();
            RefreshStatus();
        }
    }

    bool StudioRenderPath::CommitSelectedCamera(
        const bridge::CameraState& cameraState)
    {
        if (session_ == nullptr)
            return false;
        const auto entity = session_->Selection().SelectedEntity();
        auto& scene = session_->Scenes().GetScene();
        if (!scene.cameras.Contains(entity))
            return false;
        const bool changed = session_->Commands().Execute(
            std::make_unique<bridge::SetCameraCommand>(
                scene, entity, cameraState));
        RefreshInspector();
        RefreshStatus();
        return changed;
    }

    void StudioRenderPath::ApplySelectedCameraProjection(
        const bool orthographic)
    {
        if (session_ == nullptr)
            return;
        const auto entity = session_->Selection().SelectedEntity();
        const auto* authoredCamera =
            session_->Scenes().GetScene().cameras.GetComponent(entity);
        if (authoredCamera == nullptr)
            return;
        auto state = bridge::CaptureCamera(*authoredCamera);
        state.orthographic = orthographic;
        CommitSelectedCamera(state);
    }

    void StudioRenderPath::SetCameraFieldValue(
        bridge::CameraState& cameraState,
        const CameraField field,
        const float value) noexcept
    {
        switch (field)
        {
        case CameraField::FieldOfView:
            cameraState.fieldOfViewDegrees = value;
            break;
        case CameraField::NearPlane:
            cameraState.nearPlane = value;
            break;
        case CameraField::FarPlane:
            cameraState.farPlane = value;
            break;
        case CameraField::FocalLength:
            cameraState.focalLength = value;
            break;
        case CameraField::ApertureSize:
            cameraState.apertureSize = value;
            break;
        case CameraField::OrthoVerticalSize:
            cameraState.orthoVerticalSize = value;
            break;
        }
        cameraState = bridge::SanitizeCameraState(cameraState);
    }

    void StudioRenderPath::BeginCameraSlider(const CameraField field)
    {
        cameraSliderActive_ = false;
        if (session_ == nullptr)
            return;
        const auto entity = session_->Selection().SelectedEntity();
        const auto* authoredCamera =
            session_->Scenes().GetScene().cameras.GetComponent(entity);
        if (authoredCamera == nullptr)
            return;
        cameraSliderActive_ = true;
        cameraSliderField_ = field;
        cameraSliderEntity_ = entity;
        cameraSliderBefore_ = bridge::CaptureCamera(*authoredCamera);
        cameraSliderAfter_ = cameraSliderBefore_;
    }

    void StudioRenderPath::PreviewCameraSlider(
        const CameraField field,
        const float value)
    {
        if (!cameraSliderActive_ || cameraSliderField_ != field ||
            session_ == nullptr)
            return;
        cameraSliderAfter_ = cameraSliderBefore_;
        SetCameraFieldValue(cameraSliderAfter_, field, value);
        auto* authoredCamera = session_->Scenes().GetScene().cameras.GetComponent(
            cameraSliderEntity_);
        if (authoredCamera != nullptr)
            bridge::ApplyCamera(*authoredCamera, cameraSliderAfter_);
    }

    void StudioRenderPath::CommitCameraSlider(
        const CameraField field,
        const float value)
    {
        if (!cameraSliderActive_ || cameraSliderField_ != field ||
            session_ == nullptr)
            return;
        SetCameraFieldValue(cameraSliderAfter_, field, value);
        auto& scene = session_->Scenes().GetScene();
        auto* authoredCamera = scene.cameras.GetComponent(cameraSliderEntity_);
        if (authoredCamera != nullptr)
            bridge::ApplyCamera(*authoredCamera, cameraSliderBefore_);
        (void)session_->Commands().Execute(
            std::make_unique<bridge::SetCameraCommand>(
                scene,
                cameraSliderEntity_,
                cameraSliderBefore_,
                cameraSliderAfter_));
        cameraSliderActive_ = false;
        cameraSliderEntity_ = wi::ecs::INVALID_ENTITY;
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::AlignSelectedCameraToView()
    {
        if (session_ == nullptr || camera == nullptr)
            return;
        const auto entity = session_->Selection().SelectedEntity();
        auto& scene = session_->Scenes().GetScene();
        auto* authoredCamera = scene.cameras.GetComponent(entity);
        if (authoredCamera == nullptr)
            return;
        if (session_->Commands().Execute(
                std::make_unique<bridge::SetTransformCommand>(
                    scene, entity, CaptureEditorCameraTransform())))
        {
            if (auto* transform = scene.transforms.GetComponent(entity))
            {
                authoredCamera->TransformCamera(*transform);
                authoredCamera->UpdateCamera();
            }
            RefreshInspector();
            RefreshStatus();
        }
    }

    void StudioRenderPath::ViewFromSelectedCamera()
    {
        if (session_ == nullptr || camera == nullptr)
            return;
        const auto entity = session_->Selection().SelectedEntity();
        const auto& scene = session_->Scenes().GetScene();
        const auto* authoredCamera = scene.cameras.GetComponent(entity);
        const auto* transform = scene.transforms.GetComponent(entity);
        if (authoredCamera == nullptr || transform == nullptr)
            return;
        bridge::ApplyCamera(*camera, bridge::CaptureCamera(*authoredCamera));
        camera->TransformCamera(*transform);
        camera->UpdateCamera();
        RefreshStatus();
    }

'''
replace_once(
    "Studio/src/StudioApplication.cpp",
    "    bool StudioRenderPath::CommitSelectedLight(\n",
    camera_methods + "    bool StudioRenderPath::CommitSelectedLight(\n",
)

# Remove this one-shot helper from the final tree.
Path("Tools/phase5_gate2_camera_patch.py").unlink()
print("Phase 5 Gate 2 camera patch applied")
