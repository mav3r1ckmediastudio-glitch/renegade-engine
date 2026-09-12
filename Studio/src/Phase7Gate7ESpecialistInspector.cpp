#include "Phase7Gate7ESpecialistInspector.h"

#include "InspectorSectionFramework.h"
#include "Phase7Gate7AAnimationInspector.h"
#include "Phase7Gate7BHumanoidRetargetInspector.h"
#include "Phase7Gate7CCharacterControlsInspector.h"
#include "Phase7Gate7DNativeTimelineInspector.h"
#include "RenegadeStudioChrome.h"
#include "S4BScriptAttachmentInspector.h"
#include "S4DGlobalScriptInspector.h"
#include "StudioApplication.h"

#include "renegade/bridge/SpecialistComponentService.h"
#include "renegade/bridge/StudioSession.h"
#include "renegade/bridge/TerrainCreatorGapService.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace renegade::studio
{
    namespace
    {
        enum class SpecialistPanel : std::uint64_t
        {
            Hair,
            ForceField,
            Video,
            Spline,
            GaussianSplat,
            Terrain,
        };

        std::string EntityName(const wi::scene::Scene& scene, wi::ecs::Entity entity)
        {
            if (const auto* name = scene.names.GetComponent(entity);
                name != nullptr && !name->name.empty())
            {
                return name->name;
            }
            return std::string("Entity ") + std::to_string(entity);
        }

        std::string MiB(std::uint64_t bytes)
        {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(2)
                   << (static_cast<double>(bytes) / (1024.0 * 1024.0)) << " MiB";
            return stream.str();
        }

        std::string EnsureR16Extension(std::string path)
        {
            if (path.size() < 4 || path.substr(path.size() - 4) != ".r16")
                path += ".r16";
            return path;
        }

        class SpecialistInspector;

        class SpecialistSectionProvider final : public IInspectorSectionProvider
        {
        public:
            explicit SpecialistSectionProvider(SpecialistInspector& owner) noexcept
                : owner_(&owner)
            {
                descriptor_.id = Phase7SpecialistSectionId;
                descriptor_.title = "SPECIALIST COMPONENTS";
                descriptor_.order = 38;
                descriptor_.defaultExpanded = false;
                descriptor_.headerHeight = 28.0f;
                descriptor_.spacingAfter = 6.0f;
            }

            [[nodiscard]] const InspectorSectionDescriptor& Descriptor() const noexcept override
            {
                return descriptor_;
            }
            [[nodiscard]] bool IsVisible(const InspectorSectionContext& context) const override;
            [[nodiscard]] float MeasureContentHeight(const InspectorSectionContext&, float) const override;
            void Refresh(const InspectorSectionContext&) override;
            void ApplyLayout(const InspectorSectionContext&, const InspectorSectionLayout& layout) override;

        private:
            SpecialistInspector* owner_ = nullptr;
            InspectorSectionDescriptor descriptor_;
        };

        class SpecialistInspector final
        {
        public:
            SpecialistInspector(
                StudioRenderPath& owner,
                wi::gui::Window& panel,
                InspectorSectionRegistry& registry,
                std::function<void()> requestRefresh,
                std::function<void(std::string)> setStatus)
                : owner_(&owner), panel_(&panel), registry_(&registry),
                  requestRefresh_(std::move(requestRefresh)), setStatus_(std::move(setStatus))
            {
            }

            void Register()
            {
                header_.Create("Phase 7E Specialist Section Header");
                header_.SetTooltip(
                    "Expose remaining Wicked-native creator components without embedding stock Wicked editor windows.");
                header_.OnClick([this](const wi::gui::EventArgs&)
                {
                    const bool opening = !registry_->IsExpanded(Phase7SpecialistSectionId);
                    for (const char* sectionId : {
                        "transform", "rendering", "materials",
                        S4BActionSectionId, S4BScriptSectionId, S4DGlobalScriptSectionId,
                        Phase7AnimationSectionId, Phase7HumanoidRetargetSectionId,
                        Phase7CharacterControlsSectionId, Phase7NativeTimelineSectionId,
                        Phase7SpecialistSectionId})
                    {
                        (void)registry_->SetExpanded(sectionId, false);
                    }
                    if (opening)
                        (void)registry_->SetExpanded(Phase7SpecialistSectionId, true);
                    RequestRefresh();
                });
                panel_->AddWidget(&header_);

                status_.Create("Phase 7E Specialist Status");
                status_.SetColor(wi::Color::Transparent());
                status_.SetFitTextEnabled(true);
                panel_->AddWidget(&status_);

                mode_.Create("Specialist Surface");
                mode_.AddItem("HAIR / FUR", static_cast<std::uint64_t>(SpecialistPanel::Hair));
                mode_.AddItem("FORCE FIELD", static_cast<std::uint64_t>(SpecialistPanel::ForceField));
                mode_.AddItem("VIDEO", static_cast<std::uint64_t>(SpecialistPanel::Video));
                mode_.AddItem("SPLINE", static_cast<std::uint64_t>(SpecialistPanel::Spline));
                mode_.AddItem("GAUSSIAN SPLAT", static_cast<std::uint64_t>(SpecialistPanel::GaussianSplat));
                mode_.AddItem("TERRAIN GAPS", static_cast<std::uint64_t>(SpecialistPanel::Terrain));
                mode_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    selectedPanel_ = static_cast<SpecialistPanel>(args.userdata);
                    RefreshControls();
                    RequestRefresh();
                });
                panel_->AddWidget(&mode_);

                CreateButton(addComponent_, "Add Specialist Component", "ADD COMPONENT", [this]() { AddComponent(); });

                hairMesh_.Create("Hair Surface Mesh");
                hairMesh_.SetTooltip("Native mesh that emits strands.");
                hairMesh_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    CommitHair([&](bridge::HairParticleState& state)
                    {
                        state.mesh = static_cast<wi::ecs::Entity>(args.userdata);
                    });
                });
                panel_->AddWidget(&hairMesh_);

                hairCameraBend_.Create("Camera Bend: ");
                hairCameraBend_.SetTooltip("Use Wicked's native camera-facing hair bend mode.");
                hairCameraBend_.OnClick([this](const wi::gui::EventArgs& args)
                {
                    CommitHair([&](bridge::HairParticleState& state) { state.cameraBend = args.bValue; });
                });
                panel_->AddWidget(&hairCameraBend_);

                CreateSlider(hairStrands_, "Hair Strand Count", "STRANDS", "Native strand budget.", 0.0f, 100000.0f,
                    [this](float v) { CommitHair([&](auto& s) { s.strandCount = static_cast<std::uint32_t>(std::lround(v)); }); });
                CreateSlider(hairLength_, "Hair Length", "LENGTH", "Native strand length.", 0.0f, 4.0f,
                    [this](float v) { CommitHair([&](auto& s) { s.length = v; }); });
                CreateSlider(hairWidth_, "Hair Width", "WIDTH", "Native strand width.", 0.0f, 2.0f,
                    [this](float v) { CommitHair([&](auto& s) { s.width = v; }); });
                CreateSlider(hairStiffness_, "Hair Stiffness", "STIFFNESS", "Native stiffness.", 0.0f, 10.0f,
                    [this](float v) { CommitHair([&](auto& s) { s.stiffness = v; }); });
                CreateSlider(hairDrag_, "Hair Drag", "DRAG", "Native drag.", 0.0f, 1.0f,
                    [this](float v) { CommitHair([&](auto& s) { s.drag = v; }); });
                CreateSlider(hairGravity_, "Hair Gravity", "GRAVITY", "Native gravity power.", 0.0f, 1.0f,
                    [this](float v) { CommitHair([&](auto& s) { s.gravity = v; }); });
                CreateSlider(hairRandomness_, "Hair Randomness", "RANDOM", "Native strand randomness.", 0.0f, 1.0f,
                    [this](float v) { CommitHair([&](auto& s) { s.randomness = v; }); });
                CreateSlider(hairSegments_, "Hair Segments", "SEGMENTS", "Segments per strand.", 1.0f, 10.0f,
                    [this](float v) { CommitHair([&](auto& s) { s.segmentCount = static_cast<std::uint32_t>(std::lround(v)); }); });
                CreateSlider(hairBillboards_, "Hair Billboards", "BILLBOARDS", "Billboards per segment.", 1.0f, 10.0f,
                    [this](float v) { CommitHair([&](auto& s) { s.billboardCount = static_cast<std::uint32_t>(std::lround(v)); }); });
                CreateSlider(hairSeed_, "Hair Seed", "SEED", "Deterministic native random seed.", 1.0f, 65535.0f,
                    [this](float v) { CommitHair([&](auto& s) { s.randomSeed = static_cast<std::uint32_t>(std::lround(v)); }); });
                CreateSlider(hairViewDistance_, "Hair View Distance", "VIEW DIST", "Maximum native draw distance.", 0.0f, 1000.0f,
                    [this](float v) { CommitHair([&](auto& s) { s.viewDistance = v; }); });
                CreateSlider(hairUniformity_, "Hair Uniformity", "UNIFORM", "Native strand distribution uniformity.", 0.01f, 2.0f,
                    [this](float v) { CommitHair([&](auto& s) { s.uniformity = v; }); });

                hairAtlas_.Create("Hair Atlas Variant");
                hairAtlas_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    selectedAtlas_ = static_cast<std::size_t>(args.userdata);
                    RefreshHair();
                    RequestRefresh();
                });
                panel_->AddWidget(&hairAtlas_);
                CreateButton(hairAtlasAdd_, "Add Hair Atlas Variant", "ADD VARIANT", [this]() { AddHairAtlas(); });
                CreateButton(hairAtlasRemove_, "Remove Hair Atlas Variant", "REMOVE LAST", [this]() { RemoveHairAtlas(); });
                CreateSlider(hairAtlasScaleX_, "Hair Atlas Scale X", "ATLAS SX", "Atlas UV X multiplier.", 0.0f, 2.0f,
                    [this](float v) { CommitHairAtlas([&](auto& r) { r.texMulAdd.x = v; }); });
                CreateSlider(hairAtlasScaleY_, "Hair Atlas Scale Y", "ATLAS SY", "Atlas UV Y multiplier.", 0.0f, 2.0f,
                    [this](float v) { CommitHairAtlas([&](auto& r) { r.texMulAdd.y = v; }); });
                CreateSlider(hairAtlasOffsetX_, "Hair Atlas Offset X", "ATLAS OX", "Atlas UV X offset.", -1.0f, 1.0f,
                    [this](float v) { CommitHairAtlas([&](auto& r) { r.texMulAdd.z = v; }); });
                CreateSlider(hairAtlasOffsetY_, "Hair Atlas Offset Y", "ATLAS OY", "Atlas UV Y offset.", -1.0f, 1.0f,
                    [this](float v) { CommitHairAtlas([&](auto& r) { r.texMulAdd.w = v; }); });
                CreateSlider(hairAtlasSize_, "Hair Atlas Size", "ATLAS SIZE", "Relative native variant size.", 0.0f, 2.0f,
                    [this](float v) { CommitHairAtlas([&](auto& r) { r.size = v; }); });

                forceType_.Create("Force Field Type");
                forceType_.AddItem("POINT", static_cast<std::uint64_t>(wi::scene::ForceFieldComponent::Type::Point));
                forceType_.AddItem("PLANE", static_cast<std::uint64_t>(wi::scene::ForceFieldComponent::Type::Plane));
                forceType_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    CommitForce([&](auto& s) { s.type = static_cast<wi::scene::ForceFieldComponent::Type>(args.userdata); });
                });
                panel_->AddWidget(&forceType_);
                CreateSlider(forceGravity_, "Force Gravity", "GRAVITY", "Positive attracts, negative repels.", -100.0f, 100.0f,
                    [this](float v) { CommitForce([&](auto& s) { s.gravity = v; }); });
                CreateSlider(forceRange_, "Force Range", "RANGE", "Native force-field range.", 0.0f, 1000.0f,
                    [this](float v) { CommitForce([&](auto& s) { s.range = v; }); });

                CreateButton(videoOpen_, "Open Native MP4", "OPEN MP4", [this]() { BrowseVideo(); });
                videoLoop_.Create("Loop Video: ");
                videoLoop_.OnClick([this](const wi::gui::EventArgs& args)
                {
                    CommitVideo([&](auto& s) { s.looped = args.bValue; });
                });
                panel_->AddWidget(&videoLoop_);
                CreateButton(videoPlay_, "Play Video", "PLAY", [this]() { VideoTransport(0); });
                CreateButton(videoPause_, "Pause Video", "PAUSE", [this]() { VideoTransport(1); });
                CreateButton(videoStop_, "Stop Video", "STOP", [this]() { VideoTransport(2); });
                CreateSlider(videoSeek_, "Video Seek", "SEEK", "Preview-only native seek position in seconds.", 0.0f, 1.0f,
                    [this](float v) { VideoSeek(v); });
                videoInfo_.Create("Video Native Info");
                videoInfo_.SetColor(wi::Color::Transparent());
                videoInfo_.SetFitTextEnabled(true);
                panel_->AddWidget(&videoInfo_);

                splineLoop_.Create("Loop Spline: ");
                splineLoop_.OnClick([this](const wi::gui::EventArgs& args)
                {
                    CommitSpline([&](auto& s) { s.looped = args.bValue; if (!s.looped) s.filled = false; });
                });
                panel_->AddWidget(&splineLoop_);
                splineFill_.Create("Fill Spline: ");
                splineFill_.OnClick([this](const wi::gui::EventArgs& args)
                {
                    CommitSpline([&](auto& s) { s.filled = args.bValue; });
                });
                panel_->AddWidget(&splineFill_);
                splineAligned_.Create("Draw Aligned: ");
                splineAligned_.OnClick([this](const wi::gui::EventArgs& args)
                {
                    CommitSpline([&](auto& s) { s.drawAligned = args.bValue; });
                });
                panel_->AddWidget(&splineAligned_);
                CreateButton(splineAddNode_, "Add Spline Node", "ADD NODE", [this]() { AddSplineNode(); });
                CreateButton(splineRemoveNode_, "Remove Last Spline Node", "REMOVE LAST NODE", [this]() { RemoveSplineNode(); });
                CreateSlider(splineWidth_, "Spline Width", "WIDTH", "Generated native spline width.", 0.001f, 100.0f,
                    [this](float v) { CommitSpline([&](auto& s) { s.width = v; }); });
                CreateSlider(splineRotation_, "Spline Rotation", "ROTATION", "Cross-section rotation in degrees.", -180.0f, 180.0f,
                    [this](float v) { CommitSpline([&](auto& s) { s.rotationRadians = wi::math::DegreesToRadians(v); }); });
                CreateSlider(splineHorizontal_, "Spline Horizontal Subdivisions", "H SUBDIV", "Native corridor subdivisions.", 0.0f, 100.0f,
                    [this](float v) { CommitSpline([&](auto& s) { s.horizontalSubdivisions = static_cast<int>(std::lround(v)); }); });
                CreateSlider(splineVertical_, "Spline Vertical Subdivisions", "V SUBDIV", "Native tunnel subdivisions.", 0.0f, 36.0f,
                    [this](float v) { CommitSpline([&](auto& s) { s.verticalSubdivisions = static_cast<int>(std::lround(v)); }); });
                splineNormals_.Create("Spline Fill Normals");
                splineNormals_.AddItem("HARD", wi::scene::MeshComponent::COMPUTE_NORMALS_HARD);
                splineNormals_.AddItem("SMOOTH", wi::scene::MeshComponent::COMPUTE_NORMALS_SMOOTH);
                splineNormals_.AddItem("SMOOTH FAST", wi::scene::MeshComponent::COMPUTE_NORMALS_SMOOTH_FAST);
                splineNormals_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    CommitSpline([&](auto& s) { s.fillNormals = static_cast<wi::scene::MeshComponent::COMPUTE_NORMALS>(args.userdata); });
                });
                panel_->AddWidget(&splineNormals_);
                CreateSlider(splineTerrain_, "Spline Terrain Modifier", "TERRAIN", "Native terrain deformation amount.", 0.0f, 1.0f,
                    [this](float v) { CommitSpline([&](auto& s) { s.terrainModifier = v; }); });
                CreateSlider(splineFalloff_, "Spline Terrain Texture Falloff", "TEX FALLOFF", "Native spline material falloff into terrain.", 0.0f, 1.0f,
                    [this](float v) { CommitSpline([&](auto& s) { s.terrainTextureFalloff = v; }); });
                CreateSlider(splinePushdown_, "Spline Terrain Push Down", "PUSH DOWN", "Native terrain push-down distance.", 0.0f, 100.0f,
                    [this](float v) { CommitSpline([&](auto& s) { s.terrainPushDown = v; }); });
                splineInfo_.Create("Spline Native Info");
                splineInfo_.SetColor(wi::Color::Transparent());
                panel_->AddWidget(&splineInfo_);

                CreateButton(gaussianImport_, "Import Gaussian PLY", "IMPORT PLY", [this]() { BrowseGaussian(); });
                gaussianInfo_.Create("Gaussian Splat Native Info");
                gaussianInfo_.SetColor(wi::Color::Transparent());
                gaussianInfo_.SetFitTextEnabled(true);
                panel_->AddWidget(&gaussianInfo_);

                terrainLayer_.Create("Terrain Material Paint Layer");
                terrainLayer_.AddItem("BASE / GRASS", wi::terrain::MATERIAL_BASE);
                terrainLayer_.AddItem("SLOPE", wi::terrain::MATERIAL_SLOPE);
                terrainLayer_.AddItem("LOW ALTITUDE", wi::terrain::MATERIAL_LOW_ALTITUDE);
                terrainLayer_.AddItem("HIGH ALTITUDE", wi::terrain::MATERIAL_HIGH_ALTITUDE);
                terrainLayer_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    terrainMaterialLayer_ = static_cast<std::size_t>(args.userdata);
                });
                panel_->AddWidget(&terrainLayer_);
                CreateSlider(terrainPaintX_, "Terrain Paint X", "PAINT X", "World-space material brush centre X.", -5000.0f, 5000.0f,
                    [this](float v) { terrainPaintCenter_.x = v; });
                CreateSlider(terrainPaintY_, "Terrain Paint Y", "PAINT Y", "World-space material brush centre Y.", -1000.0f, 1000.0f,
                    [this](float v) { terrainPaintCenter_.y = v; });
                CreateSlider(terrainPaintZ_, "Terrain Paint Z", "PAINT Z", "World-space material brush centre Z.", -5000.0f, 5000.0f,
                    [this](float v) { terrainPaintCenter_.z = v; });
                CreateSlider(terrainPaintRadius_, "Terrain Paint Radius", "RADIUS", "Native material-paint brush radius.", 0.25f, 250.0f,
                    [this](float v) { terrainPaintRadiusValue_ = v; });
                CreateSlider(terrainPaintAmount_, "Terrain Paint Amount", "AMOUNT", "Blend-layer affection per stroke.", 0.0f, 1.0f,
                    [this](float v) { terrainPaintAmountValue_ = v; });
                CreateSlider(terrainPaintSmooth_, "Terrain Paint Smoothness", "SMOOTH", "Brush edge smoothness.", 0.01f, 1.0f,
                    [this](float v) { terrainPaintSmoothValue_ = v; });
                CreateButton(terrainPaint_, "Paint Native Terrain Material", "PAINT MATERIAL", [this]() { PaintTerrain(); });
                CreateButton(terrainImportHeightmap_, "Import R16 Heightmap", "IMPORT R16", [this]() { BrowseHeightmapImport(); });
                CreateButton(terrainExportHeightmap_, "Export R16 Heightmap", "EXPORT R16", [this]() { BrowseHeightmapExport(); });
                terrainVtInfo_.Create("Terrain Virtual Texture Info");
                terrainVtInfo_.SetColor(wi::Color::Transparent());
                terrainVtInfo_.SetFitTextEnabled(true);
                panel_->AddWidget(&terrainVtInfo_);

                std::string error;
                if (!registry_->Register(std::make_shared<SpecialistSectionProvider>(*this), error))
                    SetStatus("PHASE 7E // " + error);
            }

            [[nodiscard]] bool IsVisible(const InspectorSectionContext& context) const
            {
                return context.hasSelection;
            }

            [[nodiscard]] float Measure() const noexcept
            {
                return selectedPanel_ == SpecialistPanel::Hair ? 820.0f : 610.0f;
            }

            void Refresh()
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || !session->Selection().HasSelection())
                    return;
                const auto selection = session->Selection().SelectedEntity();
                if (selection != lastSelection_)
                {
                    lastSelection_ = selection;
                    selectedAtlas_ = 0;
                    auto& scene = session->Scenes().GetScene();
                    if (scene.gaussian_splats.Contains(selection)) selectedPanel_ = SpecialistPanel::GaussianSplat;
                    else if (scene.terrains.Contains(selection)) selectedPanel_ = SpecialistPanel::Terrain;
                    else if (scene.hairs.Contains(selection)) selectedPanel_ = SpecialistPanel::Hair;
                    else if (scene.forces.Contains(selection)) selectedPanel_ = SpecialistPanel::ForceField;
                    else if (scene.videos.Contains(selection)) selectedPanel_ = SpecialistPanel::Video;
                    else if (scene.splines.Contains(selection)) selectedPanel_ = SpecialistPanel::Spline;
                    else selectedPanel_ = SpecialistPanel::Hair;

                    if (const auto* transform = scene.transforms.GetComponent(selection))
                        terrainPaintCenter_ = transform->GetPosition();
                }
                RefreshControls();
            }

            void PrepareForLayout()
            {
                for (auto* widget : Widgets()) widget->SetVisible(false);
            }

            void Layout(const InspectorSectionLayout& layout)
            {
                header_.SetVisible(true);
                header_.SetPos(XMFLOAT2(12.0f, layout.top));
                header_.SetSize(XMFLOAT2(layout.width, layout.headerHeight));
                header_.SetText(std::string(layout.expanded ? "▼  " : "▶  ") + "SPECIALIST COMPONENTS");
                if (!layout.expanded) return;

                const float x = 12.0f;
                const float width = layout.width;
                float y = layout.contentTop;
                auto place = [&](wi::gui::Widget& widget, float height = 28.0f)
                {
                    widget.SetVisible(true);
                    widget.SetPos(XMFLOAT2(x, y));
                    widget.SetSize(XMFLOAT2(width, height));
                    y += height + 6.0f;
                };
                auto pair = [&](wi::gui::Widget& left, wi::gui::Widget& right)
                {
                    const float half = (width - 8.0f) * 0.5f;
                    left.SetVisible(true); left.SetPos(XMFLOAT2(x, y)); left.SetSize(XMFLOAT2(half, 28.0f));
                    right.SetVisible(true); right.SetPos(XMFLOAT2(x + half + 8.0f, y)); right.SetSize(XMFLOAT2(half, 28.0f));
                    y += 34.0f;
                };

                place(status_, 36.0f);
                place(mode_);
                if (selectedPanel_ != SpecialistPanel::GaussianSplat && selectedPanel_ != SpecialistPanel::Terrain)
                    place(addComponent_);

                switch (selectedPanel_)
                {
                case SpecialistPanel::Hair:
                    place(hairMesh_); place(hairCameraBend_);
                    place(hairStrands_); place(hairLength_); place(hairWidth_); place(hairStiffness_);
                    place(hairDrag_); place(hairGravity_); place(hairRandomness_); place(hairSegments_);
                    place(hairBillboards_); place(hairSeed_); place(hairViewDistance_); place(hairUniformity_);
                    place(hairAtlas_); pair(hairAtlasAdd_, hairAtlasRemove_);
                    place(hairAtlasScaleX_); place(hairAtlasScaleY_); place(hairAtlasOffsetX_);
                    place(hairAtlasOffsetY_); place(hairAtlasSize_);
                    break;
                case SpecialistPanel::ForceField:
                    place(forceType_); place(forceGravity_); place(forceRange_);
                    break;
                case SpecialistPanel::Video:
                    place(videoOpen_); place(videoLoop_); pair(videoPlay_, videoPause_); place(videoStop_);
                    place(videoSeek_); place(videoInfo_, 52.0f);
                    break;
                case SpecialistPanel::Spline:
                    pair(splineAddNode_, splineRemoveNode_); place(splineLoop_); place(splineFill_); place(splineAligned_);
                    place(splineWidth_); place(splineRotation_); place(splineHorizontal_); place(splineVertical_);
                    place(splineNormals_); place(splineTerrain_); place(splineFalloff_); place(splinePushdown_);
                    place(splineInfo_, 24.0f);
                    break;
                case SpecialistPanel::GaussianSplat:
                    place(gaussianImport_); place(gaussianInfo_, 72.0f);
                    break;
                case SpecialistPanel::Terrain:
                    place(terrainLayer_); place(terrainPaintX_); place(terrainPaintY_); place(terrainPaintZ_);
                    place(terrainPaintRadius_); place(terrainPaintAmount_); place(terrainPaintSmooth_); place(terrainPaint_);
                    pair(terrainImportHeightmap_, terrainExportHeightmap_); place(terrainVtInfo_, 72.0f);
                    break;
                }
            }

        private:
            template<typename Fn>
            void CreateButton(SceneInspectorButton& button, const char* name, const char* text, Fn&& fn)
            {
                button.Create(name);
                button.SetText(text);
                button.OnClick([action = std::forward<Fn>(fn)](const wi::gui::EventArgs&) mutable { action(); });
                panel_->AddWidget(&button);
            }

            template<typename Fn>
            void CreateSlider(
                SceneInspectorSlider& slider,
                const char* name,
                const char* label,
                const char* tooltip,
                float minimum,
                float maximum,
                Fn&& fn)
            {
                slider.Create(minimum, maximum, minimum, 10000.0f, name, label);
                slider.SetTooltip(tooltip);
                slider.OnValueCommitted([action = std::forward<Fn>(fn)](float value) mutable { action(value); });
                panel_->AddWidget(&slider);
            }

            [[nodiscard]] std::vector<wi::gui::Widget*> Widgets()
            {
                return {
                    &header_, &status_, &mode_, &addComponent_,
                    &hairMesh_, &hairCameraBend_, &hairStrands_, &hairLength_, &hairWidth_, &hairStiffness_,
                    &hairDrag_, &hairGravity_, &hairRandomness_, &hairSegments_, &hairBillboards_, &hairSeed_,
                    &hairViewDistance_, &hairUniformity_, &hairAtlas_, &hairAtlasAdd_, &hairAtlasRemove_,
                    &hairAtlasScaleX_, &hairAtlasScaleY_, &hairAtlasOffsetX_, &hairAtlasOffsetY_, &hairAtlasSize_,
                    &forceType_, &forceGravity_, &forceRange_,
                    &videoOpen_, &videoLoop_, &videoPlay_, &videoPause_, &videoStop_, &videoSeek_, &videoInfo_,
                    &splineLoop_, &splineFill_, &splineAligned_, &splineAddNode_, &splineRemoveNode_, &splineWidth_,
                    &splineRotation_, &splineHorizontal_, &splineVertical_, &splineNormals_, &splineTerrain_,
                    &splineFalloff_, &splinePushdown_, &splineInfo_,
                    &gaussianImport_, &gaussianInfo_,
                    &terrainLayer_, &terrainPaintX_, &terrainPaintY_, &terrainPaintZ_, &terrainPaintRadius_,
                    &terrainPaintAmount_, &terrainPaintSmooth_, &terrainPaint_, &terrainImportHeightmap_,
                    &terrainExportHeightmap_, &terrainVtInfo_,
                };
            }

            [[nodiscard]] std::vector<wi::gui::Widget*> HairControls()
            {
                return {
                    &hairMesh_, &hairCameraBend_, &hairStrands_, &hairLength_, &hairWidth_, &hairStiffness_,
                    &hairDrag_, &hairGravity_, &hairRandomness_, &hairSegments_, &hairBillboards_, &hairSeed_,
                    &hairViewDistance_, &hairUniformity_, &hairAtlas_, &hairAtlasAdd_, &hairAtlasRemove_,
                    &hairAtlasScaleX_, &hairAtlasScaleY_, &hairAtlasOffsetX_, &hairAtlasOffsetY_, &hairAtlasSize_,
                };
            }

            [[nodiscard]] std::vector<wi::gui::Widget*> VideoControls()
            {
                return {&videoOpen_, &videoLoop_, &videoPlay_, &videoPause_, &videoStop_, &videoSeek_};
            }

            [[nodiscard]] std::vector<wi::gui::Widget*> SplineControls()
            {
                return {
                    &splineLoop_, &splineFill_, &splineAligned_, &splineAddNode_, &splineRemoveNode_, &splineWidth_,
                    &splineRotation_, &splineHorizontal_, &splineVertical_, &splineNormals_, &splineTerrain_,
                    &splineFalloff_, &splinePushdown_,
                };
            }

            [[nodiscard]] std::vector<wi::gui::Widget*> TerrainControls()
            {
                return {
                    &terrainLayer_, &terrainPaintX_, &terrainPaintY_, &terrainPaintZ_, &terrainPaintRadius_,
                    &terrainPaintAmount_, &terrainPaintSmooth_, &terrainPaint_, &terrainImportHeightmap_,
                    &terrainExportHeightmap_,
                };
            }

            [[nodiscard]] bridge::StudioSession* Session() const
            {
                return bridge::StudioSession::Current();
            }

            [[nodiscard]] wi::ecs::Entity SelectedEntity() const
            {
                auto* session = Session();
                return session != nullptr && session->Selection().HasSelection()
                    ? session->Selection().SelectedEntity()
                    : wi::ecs::INVALID_ENTITY;
            }

            void RefreshControls()
            {
                mode_.SetSelectedByUserdataWithoutCallback(static_cast<std::uint64_t>(selectedPanel_));
                auto* session = Session();
                const auto entity = SelectedEntity();
                if (session == nullptr || entity == wi::ecs::INVALID_ENTITY)
                {
                    status_.SetText("Select an entity to expose specialist components.");
                    return;
                }
                switch (selectedPanel_)
                {
                case SpecialistPanel::Hair: RefreshHair(); break;
                case SpecialistPanel::ForceField: RefreshForce(); break;
                case SpecialistPanel::Video: RefreshVideo(); break;
                case SpecialistPanel::Spline: RefreshSpline(); break;
                case SpecialistPanel::GaussianSplat: RefreshGaussian(); break;
                case SpecialistPanel::Terrain: RefreshTerrain(); break;
                }
            }

            void RefreshHair()
            {
                auto* session = Session();
                if (session == nullptr) return;
                auto& scene = session->Scenes().GetScene();
                const auto entity = SelectedEntity();
                auto* hair = scene.hairs.GetComponent(entity);
                addComponent_.SetText(hair ? "HAIR COMPONENT PRESENT" : "ADD HAIR / FUR");
                addComponent_.SetEnabled(hair == nullptr);
                status_.SetText(hair ? "Native HairParticleSystem // authored directly" : "No native HairParticleSystem on selection.");

                hairMesh_.ClearItems();
                hairMesh_.AddItem("NONE", static_cast<std::uint64_t>(wi::ecs::INVALID_ENTITY));
                for (std::size_t i = 0; i < scene.meshes.GetCount(); ++i)
                {
                    const auto meshEntity = scene.meshes.GetEntity(i);
                    hairMesh_.AddItem(EntityName(scene, meshEntity), static_cast<std::uint64_t>(meshEntity));
                }
                const bool enabled = hair != nullptr;
                for (auto* widget : HairControls()) widget->SetEnabled(enabled);
                if (!hair) return;

                const auto state = bridge::CaptureHairParticle(*hair);
                hairMesh_.SetSelectedByUserdataWithoutCallback(static_cast<std::uint64_t>(state.mesh));
                hairCameraBend_.SetCheck(state.cameraBend);
                hairStrands_.SetValue(static_cast<float>(state.strandCount));
                hairLength_.SetValue(state.length); hairWidth_.SetValue(state.width); hairStiffness_.SetValue(state.stiffness);
                hairDrag_.SetValue(state.drag); hairGravity_.SetValue(state.gravity); hairRandomness_.SetValue(state.randomness);
                hairSegments_.SetValue(static_cast<float>(state.segmentCount));
                hairBillboards_.SetValue(static_cast<float>(state.billboardCount));
                hairSeed_.SetValue(static_cast<float>(state.randomSeed));
                hairViewDistance_.SetValue(state.viewDistance); hairUniformity_.SetValue(state.uniformity);

                hairAtlas_.ClearItems();
                for (std::size_t i = 0; i < state.atlasRects.size(); ++i)
                    hairAtlas_.AddItem("VARIANT " + std::to_string(i + 1), static_cast<std::uint64_t>(i));
                if (selectedAtlas_ >= state.atlasRects.size())
                    selectedAtlas_ = state.atlasRects.empty() ? 0 : state.atlasRects.size() - 1;
                const bool hasAtlas = !state.atlasRects.empty();
                hairAtlas_.SetEnabled(hasAtlas);
                hairAtlasRemove_.SetEnabled(hasAtlas);
                for (SceneInspectorSlider* slider : {
                        &hairAtlasScaleX_, &hairAtlasScaleY_, &hairAtlasOffsetX_, &hairAtlasOffsetY_, &hairAtlasSize_})
                {
                    slider->SetEnabled(hasAtlas);
                }
                if (hasAtlas)
                {
                    hairAtlas_.SetSelectedByUserdataWithoutCallback(static_cast<std::uint64_t>(selectedAtlas_));
                    const auto& rect = state.atlasRects[selectedAtlas_];
                    hairAtlasScaleX_.SetValue(rect.texMulAdd.x); hairAtlasScaleY_.SetValue(rect.texMulAdd.y);
                    hairAtlasOffsetX_.SetValue(rect.texMulAdd.z); hairAtlasOffsetY_.SetValue(rect.texMulAdd.w);
                    hairAtlasSize_.SetValue(rect.size);
                }
            }

            void RefreshForce()
            {
                auto* session = Session(); if (!session) return;
                auto& scene = session->Scenes().GetScene();
                auto* force = scene.forces.GetComponent(SelectedEntity());
                addComponent_.SetText(force ? "FORCE FIELD PRESENT" : "ADD FORCE FIELD");
                addComponent_.SetEnabled(force == nullptr);
                status_.SetText(force ? "Native ForceFieldComponent // authored directly" : "No native ForceFieldComponent on selection.");
                const bool enabled = force != nullptr;
                forceType_.SetEnabled(enabled); forceGravity_.SetEnabled(enabled); forceRange_.SetEnabled(enabled);
                if (!force) return;
                const auto state = bridge::CaptureForceField(*force);
                forceType_.SetSelectedByUserdataWithoutCallback(static_cast<std::uint64_t>(state.type));
                forceGravity_.SetValue(state.gravity); forceRange_.SetValue(state.range);
            }

            void RefreshVideo()
            {
                auto* session = Session(); if (!session) return;
                auto& scene = session->Scenes().GetScene();
                auto* video = scene.videos.GetComponent(SelectedEntity());
                addComponent_.SetText(video ? "VIDEO COMPONENT PRESENT" : "ADD VIDEO");
                addComponent_.SetEnabled(video == nullptr);
                status_.SetText(video ? "Native VideoComponent // H264/H265 MP4" : "No native VideoComponent on selection.");
                for (auto* widget : VideoControls())
                    widget->SetEnabled(video != nullptr);
                if (!video)
                {
                    videoInfo_.SetText("Wicked video audio-track playback is not implemented upstream.");
                    return;
                }
                const auto authored = bridge::CaptureVideoAuthoredState(*video);
                const auto info = bridge::CaptureVideoInfo(*video);
                videoLoop_.SetCheck(authored.looped);
                videoSeek_.SetRange(0.0f, std::max(0.001f, info.duration));
                videoSeek_.SetValue(info.currentTime);
                videoPlay_.SetEnabled(info.loaded); videoPause_.SetEnabled(info.loaded);
                videoStop_.SetEnabled(info.loaded); videoSeek_.SetEnabled(info.loaded);
                std::ostringstream text;
                text << (authored.filename.empty() ? "No MP4 loaded" : authored.filename);
                if (info.loaded)
                    text << "\n" << info.profile << " // " << info.width << "x" << info.height << " // "
                         << std::fixed << std::setprecision(2) << info.framesPerSecond << " fps // " << info.duration << " s";
                text << "\nVideo audio track: unsupported by Wicked upstream";
                videoInfo_.SetText(text.str());
            }

            void RefreshSpline()
            {
                auto* session = Session(); if (!session) return;
                auto& scene = session->Scenes().GetScene();
                auto* spline = scene.splines.GetComponent(SelectedEntity());
                addComponent_.SetText(spline ? "SPLINE COMPONENT PRESENT" : "ADD SPLINE");
                addComponent_.SetEnabled(spline == nullptr);
                status_.SetText(spline ? "Native SplineComponent // nodes + generated corridor/tunnel" : "No native SplineComponent on selection.");
                for (auto* widget : SplineControls()) widget->SetEnabled(spline != nullptr);
                if (!spline) return;
                const auto state = bridge::CaptureSpline(*spline);
                splineLoop_.SetCheck(state.looped); splineFill_.SetCheck(state.filled); splineAligned_.SetCheck(state.drawAligned);
                splineFill_.SetEnabled(state.looped);
                splineWidth_.SetValue(state.width);
                splineRotation_.SetValue(wi::math::RadiansToDegrees(state.rotationRadians));
                splineHorizontal_.SetValue(static_cast<float>(state.horizontalSubdivisions));
                splineVertical_.SetValue(static_cast<float>(state.verticalSubdivisions));
                splineNormals_.SetSelectedByUserdataWithoutCallback(static_cast<std::uint64_t>(state.fillNormals));
                splineTerrain_.SetValue(state.terrainModifier); splineFalloff_.SetValue(state.terrainTextureFalloff);
                splinePushdown_.SetValue(state.terrainPushDown);
                splineRemoveNode_.SetEnabled(!spline->spline_node_entities.empty());
                splineInfo_.SetText(std::to_string(spline->spline_node_entities.size()) + " native spline nodes");
            }

            void RefreshGaussian()
            {
                auto* session = Session(); if (!session) return;
                const auto info = bridge::CaptureGaussianSplatInfo(session->Scenes().GetScene(), SelectedEntity());
                status_.SetText(info.valid ? "Native GaussianSplatModel // inspection only" : "Select a Gaussian splat or IMPORT PLY.");
                if (!info.valid)
                {
                    gaussianInfo_.SetText("Wicked exposes imported splat statistics; Renegade does not invent a parallel splat editor.");
                    return;
                }
                gaussianInfo_.SetText(
                    std::to_string(info.splatCount) + " splats // SH degree " + std::to_string(info.sphericalHarmonicsDegree) +
                    "\nCPU " + MiB(info.cpuMemoryBytes) + " // GPU " + MiB(info.gpuMemoryBytes));
            }

            void RefreshTerrain()
            {
                auto* session = Session(); if (!session) return;
                auto& scene = session->Scenes().GetScene();
                auto* terrain = scene.terrains.GetComponent(SelectedEntity());
                status_.SetText(terrain ? "Native Terrain // material paint + R16 heightmap + VT visibility" : "Selected entity is not a native Terrain.");
                const bool enabled = terrain != nullptr;
                for (auto* widget : TerrainControls()) widget->SetEnabled(enabled);
                if (!terrain)
                {
                    terrainVtInfo_.SetText("No terrain selected.");
                    return;
                }
                terrainLayer_.SetSelectedByUserdataWithoutCallback(static_cast<std::uint64_t>(terrainMaterialLayer_));
                terrainPaintX_.SetValue(terrainPaintCenter_.x); terrainPaintY_.SetValue(terrainPaintCenter_.y); terrainPaintZ_.SetValue(terrainPaintCenter_.z);
                terrainPaintRadius_.SetValue(terrainPaintRadiusValue_); terrainPaintAmount_.SetValue(terrainPaintAmountValue_);
                terrainPaintSmooth_.SetValue(terrainPaintSmoothValue_);
                const auto info = bridge::CaptureTerrainVirtualTextureInfo(*terrain);
                terrainVtInfo_.SetText(
                    std::to_string(info.chunksWithVirtualTexture) + "/" + std::to_string(info.chunkCount) + " chunks with VT\n" +
                    std::to_string(info.virtualTexturesInUse) + " native VTs in use // atlas " +
                    std::to_string(info.tileWidth) + "x" + std::to_string(info.tileHeight) + " physical tiles\n" +
                    std::to_string(info.tileAllocations) + " pending tile allocations");
            }

            void AddComponent()
            {
                auto* session = Session();
                const auto entity = SelectedEntity();
                if (!session || entity == wi::ecs::INVALID_ENTITY) return;
                bridge::SpecialistComponentKind kind;
                switch (selectedPanel_)
                {
                case SpecialistPanel::Hair: kind = bridge::SpecialistComponentKind::HairParticle; break;
                case SpecialistPanel::ForceField: kind = bridge::SpecialistComponentKind::ForceField; break;
                case SpecialistPanel::Video: kind = bridge::SpecialistComponentKind::Video; break;
                case SpecialistPanel::Spline: kind = bridge::SpecialistComponentKind::Spline; break;
                default: return;
                }
                if (session->Commands().Execute(std::make_unique<bridge::CreateSpecialistComponentCommand>(
                        session->Scenes().GetScene(), entity, kind)))
                    SetStatus("PHASE 7E // native specialist component added");
                RefreshControls(); RequestRefresh();
            }

            template<typename Mutator>
            void CommitHair(Mutator&& mutator)
            {
                auto* session = Session(); if (!session) return;
                auto& scene = session->Scenes().GetScene(); const auto entity = SelectedEntity();
                auto* hair = scene.hairs.GetComponent(entity); if (!hair) return;
                auto state = bridge::CaptureHairParticle(*hair); mutator(state);
                if (session->Commands().Execute(std::make_unique<bridge::SetHairParticleStateCommand>(scene, entity, std::move(state))))
                    SetStatus("PHASE 7E // native hair state committed");
                RefreshHair(); RequestRefresh();
            }

            template<typename Mutator>
            void CommitHairAtlas(Mutator&& mutator)
            {
                CommitHair([&](bridge::HairParticleState& state)
                {
                    if (selectedAtlas_ < state.atlasRects.size()) mutator(state.atlasRects[selectedAtlas_]);
                });
            }

            void AddHairAtlas()
            {
                CommitHair([&](bridge::HairParticleState& state)
                {
                    state.atlasRects.push_back({}); selectedAtlas_ = state.atlasRects.size() - 1;
                });
            }

            void RemoveHairAtlas()
            {
                CommitHair([&](bridge::HairParticleState& state)
                {
                    if (!state.atlasRects.empty()) state.atlasRects.pop_back();
                    if (selectedAtlas_ > 0) --selectedAtlas_;
                });
            }

            template<typename Mutator>
            void CommitForce(Mutator&& mutator)
            {
                auto* session = Session(); if (!session) return;
                auto& scene = session->Scenes().GetScene(); const auto entity = SelectedEntity();
                auto* force = scene.forces.GetComponent(entity); if (!force) return;
                auto state = bridge::CaptureForceField(*force); mutator(state);
                if (session->Commands().Execute(std::make_unique<bridge::SetForceFieldStateCommand>(scene, entity, state)))
                    SetStatus("PHASE 7E // native force-field state committed");
                RefreshForce(); RequestRefresh();
            }

            template<typename Mutator>
            void CommitVideo(Mutator&& mutator)
            {
                auto* session = Session(); if (!session) return;
                auto& scene = session->Scenes().GetScene(); const auto entity = SelectedEntity();
                auto* video = scene.videos.GetComponent(entity); if (!video) return;
                auto state = bridge::CaptureVideoAuthoredState(*video); mutator(state);
                if (session->Commands().Execute(std::make_unique<bridge::SetVideoAuthoredStateCommand>(scene, entity, std::move(state))))
                    SetStatus("PHASE 7E // native video state committed");
                RefreshVideo(); RequestRefresh();
            }

            template<typename Mutator>
            void CommitSpline(Mutator&& mutator)
            {
                auto* session = Session(); if (!session) return;
                auto& scene = session->Scenes().GetScene(); const auto entity = SelectedEntity();
                auto* spline = scene.splines.GetComponent(entity); if (!spline) return;
                auto state = bridge::CaptureSpline(*spline); mutator(state);
                if (session->Commands().Execute(std::make_unique<bridge::SetSplineStateCommand>(scene, entity, state)))
                    SetStatus("PHASE 7E // native spline state committed");
                RefreshSpline(); RequestRefresh();
            }

            void AddSplineNode()
            {
                auto* session = Session(); if (!session) return;
                if (session->Commands().Execute(std::make_unique<bridge::AddSplineNodeCommand>(
                        session->Scenes().GetScene(), SelectedEntity())))
                    SetStatus("PHASE 7E // native spline node added");
                RefreshSpline(); RequestRefresh();
            }

            void RemoveSplineNode()
            {
                auto* session = Session(); if (!session) return;
                if (session->Commands().Execute(std::make_unique<bridge::RemoveLastSplineNodeCommand>(
                        session->Scenes().GetScene(), SelectedEntity())))
                    SetStatus("PHASE 7E // last native spline node removed");
                RefreshSpline(); RequestRefresh();
            }

            void BrowseVideo()
            {
                const auto entity = SelectedEntity();
                if (entity == wi::ecs::INVALID_ENTITY) return;
                wi::helper::FileDialogParams params;
                params.type = wi::helper::FileDialogParams::OPEN;
                params.description = "Native MP4 video (H264/H265)";
                params.extensions = {"mp4"};
                wi::helper::FileDialog(params, [this, entity](const std::string& filename)
                {
                    if (filename.empty()) return;
                    wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                        [this, entity, filename](std::uint64_t)
                        {
                            auto* session = Session(); if (!session) return;
                            auto& scene = session->Scenes().GetScene();
                            auto* video = scene.videos.GetComponent(entity); if (!video) return;
                            auto state = bridge::CaptureVideoAuthoredState(*video); state.filename = filename;
                            if (session->Commands().Execute(std::make_unique<bridge::SetVideoAuthoredStateCommand>(scene, entity, std::move(state))))
                                SetStatus("PHASE 7E // native MP4 loaded");
                            RefreshControls(); RequestRefresh();
                        });
                });
            }

            void VideoTransport(int action)
            {
                auto* session = Session(); if (!session) return;
                bool ok = false;
                auto& scene = session->Scenes().GetScene();
                if (action == 0) ok = bridge::PlayVideo(scene, SelectedEntity());
                else if (action == 1) ok = bridge::PauseVideo(scene, SelectedEntity());
                else ok = bridge::StopVideo(scene, SelectedEntity());
                if (ok)
                    SetStatus(action == 0 ? "PHASE 7E // video playing" :
                              action == 1 ? "PHASE 7E // video paused" : "PHASE 7E // video stopped");
                RefreshVideo(); RequestRefresh();
            }

            void VideoSeek(float seconds)
            {
                auto* session = Session(); if (!session) return;
                if (bridge::SeekVideo(session->Scenes().GetScene(), SelectedEntity(), seconds))
                    SetStatus("PHASE 7E // video seek preview");
                RefreshVideo(); RequestRefresh();
            }

            void BrowseGaussian()
            {
                wi::helper::FileDialogParams params;
                params.type = wi::helper::FileDialogParams::OPEN;
                params.description = "Gaussian splat PLY";
                params.extensions = {"ply"};
                wi::helper::FileDialog(params, [this](const std::string& filename)
                {
                    if (filename.empty()) return;
                    wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                        [this, filename](std::uint64_t)
                        {
                            auto* session = Session(); if (!session) return;
                            auto command = std::make_unique<bridge::ImportGaussianSplatCommand>(session->Scenes().GetScene(), filename);
                            auto* executed = command.get();
                            if (!executed->Execute())
                                SetStatus("PHASE 7E // " + executed->Error());
                            else
                            {
                                session->Commands().RecordExecuted(std::move(command));
                                SetStatus("PHASE 7E // native Gaussian PLY imported");
                            }
                            RefreshControls(); RequestRefresh();
                        });
                });
            }

            void PaintTerrain()
            {
                auto* session = Session(); if (!session) return;
                if (session->Commands().Execute(std::make_unique<bridge::TerrainMaterialPaintCommand>(
                        session->Scenes().GetScene(), SelectedEntity(), terrainPaintCenter_, terrainPaintRadiusValue_,
                        terrainPaintAmountValue_, terrainPaintSmoothValue_, terrainMaterialLayer_)))
                    SetStatus("PHASE 7E // native terrain material layer painted");
                RefreshTerrain(); RequestRefresh();
            }

            void BrowseHeightmapImport()
            {
                const auto entity = SelectedEntity();
                wi::helper::FileDialogParams params;
                params.type = wi::helper::FileDialogParams::OPEN;
                params.description = "Square little-endian R16 terrain heightmap";
                params.extensions = {"r16"};
                wi::helper::FileDialog(params, [this, entity](const std::string& filename)
                {
                    if (filename.empty()) return;
                    wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                        [this, entity, filename](std::uint64_t)
                        {
                            auto* session = Session(); if (!session) return;
                            auto command = std::make_unique<bridge::ImportTerrainHeightmapR16Command>(
                                session->Scenes().GetScene(), entity, filename);
                            auto* executed = command.get();
                            if (!executed->Execute())
                                SetStatus("PHASE 7E // " + executed->Error());
                            else
                            {
                                const auto info = executed->Info();
                                session->Commands().RecordExecuted(std::move(command));
                                SetStatus("PHASE 7E // R16 heightmap imported " + std::to_string(info.width) + "x" + std::to_string(info.height));
                            }
                            RefreshControls(); RequestRefresh();
                        });
                });
            }

            void BrowseHeightmapExport()
            {
                const auto entity = SelectedEntity();
                wi::helper::FileDialogParams params;
                params.type = wi::helper::FileDialogParams::SAVE;
                params.description = "Export square little-endian R16 terrain heightmap";
                params.extensions = {"r16"};
                wi::helper::FileDialog(params, [this, entity](const std::string& selectedPath)
                {
                    if (selectedPath.empty()) return;
                    const std::string filename = EnsureR16Extension(selectedPath);
                    wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                        [this, entity, filename](std::uint64_t)
                        {
                            auto* session = Session(); if (!session) return;
                            auto& scene = session->Scenes().GetScene();
                            auto* terrain = scene.terrains.GetComponent(entity); if (!terrain) return;
                            bridge::TerrainHeightmapInfo info;
                            std::string error;
                            if (bridge::ExportTerrainHeightmapR16(scene, *terrain, filename, &info, &error))
                                SetStatus("PHASE 7E // R16 heightmap exported " + std::to_string(info.width) + "x" + std::to_string(info.height));
                            else
                                SetStatus("PHASE 7E // " + error);
                            RequestRefresh();
                        });
                });
            }

            void RequestRefresh() { if (requestRefresh_) requestRefresh_(); }
            void SetStatus(std::string value) { if (setStatus_) setStatus_(std::move(value)); }

            StudioRenderPath* owner_ = nullptr;
            wi::gui::Window* panel_ = nullptr;
            InspectorSectionRegistry* registry_ = nullptr;
            std::function<void()> requestRefresh_;
            std::function<void(std::string)> setStatus_;
            SpecialistPanel selectedPanel_ = SpecialistPanel::Hair;
            wi::ecs::Entity lastSelection_ = wi::ecs::INVALID_ENTITY;
            std::size_t selectedAtlas_ = 0;
            std::size_t terrainMaterialLayer_ = wi::terrain::MATERIAL_BASE;
            XMFLOAT3 terrainPaintCenter_ = {};
            float terrainPaintRadiusValue_ = 12.0f;
            float terrainPaintAmountValue_ = 0.5f;
            float terrainPaintSmoothValue_ = 0.55f;

            SceneInspectorButton header_;
            wi::gui::Label status_;
            SceneInspectorComboBox mode_;
            SceneInspectorButton addComponent_;

            SceneInspectorComboBox hairMesh_;
            SceneInspectorCheckBox hairCameraBend_;
            SceneInspectorSlider hairStrands_, hairLength_, hairWidth_, hairStiffness_, hairDrag_, hairGravity_, hairRandomness_;
            SceneInspectorSlider hairSegments_, hairBillboards_, hairSeed_, hairViewDistance_, hairUniformity_;
            SceneInspectorComboBox hairAtlas_;
            SceneInspectorButton hairAtlasAdd_, hairAtlasRemove_;
            SceneInspectorSlider hairAtlasScaleX_, hairAtlasScaleY_, hairAtlasOffsetX_, hairAtlasOffsetY_, hairAtlasSize_;

            SceneInspectorComboBox forceType_;
            SceneInspectorSlider forceGravity_, forceRange_;

            SceneInspectorButton videoOpen_, videoPlay_, videoPause_, videoStop_;
            SceneInspectorCheckBox videoLoop_;
            SceneInspectorSlider videoSeek_;
            wi::gui::Label videoInfo_;

            SceneInspectorCheckBox splineLoop_, splineFill_, splineAligned_;
            SceneInspectorButton splineAddNode_, splineRemoveNode_;
            SceneInspectorSlider splineWidth_, splineRotation_, splineHorizontal_, splineVertical_;
            SceneInspectorComboBox splineNormals_;
            SceneInspectorSlider splineTerrain_, splineFalloff_, splinePushdown_;
            wi::gui::Label splineInfo_;

            SceneInspectorButton gaussianImport_;
            wi::gui::Label gaussianInfo_;

            SceneInspectorComboBox terrainLayer_;
            SceneInspectorSlider terrainPaintX_, terrainPaintY_, terrainPaintZ_, terrainPaintRadius_, terrainPaintAmount_, terrainPaintSmooth_;
            SceneInspectorButton terrainPaint_, terrainImportHeightmap_, terrainExportHeightmap_;
            wi::gui::Label terrainVtInfo_;
        };

        bool SpecialistSectionProvider::IsVisible(const InspectorSectionContext& context) const
        {
            return owner_ != nullptr && owner_->IsVisible(context);
        }

        float SpecialistSectionProvider::MeasureContentHeight(const InspectorSectionContext&, float) const
        {
            return owner_ != nullptr ? owner_->Measure() : 0.0f;
        }

        void SpecialistSectionProvider::Refresh(const InspectorSectionContext&)
        {
            if (owner_ != nullptr) owner_->Refresh();
        }

        void SpecialistSectionProvider::ApplyLayout(const InspectorSectionContext&, const InspectorSectionLayout& layout)
        {
            if (owner_ != nullptr) owner_->Layout(layout);
        }

        std::unique_ptr<SpecialistInspector> activeInspector;
        StudioRenderPath* activeOwner = nullptr;
    }

    void RegisterPhase7Gate7ESpecialistInspector(
        StudioRenderPath& owner,
        wi::gui::Window& inspectorPanel,
        InspectorSectionRegistry& registry,
        std::function<void()> requestRefresh,
        std::function<void(std::string)> setStatus)
    {
        activeInspector.reset();
        activeOwner = &owner;
        activeInspector = std::make_unique<SpecialistInspector>(
            owner, inspectorPanel, registry, std::move(requestRefresh), std::move(setStatus));
        activeInspector->Register();
    }

    void PreparePhase7Gate7ESpecialistInspector(StudioRenderPath& owner)
    {
        if (activeOwner == &owner && activeInspector)
            activeInspector->PrepareForLayout();
    }
}
