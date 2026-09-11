#include "Phase7NativeInspector.h"

#include "InspectorSectionFramework.h"
#include "RenegadeStudioChrome.h"
#include "StudioApplication.h"

#include "renegade/bridge/SpecialistComponentService.h"
#include "renegade/bridge/StudioSession.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace renegade::studio
{
    namespace
    {
        enum class SpecialistPage : std::uint64_t
        {
            Hair = 0,
            Force = 1,
            Video = 2,
            Spline = 3,
            Gaussian = 4,
        };

        std::string EntityName(
            const wi::scene::Scene& scene,
            const wi::ecs::Entity entity)
        {
            if (entity == wi::ecs::INVALID_ENTITY)
                return "None";
            const auto* name = scene.names.GetComponent(entity);
            if (name != nullptr && !name->name.empty())
                return name->name;
            return "Entity " + std::to_string(entity);
        }

        InspectorSectionDescriptor MakeSection()
        {
            InspectorSectionDescriptor descriptor;
            descriptor.id = Phase7SpecialistSectionId;
            descriptor.title = "SPECIALIST NATIVE";
            descriptor.order = 38;
            descriptor.defaultExpanded = false;
            descriptor.headerHeight = 28.0f;
            descriptor.spacingAfter = 6.0f;
            return descriptor;
        }

        class SpecialistInspector;

        class SpecialistProvider final : public IInspectorSectionProvider
        {
        public:
            explicit SpecialistProvider(SpecialistInspector& owner)
                : owner_(&owner), descriptor_(MakeSection()) {}
            [[nodiscard]] const InspectorSectionDescriptor& Descriptor()
                const noexcept override { return descriptor_; }
            [[nodiscard]] bool IsVisible(
                const InspectorSectionContext& context) const override;
            [[nodiscard]] float MeasureContentHeight(
                const InspectorSectionContext& context,
                float availableWidth) const override;
            void Refresh(const InspectorSectionContext& context) override;
            void ApplyLayout(
                const InspectorSectionContext& context,
                const InspectorSectionLayout& layout) override;
        private:
            SpecialistInspector* owner_ = nullptr;
            InspectorSectionDescriptor descriptor_;
        };

        struct HairControls
        {
            SceneInspectorComboBox mesh;
            SceneInspectorSlider count;
            SceneInspectorSlider length;
            SceneInspectorSlider width;
            SceneInspectorSlider stiffness;
            SceneInspectorSlider drag;
            SceneInspectorSlider gravity;
            SceneInspectorSlider randomness;
            SceneInspectorSlider segments;
            SceneInspectorSlider billboards;
            SceneInspectorSlider seed;
            SceneInspectorSlider viewDistance;
            SceneInspectorSlider uniformity;
            SceneInspectorCheckBox cameraBend;
        };

        struct ForceControls
        {
            SceneInspectorComboBox type;
            SceneInspectorSlider gravity;
            SceneInspectorSlider range;
        };

        struct VideoControls
        {
            wi::gui::Label source;
            SceneInspectorButton browse;
            SceneInspectorButton clear;
            SceneInspectorCheckBox looped;
            SceneInspectorButton playPause;
            SceneInspectorButton stop;
            SceneInspectorSlider seek;
        };

        struct SplineControls
        {
            SceneInspectorCheckBox looped;
            SceneInspectorCheckBox filled;
            SceneInspectorCheckBox aligned;
            SceneInspectorSlider width;
            SceneInspectorSlider rotation;
            SceneInspectorSlider subdivision;
            SceneInspectorSlider verticalSubdivision;
            SceneInspectorSlider terrainModifier;
            SceneInspectorSlider terrainFalloff;
            SceneInspectorSlider terrainPushdown;
            SceneInspectorComboBox fillNormals;
            SceneInspectorButton addNode;
        };

        struct GaussianControls
        {
            SceneInspectorButton import;
            wi::gui::Label info;
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
                : owner_(&owner)
                , panel_(&panel)
                , registry_(&registry)
                , requestRefresh_(std::move(requestRefresh))
                , setStatus_(std::move(setStatus))
            {
            }

            void Register()
            {
                header_.Create("Phase7 Specialist Header");
                header_.OnClick([this](const wi::gui::EventArgs&)
                {
                    const bool opening = !registry_->IsExpanded(Phase7SpecialistSectionId);
                    for (const char* id : {
                        "transform", "rendering", "materials", "action", "script",
                        "global_script", Phase7AnimationSectionId, Phase7RigSectionId,
                        Phase7IkExpressionSectionId, Phase7TimelineSectionId,
                        Phase7SpecialistSectionId})
                    {
                        (void)registry_->SetExpanded(id, false);
                    }
                    if (opening)
                        (void)registry_->SetExpanded(Phase7SpecialistSectionId, true);
                    RequestRefresh();
                });
                panel_->AddWidget(&header_);
                status_.Create("Phase7 Specialist Status");
                status_.SetWrapEnabled(true);
                panel_->AddWidget(&status_);
                page_.Create("NATIVE COMPONENT");
                page_.AddItem("Hair / Fur", static_cast<std::uint64_t>(SpecialistPage::Hair));
                page_.AddItem("Force Field", static_cast<std::uint64_t>(SpecialistPage::Force));
                page_.AddItem("Video", static_cast<std::uint64_t>(SpecialistPage::Video));
                page_.AddItem("Spline", static_cast<std::uint64_t>(SpecialistPage::Spline));
                page_.AddItem("Gaussian Splat", static_cast<std::uint64_t>(SpecialistPage::Gaussian));
                page_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    pageValue_ = static_cast<SpecialistPage>(args.userdata);
                    RequestRefresh();
                });
                panel_->AddWidget(&page_);
                add_.Create("ADD NATIVE COMPONENT");
                add_.OnClick([this](const wi::gui::EventArgs&) { AddOrImport(); });
                panel_->AddWidget(&add_);

                CreateHair();
                CreateForce();
                CreateVideo();
                CreateSpline();
                CreateGaussian();
                SetAllVisible(false);

                std::string error;
                if (!registry_->Register(std::make_shared<SpecialistProvider>(*this), error))
                    SetStatus("PHASE 7E // " + error);
            }

            void Prepare()
            {
                SetAllVisible(false);
            }

            [[nodiscard]] bool IsVisible(const InspectorSectionContext& context) const
            {
                if (!context.hasSelection)
                    return false;
                const auto* session = bridge::StudioSession::Current();
                return session != nullptr && session->Selection().HasSelection();
            }

            [[nodiscard]] float Measure() const noexcept
            {
                switch (pageValue_)
                {
                case SpecialistPage::Hair: return 580.0f;
                case SpecialistPage::Force: return 190.0f;
                case SpecialistPage::Video: return 292.0f;
                case SpecialistPage::Spline: return 510.0f;
                case SpecialistPage::Gaussian: return 178.0f;
                }
                return 200.0f;
            }

            void Refresh()
            {
                auto* session = Session();
                if (session == nullptr || !session->Selection().HasSelection())
                    return;
                auto& scene = session->Scenes().GetScene();
                const auto selected = session->Selection().SelectedEntity();
                page_.SetSelectedByUserdataWithoutCallback(static_cast<std::uint64_t>(pageValue_));

                switch (pageValue_)
                {
                case SpecialistPage::Hair:
                    RefreshHair(scene, selected);
                    break;
                case SpecialistPage::Force:
                    RefreshForce(scene, selected);
                    break;
                case SpecialistPage::Video:
                    RefreshVideo(scene, selected);
                    break;
                case SpecialistPage::Spline:
                    RefreshSpline(scene, selected);
                    break;
                case SpecialistPage::Gaussian:
                    RefreshGaussian(scene, selected);
                    break;
                }
            }

            void Layout(const InspectorSectionLayout& layout)
            {
                header_.SetVisible(true);
                Place(header_, layout.top, layout.width, layout.headerHeight);
                header_.SetText(std::string(layout.expanded ? "▼  " : "▶  ") + "SPECIALIST NATIVE");
                if (!layout.expanded)
                {
                    SetAllVisible(false, true);
                    return;
                }
                float y = layout.contentTop;
                Place(status_, y, layout.width, 42.0f); y += 46.0f;
                Place(page_, y, layout.width); y += 34.0f;
                Place(add_, y, layout.width); y += 36.0f;
                HideSpecific();
                switch (pageValue_)
                {
                case SpecialistPage::Hair:
                    LayoutHair(y, layout.width);
                    break;
                case SpecialistPage::Force:
                    LayoutForce(y, layout.width);
                    break;
                case SpecialistPage::Video:
                    LayoutVideo(y, layout.width);
                    break;
                case SpecialistPage::Spline:
                    LayoutSpline(y, layout.width);
                    break;
                case SpecialistPage::Gaussian:
                    LayoutGaussian(y, layout.width);
                    break;
                }
            }

        private:
            bridge::StudioSession* Session() const noexcept
            {
                return bridge::StudioSession::Current();
            }
            void RequestRefresh()
            {
                if (requestRefresh_) requestRefresh_();
            }
            void SetStatus(std::string text)
            {
                if (setStatus_) setStatus_(std::move(text));
            }
            static void Place(
                wi::gui::Widget& widget, const float y,
                const float width, const float height = 28.0f)
            {
                widget.SetPos(XMFLOAT2(12.0f, y));
                widget.SetSize(XMFLOAT2(width, height));
                widget.SetVisible(true);
            }

            void AddOrImport()
            {
                if (pageValue_ == SpecialistPage::Gaussian)
                {
                    ChooseGaussian();
                    return;
                }
                auto* session = Session();
                if (session == nullptr || !session->Selection().HasSelection())
                    return;
                const auto selected = session->Selection().SelectedEntity();
                auto kind = bridge::SpecialistComponentKind::Hair;
                switch (pageValue_)
                {
                case SpecialistPage::Hair: kind = bridge::SpecialistComponentKind::Hair; break;
                case SpecialistPage::Force: kind = bridge::SpecialistComponentKind::ForceField; break;
                case SpecialistPage::Video: kind = bridge::SpecialistComponentKind::Video; break;
                case SpecialistPage::Spline: kind = bridge::SpecialistComponentKind::Spline; break;
                case SpecialistPage::Gaussian: break;
                }
                if (session->Commands().Execute(
                        std::make_unique<bridge::EnsureSpecialistComponentCommand>(
                            session->Scenes().GetScene(), selected, kind)))
                {
                    SetStatus("PHASE 7E // native component added");
                }
                RequestRefresh();
            }

            void CreateHair()
            {
                hair_.mesh.Create("SOURCE MESH");
                hair_.mesh.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    CommitHair([&](auto& state) { state.mesh = static_cast<wi::ecs::Entity>(args.userdata); });
                });
                panel_->AddWidget(&hair_.mesh);
                auto slider = [this](SceneInspectorSlider& control,
                    float min, float max, float initial, float steps,
                    const char* name, const char* label, auto setter)
                {
                    control.Create(min, max, initial, steps, name, label);
                    control.OnValueCommitted([this, setter](const float value)
                    {
                        CommitHair([&](auto& state) { setter(state, value); });
                    });
                    panel_->AddWidget(&control);
                };
                slider(hair_.count, 0, 100000, 1000, 100000,
                    "Phase7 Hair Count", "STRANDS", [](auto& s, float v) { s.strandCount = static_cast<std::uint32_t>(std::lround(v)); });
                slider(hair_.length, 0, 4, 1, 1000,
                    "Phase7 Hair Length", "LENGTH", [](auto& s, float v) { s.length = v; });
                slider(hair_.width, 0, 2, 1, 1000,
                    "Phase7 Hair Width", "WIDTH", [](auto& s, float v) { s.width = v; });
                slider(hair_.stiffness, 0, 10, 0.5f, 1000,
                    "Phase7 Hair Stiffness", "STIFFNESS", [](auto& s, float v) { s.stiffness = v; });
                slider(hair_.drag, 0, 1, 0.5f, 1000,
                    "Phase7 Hair Drag", "DRAG", [](auto& s, float v) { s.drag = v; });
                slider(hair_.gravity, 0, 1, 0.5f, 1000,
                    "Phase7 Hair Gravity", "GRAVITY", [](auto& s, float v) { s.gravityPower = v; });
                slider(hair_.randomness, 0, 1, 0.2f, 1000,
                    "Phase7 Hair Random", "RANDOMNESS", [](auto& s, float v) { s.randomness = v; });
                slider(hair_.segments, 1, 10, 1, 9,
                    "Phase7 Hair Segments", "SEGMENTS", [](auto& s, float v) { s.segments = static_cast<std::uint32_t>(std::lround(v)); });
                slider(hair_.billboards, 1, 10, 1, 9,
                    "Phase7 Hair Billboards", "BILLBOARDS", [](auto& s, float v) { s.billboards = static_cast<std::uint32_t>(std::lround(v)); });
                slider(hair_.seed, 1, 12345, 1, 12344,
                    "Phase7 Hair Seed", "RANDOM SEED", [](auto& s, float v) { s.randomSeed = static_cast<std::uint32_t>(std::lround(v)); });
                slider(hair_.viewDistance, 0, 1000, 100, 10000,
                    "Phase7 Hair View", "VIEW DISTANCE", [](auto& s, float v) { s.viewDistance = v; });
                slider(hair_.uniformity, 0.01f, 2, 0.1f, 1000,
                    "Phase7 Hair Uniformity", "UNIFORMITY", [](auto& s, float v) { s.uniformity = v; });
                hair_.cameraBend.Create("CAMERA BEND: ");
                hair_.cameraBend.OnClick([this](const wi::gui::EventArgs& args)
                {
                    CommitHair([&](auto& state) { state.cameraBend = args.bValue; });
                });
                panel_->AddWidget(&hair_.cameraBend);
            }

            void CreateForce()
            {
                force_.type.Create("TYPE");
                force_.type.AddItem("Point", static_cast<std::uint64_t>(wi::scene::ForceFieldComponent::Type::Point));
                force_.type.AddItem("Plane", static_cast<std::uint64_t>(wi::scene::ForceFieldComponent::Type::Plane));
                force_.type.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    CommitForce([&](auto& state) { state.type = static_cast<wi::scene::ForceFieldComponent::Type>(args.userdata); });
                });
                panel_->AddWidget(&force_.type);
                force_.gravity.Create(-10, 10, 0, 2000,
                    "Phase7 Force Gravity", "GRAVITY");
                force_.gravity.OnValueCommitted([this](float v)
                {
                    CommitForce([&](auto& s) { s.gravity = v; });
                });
                panel_->AddWidget(&force_.gravity);
                force_.range.Create(0, 100, 10, 1000,
                    "Phase7 Force Range", "RANGE");
                force_.range.OnValueCommitted([this](float v)
                {
                    CommitForce([&](auto& s) { s.range = v; });
                });
                panel_->AddWidget(&force_.range);
            }

            void CreateVideo()
            {
                video_.source.Create("Phase7 Video Source");
                video_.source.SetWrapEnabled(true);
                panel_->AddWidget(&video_.source);
                video_.browse.Create("BROWSE MP4");
                video_.browse.OnClick([this](const wi::gui::EventArgs&) { ChooseVideo(); });
                panel_->AddWidget(&video_.browse);
                video_.clear.Create("CLEAR");
                video_.clear.OnClick([this](const wi::gui::EventArgs&)
                {
                    CommitVideo([](auto& state) { state.filename.clear(); });
                });
                panel_->AddWidget(&video_.clear);
                video_.looped.Create("LOOPED: ");
                video_.looped.OnClick([this](const wi::gui::EventArgs& args)
                {
                    CommitVideo([&](auto& state) { state.looped = args.bValue; });
                });
                panel_->AddWidget(&video_.looped);
                video_.playPause.Create("PLAY / PAUSE");
                video_.playPause.OnClick([this](const wi::gui::EventArgs&)
                {
                    auto* session = Session();
                    if (session == nullptr) return;
                    auto& scene = session->Scenes().GetScene();
                    const auto selected = session->Selection().SelectedEntity();
                    auto* video = scene.videos.GetComponent(selected);
                    if (video == nullptr) return;
                    (void)bridge::PreviewVideo(scene, selected, !video->IsPlaying());
                    RequestRefresh();
                });
                panel_->AddWidget(&video_.playPause);
                video_.stop.Create("STOP");
                video_.stop.OnClick([this](const wi::gui::EventArgs&)
                {
                    auto* session = Session();
                    if (session) (void)bridge::StopVideoPreview(
                        session->Scenes().GetScene(), session->Selection().SelectedEntity());
                    RequestRefresh();
                });
                panel_->AddWidget(&video_.stop);
                video_.seek.Create(0, 1, 0, 10000,
                    "Phase7 Video Seek", "SEEK");
                video_.seek.OnValuePreview([this](const float value)
                {
                    auto* session = Session();
                    if (session) (void)bridge::SeekVideoPreview(
                        session->Scenes().GetScene(), session->Selection().SelectedEntity(), value);
                });
                panel_->AddWidget(&video_.seek);
            }

            void CreateSpline()
            {
                spline_.looped.Create("LOOPED: ");
                spline_.looped.OnClick([this](const wi::gui::EventArgs& args)
                {
                    CommitSpline([&](auto& s) { s.looped = args.bValue; });
                });
                panel_->AddWidget(&spline_.looped);
                spline_.filled.Create("FILLED: ");
                spline_.filled.OnClick([this](const wi::gui::EventArgs& args)
                {
                    CommitSpline([&](auto& s) { s.filled = args.bValue; });
                });
                panel_->AddWidget(&spline_.filled);
                spline_.aligned.Create("DRAW ALIGNED: ");
                spline_.aligned.OnClick([this](const wi::gui::EventArgs& args)
                {
                    CommitSpline([&](auto& s) { s.drawAligned = args.bValue; });
                });
                panel_->AddWidget(&spline_.aligned);

                auto slider = [this](SceneInspectorSlider& control,
                    float min, float max, float initial, float steps,
                    const char* name, const char* label, auto setter)
                {
                    control.Create(min, max, initial, steps, name, label);
                    control.OnValueCommitted([this, setter](float value)
                    {
                        CommitSpline([&](auto& s) { setter(s, value); });
                    });
                    panel_->AddWidget(&control);
                };
                slider(spline_.width, 0.001f, 4, 1, 1000,
                    "Phase7 Spline Width", "WIDTH", [](auto& s, float v) { s.width = v; });
                slider(spline_.rotation, 0, 360, 0, 360,
                    "Phase7 Spline Rotation", "ROTATION", [](auto& s, float v) { s.rotationDegrees = v; });
                slider(spline_.subdivision, 0, 100, 0, 100,
                    "Phase7 Spline Subdivision", "MESH SUBDIV", [](auto& s, float v) { s.meshSubdivision = static_cast<int>(std::lround(v)); });
                slider(spline_.verticalSubdivision, 0, 36, 0, 36,
                    "Phase7 Spline Vertical", "VERTICAL SUBDIV", [](auto& s, float v) { s.verticalSubdivision = static_cast<int>(std::lround(v)); });
                slider(spline_.terrainModifier, 0, 1, 0, 1000,
                    "Phase7 Spline Terrain", "TERRAIN MODIFIER", [](auto& s, float v) { s.terrainModifier = v; });
                slider(spline_.terrainFalloff, 0, 1, 0, 1000,
                    "Phase7 Spline Falloff", "TEXTURE FALLOFF", [](auto& s, float v) { s.terrainTextureFalloff = v; });
                slider(spline_.terrainPushdown, 0, 10, 0, 1000,
                    "Phase7 Spline Pushdown", "TERRAIN PUSH DOWN", [](auto& s, float v) { s.terrainPushdown = v; });
                spline_.fillNormals.Create("FILL NORMALS");
                spline_.fillNormals.AddItem("Smooth", static_cast<std::uint64_t>(wi::scene::MeshComponent::COMPUTE_NORMALS_SMOOTH));
                spline_.fillNormals.AddItem("Hard", static_cast<std::uint64_t>(wi::scene::MeshComponent::COMPUTE_NORMALS_HARD));
                spline_.fillNormals.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    CommitSpline([&](auto& s) { s.fillNormals = static_cast<wi::scene::MeshComponent::COMPUTE_NORMALS>(args.userdata); });
                });
                panel_->AddWidget(&spline_.fillNormals);
                spline_.addNode.Create("ADD NODE AT SPLINE ORIGIN");
                spline_.addNode.SetTooltip("Adds a native child Transform node. Move it with the normal Renegade gizmo after creation.");
                spline_.addNode.OnClick([this](const wi::gui::EventArgs&) { AddSplineNode(); });
                panel_->AddWidget(&spline_.addNode);
            }

            void CreateGaussian()
            {
                gaussian_.import.Create("IMPORT PLY GAUSSIAN SPLAT");
                gaussian_.import.OnClick([this](const wi::gui::EventArgs&) { ChooseGaussian(); });
                panel_->AddWidget(&gaussian_.import);
                gaussian_.info.Create("Phase7 Gaussian Info");
                gaussian_.info.SetWrapEnabled(true);
                panel_->AddWidget(&gaussian_.info);
            }

            template<typename Fn>
            void CommitHair(Fn fn)
            {
                auto* session = Session();
                if (!session) return;
                auto& scene = session->Scenes().GetScene();
                const auto selected = session->Selection().SelectedEntity();
                const auto* hair = scene.hairs.GetComponent(selected);
                if (!hair) return;
                auto state = bridge::CaptureHairParticle(*hair);
                fn(state);
                (void)session->Commands().Execute(
                    std::make_unique<bridge::SetHairParticleCommand>(scene, selected, state));
                RequestRefresh();
            }

            template<typename Fn>
            void CommitForce(Fn fn)
            {
                auto* session = Session();
                if (!session) return;
                auto& scene = session->Scenes().GetScene();
                const auto selected = session->Selection().SelectedEntity();
                const auto* force = scene.forces.GetComponent(selected);
                if (!force) return;
                auto state = bridge::CaptureForceField(*force);
                fn(state);
                (void)session->Commands().Execute(
                    std::make_unique<bridge::SetForceFieldCommand>(scene, selected, state));
                RequestRefresh();
            }

            template<typename Fn>
            void CommitVideo(Fn fn)
            {
                auto* session = Session();
                if (!session) return;
                auto& scene = session->Scenes().GetScene();
                const auto selected = session->Selection().SelectedEntity();
                const auto* video = scene.videos.GetComponent(selected);
                if (!video) return;
                auto state = bridge::CaptureVideoAuthoring(*video);
                fn(state);
                (void)session->Commands().Execute(
                    std::make_unique<bridge::SetVideoAuthoringCommand>(scene, selected, state));
                RequestRefresh();
            }

            template<typename Fn>
            void CommitSpline(Fn fn)
            {
                auto* session = Session();
                if (!session) return;
                auto& scene = session->Scenes().GetScene();
                const auto selected = session->Selection().SelectedEntity();
                const auto* spline = scene.splines.GetComponent(selected);
                if (!spline) return;
                auto state = bridge::CaptureSplineAuthoring(*spline);
                fn(state);
                (void)session->Commands().Execute(
                    std::make_unique<bridge::SetSplineAuthoringCommand>(scene, selected, state));
                RequestRefresh();
            }

            void ChooseVideo()
            {
                wi::helper::FileDialogParams params;
                params.type = wi::helper::FileDialogParams::OPEN;
                params.description = "MP4 video";
                params.extensions = {"MP4"};
                wi::helper::FileDialog(params, [this](std::string filename)
                {
                    wi::eventhandler::Subscribe_Once(
                        wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                        [this, filename = std::move(filename)](std::uint64_t)
                        {
                            CommitVideo([&](auto& state) { state.filename = filename; });
                        });
                });
            }

            void ChooseGaussian()
            {
                wi::helper::FileDialogParams params;
                params.type = wi::helper::FileDialogParams::OPEN;
                params.description = "PLY Gaussian Splat";
                params.extensions = {"PLY"};
                wi::helper::FileDialog(params, [this](std::string filename)
                {
                    wi::eventhandler::Subscribe_Once(
                        wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                        [this, filename = std::move(filename)](std::uint64_t)
                        {
                            auto* session = Session();
                            if (!session) return;
                            auto command = std::make_unique<bridge::ImportGaussianSplatCommand>(
                                session->Scenes().GetScene(), filename);
                            if (session->Commands().Execute(std::move(command)))
                                SetStatus("PHASE 7E // native PLY Gaussian Splat imported");
                            else
                                SetStatus("PHASE 7E // PLY did not contain a supported Gaussian Splat model");
                            RequestRefresh();
                        });
                });
            }

            void AddSplineNode()
            {
                auto* session = Session();
                if (!session) return;
                const auto selected = session->Selection().SelectedEntity();
                auto command = std::make_unique<bridge::AddSplineNodeCommand>(
                    session->Scenes().GetScene(), selected, XMFLOAT3(0, 0, 0));
                auto* raw = command.get();
                if (session->Commands().Execute(std::move(command)))
                {
                    session->Selection().SetSelection(raw->CreatedNode());
                    SetStatus("PHASE 7E // spline node added; use the gizmo to position it");
                }
                RequestRefresh();
            }

            void RefreshHair(wi::scene::Scene& scene, const wi::ecs::Entity selected)
            {
                const auto* hair = scene.hairs.GetComponent(selected);
                add_.SetText(hair ? "HAIR COMPONENT ACTIVE" : "ADD NATIVE HAIR");
                add_.SetEnabled(hair == nullptr);
                SetHairEnabled(hair != nullptr);
                hair_.mesh.ClearItems();
                hair_.mesh.AddItem("None", wi::ecs::INVALID_ENTITY);
                if (!hair)
                {
                    status_.SetText("Native Wicked HairParticleSystem is not attached to this entity.");
                    return;
                }
                const auto state = bridge::CaptureHairParticle(*hair);
                int meshSelection = 0;
                for (std::size_t index = 0; index < scene.meshes.GetCount(); ++index)
                {
                    const auto entity = scene.meshes.GetEntity(index);
                    hair_.mesh.AddItem(EntityName(scene, entity), entity);
                    if (entity == state.mesh) meshSelection = static_cast<int>(index + 1);
                }
                hair_.mesh.SetSelectedWithoutCallback(meshSelection);
                hair_.count.SetValue(static_cast<float>(state.strandCount));
                hair_.length.SetValue(state.length);
                hair_.width.SetValue(state.width);
                hair_.stiffness.SetValue(state.stiffness);
                hair_.drag.SetValue(state.drag);
                hair_.gravity.SetValue(state.gravityPower);
                hair_.randomness.SetValue(state.randomness);
                hair_.segments.SetValue(static_cast<float>(state.segments));
                hair_.billboards.SetValue(static_cast<float>(state.billboards));
                hair_.seed.SetValue(static_cast<float>(state.randomSeed));
                hair_.viewDistance.SetValue(state.viewDistance);
                hair_.uniformity.SetValue(state.uniformity);
                hair_.cameraBend.SetCheck(state.cameraBend);
                status_.SetText("Native Wicked HairParticleSystem // " +
                    std::to_string(state.strandCount) + " strands");
            }

            void RefreshForce(wi::scene::Scene& scene, const wi::ecs::Entity selected)
            {
                const auto* force = scene.forces.GetComponent(selected);
                add_.SetText(force ? "FORCE FIELD ACTIVE" : "ADD NATIVE FORCE FIELD");
                add_.SetEnabled(force == nullptr);
                SetForceEnabled(force != nullptr);
                if (!force)
                {
                    status_.SetText("Native Wicked ForceFieldComponent is not attached to this entity.");
                    return;
                }
                const auto state = bridge::CaptureForceField(*force);
                force_.type.SetSelectedByUserdataWithoutCallback(static_cast<std::uint64_t>(state.type));
                force_.gravity.SetValue(state.gravity);
                force_.range.SetValue(state.range);
                status_.SetText("Native force field // point/plane + gravity + range");
            }

            void RefreshVideo(wi::scene::Scene& scene, const wi::ecs::Entity selected)
            {
                const auto* video = scene.videos.GetComponent(selected);
                add_.SetText(video ? "VIDEO COMPONENT ACTIVE" : "ADD NATIVE VIDEO");
                add_.SetEnabled(video == nullptr);
                SetVideoEnabled(video != nullptr);
                if (!video)
                {
                    status_.SetText("Native Wicked VideoComponent is not attached to this entity.");
                    video_.source.SetText("No video source.");
                    return;
                }
                const auto state = bridge::CaptureVideoAuthoring(*video);
                video_.source.SetText(state.filename.empty() ? "No MP4 selected." : state.filename);
                video_.looped.SetCheck(state.looped);
                if (video->videoResource.IsValid())
                {
                    const float duration = std::max(0.01f, video->videoResource.GetVideo().duration_seconds);
                    video_.seek.SetRange(0.0f, duration);
                    video_.seek.SetValue(video->videoinstance.currentTimer);
                    status_.SetText(
                        std::string(video->IsPlaying() ? "PLAYING" : "READY") +
                        " // " + std::to_string(duration) + " sec // MP4 native decoder");
                }
                else
                {
                    video_.seek.SetRange(0.0f, 1.0f);
                    video_.seek.SetValue(0.0f);
                    status_.SetText("Video component ready // choose an MP4 source");
                }
            }

            void RefreshSpline(wi::scene::Scene& scene, const wi::ecs::Entity selected)
            {
                const auto* spline = scene.splines.GetComponent(selected);
                add_.SetText(spline ? "SPLINE COMPONENT ACTIVE" : "ADD NATIVE SPLINE");
                add_.SetEnabled(spline == nullptr);
                SetSplineEnabled(spline != nullptr);
                if (!spline)
                {
                    status_.SetText("Native Wicked SplineComponent is not attached to this entity.");
                    return;
                }
                const auto state = bridge::CaptureSplineAuthoring(*spline);
                spline_.looped.SetCheck(state.looped);
                spline_.filled.SetCheck(state.filled);
                spline_.filled.SetEnabled(state.looped);
                spline_.aligned.SetCheck(state.drawAligned);
                spline_.width.SetValue(state.width);
                spline_.rotation.SetValue(state.rotationDegrees);
                spline_.subdivision.SetValue(static_cast<float>(state.meshSubdivision));
                spline_.verticalSubdivision.SetValue(static_cast<float>(state.verticalSubdivision));
                spline_.terrainModifier.SetValue(state.terrainModifier);
                spline_.terrainFalloff.SetValue(state.terrainTextureFalloff);
                spline_.terrainPushdown.SetValue(state.terrainPushdown);
                spline_.fillNormals.SetSelectedByUserdataWithoutCallback(static_cast<std::uint64_t>(state.fillNormals));
                status_.SetText(
                    "Native spline // " + std::to_string(spline->spline_node_entities.size()) +
                    " nodes // terrain + mesh generation available");
            }

            void RefreshGaussian(wi::scene::Scene& scene, const wi::ecs::Entity selected)
            {
                add_.SetText("IMPORT PLY GAUSSIAN SPLAT");
                add_.SetEnabled(true);
                const auto* splat = scene.gaussian_splats.GetComponent(selected);
                if (!splat)
                {
                    gaussian_.info.SetText(
                        "Select an imported Gaussian Splat to inspect it, or import a native Wicked PLY splat model.");
                    status_.SetText("Gaussian Splat import / inspection");
                    return;
                }
                const auto info = bridge::InspectGaussianSplat(*splat);
                gaussian_.info.SetText(
                    "SPLATS: " + std::to_string(info.splatCount) +
                    "\nSH DEGREE: L" + std::to_string(info.sphericalHarmonicsDegree) +
                    "\nCPU BYTES: " + std::to_string(info.cpuMemoryBytes) +
                    "\nGPU BYTES: " + std::to_string(info.gpuMemoryBytes));
                status_.SetText("Native Wicked GaussianSplatModel // inspection is upstream parity");
            }

            void LayoutHair(float& y, const float width)
            {
                for (wi::gui::Widget* widget : {
                    static_cast<wi::gui::Widget*>(&hair_.mesh),
                    static_cast<wi::gui::Widget*>(&hair_.count),
                    static_cast<wi::gui::Widget*>(&hair_.length),
                    static_cast<wi::gui::Widget*>(&hair_.width),
                    static_cast<wi::gui::Widget*>(&hair_.stiffness),
                    static_cast<wi::gui::Widget*>(&hair_.drag),
                    static_cast<wi::gui::Widget*>(&hair_.gravity),
                    static_cast<wi::gui::Widget*>(&hair_.randomness),
                    static_cast<wi::gui::Widget*>(&hair_.segments),
                    static_cast<wi::gui::Widget*>(&hair_.billboards),
                    static_cast<wi::gui::Widget*>(&hair_.seed),
                    static_cast<wi::gui::Widget*>(&hair_.viewDistance),
                    static_cast<wi::gui::Widget*>(&hair_.uniformity),
                    static_cast<wi::gui::Widget*>(&hair_.cameraBend)})
                {
                    Place(*widget, y, width); y += 34.0f;
                }
            }
            void LayoutForce(float& y, const float width)
            {
                Place(force_.type, y, width); y += 34;
                Place(force_.gravity, y, width); y += 34;
                Place(force_.range, y, width);
            }
            void LayoutVideo(float& y, const float width)
            {
                Place(video_.source, y, width, 42); y += 46;
                const float half = (width - 6) * 0.5f;
                video_.browse.SetPos(XMFLOAT2(12, y)); video_.browse.SetSize(XMFLOAT2(half, 28)); video_.browse.SetVisible(true);
                video_.clear.SetPos(XMFLOAT2(18 + half, y)); video_.clear.SetSize(XMFLOAT2(half, 28)); video_.clear.SetVisible(true); y += 34;
                Place(video_.looped, y, width); y += 34;
                video_.playPause.SetPos(XMFLOAT2(12, y)); video_.playPause.SetSize(XMFLOAT2(half, 28)); video_.playPause.SetVisible(true);
                video_.stop.SetPos(XMFLOAT2(18 + half, y)); video_.stop.SetSize(XMFLOAT2(half, 28)); video_.stop.SetVisible(true); y += 34;
                Place(video_.seek, y, width);
            }
            void LayoutSpline(float& y, const float width)
            {
                for (wi::gui::Widget* widget : {
                    static_cast<wi::gui::Widget*>(&spline_.looped),
                    static_cast<wi::gui::Widget*>(&spline_.filled),
                    static_cast<wi::gui::Widget*>(&spline_.aligned),
                    static_cast<wi::gui::Widget*>(&spline_.width),
                    static_cast<wi::gui::Widget*>(&spline_.rotation),
                    static_cast<wi::gui::Widget*>(&spline_.subdivision),
                    static_cast<wi::gui::Widget*>(&spline_.verticalSubdivision),
                    static_cast<wi::gui::Widget*>(&spline_.terrainModifier),
                    static_cast<wi::gui::Widget*>(&spline_.terrainFalloff),
                    static_cast<wi::gui::Widget*>(&spline_.terrainPushdown),
                    static_cast<wi::gui::Widget*>(&spline_.fillNormals),
                    static_cast<wi::gui::Widget*>(&spline_.addNode)})
                {
                    Place(*widget, y, width); y += 34;
                }
            }
            void LayoutGaussian(float& y, const float width)
            {
                Place(gaussian_.import, y, width); y += 34;
                Place(gaussian_.info, y, width, 110);
            }

            void HideSpecific()
            {
                SetHairVisible(false);
                SetForceVisible(false);
                SetVideoVisible(false);
                SetSplineVisible(false);
                SetGaussianVisible(false);
            }
            void SetAllVisible(const bool visible, const bool keepHeader = false)
            {
                if (!keepHeader) header_.SetVisible(visible);
                status_.SetVisible(visible);
                page_.SetVisible(visible);
                add_.SetVisible(visible);
                if (!visible) HideSpecific();
            }
            void SetHairVisible(bool v)
            {
                for (wi::gui::Widget* w : {static_cast<wi::gui::Widget*>(&hair_.mesh), &hair_.count, &hair_.length, &hair_.width, &hair_.stiffness, &hair_.drag, &hair_.gravity, &hair_.randomness, &hair_.segments, &hair_.billboards, &hair_.seed, &hair_.viewDistance, &hair_.uniformity, &hair_.cameraBend}) w->SetVisible(v);
            }
            void SetForceVisible(bool v)
            {
                force_.type.SetVisible(v); force_.gravity.SetVisible(v); force_.range.SetVisible(v);
            }
            void SetVideoVisible(bool v)
            {
                for (wi::gui::Widget* w : {static_cast<wi::gui::Widget*>(&video_.source), &video_.browse, &video_.clear, &video_.looped, &video_.playPause, &video_.stop, &video_.seek}) w->SetVisible(v);
            }
            void SetSplineVisible(bool v)
            {
                for (wi::gui::Widget* w : {static_cast<wi::gui::Widget*>(&spline_.looped), &spline_.filled, &spline_.aligned, &spline_.width, &spline_.rotation, &spline_.subdivision, &spline_.verticalSubdivision, &spline_.terrainModifier, &spline_.terrainFalloff, &spline_.terrainPushdown, &spline_.fillNormals, &spline_.addNode}) w->SetVisible(v);
            }
            void SetGaussianVisible(bool v)
            {
                gaussian_.import.SetVisible(v); gaussian_.info.SetVisible(v);
            }
            void SetHairEnabled(bool v)
            {
                for (wi::gui::Widget* w : {static_cast<wi::gui::Widget*>(&hair_.mesh), &hair_.count, &hair_.length, &hair_.width, &hair_.stiffness, &hair_.drag, &hair_.gravity, &hair_.randomness, &hair_.segments, &hair_.billboards, &hair_.seed, &hair_.viewDistance, &hair_.uniformity, &hair_.cameraBend}) w->SetEnabled(v);
            }
            void SetForceEnabled(bool v)
            {
                force_.type.SetEnabled(v); force_.gravity.SetEnabled(v); force_.range.SetEnabled(v);
            }
            void SetVideoEnabled(bool v)
            {
                for (wi::gui::Widget* w : {static_cast<wi::gui::Widget*>(&video_.source), &video_.browse, &video_.clear, &video_.looped, &video_.playPause, &video_.stop, &video_.seek}) w->SetEnabled(v);
                video_.browse.SetEnabled(v); // component must exist before source assignment
            }
            void SetSplineEnabled(bool v)
            {
                for (wi::gui::Widget* w : {static_cast<wi::gui::Widget*>(&spline_.looped), &spline_.filled, &spline_.aligned, &spline_.width, &spline_.rotation, &spline_.subdivision, &spline_.verticalSubdivision, &spline_.terrainModifier, &spline_.terrainFalloff, &spline_.terrainPushdown, &spline_.fillNormals, &spline_.addNode}) w->SetEnabled(v);
            }

            StudioRenderPath* owner_ = nullptr;
            wi::gui::Window* panel_ = nullptr;
            InspectorSectionRegistry* registry_ = nullptr;
            std::function<void()> requestRefresh_;
            std::function<void(std::string)> setStatus_;
            SceneInspectorButton header_;
            wi::gui::Label status_;
            SceneInspectorComboBox page_;
            SceneInspectorButton add_;
            SpecialistPage pageValue_ = SpecialistPage::Hair;
            HairControls hair_;
            ForceControls force_;
            VideoControls video_;
            SplineControls spline_;
            GaussianControls gaussian_;
        };

        bool SpecialistProvider::IsVisible(const InspectorSectionContext& context) const
        {
            return owner_ != nullptr && owner_->IsVisible(context);
        }
        float SpecialistProvider::MeasureContentHeight(
            const InspectorSectionContext&, const float) const
        {
            return owner_ == nullptr ? 0.0f : owner_->Measure();
        }
        void SpecialistProvider::Refresh(const InspectorSectionContext&)
        {
            if (owner_ != nullptr) owner_->Refresh();
        }
        void SpecialistProvider::ApplyLayout(
            const InspectorSectionContext&,
            const InspectorSectionLayout& layout)
        {
            if (owner_ != nullptr) owner_->Layout(layout);
        }

        std::unique_ptr<SpecialistInspector> activeSpecialist;
        StudioRenderPath* activeSpecialistOwner = nullptr;
    }

    void RegisterPhase7SpecialistInspector(
        StudioRenderPath& owner,
        wi::gui::Window& inspectorPanel,
        InspectorSectionRegistry& registry,
        std::function<void()> requestRefresh,
        std::function<void(std::string)> setStatus)
    {
        activeSpecialist.reset();
        activeSpecialistOwner = &owner;
        activeSpecialist = std::make_unique<SpecialistInspector>(
            owner, inspectorPanel, registry,
            std::move(requestRefresh), std::move(setStatus));
        activeSpecialist->Register();
    }

    void PreparePhase7SpecialistInspector(StudioRenderPath& owner)
    {
        if (activeSpecialistOwner == &owner && activeSpecialist)
            activeSpecialist->Prepare();
    }
}
