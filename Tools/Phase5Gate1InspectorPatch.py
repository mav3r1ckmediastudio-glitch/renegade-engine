from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected exactly one match, found {count}")
    return text.replace(old, new, 1)


# Build graph -----------------------------------------------------------------
path = Path("EngineBridge/CMakeLists.txt")
text = path.read_text(encoding="utf-8")
text = replace_once(
    text,
    "    include/renegade/bridge/SceneService.h\n",
    "    include/renegade/bridge/SceneService.h\n    include/renegade/bridge/SceneComponentService.h\n",
    "EngineBridge header registration",
)
text = replace_once(
    text,
    "    src/SceneService.cpp\n",
    "    src/SceneService.cpp\n    src/SceneComponentService.cpp\n",
    "EngineBridge source registration",
)
path.write_text(text, encoding="utf-8")

path = Path("CMakeLists.txt")
text = path.read_text(encoding="utf-8")
text = replace_once(
    text,
    "    include(Tests/SceneUiGate2.cmake)\n",
    "    include(Tests/Phase5Gate1.cmake)\n    include(Tests/SceneUiGate2.cmake)\n",
    "Phase 5 Gate 1 test registration",
)
path.write_text(text, encoding="utf-8")

# Backend: bit-preserving layer edit ------------------------------------------
path = Path("EngineBridge/include/renegade/bridge/SceneComponentService.h")
text = path.read_text(encoding="utf-8")
marker = "    class SetMetadataPresetCommand final : public ICommand\n"
addition = r'''    class SetSceneLayerBitCommand final : public ICommand
    {
    public:
        SetSceneLayerBitCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity selected,
            std::uint32_t bit,
            bool enabled);

        bool Execute() override;
        void Undo() override;

    private:
        struct TargetState
        {
            wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
            std::uint32_t mask = ~0u;
            std::uint32_t hierarchyBind = ~0u;
            bool hadLayer = false;
            bool hadHierarchy = false;
        };

        bool Apply(bool enabled);

        wi::scene::Scene* scene_ = nullptr;
        std::vector<TargetState> before_;
        std::uint32_t bit_ = 0;
        bool after_ = false;
    };

'''
text = replace_once(text, marker, addition + marker, "layer bit command declaration")
path.write_text(text, encoding="utf-8")

path = Path("EngineBridge/src/SceneComponentService.cpp")
text = path.read_text(encoding="utf-8")
marker = "    SetMetadataPresetCommand::SetMetadataPresetCommand(\n"
addition = r'''    SetSceneLayerBitCommand::SetSceneLayerBitCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity selected,
        const std::uint32_t bit,
        const bool enabled)
        : scene_(&scene)
        , bit_(bit)
        , after_(enabled)
    {
        if (bit_ >= 32u)
            return;
        const auto targets = CollectLayerTargets(scene, selected);
        before_.reserve(targets.size());
        for (const wi::ecs::Entity entity : targets)
        {
            TargetState state;
            state.entity = entity;
            if (const auto* layer = scene.layers.GetComponent(entity); layer != nullptr)
            {
                state.hadLayer = true;
                state.mask = layer->layerMask;
            }
            if (const auto* hierarchy = scene.hierarchy.GetComponent(entity);
                hierarchy != nullptr)
            {
                state.hadHierarchy = true;
                state.hierarchyBind = hierarchy->layerMask_bind;
            }
            before_.push_back(state);
        }
    }

    bool SetSceneLayerBitCommand::Apply(const bool enabled)
    {
        if (scene_ == nullptr || before_.empty() || bit_ >= 32u)
            return false;
        const std::uint32_t flag = std::uint32_t{1} << bit_;
        for (const auto& target : before_)
        {
            std::uint32_t mask = target.hadLayer ? target.mask : ~0u;
            mask = enabled ? (mask | flag) : (mask & ~flag);
            auto* layer = scene_->layers.GetComponent(target.entity);
            if (layer == nullptr)
                layer = &scene_->layers.Create(target.entity);
            layer->layerMask = mask;
            if (auto* hierarchy = scene_->hierarchy.GetComponent(target.entity);
                hierarchy != nullptr)
            {
                hierarchy->layerMask_bind = mask;
            }
            if (auto* material = scene_->materials.GetComponent(target.entity);
                material != nullptr)
            {
                material->SetDirty();
            }
        }
        return true;
    }

    bool SetSceneLayerBitCommand::Execute()
    {
        if (before_.empty() || bit_ >= 32u)
            return false;
        const std::uint32_t flag = std::uint32_t{1} << bit_;
        const bool changed = std::any_of(
            before_.begin(),
            before_.end(),
            [this, flag](const TargetState& target)
            {
                const std::uint32_t mask = target.hadLayer ? target.mask : ~0u;
                return ((mask & flag) != 0u) != after_;
            });
        return changed && Apply(after_);
    }

    void SetSceneLayerBitCommand::Undo()
    {
        if (scene_ == nullptr)
            return;
        for (const auto& target : before_)
        {
            if (!target.hadLayer)
            {
                scene_->layers.Remove(target.entity);
            }
            else
            {
                auto* layer = scene_->layers.GetComponent(target.entity);
                if (layer == nullptr)
                    layer = &scene_->layers.Create(target.entity);
                layer->layerMask = target.mask;
            }
            if (target.hadHierarchy)
            {
                if (auto* hierarchy = scene_->hierarchy.GetComponent(target.entity);
                    hierarchy != nullptr)
                {
                    hierarchy->layerMask_bind = target.hierarchyBind;
                }
            }
            if (auto* material = scene_->materials.GetComponent(target.entity);
                material != nullptr)
            {
                material->SetDirty();
            }
        }
    }

'''
text = replace_once(text, marker, addition + marker, "layer bit command implementation")
path.write_text(text, encoding="utf-8")

