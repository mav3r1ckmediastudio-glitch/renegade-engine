#include "AIPatrolRouteInspector.h"

#include "InspectorSectionFramework.h"
#include "RenegadeStudioChrome.h"
#include "StudioApplication.h"

#include "renegade/bridge/CharacterPatrolRouteCommand.h"
#include "renegade/bridge/CharacterService.h"
#include "renegade/bridge/PatrolRouteService.h"
#include "renegade/bridge/StudioSession.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace renegade::studio
{
    namespace
    {
        class PatrolInspector;

        class PatrolSectionProvider final : public IInspectorSectionProvider
        {
        public:
            explicit PatrolSectionProvider(PatrolInspector& owner) noexcept
                : owner_(&owner)
            {
                descriptor_.id = AIPatrolRouteSectionId;
                descriptor_.title = "PATROL";
                descriptor_.order = 20;
                descriptor_.defaultExpanded = true;
                descriptor_.headerHeight = 28.0f;
                descriptor_.spacingAfter = 6.0f;
            }

            [[nodiscard]] const InspectorSectionDescriptor& Descriptor() const noexcept override
            {
                return descriptor_;
            }
            [[nodiscard]] bool IsVisible(const InspectorSectionContext& context) const override;
            [[nodiscard]] float MeasureContentHeight(
                const InspectorSectionContext&, float) const override;
            void Refresh(const InspectorSectionContext&) override;
            void ApplyLayout(
                const InspectorSectionContext&,
                const InspectorSectionLayout& layout) override;

        private:
            PatrolInspector* owner_ = nullptr;
            InspectorSectionDescriptor descriptor_;
        };

        class PatrolInspector final
        {
        public:
            PatrolInspector(
                wi::gui::Window& panel,
                InspectorSectionRegistry& registry,
                std::function<void()> requestRefresh,
                std::function<void(std::string)> setStatus)
                : panel_(&panel), registry_(&registry),
                  requestRefresh_(std::move(requestRefresh)),
                  setStatus_(std::move(setStatus))
            {
            }

            void Register()
            {
                header_.Create("AI Patrol Section Header");
                header_.SetTooltip(
                    "Assign or author a governed Patrol Route. Runtime movement remains native Wicked navigation.");
                header_.OnClick([this](const wi::gui::EventArgs&)
                {
                    (void)registry_->ToggleExpanded(AIPatrolRouteSectionId);
                    RequestRefresh();
                });
                panel_->AddWidget(&header_);

                status_.Create("AI Patrol Status");
                status_.SetColor(wi::Color::Transparent());
                status_.SetFitTextEnabled(true);
                panel_->AddWidget(&status_);

                routeLabel_.Create("AI Patrol Route Label");
                routeLabel_.SetColor(wi::Color::Transparent());
                routeLabel_.SetText("PATROL ROUTE");
                routeLabel_.SetFitTextEnabled(true);
                panel_->AddWidget(&routeLabel_);

                route_.Create("AI Patrol Route Selector");
                route_.SetTooltip(
                    "Assign a governed Patrol Route by persistent identity. UI entity IDs are transient only.");
                route_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    AssignRoute(static_cast<wi::ecs::Entity>(args.userdata));
                });
                panel_->AddWidget(&route_);

                create_.Create("AI Patrol Create Route");
                create_.SetText("CREATE + ASSIGN ROUTE");
                create_.SetTooltip(
                    "Create a governed route at this Character and assign it as one Undo/Redo transaction.");
                create_.OnClick([this](const wi::gui::EventArgs&) { CreateRoute(); });
                panel_->AddWidget(&create_);

                addPoint_.Create("AI Patrol Add Point");
                addPoint_.SetText("ADD PATROL POINT");
                addPoint_.SetTooltip(
                    "Add the next ordered point. Move the point with the normal Transform gizmo.");
                addPoint_.OnClick([this](const wi::gui::EventArgs&) { AddPoint(); });
                panel_->AddWidget(&addPoint_);

                modeLabel_.Create("AI Patrol Mode Label");
                modeLabel_.SetColor(wi::Color::Transparent());
                modeLabel_.SetText("ROUTE MODE");
                modeLabel_.SetFitTextEnabled(true);
                panel_->AddWidget(&modeLabel_);

                mode_.Create("AI Patrol Route Mode");
                mode_.AddItem("LOOP", static_cast<std::uint64_t>(bridge::PatrolRouteMode::Loop));
                mode_.AddItem("PING PONG", static_cast<std::uint64_t>(bridge::PatrolRouteMode::PingPong));
                mode_.AddItem("RANDOM", static_cast<std::uint64_t>(bridge::PatrolRouteMode::Random));
                mode_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    CommitRouteSettings(
                        static_cast<bridge::PatrolRouteMode>(args.userdata),
                        wait_.GetValue());
                });
                panel_->AddWidget(&mode_);

                wait_.Create(
                    0.0f, 10.0f, 1.0f, 200.0f,
                    "AI Patrol Wait", "WAIT AT POINT (S)");
                wait_.SetTooltip("Pause duration at each Patrol Point before moving to the next point.");
                wait_.OnValueCommitted([this](const float value)
                {
                    CommitRouteSettings(
                        static_cast<bridge::PatrolRouteMode>(mode_.GetSelectedUserdata()),
                        value);
                });
                panel_->AddWidget(&wait_);

                pointInfo_.Create("AI Patrol Point Info");
                pointInfo_.SetColor(wi::Color::Transparent());
                pointInfo_.SetFitTextEnabled(true);
                panel_->AddWidget(&pointInfo_);

                std::string error;
                if (!registry_->Register(
                        std::make_shared<PatrolSectionProvider>(*this), error))
                {
                    SetStatus("AI-04 // " + error);
                }
            }

            [[nodiscard]] bool IsVisible()
            {
                ResolveSelection();
                return selectedCharacter_ != wi::ecs::INVALID_ENTITY ||
                    selectedRoute_ != wi::ecs::INVALID_ENTITY;
            }

            [[nodiscard]] float Measure() const noexcept
            {
                return 262.0f;
            }

            void Refresh()
            {
                ResolveSelection();
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr)
                    return;
                auto& scene = session->Scenes().GetScene();

                route_.ClearItems();
                route_.AddItem("NONE", static_cast<std::uint64_t>(wi::ecs::INVALID_ENTITY));
                const auto routes = bridge::CollectPatrolRoutes(scene);
                for (const auto entity : routes)
                {
                    const auto* name = scene.names.GetComponent(entity);
                    const std::string label = name != nullptr && !name->name.empty()
                        ? name->name
                        : std::string("Patrol Route ") + std::to_string(entity);
                    route_.AddItem(label, static_cast<std::uint64_t>(entity));
                }

                if (selectedCharacter_ != wi::ecs::INVALID_ENTITY)
                {
                    const auto settings = bridge::CaptureCharacterSettings(
                        scene, selectedCharacter_);
                    selectedRoute_ = ResolveRouteByStableId(
                        scene, settings.patrolRouteEntityId);
                    route_.SetSelectedByUserdataWithoutCallback(
                        static_cast<std::uint64_t>(selectedRoute_));
                    create_.SetEnabled(selectedRoute_ == wi::ecs::INVALID_ENTITY);
                    route_.SetEnabled(true);
                }
                else
                {
                    route_.SetSelectedByUserdataWithoutCallback(
                        static_cast<std::uint64_t>(selectedRoute_));
                    route_.SetEnabled(false);
                    create_.SetEnabled(false);
                }

                bridge::PatrolRoute captured;
                std::string error;
                if (selectedRoute_ != wi::ecs::INVALID_ENTITY &&
                    bridge::CapturePatrolRoute(scene, selectedRoute_, captured, error))
                {
                    mode_.SetSelectedByUserdataWithoutCallback(
                        static_cast<std::uint64_t>(captured.settings.mode));
                    wait_.SetValue(captured.settings.waitSeconds);
                    mode_.SetEnabled(true);
                    wait_.SetEnabled(true);
                    addPoint_.SetEnabled(true);
                    status_.SetText(
                        "Governed route // " + captured.stableId.substr(0, 8) + "…");
                    pointInfo_.SetText(
                        std::to_string(captured.points.size()) +
                        " ordered point(s) // Runtime uses native Wicked navigation");
                }
                else
                {
                    mode_.SetEnabled(false);
                    wait_.SetEnabled(false);
                    addPoint_.SetEnabled(false);
                    status_.SetText(selectedCharacter_ != wi::ecs::INVALID_ENTITY
                        ? "No Patrol Route assigned."
                        : "Select a Character or Patrol Route.");
                    pointInfo_.SetText(
                        "Create/assign a route, then add points and move them with the normal gizmo.");
                }
            }

            void PrepareForLayout()
            {
                for (auto* widget : Widgets())
                    widget->SetVisible(false);
            }

            void Layout(const InspectorSectionLayout& layout)
            {
                header_.SetVisible(true);
                header_.SetPos(XMFLOAT2(12.0f, layout.top));
                header_.SetSize(XMFLOAT2(layout.width, layout.headerHeight));
                header_.SetText(
                    std::string(layout.expanded ? "▼  " : "▶  ") + "PATROL");
                if (!layout.expanded)
                    return;

                const float x = 12.0f;
                const float width = layout.width;
                float y = layout.contentTop;
                status_.SetVisible(true);
                status_.SetPos(XMFLOAT2(x, y));
                status_.SetSize(XMFLOAT2(width, 22.0f));
                y += 26.0f;
                routeLabel_.SetVisible(true);
                routeLabel_.SetPos(XMFLOAT2(x, y));
                routeLabel_.SetSize(XMFLOAT2(width, 18.0f));
                y += 20.0f;
                route_.SetVisible(true);
                route_.SetPos(XMFLOAT2(x, y));
                route_.SetSize(XMFLOAT2(width, 28.0f));
                y += 34.0f;
                const float half = (width - 8.0f) * 0.5f;
                create_.SetVisible(true);
                create_.SetPos(XMFLOAT2(x, y));
                create_.SetSize(XMFLOAT2(half, 28.0f));
                addPoint_.SetVisible(true);
                addPoint_.SetPos(XMFLOAT2(x + half + 8.0f, y));
                addPoint_.SetSize(XMFLOAT2(half, 28.0f));
                y += 34.0f;
                modeLabel_.SetVisible(true);
                modeLabel_.SetPos(XMFLOAT2(x, y));
                modeLabel_.SetSize(XMFLOAT2(width, 18.0f));
                y += 20.0f;
                mode_.SetVisible(true);
                mode_.SetPos(XMFLOAT2(x, y));
                mode_.SetSize(XMFLOAT2(width, 28.0f));
                y += 34.0f;
                wait_.SetVisible(true);
                wait_.SetPos(XMFLOAT2(x, y));
                wait_.SetSize(XMFLOAT2(width, 28.0f));
                y += 34.0f;
                pointInfo_.SetVisible(true);
                pointInfo_.SetPos(XMFLOAT2(x, y));
                pointInfo_.SetSize(XMFLOAT2(width, 34.0f));
            }

        private:
            [[nodiscard]] std::vector<wi::gui::Widget*> Widgets()
            {
                return {
                    &header_, &status_, &routeLabel_, &route_, &create_, &addPoint_,
                    &modeLabel_, &mode_, &wait_, &pointInfo_};
            }

            void ResolveSelection()
            {
                selectedCharacter_ = wi::ecs::INVALID_ENTITY;
                selectedRoute_ = wi::ecs::INVALID_ENTITY;
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || !session->Selection().HasSelection())
                    return;
                const wi::ecs::Entity selected = session->Selection().SelectedEntity();
                auto& scene = session->Scenes().GetScene();
                if (bridge::IsRenegadeCharacter(scene, selected))
                    selectedCharacter_ = selected;
                else if (bridge::IsPatrolRoute(scene, selected))
                    selectedRoute_ = selected;
            }

            [[nodiscard]] static wi::ecs::Entity ResolveRouteByStableId(
                const wi::scene::Scene& scene,
                const bridge::StableId& stableId) noexcept
            {
                if (stableId.empty())
                    return wi::ecs::INVALID_ENTITY;
                for (const auto entity : bridge::CollectPatrolRoutes(scene))
                {
                    if (bridge::PersistentEntityId(scene, entity) == stableId)
                        return entity;
                }
                return wi::ecs::INVALID_ENTITY;
            }

            void AssignRoute(const wi::ecs::Entity routeEntity)
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || selectedCharacter_ == wi::ecs::INVALID_ENTITY)
                    return;
                auto& scene = session->Scenes().GetScene();
                auto after = bridge::CaptureCharacterSettings(scene, selectedCharacter_);
                after.patrolRouteEntityId = routeEntity == wi::ecs::INVALID_ENTITY
                    ? bridge::StableId{}
                    : bridge::PersistentEntityId(scene, routeEntity);
                std::string validation;
                if (!bridge::ValidateCharacterSettings(after, validation))
                {
                    SetStatus("AI-04 // " + validation);
                    return;
                }
                if (session->Commands().Execute(
                        std::make_unique<bridge::SetCharacterSettingsCommand>(
                            scene, selectedCharacter_, after)))
                {
                    SetStatus(routeEntity == wi::ecs::INVALID_ENTITY
                        ? "AI-04 // Patrol Route cleared"
                        : "AI-04 // Patrol Route assigned by stable identity");
                    RequestRefresh();
                }
            }

            void CreateRoute()
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || selectedCharacter_ == wi::ecs::INVALID_ENTITY)
                    return;
                auto& scene = session->Scenes().GetScene();
                const auto* transform = scene.transforms.GetComponent(selectedCharacter_);
                if (transform == nullptr)
                {
                    SetStatus("AI-04 // Character has no Transform for route placement");
                    return;
                }
                if (session->Commands().Execute(
                        std::make_unique<bridge::CreateCharacterPatrolRouteCommand>(
                            scene, selectedCharacter_, transform->GetPosition())))
                {
                    SetStatus("AI-04 // Patrol Route created and assigned");
                    RequestRefresh();
                }
                else
                {
                    SetStatus("AI-04 // Patrol Route creation was rejected");
                }
            }

            [[nodiscard]] XMFLOAT3 SuggestedPointPosition(
                const wi::scene::Scene& scene) const
            {
                if (selectedCharacter_ != wi::ecs::INVALID_ENTITY)
                {
                    if (const auto* transform = scene.transforms.GetComponent(selectedCharacter_))
                        return transform->GetPosition();
                }
                bridge::PatrolRoute captured;
                std::string error;
                if (selectedRoute_ != wi::ecs::INVALID_ENTITY &&
                    bridge::CapturePatrolRoute(scene, selectedRoute_, captured, error) &&
                    !captured.points.empty())
                {
                    XMFLOAT3 result = captured.points.back().position;
                    result.x += 2.0f;
                    return result;
                }
                if (const auto* transform = scene.transforms.GetComponent(selectedRoute_))
                    return transform->GetPosition();
                return XMFLOAT3(0.0f, 0.0f, 0.0f);
            }

            void AddPoint()
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || selectedRoute_ == wi::ecs::INVALID_ENTITY)
                    return;
                auto& scene = session->Scenes().GetScene();
                if (session->Commands().Execute(
                        std::make_unique<bridge::AddPatrolRoutePointCommand>(
                            scene, selectedRoute_, SuggestedPointPosition(scene))))
                {
                    SetStatus("AI-04 // Patrol Point added");
                    RequestRefresh();
                }
            }

            void CommitRouteSettings(
                const bridge::PatrolRouteMode mode,
                const float waitSeconds)
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || selectedRoute_ == wi::ecs::INVALID_ENTITY)
                    return;
                auto& scene = session->Scenes().GetScene();
                bridge::PatrolRouteSettings after;
                after.mode = mode;
                after.waitSeconds = waitSeconds;
                if (session->Commands().Execute(
                        std::make_unique<bridge::SetPatrolRouteSettingsCommand>(
                            scene, selectedRoute_, after)))
                {
                    SetStatus("AI-04 // Patrol Route settings updated");
                    RequestRefresh();
                }
            }

            void RequestRefresh()
            {
                if (requestRefresh_)
                    requestRefresh_();
            }

            void SetStatus(const std::string& value)
            {
                if (setStatus_)
                    setStatus_(value);
            }

            wi::gui::Window* panel_ = nullptr;
            InspectorSectionRegistry* registry_ = nullptr;
            std::function<void()> requestRefresh_;
            std::function<void(std::string)> setStatus_;
            wi::ecs::Entity selectedCharacter_ = wi::ecs::INVALID_ENTITY;
            wi::ecs::Entity selectedRoute_ = wi::ecs::INVALID_ENTITY;
            SceneInspectorButton header_;
            wi::gui::Label status_;
            wi::gui::Label routeLabel_;
            SceneInspectorComboBox route_;
            SceneInspectorButton create_;
            SceneInspectorButton addPoint_;
            wi::gui::Label modeLabel_;
            SceneInspectorComboBox mode_;
            SceneInspectorSlider wait_;
            wi::gui::Label pointInfo_;
        };

        [[nodiscard]] bool PatrolSectionProvider::IsVisible(
            const InspectorSectionContext&) const
        {
            return owner_ != nullptr && owner_->IsVisible();
        }

        [[nodiscard]] float PatrolSectionProvider::MeasureContentHeight(
            const InspectorSectionContext&, const float) const
        {
            return owner_ != nullptr ? owner_->Measure() : 0.0f;
        }

        void PatrolSectionProvider::Refresh(const InspectorSectionContext&)
        {
            if (owner_ != nullptr)
                owner_->Refresh();
        }

        void PatrolSectionProvider::ApplyLayout(
            const InspectorSectionContext&,
            const InspectorSectionLayout& layout)
        {
            if (owner_ != nullptr)
                owner_->Layout(layout);
        }

        std::unique_ptr<PatrolInspector> activeInspector;
        StudioRenderPath* activeOwner = nullptr;
    }

    void RegisterAIPatrolRouteInspector(
        StudioRenderPath& owner,
        wi::gui::Window& inspectorPanel,
        InspectorSectionRegistry& registry,
        std::function<void()> requestRefresh,
        std::function<void(std::string)> setStatus)
    {
        activeInspector = std::make_unique<PatrolInspector>(
            inspectorPanel,
            registry,
            std::move(requestRefresh),
            std::move(setStatus));
        activeOwner = &owner;
        activeInspector->Register();
    }

    void PrepareAIPatrolRouteInspector(StudioRenderPath& owner)
    {
        if (activeOwner == &owner && activeInspector != nullptr)
            activeInspector->PrepareForLayout();
    }
}
