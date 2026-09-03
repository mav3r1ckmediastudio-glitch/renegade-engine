#include "StudioApplication.h"

#include "InspectorSectionFramework.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>

namespace renegade::studio
{
    namespace
    {
        class CallbackInspectorSectionProvider final
            : public IInspectorSectionProvider
        {
        public:
            using VisibleFn = std::function<bool(const InspectorSectionContext&)>;
            using MeasureFn = std::function<float(const InspectorSectionContext&, float)>;
            using RefreshFn = std::function<void(const InspectorSectionContext&)>;
            using LayoutFn = std::function<void(const InspectorSectionContext&, const InspectorSectionLayout&)>;

            CallbackInspectorSectionProvider(
                InspectorSectionDescriptor descriptor,
                VisibleFn visible,
                MeasureFn measure,
                RefreshFn refresh,
                LayoutFn layout)
                : descriptor_(std::move(descriptor)),
                  visible_(std::move(visible)),
                  measure_(std::move(measure)),
                  refresh_(std::move(refresh)),
                  layout_(std::move(layout))
            {
            }

            [[nodiscard]] const InspectorSectionDescriptor& Descriptor()
                const noexcept override
            {
                return descriptor_;
            }

            [[nodiscard]] bool IsVisible(
                const InspectorSectionContext& context) const override
            {
                return visible_ ? visible_(context) : true;
            }

            [[nodiscard]] float MeasureContentHeight(
                const InspectorSectionContext& context,
                const float availableWidth) const override
            {
                return measure_ ? measure_(context, availableWidth) : 0.0f;
            }

            void Refresh(const InspectorSectionContext& context) override
            {
                if (refresh_)
                    refresh_(context);
            }

            void ApplyLayout(
                const InspectorSectionContext& context,
                const InspectorSectionLayout& layout) override
            {
                if (layout_)
                    layout_(context, layout);
            }

        private:
            InspectorSectionDescriptor descriptor_;
            VisibleFn visible_;
            MeasureFn measure_;
            RefreshFn refresh_;
            LayoutFn layout_;
        };

        InspectorSectionDescriptor MakeSection(
            std::string id,
            std::string title,
            const int order,
            const bool expanded)
        {
            InspectorSectionDescriptor descriptor;
            descriptor.id = std::move(id);
            descriptor.title = std::move(title);
            descriptor.order = order;
            descriptor.defaultExpanded = expanded;
            descriptor.headerHeight = 28.0f;
            descriptor.spacingAfter = 6.0f;
            return descriptor;
        }
    }

    bool StudioRenderPath::IsS1BTransformSectionVisible() const
    {
        if (environmentWorkspaceActive_ || terrainWorkspaceActive_)
            return false;
        if (session_ == nullptr || !session_->Selection().HasSelection())
            return true;

        const auto selected = session_->Selection().SelectedEntity();
        const auto& scene = session_->Scenes().GetScene();
        return !scene.weathers.Contains(selected) && !scene.terrains.Contains(selected);
    }

    bool StudioRenderPath::IsS1BRenderingSectionVisible() const
    {
        if (session_ == nullptr || !session_->Selection().HasSelection() ||
            environmentWorkspaceActive_ || terrainWorkspaceActive_)
        {
            return false;
        }
        const auto selected = session_->Selection().SelectedEntity();
        if (bridge::IsPlayerStart(session_->Scenes().GetScene(), selected))
            return false;
        return bridge::InspectObjectParticipation(
            session_->Scenes().GetScene(),
            selected,
            bridge::ObjectParticipationProperty::Renderable).targetCount > 0;
    }