path = Path("Tests/Phase5Gate1SceneComponentTests.cpp")
text = path.read_text(encoding="utf-8")
marker = "    const std::uint32_t authoredMask = 0x00000012u;\n"
addition = r'''    commands.Clear();
    ok &= Check(
        commands.Execute(std::make_unique<renegade::bridge::SetSceneLayerBitCommand>(
            fixture.scene,
            fixture.objectB,
            1u,
            false)),
        "layer-bit command failed");
    ok &= Check(
        fixture.scene.layers.GetComponent(fixture.objectA)->layerMask == 0x00000004u,
        "layer-bit edit destroyed unrelated bits on existing child");
    ok &= Check(
        fixture.scene.layers.GetComponent(fixture.objectB) != nullptr &&
            fixture.scene.layers.GetComponent(fixture.objectB)->layerMask == 0xFFFFFFFDu,
        "layer-bit edit did not preserve default bits on missing child layer");
    ok &= Check(commands.Undo(), "layer-bit Undo failed");
    ok &= Check(
        fixture.scene.layers.GetComponent(fixture.objectA)->layerMask == 0x00000004u &&
            fixture.scene.layers.GetComponent(fixture.objectB) == nullptr,
        "layer-bit Undo did not restore exact component state");

'''
text = replace_once(text, marker, addition + marker, "layer bit regression")
path.write_text(text, encoding="utf-8")

# Studio header ---------------------------------------------------------------
path = Path("Studio/src/StudioApplication.h")
text = path.read_text(encoding="utf-8")
text = replace_once(
    text,
    '#include "renegade/bridge/LightService.h"\n',
    '#include "renegade/bridge/LightService.h"\n#include "renegade/bridge/SceneComponentService.h"\n',
    "Studio scene component include",
)
marker = "        void ApplySelectedTransformValue(\n"
addition = r'''        void CommitSelectedSceneName(const std::string& name);
        void ApplySelectedLayerBit(std::uint32_t bit, bool enabled);
        void ApplySelectedLayerMask(std::uint32_t mask);
        void ApplySelectedMetadataPreset(
            wi::scene::MetadataComponent::Preset preset);
        void ApplySelectedObjectParticipation(
            bridge::ObjectParticipationProperty property,
            bool value);
'''
text = replace_once(text, marker, addition + marker, "Studio Gate 1 method declarations")
marker = "        SceneInspectorTextInputField scaleZ_;\n"
addition = r'''        wi::gui::Label sceneIdentityLabel_;
        SceneInspectorTextInputField sceneNameInput_;
        wi::gui::Label sceneLayerLabel_;
        SceneInspectorButton sceneLayerAllButton_;
        SceneInspectorButton sceneLayerNoneButton_;
        std::array<SceneInspectorCheckBox, 32> sceneLayerBits_;
        wi::gui::Label sceneMetadataLabel_;
        SceneInspectorComboBox sceneMetadataPreset_;
        wi::gui::Label sceneObjectLabel_;
        SceneInspectorCheckBox sceneObjectRenderable_;
        SceneInspectorCheckBox sceneObjectCastShadow_;
        SceneInspectorCheckBox sceneObjectForeground_;
        SceneInspectorCheckBox sceneObjectMainCamera_;
        SceneInspectorCheckBox sceneObjectReflections_;
        SceneInspectorCheckBox sceneObjectWetmap_;
'''
text = replace_once(text, marker, marker + addition, "Studio Gate 1 widget declarations")
path.write_text(text, encoding="utf-8")

