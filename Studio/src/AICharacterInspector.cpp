#include "AICharacterInspector.h"

#include "InspectorSectionFramework.h"
#include "RenegadeStudioChrome.h"
#include "StudioApplication.h"

#include "renegade/bridge/CharacterService.h"
#include "renegade/bridge/StudioSession.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace renegade::studio
{
    namespace
    {
        class CharacterInspector;

        class CharacterSectionProvider final : public IInspectorSectionProvider
        {
        public:
            explicit CharacterSectionProvider(CharacterInspector& owner) noexcept
                : owner_(&owner)
            {
                descriptor_.id = AICharacterSectionId;
                descriptor_.title = "CHARACTER";
                descriptor_.order = 18;
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
            CharacterInspector* owner_ = nullptr;
            InspectorSectionDescriptor descriptor_;
        };

        class CharacterInspector final
        {
        public:
            CharacterInspector(
                StudioRenderPath& owner,
                wi::gui::Window& panel,
                InspectorSectionRegistry& registry,
                std::function<void()> requestRefresh,
                std::function<void(std::string)> setStatus)
                : owner_(&owner), panel_(&panel), registry_(&registry),
                  requestRefresh_(std::move(requestRefresh)),
                  setStatus_(std::move(setStatus))
            {
            }

            void Register()
            {
                header_.Create("Character Section Header");
                header_.SetTooltip(
                    "Promote the selected hierarchy into Renegade's governed Character system.");
                header_.OnClick([this](const wi::gui::EventArgs&)
                {
                    (void)registry_->ToggleExpanded(AICharacterSectionId);
                    RequestRefresh();
                });
                panel_->AddWidget(&header_);

                CreateLabel(status_, "Character Status");
                CreateLabel(setup_, "Character Setup");

                makeRemove_.Create("Make or Remove Character");
                makeRemove_.OnClick([this](const wi::gui::EventArgs&) { ToggleCharacter(); });
                panel_->AddWidget(&makeRemove_);

                type_.Create("Character Type");
                AddEnum(type_, "HUMAN", bridge::CharacterType::Human);
                AddEnum(type_, "CREATURE", bridge::CharacterType::Creature);
                AddEnum(type_, "CUSTOM", bridge::CharacterType::Custom);
                type_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    const auto value = static_cast<bridge::CharacterType>(args.userdata);
                    Commit([value](bridge::CharacterAuthoringSettings& settings)
                    { settings.type = value; }, "character type updated");
                });
                panel_->AddWidget(&type_);

                role_.Create("Character Role");
                AddEnum(role_, "GUARD", bridge::CharacterRole::Guard);
                AddEnum(role_, "PATROL GUARD", bridge::CharacterRole::PatrolGuard);
                AddEnum(role_, "SOLDIER", bridge::CharacterRole::Soldier);
                AddEnum(role_, "CIVILIAN", bridge::CharacterRole::Civilian);
                AddEnum(role_, "COMPANION", bridge::CharacterRole::Companion);
                AddEnum(role_, "PREDATOR", bridge::CharacterRole::Predator);
                AddEnum(role_, "PASSIVE", bridge::CharacterRole::Passive);
                AddEnum(role_, "CUSTOM", bridge::CharacterRole::Custom);
                role_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    const auto value = static_cast<bridge::CharacterRole>(args.userdata);
                    Commit([value](bridge::CharacterAuthoringSettings& settings)
                    { settings.role = value; }, "character role updated");
                });
                panel_->AddWidget(&role_);

                personality_.Create("Character Personality");
                AddEnum(personality_, "CAUTIOUS", bridge::PersonalityPreset::Cautious);
                AddEnum(personality_, "BALANCED", bridge::PersonalityPreset::Balanced);
                AddEnum(personality_, "AGGRESSIVE", bridge::PersonalityPreset::Aggressive);
                AddEnum(personality_, "TIMID", bridge::PersonalityPreset::Timid);
                AddEnum(personality_, "VETERAN", bridge::PersonalityPreset::Veteran);
                AddEnum(personality_, "RECKLESS", bridge::PersonalityPreset::Reckless);
                AddEnum(personality_, "CUSTOM", bridge::PersonalityPreset::Custom);
                personality_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    const auto value = static_cast<bridge::PersonalityPreset>(args.userdata);
                    Commit([value](bridge::CharacterAuthoringSettings& settings)
                    { settings.personality = value; }, "character personality updated");
                });
                panel_->AddWidget(&personality_);

                faction_.Create("Character Faction");
                faction_.AddItem("PLAYER", 0);
                faction_.AddItem("FRIENDLY", 1);
                faction_.AddItem("ENEMY", 2);
                faction_.AddItem("CIVILIAN", 3);
                faction_.AddItem("WILDLIFE", 4);
                faction_.AddItem("NEUTRAL", 5);
                faction_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    static constexpr const char* values[] = {
                        "Player", "Friendly", "Enemy", "Civilian", "Wildlife", "Neutral"};
                    const std::size_t index = static_cast<std::size_t>(args.userdata);
                    if (index >= 6)
                        return;
                    const std::string value = values[index];
                    Commit([value](bridge::CharacterAuthoringSettings& settings)
                    { settings.factionId = value; }, "character faction updated");
                });
                panel_->AddWidget(&faction_);

                CreateLabel(animationSetLabel_, "Character Animation Set Label");
                animationSet_.Create("Character Animation Set Stable ID");
                animationSet_.SetPlaceholder("Animation Set stable ID (optional)");
                animationSet_.SetCancelInputEnabled(false);
                animationSet_.OnInput([this](const wi::gui::EventArgs& args)
                {
                    animationDraft_ = args.sValue;
                    CommitAnimationSet();
                });
                panel_->AddWidget(&animationSet_);

                autonomous_.Create("Character Autonomous");
                autonomous_.SetTooltip(
                    "Autonomous authoring is persisted now; cognition is added by later AI gates.");
                autonomous_.OnClick([this](const wi::gui::EventArgs& args)
                {
                    const bool value = args.bValue;
                    Commit([value](bridge::CharacterAuthoringSettings& settings)
                    { settings.autonomous = value; },
                    value ? "autonomous character enabled" : "autonomous character disabled");
                });
                panel_->AddWidget(&autonomous_);

                std::string error;
                if (!registry_->Register(
                    std::make_shared<CharacterSectionProvider>(*this), error))
                {
                    SetStatus("AI-01 // " + error);
                }
            }

            [[nodiscard]] bool IsVisible(const InspectorSectionContext& context)
            {
                if (!context.hasSelection)
                    return false;
                ResolveSelection();
                auto* session = bridge::StudioSession::Current();
                return session != nullptr && selected_ != wi::ecs::INVALID_ENTITY &&
                    session->Scenes().GetScene().transforms.Contains(selected_);
            }

            [[nodiscard]] float Measure() const noexcept
            {
                return isCharacter_ ? 360.0f : 142.0f;
            }

            void Refresh()
            {
                ResolveSelection();
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || selected_ == wi::ecs::INVALID_ENTITY)
                    return;

                auto& scene = session->Scenes().GetScene();
                const auto promotion = bridge::InspectCharacterPromotion(scene, selected_);
                isCharacter_ = promotion.alreadyCharacter;
                setup_.SetText(promotion.summary);
                makeRemove_.SetText(isCharacter_ ? "REMOVE CHARACTER" : "MAKE CHARACTER");
                makeRemove_.SetEnabled(isCharacter_ || promotion.canPromote);

                if (!isCharacter_)
                {
                    status_.SetText(
                        promotion.canPromote
                        ? "Ready to promote selected hierarchy. Warnings do not block setup."
                        : "Selection cannot be promoted to a Character.");
                    SetAuthoringEnabled(false);
                    return;
                }

                const auto settings = bridge::CaptureCharacterSettings(scene, selected_);
                status_.SetText(
                    "Governed Character // stable identity + native Wicked CharacterComponent");
                SetAuthoringEnabled(true);
                type_.SetSelectedByUserdataWithoutCallback(
                    static_cast<std::uint64_t>(settings.type));
                role_.SetSelectedByUserdataWithoutCallback(
                    static_cast<std::uint64_t>(settings.role));
                personality_.SetSelectedByUserdataWithoutCallback(
                    static_cast<std::uint64_t>(settings.personality));
                faction_.SetSelectedByUserdataWithoutCallback(FactionIndex(settings.factionId));
                animationDraft_ = settings.animationSetId;
                animationSet_.SetValue(animationDraft_);
                autonomous_.SetCheck(settings.autonomous);
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
                    std::string(layout.expanded ? "▼  " : "▶  ") + "CHARACTER");
                if (!layout.expanded)
                    return;

                const float x = 12.0f;
                const float width = layout.width;
                float y = layout.contentTop;
                const auto row = [&](wi::gui::Widget& widget, const float height = 28.0f)
                {
                    widget.SetVisible(true);
                    widget.SetPos(XMFLOAT2(x, y));
                    widget.SetSize(XMFLOAT2(width, height));
                    y += height + 6.0f;
                };
                const auto label = [&](wi::gui::Widget& widget)
                {
                    widget.SetVisible(true);
                    widget.SetPos(XMFLOAT2(x, y));
                    widget.SetSize(XMFLOAT2(width, 22.0f));
                    y += 26.0f;
                };

                label(status_);
                label(setup_);
                row(makeRemove_);
                if (!isCharacter_)
                    return;
                row(type_);
                row(role_);
                row(personality_);
                row(faction_);
                label(animationSetLabel_);
                row(animationSet_);
                row(autonomous_);
            }

        private:
            template<typename Enum>
            void AddEnum(SceneInspectorComboBox& combo, const char* label, const Enum value)
            {
                combo.AddItem(label, static_cast<std::uint64_t>(value));
            }

            void CreateLabel(wi::gui::Label& label, const char* name)
            {
                label.Create(name);
                label.SetColor(wi::Color::Transparent());
                label.SetFitTextEnabled(true);
                panel_->AddWidget(&label);
            }

            [[nodiscard]] std::vector<wi::gui::Widget*> Widgets()
            {
                return {
                    &header_, &status_, &setup_, &makeRemove_, &type_, &role_,
                    &personality_, &faction_, &animationSetLabel_, &animationSet_,
                    &autonomous_};
            }

            void ResolveSelection()
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || !session->Selection().HasSelection())
                {
                    selected_ = wi::ecs::INVALID_ENTITY;
                    isCharacter_ = false;
                    return;
                }
                selected_ = session->Selection().SelectedEntity();
                isCharacter_ = bridge::IsRenegadeCharacter(
                    session->Scenes().GetScene(), selected_);
            }

            void SetAuthoringEnabled(const bool enabled)
            {
                type_.SetEnabled(enabled);
                role_.SetEnabled(enabled);
                personality_.SetEnabled(enabled);
                faction_.SetEnabled(enabled);
                animationSet_.SetEnabled(enabled);
                autonomous_.SetEnabled(enabled);
                animationSetLabel_.SetText(
                    enabled ? "ANIMATION SET // stable governed asset ID (optional)" :
                    "ANIMATION SET // available after MAKE CHARACTER");
            }

            static std::uint64_t FactionIndex(const std::string& faction) noexcept
            {
                if (faction == "Player") return 0;
                if (faction == "Friendly") return 1;
                if (faction == "Enemy") return 2;
                if (faction == "Civilian") return 3;
                if (faction == "Wildlife") return 4;
                return 5;
            }

            void ToggleCharacter()
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || selected_ == wi::ecs::INVALID_ENTITY)
                    return;
                auto& scene = session->Scenes().GetScene();
                const bool removing = bridge::IsRenegadeCharacter(scene, selected_);
                std::unique_ptr<bridge::ICommand> command;
                if (removing)
                    command = std::make_unique<bridge::RemoveCharacterCommand>(scene, selected_);
                else
                    command = std::make_unique<bridge::MakeCharacterCommand>(scene, selected_);

                if (session->Commands().Execute(std::move(command)))
                {
                    SetStatus(removing
                        ? "AI-01 // Character authoring removed"
                        : "AI-01 // Character promoted with native Wicked controller");
                }
                else
                {
                    SetStatus("AI-01 // Character operation was rejected");
                }
                RequestRefresh();
            }

            template<typename Mutator>
            void Commit(Mutator&& mutator, const char* message)
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || selected_ == wi::ecs::INVALID_ENTITY)
                    return;
                auto& scene = session->Scenes().GetScene();
                if (!bridge::IsRenegadeCharacter(scene, selected_))
                    return;
                auto after = bridge::CaptureCharacterSettings(scene, selected_);
                mutator(after);
                if (session->Commands().Execute(
                    std::make_unique<bridge::SetCharacterSettingsCommand>(
                        scene, selected_, after)))
                {
                    SetStatus(std::string("AI-01 // ") + message);
                    RequestRefresh();
                }
            }

            void CommitAnimationSet()
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || selected_ == wi::ecs::INVALID_ENTITY ||
                    !bridge::IsRenegadeCharacter(session->Scenes().GetScene(), selected_))
                {
                    return;
                }
                auto& scene = session->Scenes().GetScene();
                auto after = bridge::CaptureCharacterSettings(scene, selected_);
                after.animationSetId = animationDraft_;
                std::string validation;
                if (!bridge::ValidateCharacterSettings(after, validation))
                {
                    SetStatus("AI-01 // " + validation);
                    return;
                }
                if (session->Commands().Execute(
                    std::make_unique<bridge::SetCharacterSettingsCommand>(
                        scene, selected_, after)))
                {
                    SetStatus("AI-01 // animation set reference updated");
                    RequestRefresh();
                }
            }

            void RequestRefresh()
            {
                if (requestRefresh_)
                    requestRefresh_();
            }

            void SetStatus(std::string status)
            {
                if (setStatus_)
                    setStatus_(std::move(status));
            }

            StudioRenderPath* owner_ = nullptr;
            wi::gui::Window* panel_ = nullptr;
            InspectorSectionRegistry* registry_ = nullptr;
            std::function<void()> requestRefresh_;
            std::function<void(std::string)> setStatus_;
            wi::ecs::Entity selected_ = wi::ecs::INVALID_ENTITY;
            bool isCharacter_ = false;
            std::string animationDraft_;

            SceneInspectorButton header_;
            wi::gui::Label status_;
            wi::gui::Label setup_;
            SceneInspectorButton makeRemove_;
            SceneInspectorComboBox type_;
            SceneInspectorComboBox role_;
            SceneInspectorComboBox personality_;
            SceneInspectorComboBox faction_;
            wi::gui::Label animationSetLabel_;
            SceneInspectorTextInputField animationSet_;
            SceneInspectorCheckBox autonomous_;
        };

        bool CharacterSectionProvider::IsVisible(
            const InspectorSectionContext& context) const
        {
            return owner_ != nullptr && owner_->IsVisible(context);
        }

        float CharacterSectionProvider::MeasureContentHeight(
            const InspectorSectionContext&,
            float) const
        {
            return owner_ != nullptr ? owner_->Measure() : 0.0f;
        }

        void CharacterSectionProvider::Refresh(const InspectorSectionContext&)
        {
            if (owner_ != nullptr)
                owner_->Refresh();
        }

        void CharacterSectionProvider::ApplyLayout(
            const InspectorSectionContext&,
            const InspectorSectionLayout& layout)
        {
            if (owner_ != nullptr)
                owner_->Layout(layout);
        }

        std::unique_ptr<CharacterInspector> activeInspector;
        StudioRenderPath* activeOwner = nullptr;
    }

    void RegisterAICharacterInspector(
        StudioRenderPath& owner,
        wi::gui::Window& inspectorPanel,
        InspectorSectionRegistry& registry,
        std::function<void()> requestRefresh,
        std::function<void(std::string)> setStatus)
    {
        activeInspector.reset();
        activeOwner = &owner;
        activeInspector = std::make_unique<CharacterInspector>(
            owner,
            inspectorPanel,
            registry,
            std::move(requestRefresh),
            std::move(setStatus));
        activeInspector->Register();
    }

    void PrepareAICharacterInspector(StudioRenderPath& owner)
    {
        if (activeOwner == &owner && activeInspector)
            activeInspector->PrepareForLayout();
    }
}
