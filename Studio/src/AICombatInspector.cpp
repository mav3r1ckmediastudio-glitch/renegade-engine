#include "AICombatInspector.h"

#include "InspectorSectionFramework.h"
#include "RenegadeStudioChrome.h"
#include "StudioApplication.h"

#include "renegade/bridge/CharacterService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/StudioSession.h"
#include "renegade/bridge/WeaponCombatService.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace renegade::studio
{
    namespace
    {
        constexpr std::uint64_t IntrinsicMeleeWeaponSelection =
            std::numeric_limits<std::uint64_t>::max() - 1u;

        class CombatInspector;

        class CombatSectionProvider final : public IInspectorSectionProvider
        {
        public:
            explicit CombatSectionProvider(CombatInspector& owner) noexcept
                : owner_(&owner)
            {
                descriptor_.id = AICombatSectionId;
                descriptor_.title = "COMBAT";
                descriptor_.order = 21;
                descriptor_.defaultExpanded = true;
                descriptor_.headerHeight = 28.0f;
                descriptor_.spacingAfter = 6.0f;
            }

            [[nodiscard]] const InspectorSectionDescriptor& Descriptor() const noexcept override
            {
                return descriptor_;
            }
            [[nodiscard]] bool IsVisible(const InspectorSectionContext&) const override;
            [[nodiscard]] float MeasureContentHeight(
                const InspectorSectionContext&, float) const override;
            void Refresh(const InspectorSectionContext&) override;
            void ApplyLayout(
                const InspectorSectionContext&,
                const InspectorSectionLayout& layout) override;

        private:
            CombatInspector* owner_ = nullptr;
            InspectorSectionDescriptor descriptor_;
        };

        class CombatInspector final
        {
        public:
            CombatInspector(
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
                header_.Create("AI Combat Section Header");
                header_.SetTooltip(
                    "Configure ordinary Character combat without scripts. Animation presentation is owned by AI-06.");
                header_.OnClick([this](const wi::gui::EventArgs&)
                {
                    (void)registry_->ToggleExpanded(AICombatSectionId);
                    RequestRefresh();
                });
                panel_->AddWidget(&header_);

                status_.Create("AI Combat Status");
                status_.SetColor(wi::Color::Transparent());
                status_.SetFitTextEnabled(true);
                panel_->AddWidget(&status_);

                style_.Create("AI Combat Style");
                style_.AddItem("NONE", static_cast<std::uint64_t>(bridge::CombatStyle::None));
                style_.AddItem("MELEE", static_cast<std::uint64_t>(bridge::CombatStyle::Melee));
                style_.AddItem("RANGED", static_cast<std::uint64_t>(bridge::CombatStyle::Ranged));
                style_.AddItem("MIXED", static_cast<std::uint64_t>(bridge::CombatStyle::Mixed));
                style_.AddItem("CUSTOM", static_cast<std::uint64_t>(bridge::CombatStyle::Custom));
                style_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    AssignCombatStyle(static_cast<bridge::CombatStyle>(args.userdata));
                });
                panel_->AddWidget(&style_);

                weaponLabel_.Create("AI Combat Weapon Label");
                weaponLabel_.SetColor(wi::Color::Transparent());
                weaponLabel_.SetFitTextEnabled(true);
                weaponLabel_.SetText("WEAPON / ATTACK // built-in melee or governed entity");
                panel_->AddWidget(&weaponLabel_);

                weapon_.Create("AI Combat Weapon Selector");
                weapon_.SetTooltip(
                    "Choose FISTS / CLAWS / TEETH for intrinsic melee, or assign an existing governed weapon entity. Enemy Characters should always have an attack selection.");
                weapon_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    if (args.userdata == IntrinsicMeleeWeaponSelection)
                    {
                        AssignIntrinsicMelee();
                        return;
                    }
                    AssignWeapon(static_cast<wi::ecs::Entity>(args.userdata));
                });
                panel_->AddWidget(&weapon_);

                canFlee_.Create("AI Combat Can Flee");
                canFlee_.SetText("CAN FLEE");
                canFlee_.OnClick([this](const wi::gui::EventArgs& args)
                {
                    const bool value = args.bValue;
                    Commit([value](bridge::CharacterAuthoringSettings& settings)
                    {
                        settings.canFlee = value;
                    }, value ? "flee behavior enabled" : "flee behavior disabled");
                });
                panel_->AddWidget(&canFlee_);

                canSurrender_.Create("AI Combat Can Surrender");
                canSurrender_.SetText("CAN SURRENDER");
                canSurrender_.OnClick([this](const wi::gui::EventArgs& args)
                {
                    const bool value = args.bValue;
                    Commit([value](bridge::CharacterAuthoringSettings& settings)
                    {
                        settings.canSurrender = value;
                    }, value ? "surrender behavior enabled" : "surrender behavior disabled");
                });
                panel_->AddWidget(&canSurrender_);

                descriptor_.Create("AI Combat Descriptor Summary");
                descriptor_.SetColor(wi::Color::Transparent());
                descriptor_.SetFitTextEnabled(true);
                panel_->AddWidget(&descriptor_);

                std::string error;
                if (!registry_->Register(
                        std::make_shared<CombatSectionProvider>(*this), error))
                {
                    SetStatus("AI-05 // " + error);
                }
            }

            [[nodiscard]] bool IsVisible()
            {
                ResolveSelection();
                return selectedCharacter_ != wi::ecs::INVALID_ENTITY;
            }

            [[nodiscard]] float Measure() const noexcept
            {
                return 230.0f;
            }

            void Refresh()
            {
                ResolveSelection();
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || selectedCharacter_ == wi::ecs::INVALID_ENTITY)
                    return;
                auto& scene = session->Scenes().GetScene();
                const auto settings = bridge::CaptureCharacterSettings(
                    scene, selectedCharacter_);

                style_.SetSelectedByUserdataWithoutCallback(
                    static_cast<std::uint64_t>(settings.combatStyle));
                canFlee_.SetCheck(settings.canFlee);
                canSurrender_.SetCheck(settings.canSurrender);

                weapon_.ClearItems();
                weapon_.AddItem(
                    "NONE / NO ATTACK",
                    static_cast<std::uint64_t>(wi::ecs::INVALID_ENTITY));
                weapon_.AddItem(
                    "FISTS / CLAWS / TEETH // MELEE",
                    IntrinsicMeleeWeaponSelection);

                wi::ecs::Entity selectedWeapon = wi::ecs::INVALID_ENTITY;
                for (std::size_t index = 0; index < scene.transforms.GetCount(); ++index)
                {
                    const wi::ecs::Entity entity = scene.transforms.GetEntity(index);
                    if (entity == selectedCharacter_ || bridge::IsRenegadeCharacter(scene, entity))
                        continue;
                    const bridge::StableId stableId = bridge::PersistentEntityId(scene, entity);
                    if (!bridge::IsValidStableId(stableId))
                        continue;
                    const auto* name = scene.names.GetComponent(entity);
                    const std::string label = name != nullptr && !name->name.empty()
                        ? name->name
                        : std::string("Entity ") + std::to_string(entity);
                    weapon_.AddItem(label, static_cast<std::uint64_t>(entity));
                    if (stableId == settings.weaponEntityId)
                        selectedWeapon = entity;
                }

                const bool intrinsicMelee =
                    settings.weaponEntityId.empty() &&
                    settings.combatStyle == bridge::CombatStyle::Melee;
                if (intrinsicMelee)
                {
                    weapon_.SetSelectedByUserdataWithoutCallback(
                        IntrinsicMeleeWeaponSelection);
                }
                else
                {
                    weapon_.SetSelectedByUserdataWithoutCallback(
                        static_cast<std::uint64_t>(selectedWeapon));
                }

                bridge::WeaponAiDescriptor weaponDescriptor;
                std::string error;
                if (!bridge::CaptureWeaponAiDescriptor(
                        scene,
                        selectedWeapon,
                        settings.combatStyle,
                        weaponDescriptor,
                        error))
                {
                    status_.SetText("INVALID WEAPON AI // " + error);
                    descriptor_.SetText("Combat is fail-closed until the Weapon AI payload is repaired.");
                    return;
                }

                bridge::CharacterAdvancedOverrides overrides;
                if (!bridge::CaptureCharacterAdvancedOverrides(
                        scene, selectedCharacter_, overrides, error))
                {
                    status_.SetText("INVALID ADVANCED AI // " + error);
                    descriptor_.SetText("Combat range cannot resolve until Advanced AI is repaired.");
                    return;
                }
                const auto tuning = bridge::ResolveCharacterTuning(settings, overrides);
                const auto effectiveRange = bridge::ResolveEffectiveWeaponAiRange(
                    tuning, weaponDescriptor);

                const bool enemyNeedsAttack =
                    settings.factionId == "Enemy" &&
                    settings.weaponEntityId.empty() &&
                    !intrinsicMelee;
                if (enemyNeedsAttack)
                {
                    status_.SetText(
                        "ENEMY NEEDS WEAPON // choose FISTS / CLAWS / TEETH or assign a weapon");
                }
                else if (intrinsicMelee)
                {
                    status_.SetText("Intrinsic melee equipped // fists / claws / teeth");
                }
                else if (!settings.weaponEntityId.empty())
                {
                    status_.SetText("Weapon assigned by stable identity");
                }
                else
                {
                    status_.SetText("No attack equipped");
                }

                descriptor_.SetText(
                    "EFFECTIVE RANGE " + std::to_string(effectiveRange.minRange) + " / " +
                    std::to_string(effectiveRange.preferredRange) + " / " +
                    std::to_string(effectiveRange.maxRange) + "m   WEAPON CAP " +
                    std::to_string(weaponDescriptor.maxRange) + "m   DAMAGE " +
                    std::to_string(weaponDescriptor.damage) + "   AMMO " +
                    std::to_string(weaponDescriptor.magazineSize) + "+" +
                    std::to_string(weaponDescriptor.reserveAmmo));
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
                    std::string(layout.expanded ? "▼  " : "▶  ") + "COMBAT");
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
                place(status_, 34.0f);
                place(style_);
                place(weaponLabel_, 20.0f);
                place(weapon_);
                const float half = (width - 8.0f) * 0.5f;
                canFlee_.SetVisible(true);
                canFlee_.SetPos(XMFLOAT2(x, y));
                canFlee_.SetSize(XMFLOAT2(half, 28.0f));
                canSurrender_.SetVisible(true);
                canSurrender_.SetPos(XMFLOAT2(x + half + 8.0f, y));
                canSurrender_.SetSize(XMFLOAT2(half, 28.0f));
                y += 34.0f;
                place(descriptor_, 42.0f);
            }

        private:
            [[nodiscard]] std::vector<wi::gui::Widget*> Widgets()
            {
                return {
                    &header_, &status_, &style_, &weaponLabel_, &weapon_,
                    &canFlee_, &canSurrender_, &descriptor_};
            }

            void ResolveSelection()
            {
                selectedCharacter_ = wi::ecs::INVALID_ENTITY;
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || !session->Selection().HasSelection())
                    return;
                const auto selected = session->Selection().SelectedEntity();
                if (bridge::IsRenegadeCharacter(session->Scenes().GetScene(), selected))
                    selectedCharacter_ = selected;
            }

            void AssignCombatStyle(const bridge::CombatStyle value)
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || selectedCharacter_ == wi::ecs::INVALID_ENTITY)
                    return;
                auto after = bridge::CaptureCharacterSettings(
                    session->Scenes().GetScene(), selectedCharacter_);
                if (after.weaponEntityId.empty() &&
                    value != bridge::CombatStyle::None &&
                    value != bridge::CombatStyle::Melee)
                {
                    SetStatus(
                        "AI-05 // choose an actual weapon before RANGED / MIXED / CUSTOM");
                    RequestRefresh();
                    return;
                }
                after.combatStyle = value;
                CommitSettings(
                    std::move(after),
                    value == bridge::CombatStyle::Melee
                        ? "intrinsic melee selected"
                        : "combat style updated");
            }

            void AssignIntrinsicMelee()
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || selectedCharacter_ == wi::ecs::INVALID_ENTITY)
                    return;
                auto after = bridge::CaptureCharacterSettings(
                    session->Scenes().GetScene(), selectedCharacter_);
                after.weaponEntityId.clear();
                after.combatStyle = bridge::CombatStyle::Melee;
                CommitSettings(
                    std::move(after),
                    "FISTS / CLAWS / TEETH equipped as intrinsic melee");
            }

            void AssignWeapon(const wi::ecs::Entity entity)
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || selectedCharacter_ == wi::ecs::INVALID_ENTITY)
                    return;
                auto& scene = session->Scenes().GetScene();
                auto after = bridge::CaptureCharacterSettings(scene, selectedCharacter_);
                after.weaponEntityId = entity == wi::ecs::INVALID_ENTITY
                    ? bridge::StableId{}
                    : bridge::PersistentEntityId(scene, entity);
                if (entity != wi::ecs::INVALID_ENTITY &&
                    !bridge::IsValidStableId(after.weaponEntityId))
                {
                    SetStatus("AI-05 // Weapon requires a persistent Renegade identity");
                    return;
                }
                if (entity == wi::ecs::INVALID_ENTITY)
                    after.combatStyle = bridge::CombatStyle::None;
                CommitSettings(std::move(after), entity == wi::ecs::INVALID_ENTITY
                    ? "weapon cleared; no attack equipped"
                    : "weapon assigned by stable identity");
            }

            template<typename Mutator>
            void Commit(Mutator&& mutator, const std::string& message)
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || selectedCharacter_ == wi::ecs::INVALID_ENTITY)
                    return;
                auto after = bridge::CaptureCharacterSettings(
                    session->Scenes().GetScene(), selectedCharacter_);
                mutator(after);
                CommitSettings(std::move(after), message);
            }

            void CommitSettings(
                bridge::CharacterAuthoringSettings after,
                const std::string& message)
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || selectedCharacter_ == wi::ecs::INVALID_ENTITY)
                    return;
                std::string validation;
                if (!bridge::ValidateCharacterSettings(after, validation))
                {
                    SetStatus("AI-05 // " + validation);
                    return;
                }
                if (session->Commands().Execute(
                        std::make_unique<bridge::SetCharacterSettingsCommand>(
                            session->Scenes().GetScene(), selectedCharacter_, std::move(after))))
                {
                    SetStatus("AI-05 // " + message);
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
            SceneInspectorButton header_;
            wi::gui::Label status_;
            SceneInspectorComboBox style_;
            wi::gui::Label weaponLabel_;
            SceneInspectorComboBox weapon_;
            SceneInspectorCheckBox canFlee_;
            SceneInspectorCheckBox canSurrender_;
            wi::gui::Label descriptor_;
        };

        [[nodiscard]] bool CombatSectionProvider::IsVisible(
            const InspectorSectionContext&) const
        {
            return owner_ != nullptr && owner_->IsVisible();
        }

        [[nodiscard]] float CombatSectionProvider::MeasureContentHeight(
            const InspectorSectionContext&, const float) const
        {
            return owner_ != nullptr ? owner_->Measure() : 0.0f;
        }

        void CombatSectionProvider::Refresh(const InspectorSectionContext&)
        {
            if (owner_ != nullptr)
                owner_->Refresh();
        }

        void CombatSectionProvider::ApplyLayout(
            const InspectorSectionContext&,
            const InspectorSectionLayout& layout)
        {
            if (owner_ != nullptr)
                owner_->Layout(layout);
        }

        std::unique_ptr<CombatInspector> activeInspector;
        StudioRenderPath* activeOwner = nullptr;
    }

    void RegisterAICombatInspector(
        StudioRenderPath& owner,
        wi::gui::Window& inspectorPanel,
        InspectorSectionRegistry& registry,
        std::function<void()> requestRefresh,
        std::function<void(std::string)> setStatus)
    {
        activeInspector = std::make_unique<CombatInspector>(
            inspectorPanel,
            registry,
            std::move(requestRefresh),
            std::move(setStatus));
        activeOwner = &owner;
        activeInspector->Register();
    }

    void PrepareAICombatInspector(StudioRenderPath& owner)
    {
        if (activeOwner == &owner && activeInspector != nullptr)
            activeInspector->PrepareForLayout();
    }
}