# Studio source: create controls ----------------------------------------------
path = Path("Studio/src/StudioApplication.cpp")
text = path.read_text(encoding="utf-8")
marker = '''        createSectionLabel(
            lightLabel_,
'''
addition = r'''        createSectionLabel(
            sceneIdentityLabel_,
            "Scene Identity Section",
            "SCENE // IDENTITY");
        sceneNameInput_.Create("Scene Entity Name");
        sceneNameInput_.SetPlaceholder("ENTITY NAME");
        sceneNameInput_.SetTooltip(
            "Creator-facing name. Reusable imported assets rename their stable top-level root, not an internal glTF node.");
        sceneNameInput_.OnInputAccepted([this](const wi::gui::EventArgs& args)
        {
            CommitSelectedSceneName(args.sValue);
        });
        inspectorPanel_.AddWidget(&sceneNameInput_);

        createSectionLabel(
            sceneLayerLabel_,
            "Scene Layer Section",
            "LAYERS // 32-BIT MASK");
        sceneLayerAllButton_.Create("Enable All Scene Layers");
        sceneLayerAllButton_.SetText("ALL");
        sceneLayerAllButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            ApplySelectedLayerMask(~0u);
        });
        inspectorPanel_.AddWidget(&sceneLayerAllButton_);
        sceneLayerNoneButton_.Create("Disable All Scene Layers");
        sceneLayerNoneButton_.SetText("NONE");
        sceneLayerNoneButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            ApplySelectedLayerMask(0u);
        });
        inspectorPanel_.AddWidget(&sceneLayerNoneButton_);
        for (std::uint32_t bit = 0; bit < sceneLayerBits_.size(); ++bit)
        {
            auto& checkbox = sceneLayerBits_[bit];
            checkbox.Create("Scene Layer Bit " + std::to_string(bit));
            checkbox.SetText(std::to_string(bit));
            checkbox.SetTooltip(
                "Wicked layer bit " + std::to_string(bit) +
                ". Reusable assets apply the bit to the stable root and descendant render objects.");
            checkbox.OnClick([this, bit](const wi::gui::EventArgs& args)
            {
                ApplySelectedLayerBit(bit, args.bValue);
            });
            inspectorPanel_.AddWidget(&checkbox);
        }

        createSectionLabel(
            sceneMetadataLabel_,
            "Scene Metadata Section",
            "METADATA // PRESET");
        sceneMetadataPreset_.Create("Metadata Preset");
        sceneMetadataPreset_.AddItem("CUSTOM", static_cast<std::uint64_t>(wi::scene::MetadataComponent::Preset::Custom));
        sceneMetadataPreset_.AddItem("WAYPOINT", static_cast<std::uint64_t>(wi::scene::MetadataComponent::Preset::Waypoint));
        sceneMetadataPreset_.AddItem("PLAYER", static_cast<std::uint64_t>(wi::scene::MetadataComponent::Preset::Player));
        sceneMetadataPreset_.AddItem("ENEMY", static_cast<std::uint64_t>(wi::scene::MetadataComponent::Preset::Enemy));
        sceneMetadataPreset_.AddItem("NPC", static_cast<std::uint64_t>(wi::scene::MetadataComponent::Preset::NPC));
        sceneMetadataPreset_.AddItem("PICKUP", static_cast<std::uint64_t>(wi::scene::MetadataComponent::Preset::Pickup));
        sceneMetadataPreset_.AddItem("VEHICLE", static_cast<std::uint64_t>(wi::scene::MetadataComponent::Preset::Vehicle));
        sceneMetadataPreset_.AddItem("POINT OF INTEREST", static_cast<std::uint64_t>(wi::scene::MetadataComponent::Preset::PointOfInterest));
        sceneMetadataPreset_.SetTooltip(
            "Native Wicked semantic preset. Existing typed metadata and Renegade asset identity are preserved.");
        sceneMetadataPreset_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            ApplySelectedMetadataPreset(
                static_cast<wi::scene::MetadataComponent::Preset>(args.userdata));
        });
        inspectorPanel_.AddWidget(&sceneMetadataPreset_);

        createSectionLabel(
            sceneObjectLabel_,
            "Scene Object Section",
            "OBJECT // RENDER PARTICIPATION");
        const auto createObjectToggle = [this](
            SceneInspectorCheckBox& checkbox,
            const char* name,
            const char* tooltip,
            const bridge::ObjectParticipationProperty property)
        {
            checkbox.Create(name);
            checkbox.SetTooltip(tooltip);
            checkbox.OnClick([this, property](const wi::gui::EventArgs& args)
            {
                ApplySelectedObjectParticipation(property, args.bValue);
            });
            inspectorPanel_.AddWidget(&checkbox);
        };
        createObjectToggle(sceneObjectRenderable_, "Renderable: ",
            "Participate in normal scene rendering.",
            bridge::ObjectParticipationProperty::Renderable);
        createObjectToggle(sceneObjectCastShadow_, "Cast shadow: ",
            "Allow this object to cast native Wicked shadows.",
            bridge::ObjectParticipationProperty::CastShadow);
        createObjectToggle(sceneObjectForeground_, "Foreground: ",
            "Render as foreground geometry.",
            bridge::ObjectParticipationProperty::Foreground);
        createObjectToggle(sceneObjectMainCamera_, "Main camera: ",
            "Visible to the main camera.",
            bridge::ObjectParticipationProperty::VisibleInMainCamera);
        createObjectToggle(sceneObjectReflections_, "Reflections: ",
            "Visible to reflection rendering.",
            bridge::ObjectParticipationProperty::VisibleInReflections);
        createObjectToggle(sceneObjectWetmap_, "Wetmap: ",
            "Enable native Wicked wetmap participation.",
            bridge::ObjectParticipationProperty::Wetmap);

'''
text = replace_once(text, marker, addition + marker, "Studio Gate 1 control creation")