    void StudioRenderPath::CreateS1BInspectorSections()
    {
        const auto createHeader = [this](
            SceneInspectorButton& button,
            const char* name,
            const char* tooltip,
            const char* sectionId)
        {
            button.Create(name);
            button.SetTooltip(tooltip);
            button.OnClick([this, sectionId](const wi::gui::EventArgs&)
            {
                if (inspectorSectionRegistry_.ToggleExpanded(sectionId))
                {
                    RefreshInspector();
                }
            });
            inspectorPanel_.AddWidget(&button);
        };

        createHeader(
            transformSectionHeader_,
            "S1B Transform Section Header",
            "Expand or collapse the Transform component section.",
            "transform");
        createHeader(
            renderingSectionHeader_,
            "S1B Rendering Section Header",
            "Expand or collapse native render-participation controls.",
            "rendering");
        createHeader(
            materialsSectionHeader_,
            "S1B Materials Section Header",
            "Expand or collapse the existing native Wicked material editor.",
            "materials");

        std::string error;
        auto transform = std::make_shared<CallbackInspectorSectionProvider>(
            MakeSection("transform", "TRANSFORM", 10, true),
            [this](const InspectorSectionContext&)
            {
                return IsS1BTransformSectionVisible();
            },
            [this](const InspectorSectionContext&, float)
            {
                if (session_ != nullptr && session_->Selection().HasSelection() &&
                    bridge::IsPlayerStart(
                        session_->Scenes().GetScene(),
                        session_->Selection().SelectedEntity()))
                {
                    return 108.0f;
                }
                return 168.0f;
            },
            {},
            [this](const InspectorSectionContext&, const InspectorSectionLayout& layout)
            {
                transformSectionHeader_.SetVisible(true);
                transformSectionHeader_.SetPos(XMFLOAT2(12.0f, layout.top));
                transformSectionHeader_.SetSize(XMFLOAT2(layout.width, layout.headerHeight));
                transformSectionHeader_.SetText(
                    std::string(layout.expanded ? "▼  " : "▶  ") + "TRANSFORM");

                if (!layout.expanded)
                {
                    for (wi::gui::Widget* widget : {
                        static_cast<wi::gui::Widget*>(&positionLabel_),
                        static_cast<wi::gui::Widget*>(&rotationLabel_),
                        static_cast<wi::gui::Widget*>(&scaleLabel_),
                        static_cast<wi::gui::Widget*>(&translationX_),
                        static_cast<wi::gui::Widget*>(&translationY_),
                        static_cast<wi::gui::Widget*>(&translationZ_),
                        static_cast<wi::gui::Widget*>(&rotationX_),
                        static_cast<wi::gui::Widget*>(&rotationY_),
                        static_cast<wi::gui::Widget*>(&rotationZ_),
                        static_cast<wi::gui::Widget*>(&scaleX_),
                        static_cast<wi::gui::Widget*>(&scaleY_),
                        static_cast<wi::gui::Widget*>(&scaleZ_)})
                    {
                        widget->SetVisible(false);
                    }
                    return;
                }

                const float gap = 8.0f;
                const float fieldWidth = (layout.width - 16.0f) / 3.0f;
                const auto row = [fieldWidth, gap](
                    wi::gui::TextInputField& x,
                    wi::gui::TextInputField& y,
                    wi::gui::TextInputField& z,
                    const float top)
                {
                    x.SetPos(XMFLOAT2(12.0f, top));
                    y.SetPos(XMFLOAT2(12.0f + fieldWidth + gap, top));
                    z.SetPos(XMFLOAT2(12.0f + (fieldWidth + gap) * 2.0f, top));
                    x.SetSize(XMFLOAT2(fieldWidth, 28.0f));
                    y.SetSize(XMFLOAT2(fieldWidth, 28.0f));
                    z.SetSize(XMFLOAT2(fieldWidth, 28.0f));
                };
                positionLabel_.SetPos(XMFLOAT2(12.0f, layout.contentTop));
                positionLabel_.SetSize(XMFLOAT2(layout.width, 20.0f));
                row(translationX_, translationY_, translationZ_, layout.contentTop + 20.0f);
                rotationLabel_.SetPos(XMFLOAT2(12.0f, layout.contentTop + 60.0f));
                rotationLabel_.SetSize(XMFLOAT2(layout.width, 20.0f));
                row(rotationX_, rotationY_, rotationZ_, layout.contentTop + 80.0f);
                scaleLabel_.SetPos(XMFLOAT2(12.0f, layout.contentTop + 120.0f));
                scaleLabel_.SetSize(XMFLOAT2(layout.width, 20.0f));
                row(scaleX_, scaleY_, scaleZ_, layout.contentTop + 140.0f);
            });
        if (!inspectorSectionRegistry_.Register(std::move(transform), error))
            studioChrome_.SetStatusText("S1B INSPECTOR // " + error);

        auto rendering = std::make_shared<CallbackInspectorSectionProvider>(
            MakeSection("rendering", "RENDERING", 20, true),
            [this](const InspectorSectionContext&)
            {
                return IsS1BRenderingSectionVisible();
            },
            [](const InspectorSectionContext&, float)
            {
                return 112.0f;
            },
            {},
            [this](const InspectorSectionContext&, const InspectorSectionLayout& layout)
            {
                renderingSectionHeader_.SetVisible(true);
                renderingSectionHeader_.SetPos(XMFLOAT2(12.0f, layout.top));
                renderingSectionHeader_.SetSize(XMFLOAT2(layout.width, layout.headerHeight));
                renderingSectionHeader_.SetText(
                    std::string(layout.expanded ? "▼  " : "▶  ") + "RENDERING");
                if (!layout.expanded)
                {
                    for (wi::gui::Widget* widget : {
                        static_cast<wi::gui::Widget*>(&sceneObjectLabel_),
                        static_cast<wi::gui::Widget*>(&sceneObjectRenderable_),
                        static_cast<wi::gui::Widget*>(&sceneObjectCastShadow_),
                        static_cast<wi::gui::Widget*>(&sceneObjectForeground_),
                        static_cast<wi::gui::Widget*>(&sceneObjectMainCamera_),
                        static_cast<wi::gui::Widget*>(&sceneObjectReflections_),
                        static_cast<wi::gui::Widget*>(&sceneObjectWetmap_)})
                    {
                        widget->SetVisible(false);
                    }
                    return;
                }

                const float half = (layout.width - 8.0f) * 0.5f;
                const auto toggle = [half](wi::gui::Widget& widget, const int column, const float y)
                {
                    widget.SetPos(XMFLOAT2(12.0f + static_cast<float>(column) * (half + 8.0f), y));
                    widget.SetSize(XMFLOAT2(half, 28.0f));
                };
                sceneObjectLabel_.SetPos(XMFLOAT2(12.0f, layout.contentTop));
                sceneObjectLabel_.SetSize(XMFLOAT2(layout.width, 20.0f));
                toggle(sceneObjectRenderable_, 0, layout.contentTop + 20.0f);
                toggle(sceneObjectCastShadow_, 1, layout.contentTop + 20.0f);
                toggle(sceneObjectForeground_, 0, layout.contentTop + 52.0f);
                toggle(sceneObjectMainCamera_, 1, layout.contentTop + 52.0f);
                toggle(sceneObjectReflections_, 0, layout.contentTop + 84.0f);
                toggle(sceneObjectWetmap_, 1, layout.contentTop + 84.0f);
            });
        error.clear();
        if (!inspectorSectionRegistry_.Register(std::move(rendering), error))
            studioChrome_.SetStatusText("S1B INSPECTOR // " + error);

        auto materials = std::make_shared<CallbackInspectorSectionProvider>(
            MakeSection("materials", "MATERIALS", 30, true),
            [this](const InspectorSectionContext&)
            {
                return materialInspectorVisible_;
            },
            [this](const InspectorSectionContext&, const float width)
            {
                LayoutMaterialInspector(width);
                return materialInspectorVisible_
                    ? std::max(0.0f, materialInspectorBottom_ - 630.0f)
                    : 0.0f;
            },
            {},
            [this](const InspectorSectionContext&, const InspectorSectionLayout& layout)
            {
                materialsSectionHeader_.SetVisible(true);
                materialsSectionHeader_.SetPos(XMFLOAT2(12.0f, layout.top));
                materialsSectionHeader_.SetSize(XMFLOAT2(layout.width, layout.headerHeight));
                materialsSectionHeader_.SetText(
                    std::string(layout.expanded ? "▼  " : "▶  ") + "MATERIALS");
                if (!layout.expanded)
                {
                    SetS1BMaterialContentVisible(false);
                    return;
                }

                LayoutMaterialInspector(layout.width);
                const float delta = layout.contentTop - 630.0f;
                OffsetS1BMaterialContent(delta);
                materialInspectorBottom_ += delta;
            });
        error.clear();
        if (!inspectorSectionRegistry_.Register(std::move(materials), error))
            studioChrome_.SetStatusText("S1B INSPECTOR // " + error);
    }

