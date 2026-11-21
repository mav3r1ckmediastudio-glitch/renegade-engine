#include "StudioApplication.h"

#include "InspectorSectionFramework.h"
#include "AICharacterInspector.h"
#include "Phase7Gate7DNativeTimelineInspector.h"
#include "Phase7Gate7ESpecialistInspector.h"
#include "Phase7Gate7FMeshBlendInspector.h"
#include "renegade/bridge/ObjectImpactSurfaceService.h"
#include "renegade/bridge/CollisionService.h"
#include "renegade/bridge/PhysicsGameplayService.h"
#include "renegade/bridge/DestructibleBlastPreset.h"
#include "renegade/bridge/FlightVehicleService.h"
#include "renegade/bridge/CharacterPhysicsService.h"
#include "renegade/bridge/SoftBodyPhysicsService.h"
#include "renegade/bridge/VehiclePhysicsService.h"

#include <array>
#include "S4BScriptAttachmentInspector.h"
#include "S4DGlobalScriptInspector.h"
#include "Phase7Gate7AAnimationInspector.h"

#include <algorithm>
#include <cmath>
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
            session_->Selection().Entities().size()>1 ||
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
                const bool opening =
                    !inspectorSectionRegistry_.IsExpanded(sectionId);
                ResetS1BInspectorDisclosure();
                if (opening)
                    (void)inspectorSectionRegistry_.SetExpanded(sectionId, true);
                QueueInspectorRefresh();
            });
            inspectorPanel_.AddWidget(&button);
        };

        createHeader(inspectorMetadataHeader_,"Scene metadata header","Layers and metadata for the selected object.","object-metadata");
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
        auto surfaceDescriptor = MakeSection("surface", "SURFACE", 5, true);
        surfaceDescriptor.headerHeight = 1.0f;
        auto surface = std::make_shared<CallbackInspectorSectionProvider>(
            std::move(surfaceDescriptor),
            [this](const InspectorSectionContext&)
            {
                return session_ != nullptr && !environmentWorkspaceActive_ &&
                    !terrainWorkspaceActive_ && session_->Selection().HasSelection() &&
                    session_->Selection().Entities().size()==1 &&
                    bridge::CanAuthorObjectImpactSurface(
                        session_->Scenes().GetScene(),
                        session_->Selection().SelectedEntity());
            },
            [](const InspectorSectionContext&, float) { return 58.0f; },
            [this](const InspectorSectionContext&)
            {
                materialImpactSurface_.SetVisible(false);
            },
            [this](const InspectorSectionContext&, const InspectorSectionLayout& layout)
            {
                objectSurfaceLabel_.SetVisible(true);
                objectSurfaceLabel_.SetPos(XMFLOAT2(12.0f, layout.contentTop));
                objectSurfaceLabel_.SetSize(XMFLOAT2(layout.width, 20.0f));
                materialImpactSurface_.SetVisible(true);
                materialImpactSurface_.SetPos(XMFLOAT2(12.0f, layout.contentTop + 24.0f));
                materialImpactSurface_.SetSize(XMFLOAT2(layout.width, 28.0f));
                const auto& scene = session_->Scenes().GetScene();
                const auto entity = session_->Selection().SelectedEntity();
                materialImpactSurface_.SetSelectedByUserdataWithoutCallback(
                    static_cast<std::uint64_t>(
                        bridge::ResolveObjectImpactSurface(scene, entity, 0)));
            });
        if (!inspectorSectionRegistry_.Register(std::move(surface), error))
            studioChrome_.SetStatusText("SURFACE INSPECTOR // " + error);
        error.clear();
        auto metadata=std::make_shared<CallbackInspectorSectionProvider>(
            MakeSection("object-metadata","LAYERS / METADATA",900,false),
            [this](const InspectorSectionContext& c){return c.hasSelection && !c.transformOnly;},
            [](const InspectorSectionContext&,float w){return 112.f+float(w>=332?4:8)*26.f;},
            CallbackInspectorSectionProvider::RefreshFn{},
            [this](const InspectorSectionContext&,const InspectorSectionLayout& layout){
                inspectorMetadataHeader_.SetVisible(true);
                inspectorMetadataHeader_.SetPos({12,layout.top});inspectorMetadataHeader_.SetSize({layout.width,layout.headerHeight});
                inspectorMetadataHeader_.SetText(layout.expanded?"▼ LAYERS / METADATA":"▶ LAYERS / METADATA");
                sceneLayerLabel_.SetVisible(layout.expanded);sceneLayerAllButton_.SetVisible(layout.expanded);sceneLayerNoneButton_.SetVisible(layout.expanded);
                sceneMetadataLabel_.SetVisible(layout.expanded);sceneMetadataPreset_.SetVisible(layout.expanded);scenePickupButton_.SetVisible(layout.expanded);
                for(auto& bit:sceneLayerBits_)bit.SetVisible(layout.expanded);
                if(!layout.expanded)return;
                const float innerWidth=layout.width;float y=layout.contentTop;
                const auto section=[&](wi::gui::Widget& w){w.SetPos({12,y});w.SetSize({innerWidth,20});y+=20;};
            section(sceneLayerLabel_);
            const float half = (innerWidth - 8.0f) * 0.5f;
            sceneLayerAllButton_.SetPos(XMFLOAT2(12.0f, y));
            sceneLayerNoneButton_.SetPos(XMFLOAT2(20.0f + half, y));
            sceneLayerAllButton_.SetSize(XMFLOAT2(half, 28.0f));
            sceneLayerNoneButton_.SetSize(XMFLOAT2(half, 28.0f));
            y += 34.0f;
            const float bitGap = 4.0f;
            const std::size_t bitColumns = innerWidth >= 332.f ? 8u : 4u;
            const float bitWidth = (innerWidth - bitGap * float(bitColumns-1)) / float(bitColumns);
            for (std::size_t bit = 0; bit < sceneLayerBits_.size(); ++bit)
            {
                const float x = 12.0f + static_cast<float>(bit % bitColumns) * (bitWidth + bitGap);
                const float bitY = y + static_cast<float>(bit / bitColumns) * 26.0f;
                sceneLayerBits_[bit].SetPos(XMFLOAT2(x, bitY));
                sceneLayerBits_[bit].SetSize(XMFLOAT2(bitWidth, 22.0f));
            }
            y += float((sceneLayerBits_.size()+bitColumns-1)/bitColumns)*26.f+4.f;
            section(sceneMetadataLabel_);
            sceneMetadataPreset_.SetPos({12.0f,y});
            sceneMetadataPreset_.SetSize({half,28.0f});
            scenePickupButton_.SetPos({20.0f+half,y});
            scenePickupButton_.SetSize({half,28.0f});
            y += 34.0f;
            });
        if(!inspectorSectionRegistry_.Register(std::move(metadata),error))wi::backlog::post(error);

        auto transform = std::make_shared<CallbackInspectorSectionProvider>(
            MakeSection("transform", "TRANSFORM", 10, false),
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
            CallbackInspectorSectionProvider::RefreshFn{},
            [this](const InspectorSectionContext&, const InspectorSectionLayout& layout)
            {
                transformSectionHeader_.SetVisible(true);
                transformSectionHeader_.SetPos(XMFLOAT2(12.0f, layout.top));
                transformSectionHeader_.SetSize(XMFLOAT2(layout.width, layout.headerHeight));
                transformSectionHeader_.SetText(
                    std::string(layout.expanded ? "-  " : "+  ") + "TRANSFORM");

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
                const bool multiple=session_ && session_->Selection().Entities().size()>1;
                positionLabel_.SetText(multiple?"PIVOT POSITION / WORLD":"POSITION");
                rotationLabel_.SetText(multiple?"ROTATE SELECTION / DEGREES":"ROTATION / DEGREES");
                scaleLabel_.SetText(multiple?"SCALE SELECTION / FACTOR":"SCALE");
                for(auto* field:{&rotationX_,&rotationY_,&rotationZ_})
                    field->SetTooltip(multiple?"Rotate all selected objects around their shared pivot. 0 adds no rotation.":"Local rotation in degrees.");
                for(auto* field:{&scaleX_,&scaleY_,&scaleZ_})
                    field->SetTooltip(multiple?"Scale the selection around its shared pivot. 1 keeps its current size; 2 doubles it.":"Local scale.");
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
            MakeSection("rendering", "RENDERING", 20, false),
            [this](const InspectorSectionContext&)
            {
                return IsS1BRenderingSectionVisible();
            },
            [](const InspectorSectionContext&, float)
            {
                return 112.0f;
            },
            CallbackInspectorSectionProvider::RefreshFn{},
            [this](const InspectorSectionContext&, const InspectorSectionLayout& layout)
            {
                renderingSectionHeader_.SetVisible(true);
                renderingSectionHeader_.SetPos(XMFLOAT2(12.0f, layout.top));
                renderingSectionHeader_.SetSize(XMFLOAT2(layout.width, layout.headerHeight));
                renderingSectionHeader_.SetText(
                    std::string(layout.expanded ? "-  " : "+  ") + "RENDERING");
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
            MakeSection("materials", "MATERIALS", 30, false),
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
            CallbackInspectorSectionProvider::RefreshFn{},
            [this](const InspectorSectionContext&, const InspectorSectionLayout& layout)
            {
                materialsSectionHeader_.SetVisible(true);
                materialsSectionHeader_.SetPos(XMFLOAT2(12.0f, layout.top));
                materialsSectionHeader_.SetSize(XMFLOAT2(layout.width, layout.headerHeight));
                materialsSectionHeader_.SetText(
                    std::string(layout.expanded ? "-  " : "+  ") + "MATERIALS");
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

        struct PhysicsFields
        {
            SceneInspectorButton header;
            SceneInspectorButton lab;
            SceneInspectorComboBox shape;
            SceneInspectorSlider mass;
            SceneInspectorSlider friction;
            SceneInspectorSlider bounce;
            SceneInspectorSlider wheelRadius;
            SceneInspectorCheckBox kinematic;

            SceneInspectorCheckBox sensor;
            SceneInspectorSlider collisionLayer;
            SceneInspectorSlider breakSpeed;
            SceneInspectorCheckBox explodeOnBreak;

            std::array<wi::gui::Widget*, 11> Controls()
            {
                return {&shape, &mass, &friction, &bounce, &kinematic, &sensor, &collisionLayer, &breakSpeed,
                    &explodeOnBreak, &wheelRadius, &lab};
            }
        };

        auto physics = std::make_shared<PhysicsFields>();
        auto target = [this]() -> wi::ecs::Entity
        {
            if (!session_ || session_->Selection().Entities().size() != 1)
                return wi::ecs::INVALID_ENTITY;
            auto& scene = session_->Scenes().GetScene();
            const auto selected = session_->Selection().SelectedEntity();
            if (scene.rigidbodies.Contains(selected)) return selected;
            const auto wrapper = bridge::ResolveCollisionAuthoringTarget(scene, selected);
            if (scene.rigidbodies.Contains(wrapper)) return wrapper;
            return bridge::ResolveAuthoredDestructibleRoot(scene, selected);
        };
        auto editBody = [this, target](auto change)
        {
            const auto selected = target();
            if (selected == wi::ecs::INVALID_ENTITY) return;
            auto& scene = session_->Scenes().GetScene();
            const auto before = bridge::CaptureCollision(*scene.rigidbodies.GetComponent(selected));
            auto after = before;
            change(after);
            after = bridge::SanitizeCollisionState(after);
            if (bridge::CheckCollisionTarget(scene, selected, after.shape) !=
                bridge::CollisionTargetStatus::Supported)
            {
                studioChrome_.SetStatusText("PHYSICS // COLLISION SHAPE UNAVAILABLE FOR THIS BODY");
                QueueInspectorRefresh();
                return;
            }
            if (session_->Commands().Execute(std::make_unique<bridge::SetCollisionCommand>(
                scene, selected, before, after))) QueueInspectorRefresh();
        };
        auto hasVehicle = [this, target]()
        {
            const auto selected = target();
            if (selected == wi::ecs::INVALID_ENTITY) return false;
            const auto* body = session_->Scenes().GetScene().rigidbodies.GetComponent(selected);
            return body->vehicle.type != wi::scene::RigidBodyPhysicsComponent::Vehicle::Type::None;
        };
        using PhysicsPage = RenegadePhysicsLabWorkspace::Page;
        auto selectedPhysicsPage = [this, target, hasVehicle]()
        {
            if (!session_ || session_->Selection().Entities().size() != 1)
                return PhysicsPage::Count;
            const auto& scene = session_->Scenes().GetScene();
            const auto selected = session_->Selection().SelectedEntity();
            const auto body = target();
            if (hasVehicle() || bridge::IsFlightVehicle(scene, body) || bridge::IsTrackedVehicle(scene, body))
                return PhysicsPage::Vehicle;
            if (scene.constraints.Contains(selected))
                return PhysicsPage::Constraint;
            if (scene.humanoids.Contains(selected))
                return PhysicsPage::Ragdoll;
            if (scene.softbodies.Contains(bridge::ResolveSoftBodyMeshEntity(scene, selected)))
                return PhysicsPage::SoftBody;
            if (scene.colliders.Contains(selected))
                return PhysicsPage::WickedCollider;
            if (const auto* rigid = scene.rigidbodies.GetComponent(body))
                return bridge::CaptureCharacterPhysics(*rigid).enabled ? PhysicsPage::Character : PhysicsPage::RigidBody;
            return PhysicsPage::Count;
        };
        createHeader(physics->header, "Selected asset physics", "Edit the selected asset's physics", "physics");
        physics->header.SetVisible(false);
        physics->lab.Create("Physics Lab");
        physics->lab.SetText("OPEN IN PHYSICS LAB");
        physics->lab.OnClick([this, target, selectedPhysicsPage](const wi::gui::EventArgs&) {
            const auto page = selectedPhysicsPage();
            if (page == PhysicsPage::Count)
                return;
            if (page == PhysicsPage::Vehicle || page == PhysicsPage::Character || page == PhysicsPage::RigidBody)
            {
                const auto body = target();
                if (body != wi::ecs::INVALID_ENTITY)
                    session_->Selection().Select(body);
            }
            studioChrome_.OpenPhysicsPage(page);
        });
        using Shape = wi::scene::RigidBodyPhysicsComponent::CollisionShape;
        physics->shape.Create("Collision shape");
        physics->shape.AddItem("BOX", static_cast<std::uint64_t>(Shape::BOX));
        physics->shape.AddItem("SPHERE", static_cast<std::uint64_t>(Shape::SPHERE));
        physics->shape.AddItem("CAPSULE", static_cast<std::uint64_t>(Shape::CAPSULE));
        physics->shape.AddItem("CYLINDER", static_cast<std::uint64_t>(Shape::CYLINDER));
        physics->shape.AddItem("CONVEX HULL", static_cast<std::uint64_t>(Shape::CONVEX_HULL));
        physics->shape.AddItem("TRIANGLE MESH", static_cast<std::uint64_t>(Shape::TRIANGLE_MESH));
        physics->shape.AddItem("HEIGHT FIELD", static_cast<std::uint64_t>(Shape::HEIGHTFIELD));
        physics->shape.OnSelect([editBody](const wi::gui::EventArgs& args) {
            editBody([&](auto& state) { state.shape = static_cast<Shape>(args.userdata); });
        });
        physics->mass.Create(0.f, 10000.f, 1.f, 100000, "Physics mass", "MASS (KG)");
        physics->friction.Create(0.f, 1.f, .2f, 10000, "Physics friction", "FRICTION");
        physics->bounce.Create(0.f, 1.f, 0.f, 1000, "Physics restitution", "BOUNCE");
        physics->wheelRadius.Create(.001f, 10.f, .3f, 10000, "Physics wheel radius", "WHEEL RADIUS (M)");
        physics->kinematic.Create("KINEMATIC");
        physics->sensor.Create("SENSOR");
        physics->breakSpeed.Create(0, 100, 0, 1000, "Destructible impact speed", "BREAK SPEED (M/S)");
        physics->breakSpeed.OnValueCommitted([this, target](float value)
        {
            const auto selected = target();
            if (selected == wi::ecs::INVALID_ENTITY) return;
            auto& scene = session_->Scenes().GetScene();
            if (session_->Commands().Execute(std::make_unique<bridge::SetDestructibleImpactSpeedCommand>(
                scene, selected, value))) QueueInspectorRefresh();
        });
        physics->explodeOnBreak.Create("EXPLODE ON BREAK");
        physics->explodeOnBreak.OnClick([this, target](const wi::gui::EventArgs& args)
        {
            const auto selected = target();
            if (selected == wi::ecs::INVALID_ENTITY) return;
            auto& scene = session_->Scenes().GetScene();
            if (!session_->Commands().Execute(std::make_unique<bridge::SetDestructibleBlastPresetCommand>(
                scene, selected, args.bValue)))
                studioChrome_.SetStatusText("PHYSICS // EXPLOSION REQUIRES A CONFIGURED BREAKABLE");
            QueueInspectorRefresh();
        });
        physics->collisionLayer.Create(0, bridge::BodyCollisionLayerCount - 1, 0,
            bridge::BodyCollisionLayerCount - 1, "Body simulation layer", "COLLISION LAYER");
        physics->collisionLayer.OnValueCommitted([this, target](float value)
        {
            const auto selected = target();
            if (selected == wi::ecs::INVALID_ENTITY) return;
            auto& scene = session_->Scenes().GetScene();
            auto filter = bridge::GetBodyCollisionFilter(scene, selected);
            filter.layer = static_cast<int>(std::lround(value));
            if (session_->Commands().Execute(std::make_unique<bridge::SetBodyCollisionFilterCommand>(
                scene, selected, filter))) QueueInspectorRefresh();
        });
        physics->sensor.OnClick([this, target](const wi::gui::EventArgs& args)
        {
            const auto selected = target();
            if (selected == wi::ecs::INVALID_ENTITY) return;
            auto& scene = session_->Scenes().GetScene();
            if (session_->Commands().Execute(std::make_unique<bridge::SetBodySensorCommand>(
                scene, selected, args.bValue))) QueueInspectorRefresh();
        });
        physics->mass.OnValueCommitted([editBody](float value) { editBody([value](auto& state) { state.mass = value; }); });
        physics->friction.OnValueCommitted([editBody](float value) { editBody([value](auto& state) { state.friction = value; }); });
        physics->bounce.OnValueCommitted([editBody](float value) { editBody([value](auto& state) { state.restitution = value; }); });
        physics->kinematic.OnClick([editBody](const wi::gui::EventArgs& args) { editBody([&](auto& state) { state.kinematic = args.bValue; }); });
        physics->wheelRadius.OnValueCommitted([this, target, hasVehicle](float value)
        {
            if (!hasVehicle()) return;
            auto& scene = session_->Scenes().GetScene();
            const auto selected = target();
            const auto before = bridge::CaptureVehiclePhysics(*scene.rigidbodies.GetComponent(selected));
            auto after = before;
            after.wheelRadius = value;
            if (session_->Commands().Execute(std::make_unique<bridge::SetVehiclePhysicsCommand>(
                scene, selected, before, after))) QueueInspectorRefresh();
        });
        for (auto* widget : physics->Controls())
        {
            inspectorPanel_.AddWidget(widget);
            widget->SetVisible(false);
        }
        auto physicsProvider = std::make_shared<CallbackInspectorSectionProvider>(
            MakeSection("physics", "PHYSICS", 15, false),
            [this, physics, selectedPhysicsPage](const InspectorSectionContext& context)
            {
                const bool visible = context.hasSelection && !environmentWorkspaceActive_ &&
                    !terrainWorkspaceActive_ && selectedPhysicsPage() != PhysicsPage::Count;
                physics->header.SetVisible(visible);
                if (!visible) for (auto* widget : physics->Controls()) widget->SetVisible(false);
                return visible;
            },
            [this, target, hasVehicle](const InspectorSectionContext&, float) {
                const auto id = target();
                if (id == wi::ecs::INVALID_ENTITY) return 34.f;
                const auto& body = *session_->Scenes().GetScene().rigidbodies.GetComponent(id);
                if (bridge::SupportsDestructible(body)) return 340.f;
                if (bridge::SupportsBodyCollisionFilter(body)) return 272.f;
                return hasVehicle() ? 238.f : 204.f;
            },
            [this, physics, target](const InspectorSectionContext&)
            {
                const auto id = target();
                if (id == wi::ecs::INVALID_ENTITY) return;
                const auto& scene = session_->Scenes().GetScene();
                const auto* body = scene.rigidbodies.GetComponent(id);
                const auto state = bridge::CaptureCollision(*body);
                physics->shape.SetSelectedByUserdataWithoutCallback(static_cast<std::uint64_t>(state.shape));
                physics->mass.SetValue(state.mass);
                physics->friction.SetValue(state.friction);
                physics->bounce.SetValue(state.restitution);
                physics->kinematic.SetCheck(state.kinematic);
                const bool sensor = bridge::IsBodySensor(scene, id);
                physics->sensor.SetCheck(sensor);
                physics->breakSpeed.SetValue(bridge::GetDestructibleImpactSpeed(scene, id));
                physics->breakSpeed.SetEnabled(!sensor);
                const bool explosive = bridge::GetDestructibleBlastSpeed(scene, id) > 0.f;
                const bool canExplode = bridge::SupportsDestructible(*body) &&
                    bridge::CanUseDestructibleBlastPreset(scene, id);
                physics->explodeOnBreak.SetCheck(explosive);
                physics->explodeOnBreak.SetEnabled(!sensor && (canExplode || explosive));
                physics->explodeOnBreak.SetTooltip(canExplode
                    ? "Use the oil drum blast when this prop breaks. Fine-tune Blast Speed in Physics Lab."
                    : "Requires a solid static parent, 2-16 usable fragments, and Break Speed or Hit Points.");
                physics->collisionLayer.SetValue(static_cast<float>(bridge::GetBodyCollisionFilter(scene, id).layer));
                physics->wheelRadius.SetValue(bridge::CaptureVehiclePhysics(*body).wheelRadius);
            },
            [this, physics, target, hasVehicle](const InspectorSectionContext&, const InspectorSectionLayout& layout)
            {
                physics->header.SetPos({12.f, layout.top});
                physics->header.SetSize({layout.width, layout.headerHeight});
                physics->header.SetText(layout.expanded ? "-  PHYSICS" : "+  PHYSICS");
                float y = layout.contentTop;
                const auto id = target();
                const auto* body = id != wi::ecs::INVALID_ENTITY ? session_->Scenes().GetScene().rigidbodies.GetComponent(id) : nullptr;
                for (auto* widget : physics->Controls())
                {
                    const bool bodyRow = body &&
                        (widget != &physics->wheelRadius || hasVehicle()) &&
                        (widget != &physics->sensor || bridge::SupportsBodySensor(*body)) &&
                        (widget != &physics->collisionLayer || bridge::SupportsBodyCollisionFilter(*body)) &&
                        ((widget != &physics->breakSpeed && widget != &physics->explodeOnBreak) ||
                            bridge::SupportsDestructible(*body));
                    const bool visible = layout.expanded && (widget == &physics->lab || bodyRow);
                    widget->SetVisible(visible);
                    if (!visible) continue;
                    widget->SetPos({12.f, y});
                    widget->SetSize({layout.width, 28.f});
                    y += 34.f;
                }
            });
        physicsInspectorSection_ = physicsProvider;
        if (!inspectorSectionRegistry_.Register(std::move(physicsProvider), error))
            studioChrome_.SetStatusText("PHYSICS INSPECTOR // " + error);

        struct CockpitCameraFields
        {
            SceneInspectorButton header, preview, capture, reset, advanced;
            std::array<SceneInspectorSlider, 8> controls;
            bridge::VehicleCockpitCamera displayed;
            bool showLens = false;

            void Hide()
            {
                for (auto* button : {&preview, &capture, &reset, &advanced})
                    button->SetVisible(false);
                for (auto& slider : controls) slider.SetVisible(false);
            }
        };
        auto cockpit = std::make_shared<CockpitCameraFields>();
        auto* const cockpitFields = cockpit.get();
        const auto cockpitTarget = [this]() {
            if (!session_ || session_->Selection().Entities().size() != 1)
                return wi::ecs::INVALID_ENTITY;
            return bridge::ResolveCockpitCameraChassis(
                session_->Scenes().GetScene(), session_->Selection().SelectedEntity());
        };
        const auto commitCockpit = [this, cockpitTarget](std::optional<bridge::VehicleCockpitCamera> settings) {
            cockpitPreviewDraft_.reset();
            const auto chassis = cockpitTarget();
            if (chassis != wi::ecs::INVALID_ENTITY)
                session_->Commands().Execute(std::make_unique<bridge::SetVehicleCockpitCameraCommand>(
                    session_->Scenes().GetScene(), chassis, settings));
            QueueInspectorRefresh();
        };
        createHeader(cockpit->header, "Cockpit camera", "Set the first-person driving or pilot view", "cockpit-camera");
        cockpit->header.SetVisible(false);

        cockpit->preview.Create("Preview cockpit");
        cockpit->preview.OnClick([this, cockpitTarget](const wi::gui::EventArgs&) {
            if (cockpitPreviewCamera_)
            {
                EndVehicleCockpitPreview();
                return;
            }
            const auto chassis = cockpitTarget();
            if (!camera || chassis == wi::ecs::INVALID_ENTITY) return;
            auto& scene = session_->Scenes().GetScene();
            cockpitPreviewCamera_ = *camera;
            cockpitPreviewChassis_ = chassis;
            cockpitPreviewSelection_ = session_->Selection().SelectedEntity();
            cockpitPreviewSceneRevision_ = session_->Scenes().Revision();
            cockpitPreviewGlass_ = bridge::FindCockpitGlass(scene, chassis);
            cockpitPreviewDefault_ = bridge::DefaultVehicleCockpitCamera(scene, chassis,
                bridge::ResolveDefaultCockpitMarker(scene, chassis), XMConvertToDegrees(camera->fov));
            cockpitPreviewDraft_.reset();
            flyCameraActive_ = false;
            wi::input::HidePointer(false);
            QueueInspectorRefresh();
        });

        cockpit->capture.Create("Set from view");
        cockpit->capture.SetText("SET FROM VIEW");
        cockpit->capture.SetTooltip("Save this viewport's position, aim and field of view relative to the vehicle.");
        cockpit->capture.OnClick([this, cockpitTarget, commitCockpit](const wi::gui::EventArgs&) {
            const auto chassis = cockpitTarget();
            if (!camera || cockpitPreviewCamera_ || camera->IsOrtho() || chassis == wi::ecs::INVALID_ENTITY) return;
            const auto* transform = session_->Scenes().GetScene().transforms.GetComponent(chassis);
            bridge::VehicleCockpitCamera settings;
            if (transform && bridge::CaptureVehicleCockpitFromView(*transform, *camera, settings))
                commitCockpit(settings);
        });
        cockpit->reset.Create("Reset cockpit camera");
        cockpit->reset.SetText("RESET TO DEFAULT");
        cockpit->reset.OnClick([commitCockpit](const wi::gui::EventArgs&) { commitCockpit(std::nullopt); });
        cockpit->advanced.Create("Advanced lens");
        cockpit->advanced.OnClick([this, cockpitFields](const wi::gui::EventArgs&) {
            cockpitFields->showLens = !cockpitFields->showLens;
            QueueInspectorRefresh();
        });

        constexpr const char* labels[] = {
            "LEFT / RIGHT (M)", "UP / DOWN (M)", "FORE / AFT (M)",
            "PITCH (DEG)", "YAW (DEG)", "ROLL (DEG)", "VERTICAL FOV (DEG)", "LENS EQUIV. (MM)",
        };
        for (size_t i = 0; i < cockpit->controls.size(); ++i)
        {
            auto& slider = cockpit->controls[i];
            const float minimum = i < 3 ? -100.f : i < 6 ? -180.f : i == 6 ? bridge::MinimumCockpitFov :
                bridge::CockpitLensMillimetres(bridge::MaximumCockpitFov);
            const float maximum = i < 3 ? 100.f : i < 6 ? 180.f : i == 6 ? bridge::MaximumCockpitFov :
                bridge::CockpitLensMillimetres(bridge::MinimumCockpitFov);
            const float initial = i < 6 ? 0.f : i == 6 ? 60.f : bridge::CockpitLensMillimetres(60.f);
            slider.Create(minimum, maximum, initial, 10000, labels[i], labels[i]);
            if (i == 7) slider.SetTooltip("Equivalent focal length on a 36 x 24 mm reference frame.");
            const auto withValue = [cockpitFields, i](float value) {
                auto fields = bridge::CockpitFields(cockpitFields->displayed);
                fields[i == 7 ? 6 : i] = i == 7 ? bridge::CockpitFovFromLens(value) : value;
                return bridge::CockpitFromFields(fields);
            };
            slider.OnValuePreview([this, withValue](float value) {
                if (cockpitPreviewCamera_) cockpitPreviewDraft_ = withValue(value);
            });
            slider.OnValueCommitted([commitCockpit, withValue](float value) { commitCockpit(withValue(value)); });
            inspectorPanel_.AddWidget(&slider);
        }
        for (auto* button : {&cockpit->preview, &cockpit->capture, &cockpit->reset, &cockpit->advanced})
            inspectorPanel_.AddWidget(button);
        cockpit->Hide();

        auto cockpitProvider = std::make_shared<CallbackInspectorSectionProvider>(
            MakeSection("cockpit-camera", "COCKPIT CAMERA", 16, false),
            [this, cockpit, cockpitTarget](const InspectorSectionContext& context) {
                const bool visible = context.hasSelection && !environmentWorkspaceActive_ &&
                    !terrainWorkspaceActive_ && !renderWorkspaceActive_ &&
                    !studioChrome_.IsAudioWorkspaceActive() && !studioChrome_.IsPhysicsLabActive() &&
                    cockpitTarget() != wi::ecs::INVALID_ENTITY;
                cockpit->header.SetVisible(visible);
                if (!visible) cockpit->Hide();
                return visible;
            },
            [cockpit](const InspectorSectionContext&, float) {
                return 34.f * (cockpit->showLens ? 12.f : 11.f);
            },
            [this, cockpit, cockpitTarget](const InspectorSectionContext&) {
                if (!inspectorSectionRegistry_.IsExpanded("cockpit-camera")) return;
                const auto chassis = cockpitTarget();
                if (chassis == wi::ecs::INVALID_ENTITY) return;
                auto& scene = session_->Scenes().GetScene();
                const auto saved = bridge::ReadVehicleCockpitCamera(scene, chassis);
                const bool previewing = cockpitPreviewCamera_ && cockpitPreviewChassis_ == chassis;
                if (saved)
                    cockpit->displayed = *saved;
                else if (previewing)
                    cockpit->displayed = cockpitPreviewDefault_;
                else
                    cockpit->displayed = bridge::DefaultVehicleCockpitCamera(scene, chassis,
                        bridge::ResolveDefaultCockpitMarker(scene, chassis), camera ? XMConvertToDegrees(camera->fov) : 60.f);
                const auto values = bridge::CockpitFields(cockpit->displayed);
                for (size_t i = 0; i < values.size(); ++i) cockpit->controls[i].SetValue(values[i]);
                cockpit->controls.back().SetValue(bridge::CockpitLensMillimetres(values.back()));
                cockpit->preview.SetText(previewing ? "EXIT COCKPIT PREVIEW" : "PREVIEW COCKPIT");
                cockpit->capture.SetEnabled(camera && !previewing && !camera->IsOrtho());
                cockpit->reset.SetEnabled(saved.has_value());
                cockpit->advanced.SetText(cockpit->showLens ? "HIDE LENS EQUIVALENT" : "ADVANCED LENS");
            },
            [cockpit](const InspectorSectionContext&, const InspectorSectionLayout& layout) {
                cockpit->header.SetPos({12.f, layout.top});
                cockpit->header.SetSize({layout.width, layout.headerHeight});
                cockpit->header.SetText(layout.expanded ? "-  COCKPIT CAMERA" : "+  COCKPIT CAMERA");
                if (!layout.expanded) { cockpit->Hide(); return; }
                float y = layout.contentTop;
                const auto row = [&](wi::gui::Widget& widget) {
                    widget.SetVisible(true);
                    widget.SetPos({12.f, y});
                    widget.SetSize({layout.width, 28.f});
                    y += 34.f;
                };
                row(cockpit->preview);
                row(cockpit->capture);
                for (size_t i = 0; i < 7; ++i) row(cockpit->controls[i]);
                row(cockpit->advanced);
                cockpit->controls.back().SetVisible(cockpit->showLens);
                if (cockpit->showLens) row(cockpit->controls.back());
                row(cockpit->reset);
            });
        cockpitCameraInspectorSection_ = cockpitProvider;
        error.clear();
        if (!inspectorSectionRegistry_.Register(std::move(cockpitProvider), error))
            studioChrome_.SetStatusText("COCKPIT CAMERA // " + error);

        RegisterPhase7Gate7AAnimationInspector(
            *this,
            inspectorPanel_,
            inspectorSectionRegistry_,
            [this]() { QueueInspectorRefresh(); },
            [this](std::string status)
            {
                studioChrome_.SetStatusText(std::move(status));
            });
        RegisterS4BScriptAttachmentInspector(
            *this,
            inspectorPanel_,
            inspectorSectionRegistry_,
            [this]() { QueueInspectorRefresh(); },
            [this](std::string status)
            {
                studioChrome_.SetStatusText(std::move(status));
            });
        RegisterS4DGlobalScriptInspector(
            *this,
            inspectorPanel_,
            inspectorSectionRegistry_,
            [this]() { QueueInspectorRefresh(); },
            [this](std::string status)
            {
                studioChrome_.SetStatusText(std::move(status));
            });
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

    void StudioRenderPath::ResetS1BInspectorDisclosure()
    {
        for (const char* sectionId : {"object-metadata", AICharacterSectionId, "transform", "physics", "cockpit-camera", "rendering", "materials", Phase7AnimationSectionId, S4BActionSectionId, S4BScriptSectionId, S4DGlobalScriptSectionId})
            (void)inspectorSectionRegistry_.SetExpanded(sectionId, false);
    }

    void StudioRenderPath::LayoutS1BInspectorSections()
    {
        LayoutWaterfallInspector();
        LayoutEnvironmentSections();
        LayoutTerrainSections();
        objectSurfaceLabel_.SetVisible(false);
        materialImpactSurface_.SetVisible(false);
        PreparePhase7Gate7AAnimationInspector(*this);
        PrepareS4BScriptAttachmentInspector(*this);
        PrepareS4DGlobalScriptInspector(*this);
        inspectorMetadataHeader_.SetVisible(false);
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
        // Reserve the scrollbar column before it appears, avoiding a one-frame
        // overflow and repeated scrollbar/layout width changes.
        const float innerWidth = ui::InspectorContentWidth(panelWidth, inspectorPanel_.GetControlSize());
        inspectorLabel_.SetWrapEnabled(false);
        inspectorLabel_.SetFitTextEnabled(false);
        inspectorLabel_.font.params.size = 14;
        inspectorLabel_.font.params.bolden = .12f;
        inspectorLabel_.SetSize({innerWidth,28.f});
        auto heading = inspectorLabel_.GetTooltip();
        if (heading.empty())
            heading = inspectorLabel_.GetText();
        inspectorLabel_.SetTooltip(heading);
        auto metrics = inspectorLabel_.font.params;
        metrics.h_wrap = -1;
        if (wi::font::TextWidth(heading,metrics) > innerWidth-20.f)
        {
            do {
                if (heading.empty()) break;
                auto cut=heading.size()-1;
                while (cut>0 && (static_cast<unsigned char>(heading[cut])&0xc0)==0x80) --cut;
                heading.resize(cut);
            } while (wi::font::TextWidth(heading+"...",metrics)>innerWidth-20.f);
            heading+="...";
        }
        inspectorLabel_.SetText(heading);
        const bool hasSession = session_ != nullptr;
        const auto selected = hasSession && session_->Selection().HasSelection()
            ? session_->Selection().SelectedEntity()
            : wi::ecs::INVALID_ENTITY;
        const bool multiple=hasSession && session_->Selection().Entities().size()>1;
        const std::uint64_t selectionRevision=hasSession?session_->Selection().Revision():0;
        const int viewToken = projectHubVisible_
            ? 1
            : environmentWorkspaceActive_ ? 2
            : terrainWorkspaceActive_ ? 3
            : renderWorkspaceActive_ ? 4
            : studioChrome_.IsAudioWorkspaceActive() ? 5
            : studioChrome_.IsPhysicsLabActive() ? 6
            : 16 + studioChrome_.ActiveBottomTab();
        if (selectionRevision != inspectorDisclosureSelectionRevision_ ||
            viewToken != inspectorDisclosureViewToken_)
        {
            ResetS1BInspectorDisclosure();
            // A newly selected asset starts at its identity and primary controls,
            // rather than inheriting the previous asset's scroll position.
            inspectorPanel_.scrollbar_vertical.SetOffset(0);
            inspectorPanel_.scrollbar_horizontal.SetOffset(0);
            inspectorDisclosureSelectionRevision_ = selectionRevision;
            inspectorDisclosureViewToken_ = viewToken;
            if(hasSession && session_->Scenes().GetScene().characters.Contains(selected))
                (void)inspectorSectionRegistry_.SetExpanded(AICharacterSectionId,true);
            if(multiple)(void)inspectorSectionRegistry_.SetExpanded("transform",true);
        }
        static wi::scene::Scene emptyScene;
        const auto& scene = hasSession
            ? session_->Scenes().GetScene()
            : emptyScene;
        const bool selectedWeather = hasSession && selected != wi::ecs::INVALID_ENTITY &&
            scene.weathers.Contains(selected);
        const bool selectedTerrain = hasSession && selected != wi::ecs::INVALID_ENTITY &&
            scene.terrains.Contains(selected);
        bool selectedRiver = false;
        if (hasSession && !terrainWorkspaceActive_)
        {
            for (std::size_t i = 0; i < scene.splines.GetCount(); ++i)
            {
                const auto root = scene.splines.GetEntity(i);
                const auto* material = scene.materials.GetComponent(root);
                if (material && material->shaderType ==
                        wi::scene::MaterialComponent::SHADERTYPE_WATER &&
                    (selected == root || scene.Entity_IsDescendant(selected, root)))
                {
                    selectedRiver = true;
                    break;
                }
            }
        }
        if (environmentWorkspaceActive_ || terrainWorkspaceActive_ ||
            selectedWeather || selectedTerrain || selectedRiver)
        {
            // Specialist panels skip the section registry layout, so clear
            // the asset physics widgets before handing over the dock.
            if (const auto physics = physicsInspectorSection_.lock())
                (void)physics->IsVisible({});
            if (const auto cockpit = cockpitCameraInspectorSection_.lock())
                (void)cockpit->IsVisible({});
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
        const bool groupSelection=bridge::IsSceneAssembly(scene,selected);
        const bool sceneComponents = hasSession && !multiple && !groupSelection && selected != wi::ecs::INVALID_ENTITY && !player;

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
            registryTop = y + 4.0f;
        }

        if(groupSelection && !multiple) {
            sceneIdentityLabel_.SetVisible(true);sceneIdentityLabel_.SetPos({12,44});
            sceneIdentityLabel_.SetSize({innerWidth,20});
            sceneNameInput_.SetVisible(true);sceneNameInput_.SetPos({12,68});
            sceneNameInput_.SetSize({innerWidth,28});
            if(auto* name=scene.names.GetComponent(selected))sceneNameInput_.SetValue(name->name);
            registryTop=104;
        }
        float bottom=registryTop;
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
            fullAt(playerPrefab_, y); y += 34.0f;
            fullAt(playerPrefabSave_, y); y += 34.0f;
            fullAt(playerPrefabReset_, y); y += 34.0f;
            fullAt(playerPrefabStatus_, y, 28.0f); y += 34.0f;
            fullAt(playerFirstPersonArms_, y); y += 34.0f;
            fullAt(playerHandGrips_, y); y += 34.0f;
            fullAt(playerAssembly_, y); y += 34.0f;
            fullAt(playerEquipment_, y); y += 34.0f;
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
                static_cast<wi::gui::Widget*>(&lightPickerButton_),
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

        registryTop=bottom+8.f;
        InspectorSectionContext context;
        context.transformOnly=multiple||groupSelection;
        if (context.transformOnly)
        {
            if (const auto physics = physicsInspectorSection_.lock())
                (void)physics->IsVisible({});
            if (const auto cockpit = cockpitCameraInspectorSection_.lock())
                (void)cockpit->IsVisible({});
        }
        context.hasSelection = hasSession && !multiple && selected != wi::ecs::INVALID_ENTITY;
        context.selectionRevision=selectionRevision;
        context.userData = this;
        inspectorSectionRegistry_.RefreshVisibleSections(context);
        const auto layouts = inspectorSectionRegistry_.LayoutVisibleSections(
            context, registryTop, innerWidth);
        bottom = registryTop;
        for (const auto& layout : layouts)
            bottom = std::max(bottom, layout.top + layout.totalHeight);


        if(hasSession&&selected!=wi::ecs::INVALID_ENTITY&&!light&&(bridge::ParticleEffectRoot(scene,selected)!=wi::ecs::INVALID_ENTITY||scene.emitters.Contains(selected))){
            fullAt(lightPickerButton_,bottom+8.0f);bottom+=42.0f;
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

        // Placed particle effects have one compact authoring route. Keep the
        // existing command-backed name/transforms and move all layer editing
        // into the picker, including legacy standalone emitters.
        const bool particleEffect = hasSession && selected != wi::ecs::INVALID_ENTITY &&
            (bridge::ParticleEffectRoot(scene, selected) != wi::ecs::INVALID_ENTITY ||
                scene.emitters.Contains(selected));
        if (particleEffect)
        {
            // Each provider owns and hides its own controls. Do not toggle
            // native Window visibility: it re-shows uncollapsed children.
            PrepareAICharacterInspector(*this);
            PreparePhase7Gate7AAnimationInspector(*this);
            PreparePhase7Gate7DNativeTimelineInspector(*this);
            PreparePhase7Gate7ESpecialistInspector(*this);
            PreparePhase7Gate7FMeshBlendInspector(*this);
            PrepareS4BScriptAttachmentInspector(*this);
            PrepareS4DGlobalScriptInspector(*this);
            SetS1BMaterialContentVisible(false);
            inspectorMetadataHeader_.SetVisible(false);
            for(auto& bit:sceneLayerBits_)bit.SetVisible(false);
            sceneLayerLabel_.SetVisible(false);
            sceneLayerAllButton_.SetVisible(false);
            sceneLayerNoneButton_.SetVisible(false);
            sceneMetadataLabel_.SetVisible(false);
            sceneMetadataPreset_.SetVisible(false);
            transformSectionHeader_.SetVisible(false);
            renderingSectionHeader_.SetVisible(false);
            materialsSectionHeader_.SetVisible(false);
            for (wi::gui::Widget* widget : {
                static_cast<wi::gui::Widget*>(&sceneIdentityLabel_),
                static_cast<wi::gui::Widget*>(&sceneNameInput_),
                static_cast<wi::gui::Widget*>(&lightPickerButton_),
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
                static_cast<wi::gui::Widget*>(&scaleZ_),
                static_cast<wi::gui::Widget*>(&focusButton_),
                static_cast<wi::gui::Widget*>(&duplicateButton_),
                static_cast<wi::gui::Widget*>(&deleteButton_),
                static_cast<wi::gui::Widget*>(&undoButton_),
                static_cast<wi::gui::Widget*>(&redoButton_),
                static_cast<wi::gui::Widget*>(&saveButton_),
                static_cast<wi::gui::Widget*>(&saveAsButton_),
                static_cast<wi::gui::Widget*>(&reopenButton_)})
                widget->SetVisible(true);
            fullAt(sceneIdentityLabel_,20,20);fullAt(sceneNameInput_,44);
            fullAt(lightPickerButton_,84,32);
            const float fieldWidth=(innerWidth-16)/3;
            const auto vectorRow=[&](wi::gui::Widget& label,wi::gui::Widget& x,
                wi::gui::Widget& y,wi::gui::Widget& z,float top) {
                fullAt(label,top,20);
                wi::gui::Widget* fields[]={&x,&y,&z};
                for(int i=0;i<3;++i){fields[i]->SetPos({12+i*(fieldWidth+8),top+20});fields[i]->SetSize({fieldWidth,28});}
            };
            vectorRow(positionLabel_,translationX_,translationY_,translationZ_,134);
            vectorRow(rotationLabel_,rotationX_,rotationY_,rotationZ_,194);
            vectorRow(scaleLabel_,scaleX_,scaleY_,scaleZ_,254);
            focusButton_.SetPos({12,326});duplicateButton_.SetPos({12+three+gap,326});deleteButton_.SetPos({12+(three+gap)*2,326});
            undoButton_.SetPos({12,366});redoButton_.SetPos({20+two,366});
            saveButton_.SetPos({12,406});saveAsButton_.SetPos({12+three+gap,406});reopenButton_.SetPos({12+(three+gap)*2,406});
        }
    }
}