# Studio source: layout generic controls and move Light below them.
old = r'''        positionEnvironmentWidget(lightLabel_, 224.0f, 20.0f);
        positionEnvironmentWidget(lightType_, 244.0f);
        positionEnvironmentWidget(lightColorRed_, 278.0f);
        positionEnvironmentWidget(lightColorGreen_, 312.0f);
        positionEnvironmentWidget(lightColorBlue_, 346.0f);
        positionEnvironmentWidget(lightIntensity_, 380.0f);
        positionEnvironmentWidget(lightRange_, 414.0f);
        positionEnvironmentWidget(lightOuterCone_, 448.0f);
        positionEnvironmentWidget(lightInnerCone_, 482.0f);
        positionEnvironmentWidget(lightRadius_, 516.0f);
        positionEnvironmentWidget(lightLength_, 550.0f);
        positionEnvironmentWidget(lightHeight_, 584.0f);
        positionEnvironmentWidget(lightCastShadow_, 618.0f);
        positionEnvironmentWidget(lightVolumetrics_, 650.0f);
        positionEnvironmentWidget(lightVolumetricBoost_, 682.0f);
'''
new = r'''        positionEnvironmentWidget(sceneIdentityLabel_, 224.0f, 20.0f);
        positionEnvironmentWidget(sceneNameInput_, 244.0f);
        positionEnvironmentWidget(sceneLayerLabel_, 282.0f, 20.0f);
        const float layerActionWidth = (environmentFieldWidth - 8.0f) * 0.5f;
        sceneLayerAllButton_.SetPos(XMFLOAT2(12.0f, 302.0f));
        sceneLayerNoneButton_.SetPos(XMFLOAT2(20.0f + layerActionWidth, 302.0f));
        sceneLayerAllButton_.SetSize(XMFLOAT2(layerActionWidth, 28.0f));
        sceneLayerNoneButton_.SetSize(XMFLOAT2(layerActionWidth, 28.0f));
        const float layerBitGap = 4.0f;
        const float layerBitWidth =
            (environmentFieldWidth - layerBitGap * 7.0f) / 8.0f;
        for (std::size_t bit = 0; bit < sceneLayerBits_.size(); ++bit)
        {
            const float x = 12.0f +
                static_cast<float>(bit % 8u) * (layerBitWidth + layerBitGap);
            const float y = 336.0f +
                static_cast<float>(bit / 8u) * 26.0f;
            sceneLayerBits_[bit].SetPos(XMFLOAT2(x, y));
            sceneLayerBits_[bit].SetSize(XMFLOAT2(layerBitWidth, 22.0f));
        }
        positionEnvironmentWidget(sceneMetadataLabel_, 446.0f, 20.0f);
        positionEnvironmentWidget(sceneMetadataPreset_, 466.0f);
        positionEnvironmentWidget(sceneObjectLabel_, 506.0f, 20.0f);
        const float objectToggleWidth = (environmentFieldWidth - 8.0f) * 0.5f;
        const auto layoutObjectToggle = [objectToggleWidth](
            wi::gui::Widget& widget,
            const int column,
            const float y)
        {
            widget.SetPos(XMFLOAT2(
                12.0f + static_cast<float>(column) * (objectToggleWidth + 8.0f),
                y));
            widget.SetSize(XMFLOAT2(objectToggleWidth, 28.0f));
        };
        layoutObjectToggle(sceneObjectRenderable_, 0, 526.0f);
        layoutObjectToggle(sceneObjectCastShadow_, 1, 526.0f);
        layoutObjectToggle(sceneObjectForeground_, 0, 558.0f);
        layoutObjectToggle(sceneObjectMainCamera_, 1, 558.0f);
        layoutObjectToggle(sceneObjectReflections_, 0, 590.0f);
        layoutObjectToggle(sceneObjectWetmap_, 1, 590.0f);

        positionEnvironmentWidget(lightLabel_, 630.0f, 20.0f);
        positionEnvironmentWidget(lightType_, 650.0f);
        positionEnvironmentWidget(lightColorRed_, 684.0f);
        positionEnvironmentWidget(lightColorGreen_, 718.0f);
        positionEnvironmentWidget(lightColorBlue_, 752.0f);
        positionEnvironmentWidget(lightIntensity_, 786.0f);
        positionEnvironmentWidget(lightRange_, 820.0f);
        positionEnvironmentWidget(lightOuterCone_, 854.0f);
        positionEnvironmentWidget(lightInnerCone_, 888.0f);
        positionEnvironmentWidget(lightRadius_, 922.0f);
        positionEnvironmentWidget(lightLength_, 956.0f);
        positionEnvironmentWidget(lightHeight_, 990.0f);
        positionEnvironmentWidget(lightCastShadow_, 1024.0f);
        positionEnvironmentWidget(lightVolumetrics_, 1056.0f);
        positionEnvironmentWidget(lightVolumetricBoost_, 1088.0f);
'''
text = replace_once(text, old, new, "Studio Gate 1 Inspector layout")

