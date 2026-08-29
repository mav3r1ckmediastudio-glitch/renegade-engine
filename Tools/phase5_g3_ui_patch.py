from pathlib import Path


def patch(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected one anchor, found {count}: {old[:120]!r}")
    p.write_text(text.replace(old, new, 1), encoding="utf-8")


# Chrome actions + ADD menu.
patch(
    "Studio/src/RenegadeStudioChrome.h",
    "            CreateCamera,\n            ImportModel,",
    "            CreateCamera,\n            CreateDecal,\n            CreateEnvironmentProbe,\n            ImportModel,",
)

patch(
    "Studio/src/RenegadeStudioChrome.cpp",
    '                items = {{"POINT LIGHT", true}, {"SPOT LIGHT", true},\n'
    '                    {"DIRECTIONAL LIGHT", true}, {"RECTANGLE LIGHT", true},\n'
    '                    {"CAMERA", true}, {"IMPORT MODEL...", true}};',
    '                items = {{"POINT LIGHT", true}, {"SPOT LIGHT", true},\n'
    '                    {"DIRECTIONAL LIGHT", true}, {"RECTANGLE LIGHT", true},\n'
    '                    {"CAMERA", true}, {"DECAL", true},\n'
    '                    {"ENVIRONMENT PROBE", true}, {"IMPORT MODEL...", true}};',
)

patch(
    "Studio/src/RenegadeStudioChrome.cpp",
    "                        constexpr std::array<Action, 6> actions = {\n"
    "                            Action::CreatePointLight,\n"
    "                            Action::CreateSpotLight,\n"
    "                            Action::CreateDirectionalLight,\n"
    "                            Action::CreateRectangleLight,\n"
    "                            Action::CreateCamera,\n"
    "                            Action::ImportModel};",
    "                        constexpr std::array<Action, 8> actions = {\n"
    "                            Action::CreatePointLight,\n"
    "                            Action::CreateSpotLight,\n"
    "                            Action::CreateDirectionalLight,\n"
    "                            Action::CreateRectangleLight,\n"
    "                            Action::CreateCamera,\n"
    "                            Action::CreateDecal,\n"
    "                            Action::CreateEnvironmentProbe,\n"
    "                            Action::ImportModel};",
)

# Studio declarations.
patch(
    "Studio/src/StudioApplication.h",
    '#include "renegade/bridge/CameraService.h"\n#include "renegade/bridge/SceneComponentService.h"',
    '#include "renegade/bridge/CameraService.h"\n'
    '#include "renegade/bridge/DecalProbeService.h"\n'
    '#include "renegade/bridge/MaterialService.h"\n'
    '#include "renegade/bridge/SceneComponentService.h"',
)

patch(
    "Studio/src/StudioApplication.h",
    "            CreateLight,\n            CreateCamera,\n            OpenScene,",
    "            CreateLight,\n            CreateCamera,\n            CreateDecal,\n"
    "            CreateEnvironmentProbe,\n            OpenScene,",
)

patch(
    "Studio/src/StudioApplication.h",
    "        void CreateCameraFromView();\n"
    "        void AlignSelectedCameraToView();\n"
    "        void ViewFromSelectedCamera();\n"
    "        void CreateLight(",
    "        void CreateCameraFromView();\n"
    "        void AlignSelectedCameraToView();\n"
    "        void ViewFromSelectedCamera();\n"
    "        void CreateDecalFromView();\n"
    "        void CreateEnvironmentProbeFromView();\n"
    "        bool CommitSelectedDecal(const bridge::DecalState& state);\n"
    "        bool CommitSelectedEnvironmentProbe(\n"
    "            const bridge::EnvironmentProbeState& state);\n"
    "        void CreateLight(",
)

patch(
    "Studio/src/StudioApplication.h",
    "        bool HandleCameraSceneIcons(const XMFLOAT4& pointer);\n"
    "        bool HandleLightSceneIcons(const XMFLOAT4& pointer);",
    "        bool HandleCameraSceneIcons(const XMFLOAT4& pointer);\n"
    "        bool HandleDecalProbeSceneIcons(const XMFLOAT4& pointer);\n"
    "        bool HandleLightSceneIcons(const XMFLOAT4& pointer);",
)

patch(
    "Studio/src/StudioApplication.h",
    "        SceneInspectorButton cameraAlignToView_;\n"
    "        SceneInspectorButton cameraViewFrom_;\n"
    "        wi::gui::Label lightLabel_;",
    "        SceneInspectorButton cameraAlignToView_;\n"
    "        SceneInspectorButton cameraViewFrom_;\n"
    "        wi::gui::Label decalLabel_;\n"
    "        SceneInspectorCheckBox decalBaseColorOnlyAlpha_;\n"
    "        SceneInspectorSlider decalSlopeBlend_;\n"
    "        wi::gui::Label decalMaterialLabel_;\n"
    "        SceneInspectorSlider decalBaseColorRed_;\n"
    "        SceneInspectorSlider decalBaseColorGreen_;\n"
    "        SceneInspectorSlider decalBaseColorBlue_;\n"
    "        SceneInspectorSlider decalOpacity_;\n"
    "        wi::gui::Label environmentProbeLabel_;\n"
    "        SceneInspectorComboBox environmentProbeResolution_;\n"
    "        SceneInspectorCheckBox environmentProbeRealtime_;\n"
    "        SceneInspectorSlider environmentProbeInterval_;\n"
    "        SceneInspectorCheckBox environmentProbeMsaa_;\n"
    "        SceneInspectorSlider environmentProbeViewDistance_;\n"
    "        SceneInspectorButton environmentProbeRefresh_;\n"
    "        wi::gui::Label lightLabel_;",
)

# Chrome action routing + pending action execution.
patch(
    "Studio/src/StudioApplication.cpp",
    "            case RenegadeStudioChrome::Action::CreateCamera:\n"
    "                pendingAction_ = EditorAction::CreateCamera;\n"
    "                break;\n"
    "            case RenegadeStudioChrome::Action::Focus:",
    "            case RenegadeStudioChrome::Action::CreateCamera:\n"
    "                pendingAction_ = EditorAction::CreateCamera;\n"
    "                break;\n"
    "            case RenegadeStudioChrome::Action::CreateDecal:\n"
    "                pendingAction_ = EditorAction::CreateDecal;\n"
    "                break;\n"
    "            case RenegadeStudioChrome::Action::CreateEnvironmentProbe:\n"
    "                pendingAction_ = EditorAction::CreateEnvironmentProbe;\n"
    "                break;\n"
    "            case RenegadeStudioChrome::Action::Focus:",
)

patch(
    "Studio/src/StudioApplication.cpp",
    "        case EditorAction::CreateCamera:\n"
    "            CreateCameraFromView();\n"
    "            break;\n"
    "        case EditorAction::OpenScene:",
    "        case EditorAction::CreateCamera:\n"
    "            CreateCameraFromView();\n"
    "            break;\n"
    "        case EditorAction::CreateDecal:\n"
    "            CreateDecalFromView();\n"
    "            break;\n"
    "        case EditorAction::CreateEnvironmentProbe:\n"
    "            CreateEnvironmentProbeFromView();\n"
    "            break;\n"
    "        case EditorAction::OpenScene:",
)

# Inspector controls.
inspector_block = r'''        inspectorPanel_.AddWidget(&cameraViewFrom_);

        createSectionLabel(
            decalLabel_,
            "Decal Section",
            "DECAL // NATIVE WICKED");
        decalBaseColorOnlyAlpha_.Create("Base color alpha only: ");
        decalBaseColorOnlyAlpha_.SetTooltip(
            "Use only base-colour alpha while preserving normal/surface decal detail.");
        decalBaseColorOnlyAlpha_.OnClick([this](const wi::gui::EventArgs& args)
        {
            if (session_ == nullptr)
                return;
            const auto entity = session_->Selection().SelectedEntity();
            auto* decal = session_->Scenes().GetScene().decals.GetComponent(entity);
            if (decal == nullptr)
                return;
            auto state = bridge::CaptureDecal(*decal);
            state.baseColorOnlyAlpha = args.bValue;
            CommitSelectedDecal(state);
        });
        inspectorPanel_.AddWidget(&decalBaseColorOnlyAlpha_);

        decalSlopeBlend_.Create(
            0.0f, 8.0f, 0.0f, 801.0f,
            "Decal Slope Blend", "SLOPE BLEND");
        decalSlopeBlend_.SetTooltip(
            "Blend decal projection by receiving-surface slope. Zero disables slope rejection.");
        decalSlopeBlend_.OnValueCommitted([this](float value)
        {
            if (session_ == nullptr)
                return;
            const auto entity = session_->Selection().SelectedEntity();
            auto* decal = session_->Scenes().GetScene().decals.GetComponent(entity);
            if (decal == nullptr)
                return;
            auto state = bridge::CaptureDecal(*decal);
            state.slopeBlendPower = value;
            CommitSelectedDecal(state);
        });
        inspectorPanel_.AddWidget(&decalSlopeBlend_);

        createSectionLabel(
            decalMaterialLabel_,
            "Decal Material Section",
            "MATERIAL // RENEGRADE CORE");
        const auto createDecalMaterialSlider = [this](
            SceneInspectorSlider& slider,
            const char* name,
            const char* label,
            const int component)
        {
            slider.Create(0.0f, 1.0f, 1.0f, 1001.0f, name, label);
            slider.OnValueCommitted([this, component](float value)
            {
                if (session_ == nullptr)
                    return;
                const auto selected = session_->Selection().SelectedEntity();
                auto& scene = session_->Scenes().GetScene();
                if (!scene.decals.Contains(selected))
                    return;
                const auto materialEntity =
                    bridge::ResolveEditableMaterialEntity(scene, selected);
                auto* material = scene.materials.GetComponent(materialEntity);
                if (material == nullptr)
                    return;
                auto state = bridge::CaptureMaterial(*material);
                if (component == 0)
                    state.baseColor.x = value;
                else if (component == 1)
                    state.baseColor.y = value;
                else if (component == 2)
                    state.baseColor.z = value;
                else
                    state.baseColor.w = value;
                (void)session_->Commands().Execute(
                    std::make_unique<bridge::SetMaterialCommand>(
                        scene, materialEntity, state));
                RefreshInspector();
                RefreshStatus();
            });
            inspectorPanel_.AddWidget(&slider);
        };
        createDecalMaterialSlider(
            decalBaseColorRed_, "Decal Material Red", "BASE COLOR // R", 0);
        createDecalMaterialSlider(
            decalBaseColorGreen_, "Decal Material Green", "BASE COLOR // G", 1);
        createDecalMaterialSlider(
            decalBaseColorBlue_, "Decal Material Blue", "BASE COLOR // B", 2);
        createDecalMaterialSlider(
            decalOpacity_, "Decal Material Opacity", "OPACITY", 3);

        createSectionLabel(
            environmentProbeLabel_,
            "Environment Probe Section",
            "ENVIRONMENT PROBE // NATIVE WICKED");
        environmentProbeResolution_.Create("Probe Resolution");
        for (const std::uint64_t resolution :
            {32ull, 64ull, 128ull, 256ull, 512ull, 1024ull, 2048ull})
        {
            environmentProbeResolution_.AddItem(
                std::to_string(resolution), resolution);
        }
        environmentProbeResolution_.SetTooltip(
            "Cubemap face resolution. Higher values cost more GPU memory and capture time.");
        environmentProbeResolution_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            if (session_ == nullptr)
                return;
            const auto entity = session_->Selection().SelectedEntity();
            auto* probe = session_->Scenes().GetScene().probes.GetComponent(entity);
            if (probe == nullptr)
                return;
            auto state = bridge::CaptureEnvironmentProbe(*probe);
            state.resolution = static_cast<std::uint32_t>(args.userdata);
            CommitSelectedEnvironmentProbe(state);
        });
        inspectorPanel_.AddWidget(&environmentProbeResolution_);

        environmentProbeRealtime_.Create("Real-time update: ");
        environmentProbeRealtime_.SetTooltip(
            "Continuously recapture this probe using the configured interval.");
        environmentProbeRealtime_.OnClick([this](const wi::gui::EventArgs& args)
        {
            if (session_ == nullptr)
                return;
            const auto entity = session_->Selection().SelectedEntity();
            auto* probe = session_->Scenes().GetScene().probes.GetComponent(entity);
            if (probe == nullptr)
                return;
            auto state = bridge::CaptureEnvironmentProbe(*probe);
            state.realTime = args.bValue;
            CommitSelectedEnvironmentProbe(state);
        });
        inspectorPanel_.AddWidget(&environmentProbeRealtime_);

        environmentProbeInterval_.Create(
            0.0f, 60.0f, 0.0f, 601.0f,
            "Probe Update Interval", "UPDATE INTERVAL // S");
        environmentProbeInterval_.OnValueCommitted([this](float value)
        {
            if (session_ == nullptr)
                return;
            const auto entity = session_->Selection().SelectedEntity();
            auto* probe = session_->Scenes().GetScene().probes.GetComponent(entity);
            if (probe == nullptr)
                return;
            auto state = bridge::CaptureEnvironmentProbe(*probe);
            state.updateInterval = value;
            CommitSelectedEnvironmentProbe(state);
        });
        inspectorPanel_.AddWidget(&environmentProbeInterval_);

        environmentProbeMsaa_.Create("8x MSAA capture: ");
        environmentProbeMsaa_.SetTooltip(
            "Use Wicked's native 8-sample MSAA environment-probe capture path.");
        environmentProbeMsaa_.OnClick([this](const wi::gui::EventArgs& args)
        {
            if (session_ == nullptr)
                return;
            const auto entity = session_->Selection().SelectedEntity();
            auto* probe = session_->Scenes().GetScene().probes.GetComponent(entity);
            if (probe == nullptr)
                return;
            auto state = bridge::CaptureEnvironmentProbe(*probe);
            state.msaa = args.bValue;
            CommitSelectedEnvironmentProbe(state);
        });
        inspectorPanel_.AddWidget(&environmentProbeMsaa_);

        environmentProbeViewDistance_.Create(
            -1.0f, 5000.0f, -1.0f, 5002.0f,
            "Probe View Distance", "VIEW DISTANCE // -1 = CAMERA");
        environmentProbeViewDistance_.OnValueCommitted([this](float value)
        {
            if (session_ == nullptr)
                return;
            const auto entity = session_->Selection().SelectedEntity();
            auto* probe = session_->Scenes().GetScene().probes.GetComponent(entity);
            if (probe == nullptr)
                return;
            auto state = bridge::CaptureEnvironmentProbe(*probe);
            state.viewDistance = value < 0.0f ? -1.0f : value;
            CommitSelectedEnvironmentProbe(state);
        });
        inspectorPanel_.AddWidget(&environmentProbeViewDistance_);

        environmentProbeRefresh_.Create("Refresh Environment Probe");
        environmentProbeRefresh_.SetText("REFRESH PROBE");
        environmentProbeRefresh_.SetTooltip(
            "Discard the generated cubemap and force Wicked to recapture this probe.");
        environmentProbeRefresh_.OnClick([this](const wi::gui::EventArgs&)
        {
            if (session_ == nullptr)
                return;
            const auto entity = session_->Selection().SelectedEntity();
            if (bridge::RefreshEnvironmentProbe(
                    session_->Scenes().GetScene(), entity))
            {
                statusOverride_ = "ENVIRONMENT PROBE REFRESH REQUESTED";
                RefreshInspector();
                RefreshStatus();
            }
        });
        inspectorPanel_.AddWidget(&environmentProbeRefresh_);

        createSectionLabel(
            lightLabel_,'''
patch(
    "Studio/src/StudioApplication.cpp",
    "        inspectorPanel_.AddWidget(&cameraViewFrom_);\n\n"
    "        createSectionLabel(\n            lightLabel_,",
    inspector_block,
)

# Theme and layout.
patch(
    "Studio/src/StudioApplication.cpp",
    "        ownLabel(cameraLabel_);\n        ownLabel(lightLabel_);",
    "        ownLabel(cameraLabel_);\n"
    "        ownLabel(decalLabel_);\n"
    "        ownLabel(decalMaterialLabel_);\n"
    "        ownLabel(environmentProbeLabel_);\n"
    "        ownLabel(lightLabel_);",
)

layout_block = r'''        cameraAlignToView_.SetSize(XMFLOAT2(cameraActionWidth, 28.0f));
        cameraViewFrom_.SetSize(XMFLOAT2(cameraActionWidth, 28.0f));

        positionEnvironmentWidget(decalLabel_, 506.0f, 20.0f);
        positionEnvironmentWidget(decalBaseColorOnlyAlpha_, 528.0f);
        positionEnvironmentWidget(decalSlopeBlend_, 562.0f);
        positionEnvironmentWidget(decalMaterialLabel_, 596.0f, 20.0f);
        positionEnvironmentWidget(decalBaseColorRed_, 620.0f);
        positionEnvironmentWidget(decalBaseColorGreen_, 654.0f);
        positionEnvironmentWidget(decalBaseColorBlue_, 688.0f);
        positionEnvironmentWidget(decalOpacity_, 722.0f);

        positionEnvironmentWidget(environmentProbeLabel_, 506.0f, 20.0f);
        positionEnvironmentWidget(environmentProbeResolution_, 530.0f);
        positionEnvironmentWidget(environmentProbeRealtime_, 564.0f);
        positionEnvironmentWidget(environmentProbeInterval_, 598.0f);
        positionEnvironmentWidget(environmentProbeMsaa_, 632.0f);
        positionEnvironmentWidget(environmentProbeViewDistance_, 666.0f);
        positionEnvironmentWidget(environmentProbeRefresh_, 708.0f);

        positionEnvironmentWidget(lightLabel_, 630.0f, 20.0f);'''
patch(
    "Studio/src/StudioApplication.cpp",
    "        cameraAlignToView_.SetSize(XMFLOAT2(cameraActionWidth, 28.0f));\n"
    "        cameraViewFrom_.SetSize(XMFLOAT2(cameraActionWidth, 28.0f));\n\n"
    "        positionEnvironmentWidget(lightLabel_, 630.0f, 20.0f);",
    layout_block,
)

# Creation and command helpers.
creation_block = r'''    void StudioRenderPath::CreateDecalFromView()
    {
        if (session_ == nullptr || camera == nullptr)
            return;
        auto transform = CaptureEditorCameraTransform();
        const XMVECTOR forward = XMVector3Rotate(
            XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
            XMLoadFloat4(&transform.rotation));
        XMFLOAT3 direction = {};
        XMStoreFloat3(&direction, XMVector3Normalize(forward));
        transform.translation.x += direction.x * 4.0f;
        transform.translation.y += direction.y * 4.0f;
        transform.translation.z += direction.z * 4.0f;
        transform.scale = XMFLOAT3(1.5f, 1.5f, 0.5f);

        auto command = std::make_unique<bridge::CreateDecalCommand>(
            session_->Scenes().GetScene(), bridge::DecalState{}, transform);
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

    void StudioRenderPath::CreateEnvironmentProbeFromView()
    {
        if (session_ == nullptr || camera == nullptr)
            return;
        auto transform = CaptureEditorCameraTransform();
        const XMVECTOR forward = XMVector3Rotate(
            XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
            XMLoadFloat4(&transform.rotation));
        XMFLOAT3 direction = {};
        XMStoreFloat3(&direction, XMVector3Normalize(forward));
        transform.translation.x += direction.x * 5.0f;
        transform.translation.y += direction.y * 5.0f;
        transform.translation.z += direction.z * 5.0f;
        transform.rotation = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        transform.scale = XMFLOAT3(5.0f, 5.0f, 5.0f);

        auto command = std::make_unique<bridge::CreateEnvironmentProbeCommand>(
            session_->Scenes().GetScene(),
            bridge::EnvironmentProbeState{}, transform);
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

    bool StudioRenderPath::CommitSelectedDecal(
        const bridge::DecalState& state)
    {
        if (session_ == nullptr)
            return false;
        const auto entity = session_->Selection().SelectedEntity();
        auto& scene = session_->Scenes().GetScene();
        if (!scene.decals.Contains(entity))
            return false;
        const bool changed = session_->Commands().Execute(
            std::make_unique<bridge::SetDecalCommand>(scene, entity, state));
        RefreshInspector();
        RefreshStatus();
        return changed;
    }

    bool StudioRenderPath::CommitSelectedEnvironmentProbe(
        const bridge::EnvironmentProbeState& state)
    {
        if (session_ == nullptr)
            return false;
        const auto entity = session_->Selection().SelectedEntity();
        auto& scene = session_->Scenes().GetScene();
        if (!scene.probes.Contains(entity))
            return false;
        const bool changed = session_->Commands().Execute(
            std::make_unique<bridge::SetEnvironmentProbeCommand>(
                scene, entity, state));
        RefreshInspector();
        RefreshStatus();
        return changed;
    }

    void StudioRenderPath::CreateLight(
        const wi::scene::LightComponent::LightType type)'''
patch(
    "Studio/src/StudioApplication.cpp",
    "    void StudioRenderPath::CreateLight(\n"
    "        const wi::scene::LightComponent::LightType type)",
    creation_block,
)

# RefreshInspector pointers and state.
patch(
    "Studio/src/StudioApplication.cpp",
    "        auto* authoredCamera = hasSession && !environmentWorkspaceActive_ &&\n"
    "            !terrainWorkspaceActive_\n"
    "            ? session_->Scenes().GetScene().cameras.GetComponent(entity)\n"
    "            : nullptr;\n"
    "        SyncSelectionOutline();",
    "        auto* authoredCamera = hasSession && !environmentWorkspaceActive_ &&\n"
    "            !terrainWorkspaceActive_\n"
    "            ? session_->Scenes().GetScene().cameras.GetComponent(entity)\n"
    "            : nullptr;\n"
    "        auto* decal = hasSession && !environmentWorkspaceActive_ &&\n"
    "            !terrainWorkspaceActive_\n"
    "            ? session_->Scenes().GetScene().decals.GetComponent(entity)\n"
    "            : nullptr;\n"
    "        auto* environmentProbe = hasSession && !environmentWorkspaceActive_ &&\n"
    "            !terrainWorkspaceActive_\n"
    "            ? session_->Scenes().GetScene().probes.GetComponent(entity)\n"
    "            : nullptr;\n"
    "        auto* decalMaterial = hasSession && decal != nullptr\n"
    "            ? session_->Scenes().GetScene().materials.GetComponent(entity)\n"
    "            : nullptr;\n"
    "        SyncSelectionOutline();",
)

patch(
    "Studio/src/StudioApplication.cpp",
    "        const bool hasLight = light != nullptr;\n"
    "        const bool hasCamera = authoredCamera != nullptr;\n"
    "        const bool sceneComponentsVisible =",
    "        const bool hasLight = light != nullptr;\n"
    "        const bool hasCamera = authoredCamera != nullptr;\n"
    "        const bool hasDecal = decal != nullptr;\n"
    "        const bool hasEnvironmentProbe = environmentProbe != nullptr;\n"
    "        const bool sceneComponentsVisible =",
)

patch(
    "Studio/src/StudioApplication.cpp",
    "        LayoutInspectorActions(hasWeather, hasTerrain, hasLight, hasCamera);",
    "        LayoutInspectorActions(\n"
    "            hasWeather, hasTerrain, hasLight,\n"
    "            hasCamera || hasDecal || hasEnvironmentProbe);",
)

visibility_block = r'''        cameraAlignToView_.SetVisible(hasCamera);
        cameraViewFrom_.SetVisible(hasCamera);

        decalLabel_.SetVisible(hasDecal);
        decalBaseColorOnlyAlpha_.SetVisible(hasDecal);
        decalSlopeBlend_.SetVisible(hasDecal);
        decalMaterialLabel_.SetVisible(hasDecal && decalMaterial != nullptr);
        decalBaseColorRed_.SetVisible(hasDecal && decalMaterial != nullptr);
        decalBaseColorGreen_.SetVisible(hasDecal && decalMaterial != nullptr);
        decalBaseColorBlue_.SetVisible(hasDecal && decalMaterial != nullptr);
        decalOpacity_.SetVisible(hasDecal && decalMaterial != nullptr);
        if (hasDecal)
        {
            const auto state = bridge::CaptureDecal(*decal);
            decalBaseColorOnlyAlpha_.SetCheck(state.baseColorOnlyAlpha);
            decalSlopeBlend_.SetValue(state.slopeBlendPower);
            if (decalMaterial != nullptr)
            {
                const auto material = bridge::CaptureMaterial(*decalMaterial);
                decalBaseColorRed_.SetValue(material.baseColor.x);
                decalBaseColorGreen_.SetValue(material.baseColor.y);
                decalBaseColorBlue_.SetValue(material.baseColor.z);
                decalOpacity_.SetValue(material.baseColor.w);
            }
        }

        environmentProbeLabel_.SetVisible(hasEnvironmentProbe);
        environmentProbeResolution_.SetVisible(hasEnvironmentProbe);
        environmentProbeRealtime_.SetVisible(hasEnvironmentProbe);
        environmentProbeInterval_.SetVisible(hasEnvironmentProbe);
        environmentProbeMsaa_.SetVisible(hasEnvironmentProbe);
        environmentProbeViewDistance_.SetVisible(hasEnvironmentProbe);
        environmentProbeRefresh_.SetVisible(hasEnvironmentProbe);
        if (hasEnvironmentProbe)
        {
            const auto state = bridge::CaptureEnvironmentProbe(*environmentProbe);
            environmentProbeResolution_.SetSelectedByUserdataWithoutCallback(
                state.resolution);
            environmentProbeRealtime_.SetCheck(state.realTime);
            environmentProbeInterval_.SetValue(state.updateInterval);
            environmentProbeMsaa_.SetCheck(state.msaa);
            environmentProbeViewDistance_.SetValue(state.viewDistance);
        }

        if (hasCamera)'''
patch(
    "Studio/src/StudioApplication.cpp",
    "        cameraAlignToView_.SetVisible(hasCamera);\n"
    "        cameraViewFrom_.SetVisible(hasCamera);\n"
    "        if (hasCamera)",
    visibility_block,
)

# Editor-only world markers and transformed selection volume.
marker_block = r'''bool StudioRenderPath::HandleDecalProbeSceneIcons(
    const XMFLOAT4& pointer)
{
    if (session_ == nullptr || camera == nullptr || projectHubVisible_ ||
        creatorModelImporter.thumbnailCapturePending)
    {
        return false;
    }

    auto& scene = session_->Scenes().GetScene();
    const bool canSelect = !lightPlacementActive_ && !flyCameraActive_ &&
        !GetGUI().HasFocus() && !gizmo_.IsInteracting() &&
        IsPointerOverViewport(pointer) &&
        wi::input::Press(wi::input::MOUSE_BUTTON_LEFT);
    wi::ecs::Entity best = wi::ecs::INVALID_ENTITY;
    float bestDistanceSquared = 22.0f * 22.0f;
    const auto selected = session_->Selection().SelectedEntity();

    const auto drawVolume = [this](
        const wi::scene::TransformComponent& transform,
        const XMFLOAT4& color)
    {
        constexpr XMFLOAT3 local[8] = {
            XMFLOAT3(-1, -1, -1), XMFLOAT3(1, -1, -1),
            XMFLOAT3(1, 1, -1), XMFLOAT3(-1, 1, -1),
            XMFLOAT3(-1, -1, 1), XMFLOAT3(1, -1, 1),
            XMFLOAT3(1, 1, 1), XMFLOAT3(-1, 1, 1)};
        constexpr int edges[12][2] = {
            {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},
            {0,4},{1,5},{2,6},{3,7}};
        XMFLOAT2 projected[8] = {};
        bool visible[8] = {};
        const XMMATRIX world = transform.GetWorldMatrix();
        for (int index = 0; index < 8; ++index)
        {
            XMFLOAT3 worldPoint = {};
            XMStoreFloat3(
                &worldPoint,
                XMVector3TransformCoord(XMLoadFloat3(&local[index]), world));
            visible[index] = ProjectEditorPoint(worldPoint, projected[index]);
        }
        for (const auto& edge : edges)
        {
            if (visible[edge[0]] && visible[edge[1]])
                DrawEditorLine(projected[edge[0]], projected[edge[1]], color);
        }
    };

    const auto drawEntity = [&](
        const wi::ecs::Entity entity,
        const bool probe)
    {
        if (!session_->Scenes().IsHierarchyVisible(entity))
            return;
        const auto* transform = scene.transforms.GetComponent(entity);
        if (transform == nullptr)
            return;
        XMFLOAT2 center = {};
        if (!ProjectEditorPoint(transform->GetPosition(), center))
            return;
        const XMFLOAT4 color = probe
            ? XMFLOAT4(0.20f, 0.92f, 1.0f, 0.95f)
            : XMFLOAT4(1.0f, 0.48f, 0.10f, 0.95f);
        constexpr float radius = 8.0f;
        if (probe)
        {
            DrawEditorLine(
                XMFLOAT2(center.x - radius, center.y - radius),
                XMFLOAT2(center.x + radius, center.y - radius), color);
            DrawEditorLine(
                XMFLOAT2(center.x + radius, center.y - radius),
                XMFLOAT2(center.x + radius, center.y + radius), color);
            DrawEditorLine(
                XMFLOAT2(center.x + radius, center.y + radius),
                XMFLOAT2(center.x - radius, center.y + radius), color);
            DrawEditorLine(
                XMFLOAT2(center.x - radius, center.y + radius),
                XMFLOAT2(center.x - radius, center.y - radius), color);
            DrawEditorLine(
                XMFLOAT2(center.x - radius, center.y),
                XMFLOAT2(center.x + radius, center.y), color);
            DrawEditorLine(
                XMFLOAT2(center.x, center.y - radius),
                XMFLOAT2(center.x, center.y + radius), color);
        }
        else
        {
            DrawEditorLine(
                XMFLOAT2(center.x, center.y - radius - 3.0f),
                XMFLOAT2(center.x + radius + 3.0f, center.y), color);
            DrawEditorLine(
                XMFLOAT2(center.x + radius + 3.0f, center.y),
                XMFLOAT2(center.x, center.y + radius + 3.0f), color);
            DrawEditorLine(
                XMFLOAT2(center.x, center.y + radius + 3.0f),
                XMFLOAT2(center.x - radius - 3.0f, center.y), color);
            DrawEditorLine(
                XMFLOAT2(center.x - radius - 3.0f, center.y),
                XMFLOAT2(center.x, center.y - radius - 3.0f), color);
        }
        if (selected == entity)
        {
            drawVolume(
                *transform,
                XMFLOAT4(color.x, color.y, color.z, 0.72f));
        }
        if (canSelect)
        {
            const float dx = pointer.x - center.x;
            const float dy = pointer.y - center.y;
            const float distanceSquared = dx * dx + dy * dy;
            if (distanceSquared < bestDistanceSquared)
            {
                bestDistanceSquared = distanceSquared;
                best = entity;
            }
        }
    };

    for (std::size_t index = 0; index < scene.decals.GetCount(); ++index)
        drawEntity(scene.decals.GetEntity(index), false);
    for (std::size_t index = 0; index < scene.probes.GetCount(); ++index)
        drawEntity(scene.probes.GetEntity(index), true);

    if (canSelect && best != wi::ecs::INVALID_ENTITY)
    {
        session_->Selection().Select(best);
        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
        return true;
    }
    return false;
}

bool StudioRenderPath::HandleCameraSceneIcons(
    const XMFLOAT4& pointer)'''
patch(
    "Studio/src/StudioApplication.cpp",
    "bool StudioRenderPath::HandleCameraSceneIcons(\n    const XMFLOAT4& pointer)",
    marker_block,
)

patch(
    "Studio/src/StudioApplication.cpp",
    "        const bool cameraIconConsumed = HandleCameraSceneIcons(pointer);\n"
    "        const bool lightIconConsumed = HandleLightSceneIcons(pointer);",
    "        const bool cameraIconConsumed = HandleCameraSceneIcons(pointer);\n"
    "        const bool decalProbeIconConsumed = HandleDecalProbeSceneIcons(pointer);\n"
    "        const bool lightIconConsumed = HandleLightSceneIcons(pointer);",
)

patch(
    "Studio/src/StudioApplication.cpp",
    "        if (cameraIconConsumed || lightIconConsumed)\n        {",
    "        if (cameraIconConsumed || decalProbeIconConsumed ||\n"
    "            lightIconConsumed)\n        {",
)

# Tighten source contract now that Studio UI is mandatory.
contract = Path("Tests/Phase5Gate3SourceContract.cmake")
text = contract.read_text(encoding="utf-8")
old = '''# Studio-facing checks become active once the Gate 3 UI integration lands.
if(EXISTS "${studio_source}" AND EXISTS "${studio_header}" AND EXISTS "${chrome_source}")
    file(READ "${studio_source}" studio_text)
    if(studio_text MATCHES "DECAL // NATIVE WICKED" OR
       studio_text MATCHES "ENVIRONMENT PROBE // NATIVE WICKED")
        foreach(token IN ITEMS
            "DECAL // NATIVE WICKED"
            "ENVIRONMENT PROBE // NATIVE WICKED"
            "RefreshEnvironmentProbe")
            string(FIND "${studio_text}" "${token}" found)
            if(found EQUAL -1)
                message(FATAL_ERROR "Gate 3 partial Studio integration missing ${token}")
            endif()
        endforeach()
    endif()
endif()
'''
new = '''foreach(path IN ITEMS "${studio_source}" "${studio_header}" "${chrome_source}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Gate 3 missing Studio source: ${path}")
    endif()
endforeach()
file(READ "${studio_source}" studio_text)
file(READ "${studio_header}" studio_header_text)
file(READ "${chrome_source}" chrome_text)
foreach(token IN ITEMS
    "DECAL // NATIVE WICKED"
    "ENVIRONMENT PROBE // NATIVE WICKED"
    "CreateDecalFromView"
    "CreateEnvironmentProbeFromView"
    "HandleDecalProbeSceneIcons"
    "RefreshEnvironmentProbe"
    "SetMaterialCommand")
    string(FIND "${studio_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 3 Studio integration missing ${token}")
    endif()
endforeach()
foreach(token IN ITEMS
    "CreateDecal"
    "CreateEnvironmentProbe"
    "DecalProbeService.h")
    string(FIND "${studio_header_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 3 Studio header missing ${token}")
    endif()
endforeach()
foreach(token IN ITEMS
    "DECAL"
    "ENVIRONMENT PROBE"
    "Action::CreateDecal"
    "Action::CreateEnvironmentProbe")
    string(FIND "${chrome_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 3 ADD menu missing ${token}")
    endif()
endforeach()
'''
if text.count(old) != 1:
    raise SystemExit("Gate 3 source contract anchor changed")
contract.write_text(text.replace(old, new, 1), encoding="utf-8")