    void StudioRenderPath::SetS1BMaterialContentVisible(const bool visible)
    {
        for (wi::gui::Widget* widget : {
            static_cast<wi::gui::Widget*>(&materialLabel_),
            static_cast<wi::gui::Widget*>(&materialSelector_),
            static_cast<wi::gui::Widget*>(&materialShaderType_),
            static_cast<wi::gui::Widget*>(&materialBlendMode_),
            static_cast<wi::gui::Widget*>(&materialCoreLabel_),
            static_cast<wi::gui::Widget*>(&materialBaseColorRed_),
            static_cast<wi::gui::Widget*>(&materialBaseColorGreen_),
            static_cast<wi::gui::Widget*>(&materialBaseColorBlue_),
            static_cast<wi::gui::Widget*>(&materialOpacity_),
            static_cast<wi::gui::Widget*>(&materialMetalness_),
            static_cast<wi::gui::Widget*>(&materialRoughness_),
            static_cast<wi::gui::Widget*>(&materialReflectance_),
            static_cast<wi::gui::Widget*>(&materialNormalStrength_),
            static_cast<wi::gui::Widget*>(&materialAlphaRef_),
            static_cast<wi::gui::Widget*>(&materialEmissiveRed_),
            static_cast<wi::gui::Widget*>(&materialEmissiveGreen_),
            static_cast<wi::gui::Widget*>(&materialEmissiveBlue_),
            static_cast<wi::gui::Widget*>(&materialEmissiveStrength_),
            static_cast<wi::gui::Widget*>(&materialReceiveShadow_),
            static_cast<wi::gui::Widget*>(&materialCastShadow_),
            static_cast<wi::gui::Widget*>(&materialUseVertexColors_),
            static_cast<wi::gui::Widget*>(&materialDoubleSided_),
            static_cast<wi::gui::Widget*>(&materialUvLabel_),
            static_cast<wi::gui::Widget*>(&materialTilingU_),
            static_cast<wi::gui::Widget*>(&materialTilingV_),
            static_cast<wi::gui::Widget*>(&materialOffsetU_),
            static_cast<wi::gui::Widget*>(&materialOffsetV_),
            static_cast<wi::gui::Widget*>(&materialTexturesLabel_),
            static_cast<wi::gui::Widget*>(&materialShaderSpecificLabel_),
            static_cast<wi::gui::Widget*>(&materialPomStrength_),
            static_cast<wi::gui::Widget*>(&materialAnisotropyStrength_),
            static_cast<wi::gui::Widget*>(&materialAnisotropyRotation_),
            static_cast<wi::gui::Widget*>(&materialSheenRed_),
            static_cast<wi::gui::Widget*>(&materialSheenGreen_),
            static_cast<wi::gui::Widget*>(&materialSheenBlue_),
            static_cast<wi::gui::Widget*>(&materialSheenRoughness_),
            static_cast<wi::gui::Widget*>(&materialClearcoat_),
            static_cast<wi::gui::Widget*>(&materialClearcoatRoughness_),
            static_cast<wi::gui::Widget*>(&materialTransmission_),
            static_cast<wi::gui::Widget*>(&materialRefraction_),
            static_cast<wi::gui::Widget*>(&materialTerrainBlendHeight_),
            static_cast<wi::gui::Widget*>(&materialMeshBlend_),
            static_cast<wi::gui::Widget*>(&materialInteriorScaleX_),
            static_cast<wi::gui::Widget*>(&materialInteriorScaleY_),
            static_cast<wi::gui::Widget*>(&materialInteriorScaleZ_),
            static_cast<wi::gui::Widget*>(&materialInteriorOffsetX_),
            static_cast<wi::gui::Widget*>(&materialInteriorOffsetY_),
            static_cast<wi::gui::Widget*>(&materialInteriorOffsetZ_),
            static_cast<wi::gui::Widget*>(&materialInteriorRotation_)})
        {
            if (!visible)
                widget->SetVisible(false);
        }
        if (!visible)
        {
            for (auto& widget : materialTextureAssign_) widget.SetVisible(false);
            for (auto& widget : materialTextureClear_) widget.SetVisible(false);
        }
    }