text = replace_once(
    text,
    '''                : light
                    ? 726.0f
                    : 230.0f;''',
    '''                : light
                    ? 1130.0f
                    : 630.0f;''',
    "Inspector action position after core scene controls",
)

# Studio source: command-only mutation methods.
marker = "    bool StudioRenderPath::CommitSelectedLight(\n"
addition = r'''    void StudioRenderPath::CommitSelectedSceneName(const std::string& name)
    {
        if (session_ == nullptr || !session_->Selection().HasSelection())
            return;
        auto& scene = session_->Scenes().GetScene();
        const auto selected = session_->Selection().SelectedEntity();
        const auto target = bridge::ResolveSceneComponentAuthoringRoot(scene, selected);
        const bool changed = session_->Commands().Execute(
            std::make_unique<bridge::SetSceneNameCommand>(scene, selected, name));
        if (changed && target != wi::ecs::INVALID_ENTITY)
            session_->Selection().Select(target);
        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::ApplySelectedLayerBit(
        const std::uint32_t bit,
        const bool enabled)
    {
        if (session_ == nullptr || !session_->Selection().HasSelection())
            return;
        auto& scene = session_->Scenes().GetScene();
        const auto selected = session_->Selection().SelectedEntity();
        (void)session_->Commands().Execute(
            std::make_unique<bridge::SetSceneLayerBitCommand>(
                scene, selected, bit, enabled));
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::ApplySelectedLayerMask(const std::uint32_t mask)
    {
        if (session_ == nullptr || !session_->Selection().HasSelection())
            return;
        auto& scene = session_->Scenes().GetScene();
        const auto selected = session_->Selection().SelectedEntity();
        (void)session_->Commands().Execute(
            std::make_unique<bridge::SetSceneLayerMaskCommand>(
                scene, selected, mask));
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::ApplySelectedMetadataPreset(
        const wi::scene::MetadataComponent::Preset preset)
    {
        if (session_ == nullptr || !session_->Selection().HasSelection())
            return;
        auto& scene = session_->Scenes().GetScene();
        const auto selected = session_->Selection().SelectedEntity();
        (void)session_->Commands().Execute(
            std::make_unique<bridge::SetMetadataPresetCommand>(
                scene, selected, preset));
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::ApplySelectedObjectParticipation(
        const bridge::ObjectParticipationProperty property,
        const bool value)
    {
        if (session_ == nullptr || !session_->Selection().HasSelection())
            return;
        auto& scene = session_->Scenes().GetScene();
        const auto selected = session_->Selection().SelectedEntity();
        (void)session_->Commands().Execute(
            std::make_unique<bridge::SetObjectParticipationCommand>(
                scene, selected, property, value));
        RefreshInspector();
        RefreshStatus();
    }

'''
text = replace_once(text, marker, addition + marker, "Studio Gate 1 command methods")

