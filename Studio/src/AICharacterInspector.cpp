#include "AICharacterInspector.h"

#include "InspectorSectionFramework.h"
#include "RenegadeStudioChrome.h"
#include "StudioApplication.h"

#include "renegade/bridge/CharacterProfileService.h"
#include "renegade/bridge/CharacterService.h"
#include "renegade/bridge/FactionService.h"
#include "renegade/bridge/StudioSession.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
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

        class CharacterAdvancedSectionProvider final : public IInspectorSectionProvider
        {
        public:
            explicit CharacterAdvancedSectionProvider(CharacterInspector& owner) noexcept
                : owner_(&owner)
            {
                descriptor_.id = AICharacterAdvancedSectionId;
                descriptor_.title = "ADVANCED AI";
                descriptor_.order = 19;
                descriptor_.defaultExpanded = false;
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
            using OverrideMember =
                std::optional<float> bridge::CharacterAdvancedOverrides::*;

            CharacterInspector(
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
                CreateLabel(effective_, "Character Effective Profile");

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
                faction_.AddItem("CUSTOM (EDIT IN ADVANCED AI)", 6);
                faction_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    static constexpr const char* values[] = {
                        "Player", "Friendly", "Enemy", "Civilian", "Wildlife", "Neutral"};
                    const std::size_t index = static_cast<std::size_t>(args.userdata);
                    if (index >= 6)
                    {
                        (void)registry_->SetExpanded(AICharacterAdvancedSectionId, true);
                        RequestRefresh();
                        return;
                    }
                    const std::string value = values[index];
                    Commit([value](bridge::CharacterAuthoringSettings& settings)
                    { settings.factionId = value; }, "character faction updated");
                });
                panel_->AddWidget(&faction_);

                skill_.Create("Character Skill");
                AddEnum(skill_, "UNTRAINED", bridge::SkillPreset::Untrained);
                AddEnum(skill_, "NOVICE", bridge::SkillPreset::Novice);
                AddEnum(skill_, "TRAINED", bridge::SkillPreset::Trained);
                AddEnum(skill_, "VETERAN", bridge::SkillPreset::Veteran);
                AddEnum(skill_, "ELITE", bridge::SkillPreset::Elite);
                skill_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    const auto value = static_cast<bridge::SkillPreset>(args.userdata);
                    Commit([value](bridge::CharacterAuthoringSettings& settings)
                    { settings.skill = value; }, "character skill updated");
                });
                panel_->AddWidget(&skill_);

                awareness_.Create("Character Awareness");
                AddEnum(awareness_, "RELAXED", bridge::AwarenessPreset::Relaxed);
                AddEnum(awareness_, "NORMAL", bridge::AwarenessPreset::Normal);
                AddEnum(awareness_, "ALERT", bridge::AwarenessPreset::Alert);
                AddEnum(awareness_, "VIGILANT", bridge::AwarenessPreset::Vigilant);
                awareness_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    const auto value = static_cast<bridge::AwarenessPreset>(args.userdata);
                    Commit([value](bridge::CharacterAuthoringSettings& settings)
                    { settings.awareness = value; }, "character awareness updated");
                });
                panel_->AddWidget(&awareness_);

                CreateLabel(animationSetLabel_, "Character Animation Set Label");
                animationSet_.Create("Character Animation Set Stable ID");
                animationSet_.SetPlaceholder("Animation Set stable ID (optional)");
                animationSet_.SetCancelInputEnabled(false);
                animationSet_.OnInput([this](const wi::gui::EventArgs& args)
                {
                    animationDraft_ = args.sValue;
                });
                animationSet_.OnInputAccepted([this](const wi::gui::EventArgs& args)
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

                RegisterAdvancedControls();

                std::string error;
                if (!registry_->Register(
                    std::make_shared<CharacterSectionProvider>(*this), error))
                {
                    SetStatus("AI-02 // " + error);
                }
                error.clear();
                if (!registry_->Register(
                    std::make_shared<CharacterAdvancedSectionProvider>(*this), error))
                {
                    SetStatus("AI-02 // " + error);
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

            [[nodiscard]] bool IsAdvancedVisible(const InspectorSectionContext& context)
            {
                if (!context.hasSelection)
                    return false;
                ResolveSelection();
                return isCharacter_;
            }

            [[nodiscard]] float Measure() const noexcept
            {
                return isCharacter_ ? 454.0f : 142.0f;
            }

            [[nodiscard]] float MeasureAdvanced() const noexcept
            {
                return 956.0f;
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
                    effective_.SetText("");
                    SetAuthoringEnabled(false);
                    return;
                }

                const auto settings = bridge::CaptureCharacterSettings(scene, selected_);
                SetAuthoringEnabled(true);
                ApplyBasicControlValues(settings);

                bridge::CharacterAdvancedOverrides overrides;
                std::string profileError;
                if (!bridge::CaptureCharacterAdvancedOverrides(
                        scene, selected_, overrides, profileError))
                {
                    status_.SetText(
                        "Governed Character // advanced authoring requires repair");
                    effective_.SetText("INVALID ADVANCED AI // " + profileError);
                    return;
                }

                const auto tuning = bridge::ResolveCharacterTuning(settings, overrides);
                status_.SetText(
                    "Governed Character // stable identity + native Wicked CharacterComponent");
                effective_.SetText(
                    "EFFECTIVE // vision " + std::to_string(tuning.visionDistance) +
                    "m  aggression " + std::to_string(tuning.aggression) +
                    "  accuracy " + std::to_string(tuning.accuracy));
            }

            void RefreshAdvanced()
            {
                ResolveSelection();
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || selected_ == wi::ecs::INVALID_ENTITY || !isCharacter_)
                    return;

                auto& scene = session->Scenes().GetScene();
                const auto settings = bridge::CaptureCharacterSettings(scene, selected_);
                factionDraft_ = settings.factionId;
                customFaction_.SetValue(factionDraft_);
                canCommunicate_.SetCheck(settings.canCommunicate);

                bridge::CharacterAdvancedOverrides overrides;
                std::string error;
                if (!bridge::CaptureCharacterAdvancedOverrides(
                        scene, selected_, overrides, error))
                {
                    advancedStatus_.SetText(
                        "INVALID ADVANCED AI // RESET ADVANCED OVERRIDES repairs this payload");
                    resetAdvanced_.SetEnabled(true);
                    SetAdvancedTuningEnabled(false);
                    ApplyAdvancedSliderValues(
                        bridge::ResolveCharacterTuning(settings));
                    return;
                }

                const auto tuning = bridge::ResolveCharacterTuning(settings, overrides);
                advancedStatus_.SetText(
                    bridge::HasAnyCharacterAdvancedOverride(overrides)
                        ? "CUSTOM OVERRIDES ACTIVE // sliders show effective values"
                        : "PROFILE DRIVEN // moving a slider creates an explicit override");
                resetAdvanced_.SetEnabled(bridge::HasAnyCharacterAdvancedOverride(overrides));
                SetAdvancedTuningEnabled(true);
                ApplyAdvancedSliderValues(tuning);
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
                const auto label = [&](wi::gui::Widget& widget, const float height = 22.0f)
                {
                    widget.SetVisible(true);
                    widget.SetPos(XMFLOAT2(x, y));
                    widget.SetSize(XMFLOAT2(width, height));
                    y += height + 4.0f;
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
                row(skill_);
                row(awareness_);
                label(effective_, 34.0f);
                label(animationSetLabel_);
                row(animationSet_);
                row(autonomous_);
            }

            void LayoutAdvanced(const InspectorSectionLayout& layout)
            {
                advancedHeader_.SetVisible(true);
                advancedHeader_.SetPos(XMFLOAT2(12.0f, layout.top));
                advancedHeader_.SetSize(XMFLOAT2(layout.width, layout.headerHeight));
                advancedHeader_.SetText(
                    std::string(layout.expanded ? "▼  " : "▶  ") + "ADVANCED AI");
                if (!layout.expanded)
                    return;

                const float x = 12.0f;
                const float width = layout.width;
                float y = layout.contentTop;
                const auto place = [&](wi::gui::Widget& widget, const float height = 28.0f)
                {
                    widget.SetVisible(true);
                    widget.SetPos(XMFLOAT2(x, y));
                    widget.SetSize(XMFLOAT2(width, height));
                    y += height + 6.0f;
                };
                const auto group = [&](wi::gui::Widget& widget)
                {
                    widget.SetVisible(true);
                    widget.SetPos(XMFLOAT2(x, y));
                    widget.SetSize(XMFLOAT2(width, 20.0f));
                    y += 24.0f;
                };

                place(advancedStatus_, 34.0f);
                place(resetAdvanced_);
                group(customFactionLabel_);
                place(customFaction_);
                group(perceptionGroup_);
                place(vision_);
                place(horizontalFov_);
                place(hearing_);
                place(reaction_);
                group(memoryGroup_);
                place(memory_);
                place(suspicionDecay_);
                place(search_);
                group(personalityGroup_);
                place(aggression_);
                place(courage_);
                place(curiosity_);
                place(alertness_);
                place(loyalty_);
                group(combatGroup_);
                place(accuracy_);
                place(preferredRange_);
                place(cover_);
                place(retreat_);
                place(pursuit_);
                place(suppression_);
                group(communicationGroup_);
                place(canCommunicate_);
                place(communication_);
            }

        private:
            template<typename Enum>
            void AddEnum(SceneInspectorComboBox& combo, const char* label, const Enum value)
            {
                combo.AddItem(label, static_cast<std::uint64_t>(value));
            }

            void RegisterAdvancedControls()
            {
                advancedHeader_.Create("Character Advanced AI Section Header");
                advancedHeader_.SetTooltip(
                    "Optional overrides layered after Character type, role, personality, skill and awareness profiles.");
                advancedHeader_.OnClick([this](const wi::gui::EventArgs&)
                {
                    (void)registry_->ToggleExpanded(AICharacterAdvancedSectionId);
                    RequestRefresh();
                });
                panel_->AddWidget(&advancedHeader_);

                CreateLabel(advancedStatus_, "Character Advanced AI Status");
                CreateGroupLabel(customFactionLabel_, "Character Custom Faction Label",
                    "FACTION // custom creator-defined ID");
                customFaction_.Create("Character Custom Faction ID");
                customFaction_.SetPlaceholder("Custom faction ID");
                customFaction_.SetCancelInputEnabled(false);
                customFaction_.OnInput([this](const wi::gui::EventArgs& args)
                {
                    factionDraft_ = args.sValue;
                });
                customFaction_.OnInputAccepted([this](const wi::gui::EventArgs& args)
                {
                    factionDraft_ = args.sValue;
                    CommitCustomFaction();
                });
                panel_->AddWidget(&customFaction_);

                resetAdvanced_.Create("Reset Character Advanced Overrides");
                resetAdvanced_.SetText("RESET ADVANCED OVERRIDES");
                resetAdvanced_.SetTooltip(
                    "Remove all explicit tuning overrides and return to profile-driven effective values. Also repairs an invalid advanced payload.");
                resetAdvanced_.OnClick([this](const wi::gui::EventArgs&) { ResetAdvanced(); });
                panel_->AddWidget(&resetAdvanced_);

                CreateGroupLabel(perceptionGroup_, "AI Perception Group", "PERCEPTION");
                CreateOverrideSlider(vision_, 0.0f, 200.0f, 35.0f, 2000.0f,
                    "AI Vision Distance", "VISION DISTANCE",
                    &bridge::CharacterAdvancedOverrides::visionDistance);
                CreateOverrideSlider(horizontalFov_, 30.0f, 180.0f, 100.0f, 1500.0f,
                    "AI Horizontal FOV", "FIELD OF VIEW °",
                    &bridge::CharacterAdvancedOverrides::horizontalFovDegrees);
                CreateOverrideSlider(hearing_, 0.0f, 3.0f, 1.0f, 1500.0f,
                    "AI Hearing Sensitivity", "HEARING",
                    &bridge::CharacterAdvancedOverrides::hearingSensitivity);
                CreateOverrideSlider(reaction_, 0.0f, 3.0f, 0.35f, 1500.0f,
                    "AI Visual Reaction", "REACTION SECONDS",
                    &bridge::CharacterAdvancedOverrides::visualReactionSeconds);

                CreateGroupLabel(memoryGroup_, "AI Memory Group", "MEMORY");
                CreateOverrideSlider(memory_, 0.0f, 120.0f, 12.0f, 1200.0f,
                    "AI Memory Seconds", "MEMORY DURATION",
                    &bridge::CharacterAdvancedOverrides::memorySeconds);
                CreateOverrideSlider(suspicionDecay_, 0.0f, 30.0f, 4.0f, 1500.0f,
                    "AI Suspicion Decay", "SUSPICION DECAY",
                    &bridge::CharacterAdvancedOverrides::suspicionDecay);
                CreateOverrideSlider(search_, 0.0f, 120.0f, 12.0f, 1200.0f,
                    "AI Search Seconds", "SEARCH DURATION",
                    &bridge::CharacterAdvancedOverrides::searchSeconds);

                CreateGroupLabel(personalityGroup_, "AI Personality Group", "PERSONALITY");
                CreateOverrideSlider(aggression_, 0.0f, 1.0f, 0.5f, 1000.0f,
                    "AI Aggression", "AGGRESSION",
                    &bridge::CharacterAdvancedOverrides::aggression);
                CreateOverrideSlider(courage_, 0.0f, 1.0f, 0.5f, 1000.0f,
                    "AI Courage", "COURAGE",
                    &bridge::CharacterAdvancedOverrides::courage);
                CreateOverrideSlider(curiosity_, 0.0f, 1.0f, 0.5f, 1000.0f,
                    "AI Curiosity", "CURIOSITY",
                    &bridge::CharacterAdvancedOverrides::curiosity);
                CreateOverrideSlider(alertness_, 0.0f, 1.0f, 0.5f, 1000.0f,
                    "AI Alertness", "ALERTNESS",
                    &bridge::CharacterAdvancedOverrides::alertness);
                CreateOverrideSlider(loyalty_, 0.0f, 1.0f, 0.5f, 1000.0f,
                    "AI Loyalty", "LOYALTY",
                    &bridge::CharacterAdvancedOverrides::loyalty);

                CreateGroupLabel(combatGroup_, "AI Combat Group", "COMBAT");
                CreateOverrideSlider(accuracy_, 0.0f, 1.0f, 0.5f, 1000.0f,
                    "AI Accuracy", "ACCURACY",
                    &bridge::CharacterAdvancedOverrides::accuracy);
                CreateOverrideSlider(preferredRange_, 0.0f, 100.0f, 15.0f, 1000.0f,
                    "AI Preferred Combat Range", "PREFERRED RANGE",
                    &bridge::CharacterAdvancedOverrides::preferredCombatRange);
                CreateOverrideSlider(cover_, 0.0f, 1.0f, 0.5f, 1000.0f,
                    "AI Cover Preference", "COVER PREFERENCE",
                    &bridge::CharacterAdvancedOverrides::coverPreference);
                CreateOverrideSlider(retreat_, 0.0f, 1.0f, 0.25f, 1000.0f,
                    "AI Retreat Health", "RETREAT HEALTH",
                    &bridge::CharacterAdvancedOverrides::retreatHealthThreshold);
                CreateOverrideSlider(pursuit_, 0.0f, 60.0f, 10.0f, 1200.0f,
                    "AI Pursuit Seconds", "PURSUIT DURATION",
                    &bridge::CharacterAdvancedOverrides::pursuitSeconds);
                CreateOverrideSlider(suppression_, 0.0f, 1.0f, 0.5f, 1000.0f,
                    "AI Suppression Tolerance", "SUPPRESSION TOLERANCE",
                    &bridge::CharacterAdvancedOverrides::suppressionTolerance);

                CreateGroupLabel(communicationGroup_, "AI Communication Group", "COMMUNICATION");
                canCommunicate_.Create("Character Can Communicate");
                canCommunicate_.SetTooltip(
                    "Allow later squad/alert gates to communicate legitimate perceived information.");
                canCommunicate_.OnClick([this](const wi::gui::EventArgs& args)
                {
                    const bool value = args.bValue;
                    Commit([value](bridge::CharacterAuthoringSettings& settings)
                    { settings.canCommunicate = value; },
                    value ? "communication enabled" : "communication disabled");
                });
                panel_->AddWidget(&canCommunicate_);
                CreateOverrideSlider(communication_, 0.0f, 100.0f, 25.0f, 1000.0f,
                    "AI Communication Range", "COMMUNICATION RANGE",
                    &bridge::CharacterAdvancedOverrides::communicationRange);
            }

            void CreateLabel(wi::gui::Label& label, const char* name)
            {
                label.Create(name);
                label.SetColor(wi::Color::Transparent());
                label.SetFitTextEnabled(true);
                panel_->AddWidget(&label);
            }

            void CreateGroupLabel(
                wi::gui::Label& label,
                const char* name,
                const char* text)
            {
                CreateLabel(label, name);
                label.SetText(text);
            }

            void CreateOverrideSlider(
                SceneInspectorSlider& slider,
                const float minimum,
                const float maximum,
                const float defaultValue,
                const float steps,
                const char* name,
                const char* label,
                const OverrideMember member)
            {
                slider.Create(minimum, maximum, defaultValue, steps, name, label);
                slider.SetTooltip(
                    "Effective profile value. Committing a change creates an explicit per-Character override.");
                slider.OnValueCommitted([this, member](const float value)
                {
                    CommitOverride(member, value);
                });
                panel_->AddWidget(&slider);
            }

            [[nodiscard]] std::vector<wi::gui::Widget*> Widgets()
            {
                return {
                    &header_, &status_, &setup_, &effective_, &makeRemove_, &type_, &role_,
                    &personality_, &faction_, &skill_, &awareness_, &animationSetLabel_,
                    &animationSet_, &autonomous_, &advancedHeader_, &advancedStatus_,
                    &resetAdvanced_, &customFactionLabel_, &customFaction_,
                    &perceptionGroup_, &vision_, &horizontalFov_, &hearing_, &reaction_,
                    &memoryGroup_, &memory_, &suspicionDecay_, &search_,
                    &personalityGroup_, &aggression_, &courage_, &curiosity_, &alertness_,
                    &loyalty_, &combatGroup_, &accuracy_, &preferredRange_, &cover_,
                    &retreat_, &pursuit_, &suppression_, &communicationGroup_,
                    &canCommunicate_, &communication_};
            }

            [[nodiscard]] std::vector<wi::gui::Widget*> AdvancedTuningWidgets()
            {
                return {
                    &vision_, &horizontalFov_, &hearing_, &reaction_, &memory_,
                    &suspicionDecay_, &search_, &aggression_, &courage_, &curiosity_,
                    &alertness_, &loyalty_, &accuracy_, &preferredRange_, &cover_,
                    &retreat_, &pursuit_, &suppression_, &communication_};
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
                skill_.SetEnabled(enabled);
                awareness_.SetEnabled(enabled);
                animationSet_.SetEnabled(enabled);
                autonomous_.SetEnabled(enabled);
                animationSetLabel_.SetText(
                    enabled ? "ANIMATION SET // stable governed asset ID (optional)" :
                    "ANIMATION SET // available after MAKE CHARACTER");
            }

            void SetAdvancedTuningEnabled(const bool enabled)
            {
                for (auto* widget : AdvancedTuningWidgets())
                    widget->SetEnabled(enabled);
            }

            void ApplyBasicControlValues(const bridge::CharacterAuthoringSettings& settings)
            {
                type_.SetSelectedByUserdataWithoutCallback(
                    static_cast<std::uint64_t>(settings.type));
                role_.SetSelectedByUserdataWithoutCallback(
                    static_cast<std::uint64_t>(settings.role));
                personality_.SetSelectedByUserdataWithoutCallback(
                    static_cast<std::uint64_t>(settings.personality));
                faction_.SetSelectedByUserdataWithoutCallback(FactionIndex(settings.factionId));
                skill_.SetSelectedByUserdataWithoutCallback(
                    static_cast<std::uint64_t>(settings.skill));
                awareness_.SetSelectedByUserdataWithoutCallback(
                    static_cast<std::uint64_t>(settings.awareness));
                animationDraft_ = settings.animationSetId;
                animationSet_.SetValue(animationDraft_);
                autonomous_.SetCheck(settings.autonomous);
            }

            void ApplyAdvancedSliderValues(const bridge::CharacterTuning& tuning)
            {
                vision_.SetValue(tuning.visionDistance);
                horizontalFov_.SetValue(tuning.horizontalFovDegrees);
                hearing_.SetValue(tuning.hearingSensitivity);
                reaction_.SetValue(tuning.visualReactionSeconds);
                memory_.SetValue(tuning.memorySeconds);
                suspicionDecay_.SetValue(tuning.suspicionDecay);
                search_.SetValue(tuning.searchSeconds);
                aggression_.SetValue(tuning.aggression);
                courage_.SetValue(tuning.courage);
                curiosity_.SetValue(tuning.curiosity);
                alertness_.SetValue(tuning.alertness);
                loyalty_.SetValue(tuning.loyalty);
                accuracy_.SetValue(tuning.accuracy);
                preferredRange_.SetValue(tuning.preferredCombatRange);
                cover_.SetValue(tuning.coverPreference);
                retreat_.SetValue(tuning.retreatHealthThreshold);
                pursuit_.SetValue(tuning.pursuitSeconds);
                suppression_.SetValue(tuning.suppressionTolerance);
                communication_.SetValue(tuning.communicationRange);
            }

            static std::uint64_t FactionIndex(const std::string& faction) noexcept
            {
                if (faction == "Player") return 0;
                if (faction == "Friendly") return 1;
                if (faction == "Enemy") return 2;
                if (faction == "Civilian") return 3;
                if (faction == "Wildlife") return 4;
                if (faction == "Neutral") return 5;
                return 6;
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
                        ? "AI-02 // Character authoring removed"
                        : "AI-02 // Character promoted with native Wicked controller");
                }
                else
                {
                    SetStatus("AI-02 // Character operation was rejected");
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
                    SetStatus(std::string("AI-02 // ") + message);
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
                    SetStatus("AI-02 // " + validation);
                    return;
                }
                if (session->Commands().Execute(
                    std::make_unique<bridge::SetCharacterSettingsCommand>(
                        scene, selected_, after)))
                {
                    SetStatus("AI-02 // animation set reference updated");
                    RequestRefresh();
                }
            }

            void CommitCustomFaction()
            {
                std::string validation;
                if (!bridge::ValidateFactionId(factionDraft_, validation))
                {
                    SetStatus("AI-02 // " + validation);
                    return;
                }
                const std::string value = factionDraft_;
                Commit([value](bridge::CharacterAuthoringSettings& settings)
                { settings.factionId = value; }, "custom faction updated");
            }

            void CommitOverride(const OverrideMember member, const float value)
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || selected_ == wi::ecs::INVALID_ENTITY)
                    return;
                auto& scene = session->Scenes().GetScene();
                bridge::CharacterAdvancedOverrides overrides;
                std::string error;
                if (!bridge::CaptureCharacterAdvancedOverrides(
                        scene, selected_, overrides, error))
                {
                    SetStatus("AI-02 // invalid advanced payload; RESET ADVANCED OVERRIDES first");
                    return;
                }
                overrides.*member = value;
                if (session->Commands().Execute(
                    std::make_unique<bridge::SetCharacterAdvancedOverridesCommand>(
                        scene, selected_, overrides)))
                {
                    SetStatus("AI-02 // advanced AI override updated");
                    RequestRefresh();
                }
            }

            void ResetAdvanced()
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || selected_ == wi::ecs::INVALID_ENTITY)
                    return;
                auto& scene = session->Scenes().GetScene();
                if (session->Commands().Execute(
                    std::make_unique<bridge::SetCharacterAdvancedOverridesCommand>(
                        scene, selected_, bridge::CharacterAdvancedOverrides{})))
                {
                    SetStatus("AI-02 // advanced overrides reset to profile values");
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

            wi::gui::Window* panel_ = nullptr;
            InspectorSectionRegistry* registry_ = nullptr;
            std::function<void()> requestRefresh_;
            std::function<void(std::string)> setStatus_;
            wi::ecs::Entity selected_ = wi::ecs::INVALID_ENTITY;
            bool isCharacter_ = false;
            std::string animationDraft_;
            std::string factionDraft_;

            SceneInspectorButton header_;
            wi::gui::Label status_;
            wi::gui::Label setup_;
            wi::gui::Label effective_;
            SceneInspectorButton makeRemove_;
            SceneInspectorComboBox type_;
            SceneInspectorComboBox role_;
            SceneInspectorComboBox personality_;
            SceneInspectorComboBox faction_;
            SceneInspectorComboBox skill_;
            SceneInspectorComboBox awareness_;
            wi::gui::Label animationSetLabel_;
            SceneInspectorTextInputField animationSet_;
            SceneInspectorCheckBox autonomous_;

            SceneInspectorButton advancedHeader_;
            wi::gui::Label advancedStatus_;
            SceneInspectorButton resetAdvanced_;
            wi::gui::Label customFactionLabel_;
            SceneInspectorTextInputField customFaction_;
            wi::gui::Label perceptionGroup_;
            SceneInspectorSlider vision_;
            SceneInspectorSlider horizontalFov_;
            SceneInspectorSlider hearing_;
            SceneInspectorSlider reaction_;
            wi::gui::Label memoryGroup_;
            SceneInspectorSlider memory_;
            SceneInspectorSlider suspicionDecay_;
            SceneInspectorSlider search_;
            wi::gui::Label personalityGroup_;
            SceneInspectorSlider aggression_;
            SceneInspectorSlider courage_;
            SceneInspectorSlider curiosity_;
            SceneInspectorSlider alertness_;
            SceneInspectorSlider loyalty_;
            wi::gui::Label combatGroup_;
            SceneInspectorSlider accuracy_;
            SceneInspectorSlider preferredRange_;
            SceneInspectorSlider cover_;
            SceneInspectorSlider retreat_;
            SceneInspectorSlider pursuit_;
            SceneInspectorSlider suppression_;
            wi::gui::Label communicationGroup_;
            SceneInspectorCheckBox canCommunicate_;
            SceneInspectorSlider communication_;
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

        bool CharacterAdvancedSectionProvider::IsVisible(
            const InspectorSectionContext& context) const
        {
            return owner_ != nullptr && owner_->IsAdvancedVisible(context);
        }

        float CharacterAdvancedSectionProvider::MeasureContentHeight(
            const InspectorSectionContext&,
            float) const
        {
            return owner_ != nullptr ? owner_->MeasureAdvanced() : 0.0f;
        }

        void CharacterAdvancedSectionProvider::Refresh(const InspectorSectionContext&)
        {
            if (owner_ != nullptr)
                owner_->RefreshAdvanced();
        }

        void CharacterAdvancedSectionProvider::ApplyLayout(
            const InspectorSectionContext&,
            const InspectorSectionLayout& layout)
        {
            if (owner_ != nullptr)
                owner_->LayoutAdvanced(layout);
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