    void StudioRenderPath::OffsetS1BMaterialContent(const float delta)
    {
        const auto offset = [delta](wi::gui::Widget& widget)
        {
            const auto pos = widget.GetPos();
            widget.SetPos(XMFLOAT2(pos.x, pos.y + delta));
        };
        for (wi::gui::Widget* widget : {
            static_cast<wi::gui::Widget*>(&materialLabel_),
            static_cast<wi::gui::Widget*>(&materialSelector_),
            static_cast<wi::gui::Widget*>(&materialShaderType_),
            static_cast<wi::gui::Widget*>(&materialBlendMode_),
            static_cast<wi::gui::Widget*>(&materialCoreLabel_),
            static_cast<wi::gui::Widget*>(&materialBaseColorRed_),
            static_cast<wi::gui::Widget*>(&materialBaseColorGreen_),
            static_cast<wi::gui::Widget*>(&materialBaseColorBlue_),
            static_cast<wi::gui::Widget*>(&materialOpacity_),
            static_cast<wi::gui::Widget*>(&materialMetalness_),
            static_cast<wi::gui::Widget*>(&materialRoughness_),
            static_cast<wi::gui::Widget*>(&materialReflectance_),
            static_cast<wi::gui::Widget*>(&materialNormalStrength_),
            static_cast<wi::gui::Widget*>(&materialAlphaRef_),
            static_cast<wi::gui::Widget*>(&materialEmissiveRed_),
            static_cast<wi::gui::Widget*>(&materialEmissiveGreen_),
            static_cast<wi::gui::Widget*>(&materialEmissiveBlue_),
            static_cast<wi::gui::Widget*>(&materialEmissiveStrength_),
            static_cast<wi::gui::Widget*>(&materialReceiveShadow_),
            static_cast<wi::gui::Widget*>(&materialCastShadow_),
            static_cast<wi::gui::Widget*>(&materialUseVertexColors_),
            static_cast<wi::gui::Widget*>(&materialDoubleSided_),
            static_cast<wi::gui::Widget*>(&materialUvLabel_),
            static_cast<wi::gui::Widget*>(&materialTilingU_),
            static_cast<wi::gui::Widget*>(&materialTilingV_),
            static_cast<wi::gui::Widget*>(&materialOffsetU_),
            static_cast<wi::gui::Widget*>(&materialOffsetV_),
            static_cast<wi::gui::Widget*>(&materialTexturesLabel_),
            static_cast<wi::gui::Widget*>(&materialShaderSpecificLabel_),
            static_cast<wi::gui::Widget*>(&materialPomStrength_),
            static_cast<wi::gui::Widget*>(&materialAnisotropyStrength_),
            static_cast<wi::gui::Widget*>(&materialAnisotropyRotation_),
            static_cast<wi::gui::Widget*>(&materialSheenRed_),
            static_cast<wi::gui::Widget*>(&materialSheenGreen_),
            static_cast<wi::gui::Widget*>(&materialSheenBlue_),
            static_cast<wi::gui::Widget*>(&materialSheenRoughness_),
            static_cast<wi::gui::Widget*>(&materialClearcoat_),
            static_cast<wi::gui::Widget*>(&materialClearcoatRoughness_),
            static_cast<wi::gui::Widget*>(&materialTransmission_),
            static_cast<wi::gui::Widget*>(&materialRefraction_),
            static_cast<wi::gui::Widget*>(&materialTerrainBlendHeight_),
            static_cast<wi::gui::Widget*>(&materialMeshBlend_),
            static_cast<wi::gui::Widget*>(&materialInteriorScaleX_),
            static_cast<wi::gui::Widget*>(&materialInteriorScaleY_),
            static_cast<wi::gui::Widget*>(&materialInteriorScaleZ_),
            static_cast<wi::gui::Widget*>(&materialInteriorOffsetX_),
            static_cast<wi::gui::Widget*>(&materialInteriorOffsetY_),
            static_cast<wi::gui::Widget*>(&materialInteriorOffsetZ_),
            static_cast<wi::gui::Widget*>(&materialInteriorRotation_)})
        {
            offset(*widget);
        }
        for (auto& widget : materialTextureAssign_) offset(widget);
        for (auto& widget : materialTextureClear_) offset(widget);
    }