# Studio source: refresh/visibility/state.
marker = '''        const bool hasLight = light != nullptr;
        if (hasSession && selectedEntity != wi::ecs::INVALID_ENTITY &&
'''
addition = r'''        const bool hasLight = light != nullptr;
        const bool sceneComponentsVisible =
            hasSession && selectedEntity != wi::ecs::INVALID_ENTITY &&
            !environmentWorkspaceActive_ && !terrainWorkspaceActive_ &&
            !hasWeather && !hasTerrain;
        wi::ecs::Entity sceneAuthoringRoot = wi::ecs::INVALID_ENTITY;
        bridge::SceneLayerMaskState sceneLayerState;
        bridge::ObjectParticipationState objectRenderableState;
        bridge::ObjectParticipationState objectCastShadowState;
        bridge::ObjectParticipationState objectForegroundState;
        bridge::ObjectParticipationState objectMainCameraState;
        bridge::ObjectParticipationState objectReflectionsState;
        bridge::ObjectParticipationState objectWetmapState;
        if (sceneComponentsVisible)
        {
            const auto& currentScene = session_->Scenes().GetScene();
            sceneAuthoringRoot = bridge::ResolveSceneComponentAuthoringRoot(
                currentScene, selectedEntity);
            sceneLayerState = bridge::InspectSceneLayerMask(
                currentScene, selectedEntity);
            objectRenderableState = bridge::InspectObjectParticipation(
                currentScene, selectedEntity,
                bridge::ObjectParticipationProperty::Renderable);
            objectCastShadowState = bridge::InspectObjectParticipation(
                currentScene, selectedEntity,
                bridge::ObjectParticipationProperty::CastShadow);
            objectForegroundState = bridge::InspectObjectParticipation(
                currentScene, selectedEntity,
                bridge::ObjectParticipationProperty::Foreground);
            objectMainCameraState = bridge::InspectObjectParticipation(
                currentScene, selectedEntity,
                bridge::ObjectParticipationProperty::VisibleInMainCamera);
            objectReflectionsState = bridge::InspectObjectParticipation(
                currentScene, selectedEntity,
                bridge::ObjectParticipationProperty::VisibleInReflections);
            objectWetmapState = bridge::InspectObjectParticipation(
                currentScene, selectedEntity,
                bridge::ObjectParticipationProperty::Wetmap);
        }
        const bool hasObjectTargets = objectRenderableState.targetCount > 0;
        if (hasSession && selectedEntity != wi::ecs::INVALID_ENTITY &&
'''
text = replace_once(text, marker, addition, "Studio Gate 1 inspector state capture")

marker = '''        LayoutInspectorActions(hasWeather, hasTerrain, hasLight);
        const auto setTransformVisible = [this, hasWeather, hasTerrain](wi::gui::Widget& widget)
'''
addition = r'''        LayoutInspectorActions(hasWeather, hasTerrain, hasLight);

        sceneIdentityLabel_.SetVisible(sceneComponentsVisible);
        sceneNameInput_.SetVisible(sceneComponentsVisible);
        sceneLayerLabel_.SetVisible(sceneComponentsVisible);
        sceneLayerAllButton_.SetVisible(sceneComponentsVisible);
        sceneLayerNoneButton_.SetVisible(sceneComponentsVisible);
        for (auto& bit : sceneLayerBits_)
            bit.SetVisible(sceneComponentsVisible);
        sceneMetadataLabel_.SetVisible(sceneComponentsVisible);
        sceneMetadataPreset_.SetVisible(sceneComponentsVisible);
        sceneObjectLabel_.SetVisible(sceneComponentsVisible && hasObjectTargets);
        sceneObjectRenderable_.SetVisible(sceneComponentsVisible && hasObjectTargets);
        sceneObjectCastShadow_.SetVisible(sceneComponentsVisible && hasObjectTargets);
        sceneObjectForeground_.SetVisible(sceneComponentsVisible && hasObjectTargets);
        sceneObjectMainCamera_.SetVisible(sceneComponentsVisible && hasObjectTargets);
        sceneObjectReflections_.SetVisible(sceneComponentsVisible && hasObjectTargets);
        sceneObjectWetmap_.SetVisible(sceneComponentsVisible && hasObjectTargets);

        if (sceneComponentsVisible && sceneAuthoringRoot != wi::ecs::INVALID_ENTITY)
        {
            const auto& currentScene = session_->Scenes().GetScene();
            const auto* sceneName = currentScene.names.GetComponent(sceneAuthoringRoot);
            sceneNameInput_.SetValue(sceneName != nullptr ? sceneName->name : std::string{});
            sceneLayerLabel_.SetText(sceneLayerState.mixed
                ? "LAYERS // MIXED // EDITING PRESERVES OTHER BITS"
                : "LAYERS // 32-BIT MASK");
            for (std::uint32_t bit = 0; bit < sceneLayerBits_.size(); ++bit)
            {
                sceneLayerBits_[bit].SetCheck(
                    (sceneLayerState.mask & (std::uint32_t{1} << bit)) != 0u);
            }
            const auto* metadata = currentScene.metadatas.GetComponent(sceneAuthoringRoot);
            sceneMetadataPreset_.SetSelectedByUserdataWithoutCallback(
                static_cast<std::uint64_t>(metadata != nullptr
                    ? metadata->preset
                    : wi::scene::MetadataComponent::Preset::Custom));
            const bool objectMixed = objectRenderableState.mixed ||
                objectCastShadowState.mixed || objectForegroundState.mixed ||
                objectMainCameraState.mixed || objectReflectionsState.mixed ||
                objectWetmapState.mixed;
            sceneObjectLabel_.SetText(objectMixed
                ? "OBJECT // MIXED // WHOLE-ASSET EDIT"
                : "OBJECT // RENDER PARTICIPATION");
            sceneObjectRenderable_.SetCheck(objectRenderableState.value);
            sceneObjectCastShadow_.SetCheck(objectCastShadowState.value);
            sceneObjectForeground_.SetCheck(objectForegroundState.value);
            sceneObjectMainCamera_.SetCheck(objectMainCameraState.value);
            sceneObjectReflections_.SetCheck(objectReflectionsState.value);
            sceneObjectWetmap_.SetCheck(objectWetmapState.value);
        }

        const auto setTransformVisible = [this, hasWeather, hasTerrain](wi::gui::Widget& widget)
'''
text = replace_once(text, marker, addition, "Studio Gate 1 inspector refresh")
path.write_text(text, encoding="utf-8")