    void StudioRenderPath::LayoutS1BInspectorSections()
    {
        transformSectionHeader_.SetVisible(false);
        renderingSectionHeader_.SetVisible(false);
        materialsSectionHeader_.SetVisible(false);

        if (session_ != nullptr && inspectorSectionPreferences_ == nullptr)
        {
            inspectorSectionPreferences_ =
                std::make_unique<ProjectInspectorSectionPreferenceStore>(
                    session_->Projects());
            inspectorSectionRegistry_.SetPreferenceStore(
                inspectorSectionPreferences_.get());
        }

        const float panelWidth = inspectorPanel_.GetSize().x;
        const float innerWidth = std::max(1.0f, panelWidth - 24.0f);
        const bool hasSession = session_ != nullptr;
        const auto selected = hasSession && session_->Selection().HasSelection()
            ? session_->Selection().SelectedEntity()
            : wi::ecs::INVALID_ENTITY;
        const auto& scene = hasSession
            ? session_->Scenes().GetScene()
            : wi::scene::GetScene();
        const bool selectedWeather = hasSession && selected != wi::ecs::INVALID_ENTITY &&
            scene.weathers.Contains(selected);
        const bool selectedTerrain = hasSession && selected != wi::ecs::INVALID_ENTITY &&
            scene.terrains.Contains(selected);
        if (environmentWorkspaceActive_ || terrainWorkspaceActive_ ||
            selectedWeather || selectedTerrain)
        {
            return;
        }

        const bool player = hasSession && selected != wi::ecs::INVALID_ENTITY &&
            bridge::IsPlayerStart(scene, selected);
        const bool camera = hasSession && selected != wi::ecs::INVALID_ENTITY &&
            scene.cameras.Contains(selected);
        const bool decal = hasSession && selected != wi::ecs::INVALID_ENTITY &&
            scene.decals.Contains(selected);
        const bool probe = hasSession && selected != wi::ecs::INVALID_ENTITY &&
            scene.probes.Contains(selected);
        const bool light = hasSession && selected != wi::ecs::INVALID_ENTITY &&
            scene.lights.Contains(selected);
        const bool sceneComponents = hasSession && selected != wi::ecs::INVALID_ENTITY && !player;

        float registryTop = 44.0f;
        if (sceneComponents)
        {
            float y = registryTop;
            const auto full = [innerWidth, &y](wi::gui::Widget& widget, const float height = 28.0f)
            {
                widget.SetPos(XMFLOAT2(12.0f, y));
                widget.SetSize(XMFLOAT2(innerWidth, height));
                y += height + 6.0f;
            };
            const auto section = [innerWidth, &y](wi::gui::Widget& widget)
            {
                widget.SetPos(XMFLOAT2(12.0f, y));
                widget.SetSize(XMFLOAT2(innerWidth, 20.0f));
                y += 20.0f;
            };
            section(sceneIdentityLabel_);
            full(sceneNameInput_);
            section(sceneLayerLabel_);
            const float half = (innerWidth - 8.0f) * 0.5f;
            sceneLayerAllButton_.SetPos(XMFLOAT2(12.0f, y));
            sceneLayerNoneButton_.SetPos(XMFLOAT2(20.0f + half, y));
            sceneLayerAllButton_.SetSize(XMFLOAT2(half, 28.0f));
            sceneLayerNoneButton_.SetSize(XMFLOAT2(half, 28.0f));
            y += 34.0f;
            const float bitGap = 4.0f;
            const float bitWidth = (innerWidth - bitGap * 7.0f) / 8.0f;
            for (std::size_t bit = 0; bit < sceneLayerBits_.size(); ++bit)
            {
                const float x = 12.0f + static_cast<float>(bit % 8u) * (bitWidth + bitGap);
                const float bitY = y + static_cast<float>(bit / 8u) * 26.0f;
                sceneLayerBits_[bit].SetPos(XMFLOAT2(x, bitY));
                sceneLayerBits_[bit].SetSize(XMFLOAT2(bitWidth, 22.0f));
            }
            y += 108.0f;
            section(sceneMetadataLabel_);
            full(sceneMetadataPreset_);
            registryTop = y + 4.0f;
        }

        InspectorSectionContext context;
        context.hasSelection = hasSession && selected != wi::ecs::INVALID_ENTITY;
        context.selectionRevision = selected != wi::ecs::INVALID_ENTITY
            ? static_cast<std::uint64_t>(selected)
            : 0u;
        context.userData = this;
        inspectorSectionRegistry_.RefreshVisibleSections(context);
        const auto layouts = inspectorSectionRegistry_.LayoutVisibleSections(
            context, registryTop, innerWidth);
        float bottom = registryTop;
        for (const auto& layout : layouts)
            bottom = std::max(bottom, layout.top + layout.totalHeight);

        const auto fullAt = [innerWidth](wi::gui::Widget& widget, const float y, const float height = 28.0f)
        {
            widget.SetPos(XMFLOAT2(12.0f, y));
            widget.SetSize(XMFLOAT2(innerWidth, height));
        };
        if (player)
        {
            float y = bottom + 8.0f;
            fullAt(playerLabel_, y, 20.0f); y += 20.0f;
            fullAt(playerCameraMode_, y, 32.0f); y += 36.0f;
            for (wi::gui::Widget* widget : {
                static_cast<wi::gui::Widget*>(&playerCapsuleRadius_),
                static_cast<wi::gui::Widget*>(&playerCapsuleHeight_),
                static_cast<wi::gui::Widget*>(&playerEyeHeight_),
                static_cast<wi::gui::Widget*>(&playerWalkSpeed_),
                static_cast<wi::gui::Widget*>(&playerSprintSpeed_),
                static_cast<wi::gui::Widget*>(&playerJumpSpeed_),
                static_cast<wi::gui::Widget*>(&playerLookSensitivity_),
                static_cast<wi::gui::Widget*>(&playerMaximumSlope_),
                static_cast<wi::gui::Widget*>(&playerGravityFactor_),
                static_cast<wi::gui::Widget*>(&playerMinimumPitch_),
                static_cast<wi::gui::Widget*>(&playerMaximumPitch_)})
            {
                fullAt(*widget, y); y += 34.0f;
            }
            bottom = y;
        }
        else if (camera)
        {
            float y = bottom + 8.0f;
            fullAt(cameraLabel_, y, 20.0f); y += 20.0f;
            for (wi::gui::Widget* widget : {
                static_cast<wi::gui::Widget*>(&cameraProjection_),
                static_cast<wi::gui::Widget*>(&cameraFieldOfView_),
                static_cast<wi::gui::Widget*>(&cameraNearPlane_),
                static_cast<wi::gui::Widget*>(&cameraFarPlane_),
                static_cast<wi::gui::Widget*>(&cameraFocalLength_),
                static_cast<wi::gui::Widget*>(&cameraApertureSize_),
                static_cast<wi::gui::Widget*>(&cameraOrthoVerticalSize_)})
            {
                fullAt(*widget, y); y += 34.0f;
            }
            const float half = (innerWidth - 8.0f) * 0.5f;
            cameraAlignToView_.SetPos(XMFLOAT2(12.0f, y));
            cameraViewFrom_.SetPos(XMFLOAT2(20.0f + half, y));
            cameraAlignToView_.SetSize(XMFLOAT2(half, 28.0f));
            cameraViewFrom_.SetSize(XMFLOAT2(half, 28.0f));
            bottom = y + 34.0f;
        }
        else if (decal)
        {
            float y = bottom + 8.0f;
            fullAt(decalLabel_, y, 20.0f); y += 22.0f;
            for (wi::gui::Widget* widget : {
                static_cast<wi::gui::Widget*>(&decalBaseColorOnlyAlpha_),
                static_cast<wi::gui::Widget*>(&decalSlopeBlend_)})
            {
                fullAt(*widget, y); y += 34.0f;
            }
            fullAt(decalMaterialLabel_, y, 20.0f); y += 24.0f;
            for (wi::gui::Widget* widget : {
                static_cast<wi::gui::Widget*>(&decalBaseColorRed_),
                static_cast<wi::gui::Widget*>(&decalBaseColorGreen_),
                static_cast<wi::gui::Widget*>(&decalBaseColorBlue_),
                static_cast<wi::gui::Widget*>(&decalOpacity_),
                static_cast<wi::gui::Widget*>(&decalBaseColorTexture_)})
            {
                fullAt(*widget, y); y += 34.0f;
            }
            bottom = y;
        }
        else if (probe)
        {
            float y = bottom + 8.0f;
            fullAt(environmentProbeLabel_, y, 20.0f); y += 24.0f;
            for (wi::gui::Widget* widget : {
                static_cast<wi::gui::Widget*>(&environmentProbeResolution_),
                static_cast<wi::gui::Widget*>(&environmentProbeRealtime_),
                static_cast<wi::gui::Widget*>(&environmentProbeInterval_),
                static_cast<wi::gui::Widget*>(&environmentProbeMsaa_),
                static_cast<wi::gui::Widget*>(&environmentProbeViewDistance_),
                static_cast<wi::gui::Widget*>(&environmentProbeRefresh_)})
            {
                fullAt(*widget, y); y += 34.0f;
            }
            bottom = y;
        }
        else if (light)
        {
            float y = bottom + 8.0f;
            fullAt(lightLabel_, y, 20.0f); y += 20.0f;
            for (wi::gui::Widget* widget : {
                static_cast<wi::gui::Widget*>(&lightType_),
                static_cast<wi::gui::Widget*>(&lightColorRed_),
                static_cast<wi::gui::Widget*>(&lightColorGreen_),
                static_cast<wi::gui::Widget*>(&lightColorBlue_),
                static_cast<wi::gui::Widget*>(&lightIntensity_),
                static_cast<wi::gui::Widget*>(&lightRange_),
                static_cast<wi::gui::Widget*>(&lightOuterCone_),
                static_cast<wi::gui::Widget*>(&lightInnerCone_),
                static_cast<wi::gui::Widget*>(&lightRadius_),
                static_cast<wi::gui::Widget*>(&lightLength_),
                static_cast<wi::gui::Widget*>(&lightHeight_),
                static_cast<wi::gui::Widget*>(&lightCastShadow_),
                static_cast<wi::gui::Widget*>(&lightVolumetrics_),
                static_cast<wi::gui::Widget*>(&lightVolumetricBoost_)})
            {
                fullAt(*widget, y); y += 34.0f;
            }
            bottom = y;
        }

        const float actionStart = bottom + 16.0f;
        constexpr float gap = 8.0f;
        const float three = (panelWidth - 40.0f) / 3.0f;
        const float two = (panelWidth - 32.0f) / 2.0f;
        focusButton_.SetPos(XMFLOAT2(12.0f, actionStart));
        duplicateButton_.SetPos(XMFLOAT2(12.0f + three + gap, actionStart));
        deleteButton_.SetPos(XMFLOAT2(12.0f + (three + gap) * 2.0f, actionStart));
        focusButton_.SetSize(XMFLOAT2(three, 28.0f));
        duplicateButton_.SetSize(XMFLOAT2(three, 28.0f));
        deleteButton_.SetSize(XMFLOAT2(three, 28.0f));
        const float history = actionStart + 40.0f;
        undoButton_.SetPos(XMFLOAT2(12.0f, history));
        redoButton_.SetPos(XMFLOAT2(20.0f + two, history));
        undoButton_.SetSize(XMFLOAT2(two, 28.0f));
        redoButton_.SetSize(XMFLOAT2(two, 28.0f));
        const float save = history + 40.0f;
        saveButton_.SetPos(XMFLOAT2(12.0f, save));
        saveAsButton_.SetPos(XMFLOAT2(12.0f + three + gap, save));
        reopenButton_.SetPos(XMFLOAT2(12.0f + (three + gap) * 2.0f, save));
        saveButton_.SetSize(XMFLOAT2(three, 28.0f));
        saveAsButton_.SetSize(XMFLOAT2(three, 28.0f));
        reopenButton_.SetSize(XMFLOAT2(three, 28.0f));
    }
}