# Source contract --------------------------------------------------------------
Path("Tests/Phase5Gate1SourceContract.cmake").write_text(r'''if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

set(SERVICE_HEADER "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/SceneComponentService.h")
set(SERVICE_SOURCE "${RENEGADE_SOURCE_DIR}/EngineBridge/src/SceneComponentService.cpp")
set(STUDIO_HEADER "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.h")
set(STUDIO_SOURCE "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.cpp")
set(TEST_SOURCE "${RENEGADE_SOURCE_DIR}/Tests/Phase5Gate1SceneComponentTests.cpp")

foreach(path IN ITEMS "${SERVICE_HEADER}" "${SERVICE_SOURCE}" "${STUDIO_HEADER}" "${STUDIO_SOURCE}" "${TEST_SOURCE}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Phase 5 Gate 1 source contract input missing: ${path}")
    endif()
endforeach()

file(READ "${SERVICE_HEADER}" service_header)
file(READ "${SERVICE_SOURCE}" service_source)
file(READ "${STUDIO_HEADER}" studio_header)
file(READ "${STUDIO_SOURCE}" studio_source)
file(READ "${TEST_SOURCE}" test_source)

function(require_text haystack needle description)
    string(FIND "${${haystack}}" "${needle}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 5 Gate 1 contract missing ${description}: ${needle}")
    endif()
endfunction()

require_text(service_header "ResolveSceneComponentAuthoringRoot" "reusable-root scene authoring resolver")
require_text(service_header "SetSceneNameCommand" "undoable creator name command")
require_text(service_header "SetSceneLayerMaskCommand" "undoable full layer-mask command")
require_text(service_header "SetSceneLayerBitCommand" "bit-preserving layer command")
require_text(service_header "SetMetadataPresetCommand" "undoable Metadata preset command")
require_text(service_header "SetObjectParticipationCommand" "undoable Object participation command")
require_text(service_source "ReusableAssetInstanceIdMetadataKey" "stable reusable-root detection")
require_text(service_source "scene.Entity_IsDescendant(entity, root)" "descendant whole-asset targeting")
require_text(service_source "scene_->layers.Remove(target.entity);" "Undo removal of Gate-created LayerComponent")
require_text(service_source "target.hadLayer ? target.mask : ~0u" "bit edit preserving each target mask")
require_text(service_source "object.SetNotVisibleInMainCamera(!value);" "native main-camera visibility mapping")
require_text(service_source "object.SetNotVisibleInReflections(!value);" "native reflection visibility mapping")

require_text(studio_header "std::array<SceneInspectorCheckBox, 32> sceneLayerBits_;" "32-bit layer UI")
require_text(studio_source "sceneLayerAllButton_.SetText(\"ALL\")" "layer ALL action")
require_text(studio_source "sceneLayerNoneButton_.SetText(\"NONE\")" "layer NONE action")
require_text(studio_source "MetadataComponent::Preset::PointOfInterest" "complete native Metadata preset list")
require_text(studio_source "std::make_unique<bridge::SetSceneNameCommand>" "Inspector rename through CommandService")
require_text(studio_source "std::make_unique<bridge::SetSceneLayerBitCommand>" "Inspector layer-bit edit through CommandService")
require_text(studio_source "std::make_unique<bridge::SetSceneLayerMaskCommand>" "Inspector ALL/NONE through CommandService")
require_text(studio_source "std::make_unique<bridge::SetMetadataPresetCommand>" "Inspector Metadata through CommandService")
require_text(studio_source "std::make_unique<bridge::SetObjectParticipationCommand>" "Inspector Object flags through CommandService")
require_text(studio_source "OBJECT // MIXED // WHOLE-ASSET EDIT" "mixed reusable-object state presentation")
require_text(test_source "layer-bit edit destroyed unrelated bits on existing child" "mixed layer bit-preservation regression")
require_text(test_source "object participation Undo did not restore mixed prior state" "mixed Object Undo regression")
require_text(test_source "metadata Undo destroyed reusable asset identity" "Metadata identity preservation regression")

message(STATUS "Phase 5 Gate 1 source contract passed")
''', encoding="utf-8")
