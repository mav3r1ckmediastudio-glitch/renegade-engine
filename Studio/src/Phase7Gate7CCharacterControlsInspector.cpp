#include "Phase7Gate7CCharacterControlsInspector.h"

#include "InspectorSectionFramework.h"
#include "Phase7Gate7AAnimationInspector.h"
#include "Phase7Gate7BHumanoidRetargetInspector.h"
#include "RenegadeStudioChrome.h"
#include "S4BScriptAttachmentInspector.h"
#include "S4DGlobalScriptInspector.h"
#include "StudioApplication.h"

#include "renegade/bridge/CharacterAnimationControlService.h"
#include "renegade/bridge/HumanoidRetargetService.h"
#include "renegade/bridge/StudioSession.h"

#include <algorithm>
#include <cmath>
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
        std::string EntityName(const wi::scene::Scene& scene, const wi::ecs::Entity entity)
        {
            if (const auto* name = scene.names.GetComponent(entity);
                name != nullptr && !name->name.empty())
            {
                return name->name;
            }
            return std::string("Entity ") + std::to_string(entity);
        }

        void PopulateTransformTargets(
            SceneInspectorComboBox& combo,
            const wi::scene::Scene& scene,
            const wi::ecs::Entity selected)
        {
            combo.ClearItems();
            combo.AddItem("NO TARGET", static_cast<std::uint64_t>(wi::ecs::INVALID_ENTITY));
            for (std::size_t i = 0; i < scene.transforms.GetCount(); ++i)
            {
                const auto entity = scene.transforms.GetEntity(i);
                combo.AddItem(EntityName(scene, entity), static_cast<std::uint64_t>(entity));
            }
            combo.SetSelectedByUserdataWithoutCallback(static_cast<std::uint64_t>(selected));
        }

        class CharacterControlsInspector;

        class CharacterControlsSectionProvider final : public IInspectorSectionProvider
        {
        public:
            explicit CharacterControlsSectionProvider(CharacterControlsInspector& owner) noexcept
                : owner_(&owner)
            {
                descriptor_.id = Phase7CharacterControlsSectionId;
                descriptor_.title = "IK / LOOK-AT / EXPRESSIONS";
                descriptor_.order = 36;
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
            CharacterControlsInspector* owner_ = nullptr;
            InspectorSectionDescriptor descriptor_;
        };

        class CharacterControlsInspector final
        {
        public:
            CharacterControlsInspector(
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
                header_.Create("Phase 7 Character Controls Section Header");
                header_.SetTooltip(
                    "Wicked-native IK, humanoid head/eye look-at, blinking, talking and expression weights.");
                header_.OnClick([this](const wi::gui::EventArgs&)
                {
                    const bool opening = !registry_->IsExpanded(Phase7CharacterControlsSectionId);
                    for (const char* sectionId : {
                        "transform", "rendering", "materials",
                        S4BActionSectionId, S4BScriptSectionId, S4DGlobalScriptSectionId,
                        Phase7HumanoidRetargetSectionId, Phase7AnimationSectionId,
                        Phase7CharacterControlsSectionId})
                    {
                        (void)registry_->SetExpanded(sectionId, false);
                    }
                    if (opening)
                        (void)registry_->SetExpanded(Phase7CharacterControlsSectionId, true);
                    RequestRefresh();
                });
                panel_->AddWidget(&header_);

                CreateLabel(status_, "Phase 7 Character Controls Status");
                CreateLabel(ikLabel_, "Phase 7 IK Label");
                CreateLabel(lookAtLabel_, "Phase 7 Look At Label");
                CreateLabel(expressionLabel_, "Phase 7 Expression Label");

                ikBone_.Create("IK Bone");
                ikBone_.SetTooltip("Choose the armature bone that owns Wicked's InverseKinematicsComponent.");
                ikBone_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    ikEntity_ = static_cast<wi::ecs::Entity>(args.userdata);
                    RefreshIkControls();
                    RequestRefresh();
                });
                panel_->AddWidget(&ikBone_);

                CreateButton(ikComponent_, "IK Component", "ADD IK", [this]() { ToggleIkComponent(); });

                ikTarget_.Create("IK Target");
                ikTarget_.SetTooltip("Transform entity followed by Wicked's native IK solver.");
                ikTarget_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    const auto target = static_cast<wi::ecs::Entity>(args.userdata);
                    CommitIk([target](bridge::InverseKinematicsState& state) { state.target = target; },
                        "IK target updated");
                });
                panel_->AddWidget(&ikTarget_);

                ikDisabled_.Create("IK Disabled");
                ikDisabled_.SetTooltip("Disable native IK simulation without removing its authored settings.");
                ikDisabled_.OnClick([this](const wi::gui::EventArgs& args)
                {
                    const bool value = args.bValue;
                    CommitIk([value](bridge::InverseKinematicsState& state) { state.disabled = value; },
                        value ? "IK disabled" : "IK enabled");
                });
                panel_->AddWidget(&ikDisabled_);

                CreateSlider(
                    ikChainLength_, 0.0f, 10.0f, 0.0f, 10.0f,
                    "IK Chain Length", "CHAIN LENGTH",
                    [this](const float value)
                    {
                        const auto chain = static_cast<std::uint32_t>(
                            std::clamp(std::lround(value), 0l, 10l));
                        CommitIk([chain](bridge::InverseKinematicsState& state) { state.chainLength = chain; },
                            "IK chain length updated");
                    });

                CreateSlider(
                    ikIterations_, 0.0f, 10.0f, 1.0f, 10.0f,
                    "IK Iterations", "ITERATIONS",
                    [this](const float value)
                    {
                        const auto iterations = static_cast<std::uint32_t>(
                            std::clamp(std::lround(value), 0l, 10l));
                        CommitIk([iterations](bridge::InverseKinematicsState& state)
                            { state.iterationCount = iterations; },
                            "IK iteration count updated");
                    });

                lookAtEnabled_.Create("Look At Enabled");
                lookAtEnabled_.OnClick([this](const wi::gui::EventArgs& args)
                {
                    const bool value = args.bValue;
                    CommitLookAt([value](bridge::HumanoidLookAtState& state) { state.enabled = value; },
                        value ? "humanoid look-at enabled" : "humanoid look-at disabled");
                });
                panel_->AddWidget(&lookAtEnabled_);

                lookAtTarget_.Create("Look At Entity");
                lookAtTarget_.SetTooltip("Persistent Transform target for Wicked humanoid head and eye look-at.");
                lookAtTarget_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    const auto target = static_cast<wi::ecs::Entity>(args.userdata);
                    CommitLookAt([target](bridge::HumanoidLookAtState& state) { state.target = target; },
                        "look-at target updated");
                });
                panel_->AddWidget(&lookAtTarget_);

                CreateSlider(
                    headHorizontal_, 0.0f, 90.0f, 60.0f, 180.0f,
                    "Head Horizontal", "HEAD HORIZONTAL °",
                    [this](const float value)
                    {
                        CommitLookAt([value](bridge::HumanoidLookAtState& state)
                            { state.headRotationMax.x = wi::math::DegreesToRadians(value); },
                            "head horizontal limit updated");
                    });
                CreateSlider(
                    headVertical_, 0.0f, 60.0f, 30.0f, 120.0f,
                    "Head Vertical", "HEAD VERTICAL °",
                    [this](const float value)
                    {
                        CommitLookAt([value](bridge::HumanoidLookAtState& state)
                            { state.headRotationMax.y = wi::math::DegreesToRadians(value); },
                            "head vertical limit updated");
                    });
                CreateSlider(
                    headSpeed_, 0.05f, 1.0f, 0.1f, 1000.0f,
                    "Head Speed", "HEAD SPEED",
                    [this](const float value)
                    {
                        CommitLookAt([value](bridge::HumanoidLookAtState& state)
                            { state.headRotationSpeed = value; },
                            "head look-at speed updated");
                    });
                CreateSlider(
                    eyeHorizontal_, 0.0f, 40.0f, 20.0f, 80.0f,
                    "Eye Horizontal", "EYE HORIZONTAL °",
                    [this](const float value)
                    {
                        CommitLookAt([value](bridge::HumanoidLookAtState& state)
                            { state.eyeRotationMax.x = wi::math::DegreesToRadians(value); },
                            "eye horizontal limit updated");
                    });
                CreateSlider(
                    eyeVertical_, 0.0f, 30.0f, 15.0f, 60.0f,
                    "Eye Vertical", "EYE VERTICAL °",
                    [this](const float value)
                    {
                        CommitLookAt([value](bridge::HumanoidLookAtState& state)
                            { state.eyeRotationMax.y = wi::math::DegreesToRadians(value); },
                            "eye vertical limit updated");
                    });
                CreateSlider(
                    eyeSpeed_, 0.05f, 1.0f, 0.1f, 1000.0f,
                    "Eye Speed", "EYE SPEED",
                    [this](const float value)
                    {
                        CommitLookAt([value](bridge::HumanoidLookAtState& state)
                            { state.eyeRotationSpeed = value; },
                            "eye look-at speed updated");
                    });

                forceTalking_.Create("Force Talking");
                forceTalking_.SetTooltip("Force continuous native talking animation even when no voice is playing.");
                forceTalking_.OnClick([this](const wi::gui::EventArgs& args)
                {
                    const bool value = args.bValue;
                    CommitExpressionMaster([value](bridge::ExpressionMasterState& state)
                        { state.forceTalking = value; },
                        value ? "force talking enabled" : "force talking disabled");
                });
                panel_->AddWidget(&forceTalking_);

                CreateSlider(
                    blinkFrequency_, 0.0f, 1.0f, 0.3f, 1000.0f,
                    "Blink Frequency", "BLINKS / SEC",
                    [this](const float value)
                    {
                        CommitExpressionMaster([value](bridge::ExpressionMasterState& state)
                            { state.blinkFrequency = value; }, "blink frequency updated");
                    });
                CreateSlider(
                    blinkLength_, 0.0f, 1.0f, 0.1f, 1000.0f,
                    "Blink Length", "BLINK LENGTH",
                    [this](const float value)
                    {
                        CommitExpressionMaster([value](bridge::ExpressionMasterState& state)
                            { state.blinkLength = value; }, "blink length updated");
                    });
                CreateSlider(
                    blinkCount_, 1.0f, 4.0f, 2.0f, 3.0f,
                    "Blink Count", "BLINK COUNT",
                    [this](const float value)
                    {
                        const int count = static_cast<int>(std::clamp(std::lround(value), 1l, 4l));
                        CommitExpressionMaster([count](bridge::ExpressionMasterState& state)
                            { state.blinkCount = count; }, "blink count updated");
                    });
                CreateSlider(
                    lookFrequency_, 0.0f, 1.0f, 0.0f, 1000.0f,
                    "Look Away Frequency", "LOOK-AWAYS / SEC",
                    [this](const float value)
                    {
                        CommitExpressionMaster([value](bridge::ExpressionMasterState& state)
                            { state.lookFrequency = value; }, "look-away frequency updated");
                    });
                CreateSlider(
                    lookLength_, 0.0f, 1.0f, 0.6f, 1000.0f,
                    "Look Away Length", "LOOK-AWAY LENGTH",
                    [this](const float value)
                    {
                        CommitExpressionMaster([value](bridge::ExpressionMasterState& state)
                            { state.lookLength = value; }, "look-away length updated");
                    });

                expressionList_.Create("Expression");
                expressionList_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    selectedExpression_ = static_cast<std::size_t>(args.userdata);
                    RefreshExpressionEntryControls();
                    RequestRefresh();
                });
                panel_->AddWidget(&expressionList_);

                expressionBinary_.Create("Binary Expression");
                expressionBinary_.OnClick([this](const wi::gui::EventArgs& args)
                {
                    const bool value = args.bValue;
                    CommitExpressionEntry([value](bridge::ExpressionEntryState& state)
                        { state.binary = value; }, "expression binary mode updated");
                });
                panel_->AddWidget(&expressionBinary_);

                CreateSlider(
                    expressionWeight_, 0.0f, 1.0f, 0.0f, 1000.0f,
                    "Expression Weight", "WEIGHT",
                    [this](const float value)
                    {
                        CommitExpressionEntry([value](bridge::ExpressionEntryState& state)
                            { state.weight = value; }, "expression weight updated");
                    });

                CreateOverrideCombo(overrideMouth_, "Override Mouth", "MOUTH");
                overrideMouth_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    const auto value = static_cast<bridge::ExpressionOverride>(args.userdata);
                    CommitExpressionEntry([value](bridge::ExpressionEntryState& state)
                        { state.overrideMouth = value; }, "mouth override updated");
                });

                CreateOverrideCombo(overrideBlink_, "Override Blink", "BLINK");
                overrideBlink_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    const auto value = static_cast<bridge::ExpressionOverride>(args.userdata);
                    CommitExpressionEntry([value](bridge::ExpressionEntryState& state)
                        { state.overrideBlink = value; }, "blink override updated");
                });

                CreateOverrideCombo(overrideLook_, "Override Look", "LOOK");
                overrideLook_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    const auto value = static_cast<bridge::ExpressionOverride>(args.userdata);
                    CommitExpressionEntry([value](bridge::ExpressionEntryState& state)
                        { state.overrideLook = value; }, "look override updated");
                });

                std::string error;
                if (!registry_->Register(
                    std::make_shared<CharacterControlsSectionProvider>(*this), error))
                {
                    SetStatus("PHASE 7C // " + error);
                }
            }

            [[nodiscard]] bool IsVisible(const InspectorSectionContext& context)
            {
                if (!context.hasSelection)
                    return false;
                ResolveRig();
                return rigEntity_ != wi::ecs::INVALID_ENTITY;
            }

            [[nodiscard]] float Measure() const noexcept
            {
                return 860.0f;
            }

            void Refresh()
            {
                ResolveRig();
                RefreshStatus();
                RefreshIkControls();
                RefreshLookAtControls();
                RefreshExpressionControls();
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
                    std::string(layout.expanded ? "▼  " : "▶  ") +
                    "IK / LOOK-AT / EXPRESSIONS");
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
                label(ikLabel_);
                row(ikBone_);
                row(ikComponent_);
                row(ikTarget_);
                row(ikDisabled_);
                row(ikChainLength_);
                row(ikIterations_);

                label(lookAtLabel_);
                row(lookAtEnabled_);
                row(lookAtTarget_);
                row(headHorizontal_);
                row(headVertical_);
                row(headSpeed_);
                row(eyeHorizontal_);
                row(eyeVertical_);
                row(eyeSpeed_);

                label(expressionLabel_);
                row(forceTalking_);
                row(blinkFrequency_);
                row(blinkLength_);
                row(blinkCount_);
                row(lookFrequency_);
                row(lookLength_);
                row(expressionList_);
                row(expressionBinary_);
                row(expressionWeight_);
                row(overrideMouth_);
                row(overrideBlink_);
                row(overrideLook_);
            }

        private:
            void CreateLabel(wi::gui::Label& label, const char* name)
            {
                label.Create(name);
                label.SetColor(wi::Color::Transparent());
                label.SetFitTextEnabled(true);
                panel_->AddWidget(&label);
            }

            template<typename Fn>
            void CreateButton(
                SceneInspectorButton& button,
                const char* name,
                const char* text,
                Fn&& fn)
            {
                button.Create(name);
                button.SetText(text);
                button.OnClick(
                    [action = std::forward<Fn>(fn)](const wi::gui::EventArgs&) mutable
                    {
                        action();
                    });
                panel_->AddWidget(&button);
            }

            template<typename Fn>
            void CreateSlider(
                SceneInspectorSlider& slider,
                const float minimum,
                const float maximum,
                const float defaultValue,
                const float steps,
                const char* name,
                const char* label,
                Fn&& onCommit)
            {
                slider.Create(minimum, maximum, defaultValue, steps, name, label);
                slider.OnValueCommitted(std::forward<Fn>(onCommit));
                panel_->AddWidget(&slider);
            }

            void CreateOverrideCombo(
                SceneInspectorComboBox& combo,
                const char* name,
                const char* label)
            {
                combo.Create(name);
                combo.SetTooltip(std::string(label) + " expression automation override.");
                combo.AddItem("NONE", static_cast<std::uint64_t>(bridge::ExpressionOverride::None));
                combo.AddItem("BLOCK", static_cast<std::uint64_t>(bridge::ExpressionOverride::Block));
                combo.AddItem("BLEND", static_cast<std::uint64_t>(bridge::ExpressionOverride::Blend));
                panel_->AddWidget(&combo);
            }

            [[nodiscard]] std::vector<wi::gui::Widget*> Widgets()
            {
                return {
                    &header_, &status_, &ikLabel_, &ikBone_, &ikComponent_, &ikTarget_,
                    &ikDisabled_, &ikChainLength_, &ikIterations_,
                    &lookAtLabel_, &lookAtEnabled_, &lookAtTarget_, &headHorizontal_,
                    &headVertical_, &headSpeed_, &eyeHorizontal_, &eyeVertical_, &eyeSpeed_,
                    &expressionLabel_, &forceTalking_, &blinkFrequency_, &blinkLength_,
                    &blinkCount_, &lookFrequency_, &lookLength_, &expressionList_,
                    &expressionBinary_, &expressionWeight_, &overrideMouth_, &overrideBlink_,
                    &overrideLook_};
            }

            void ResolveRig()
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || !session->Selection().HasSelection())
                {
                    rigEntity_ = wi::ecs::INVALID_ENTITY;
                    expressionEntity_ = wi::ecs::INVALID_ENTITY;
                    return;
                }

                auto& scene = session->Scenes().GetScene();
                rigEntity_ = bridge::FindHumanoidRigEntity(
                    scene, session->Selection().SelectedEntity());
                expressionEntity_ = bridge::FindExpressionEntity(scene, rigEntity_);
            }

            void RefreshStatus()
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || rigEntity_ == wi::ecs::INVALID_ENTITY)
                {
                    status_.SetText("No native humanoid or armature selected.");
                    return;
                }

                const auto& scene = session->Scenes().GetScene();
                const bool humanoid = scene.humanoids.Contains(rigEntity_);
                status_.SetText(
                    humanoid
                    ? "Native Wicked character controls // humanoid ready"
                    : "Native armature selected // complete HUMANOID / RETARGET mapping first");
            }

            void RefreshIkControls()
            {
                ikBone_.ClearItems();
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || rigEntity_ == wi::ecs::INVALID_ENTITY)
                {
                    SetIkControlsEnabled(false);
                    ikLabel_.SetText("IK // no armature");
                    return;
                }

                const auto& scene = session->Scenes().GetScene();
                const auto bones = bridge::CollectArmatureBones(scene, rigEntity_);
                if (bones.empty())
                {
                    SetIkControlsEnabled(false);
                    ikLabel_.SetText("IK // armature has no bones");
                    return;
                }

                if (std::find(bones.begin(), bones.end(), ikEntity_) == bones.end())
                {
                    const auto existing = std::find_if(
                        bones.begin(), bones.end(),
                        [&scene](const wi::ecs::Entity entity)
                        {
                            return scene.inverse_kinematics.Contains(entity);
                        });
                    ikEntity_ = existing != bones.end() ? *existing : bones.front();
                }

                for (const auto bone : bones)
                    ikBone_.AddItem(EntityName(scene, bone), static_cast<std::uint64_t>(bone));
                ikBone_.SetSelectedByUserdataWithoutCallback(static_cast<std::uint64_t>(ikEntity_));

                const auto state = bridge::CaptureInverseKinematicsState(scene, ikEntity_);
                ikLabel_.SetText(
                    state.componentExists
                    ? "IK // native InverseKinematicsComponent active on selected bone"
                    : "IK // selected bone has no InverseKinematicsComponent");
                ikComponent_.SetText(state.componentExists ? "REMOVE IK" : "ADD IK");
                ikComponent_.SetEnabled(true);
                PopulateTransformTargets(ikTarget_, scene, state.target);
                ikDisabled_.SetCheck(state.disabled);
                ikChainLength_.SetValue(static_cast<float>(state.chainLength));
                ikIterations_.SetValue(static_cast<float>(state.iterationCount));
                ikTarget_.SetEnabled(state.componentExists);
                ikDisabled_.SetEnabled(state.componentExists);
                ikChainLength_.SetEnabled(state.componentExists);
                ikIterations_.SetEnabled(state.componentExists);
            }

            void SetIkControlsEnabled(const bool enabled)
            {
                ikBone_.SetEnabled(enabled);
                ikComponent_.SetEnabled(enabled);
                ikTarget_.SetEnabled(enabled);
                ikDisabled_.SetEnabled(enabled);
                ikChainLength_.SetEnabled(enabled);
                ikIterations_.SetEnabled(enabled);
            }

            void RefreshLookAtControls()
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || rigEntity_ == wi::ecs::INVALID_ENTITY)
                {
                    SetLookAtControlsEnabled(false);
                    lookAtLabel_.SetText("LOOK-AT // no humanoid");
                    return;
                }

                const auto& scene = session->Scenes().GetScene();
                const auto state = bridge::CaptureHumanoidLookAtState(scene, rigEntity_);
                lookAtLabel_.SetText(
                    state.componentExists
                    ? "LOOK-AT // native humanoid head + eye solver"
                    : "LOOK-AT // requires HumanoidComponent from Gate 7B");
                SetLookAtControlsEnabled(state.componentExists);
                PopulateTransformTargets(lookAtTarget_, scene, state.target);
                lookAtEnabled_.SetCheck(state.enabled);
                headHorizontal_.SetValue(wi::math::RadiansToDegrees(state.headRotationMax.x));
                headVertical_.SetValue(wi::math::RadiansToDegrees(state.headRotationMax.y));
                headSpeed_.SetValue(state.headRotationSpeed);
                eyeHorizontal_.SetValue(wi::math::RadiansToDegrees(state.eyeRotationMax.x));
                eyeVertical_.SetValue(wi::math::RadiansToDegrees(state.eyeRotationMax.y));
                eyeSpeed_.SetValue(state.eyeRotationSpeed);
            }

            void SetLookAtControlsEnabled(const bool enabled)
            {
                lookAtEnabled_.SetEnabled(enabled);
                lookAtTarget_.SetEnabled(enabled);
                headHorizontal_.SetEnabled(enabled);
                headVertical_.SetEnabled(enabled);
                headSpeed_.SetEnabled(enabled);
                eyeHorizontal_.SetEnabled(enabled);
                eyeVertical_.SetEnabled(enabled);
                eyeSpeed_.SetEnabled(enabled);
            }

            void RefreshExpressionControls()
            {
                expressionList_.ClearItems();
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || rigEntity_ == wi::ecs::INVALID_ENTITY)
                {
                    SetExpressionControlsEnabled(false);
                    expressionLabel_.SetText("EXPRESSIONS // no humanoid");
                    return;
                }

                auto& scene = session->Scenes().GetScene();
                expressionEntity_ = bridge::FindExpressionEntity(scene, rigEntity_);
                const auto state = bridge::CaptureExpressionMasterState(scene, expressionEntity_);
                if (!state.componentExists)
                {
                    SetExpressionControlsEnabled(false);
                    expressionLabel_.SetText(
                        "EXPRESSIONS // no native ExpressionComponent on this character");
                    return;
                }

                const auto* expressions = scene.expressions.GetComponent(expressionEntity_);
                expressionLabel_.SetText(
                    std::string("EXPRESSIONS // native blink/look/talk // ") +
                    std::to_string(expressions->expressions.size()) + " expression(s)");

                forceTalking_.SetEnabled(true);
                blinkFrequency_.SetEnabled(true);
                blinkLength_.SetEnabled(true);
                blinkCount_.SetEnabled(true);
                lookFrequency_.SetEnabled(true);
                lookLength_.SetEnabled(true);
                forceTalking_.SetCheck(state.forceTalking);
                blinkFrequency_.SetValue(state.blinkFrequency);
                blinkLength_.SetValue(state.blinkLength);
                blinkCount_.SetValue(static_cast<float>(state.blinkCount));
                lookFrequency_.SetValue(state.lookFrequency);
                lookLength_.SetValue(state.lookLength);

                if (selectedExpression_ >= expressions->expressions.size())
                    selectedExpression_ = 0;
                for (std::size_t i = 0; i < expressions->expressions.size(); ++i)
                {
                    const auto& expression = expressions->expressions[i];
                    expressionList_.AddItem(
                        expression.name.empty()
                        ? std::string("Expression ") + std::to_string(i)
                        : expression.name,
                        static_cast<std::uint64_t>(i));
                }
                expressionList_.SetEnabled(!expressions->expressions.empty());
                if (!expressions->expressions.empty())
                {
                    expressionList_.SetSelectedByUserdataWithoutCallback(
                        static_cast<std::uint64_t>(selectedExpression_));
                }
                RefreshExpressionEntryControls();
            }

            void RefreshExpressionEntryControls()
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || expressionEntity_ == wi::ecs::INVALID_ENTITY)
                {
                    SetExpressionEntryControlsEnabled(false);
                    return;
                }

                const auto state = bridge::CaptureExpressionEntryState(
                    session->Scenes().GetScene(), expressionEntity_, selectedExpression_);
                SetExpressionEntryControlsEnabled(state.valid);
                if (!state.valid)
                    return;

                expressionBinary_.SetCheck(state.binary);
                expressionWeight_.SetValue(state.weight);
                overrideMouth_.SetSelectedByUserdataWithoutCallback(
                    static_cast<std::uint64_t>(state.overrideMouth));
                overrideBlink_.SetSelectedByUserdataWithoutCallback(
                    static_cast<std::uint64_t>(state.overrideBlink));
                overrideLook_.SetSelectedByUserdataWithoutCallback(
                    static_cast<std::uint64_t>(state.overrideLook));
            }

            void SetExpressionControlsEnabled(const bool enabled)
            {
                forceTalking_.SetEnabled(enabled);
                blinkFrequency_.SetEnabled(enabled);
                blinkLength_.SetEnabled(enabled);
                blinkCount_.SetEnabled(enabled);
                lookFrequency_.SetEnabled(enabled);
                lookLength_.SetEnabled(enabled);
                expressionList_.SetEnabled(enabled);
                SetExpressionEntryControlsEnabled(enabled);
            }

            void SetExpressionEntryControlsEnabled(const bool enabled)
            {
                expressionBinary_.SetEnabled(enabled);
                expressionWeight_.SetEnabled(enabled);
                overrideMouth_.SetEnabled(enabled);
                overrideBlink_.SetEnabled(enabled);
                overrideLook_.SetEnabled(enabled);
            }

            void ToggleIkComponent()
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || ikEntity_ == wi::ecs::INVALID_ENTITY)
                    return;
                auto& scene = session->Scenes().GetScene();
                auto after = bridge::CaptureInverseKinematicsState(scene, ikEntity_);
                after.componentExists = !after.componentExists;
                const bool adding = after.componentExists;
                if (session->Commands().Execute(
                    std::make_unique<bridge::SetInverseKinematicsStateCommand>(
                        scene, ikEntity_, after)))
                {
                    SetStatus(adding ? "PHASE 7C // native IK component added" :
                        "PHASE 7C // native IK component removed");
                }
                RefreshIkControls();
                RequestRefresh();
            }

            template<typename Mutator>
            void CommitIk(Mutator&& mutator, const char* message)
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || ikEntity_ == wi::ecs::INVALID_ENTITY)
                    return;
                auto& scene = session->Scenes().GetScene();
                auto after = bridge::CaptureInverseKinematicsState(scene, ikEntity_);
                if (!after.componentExists)
                    return;
                mutator(after);
                if (session->Commands().Execute(
                    std::make_unique<bridge::SetInverseKinematicsStateCommand>(
                        scene, ikEntity_, after)))
                {
                    SetStatus(std::string("PHASE 7C // ") + message);
                }
                RefreshIkControls();
                RequestRefresh();
            }

            template<typename Mutator>
            void CommitLookAt(Mutator&& mutator, const char* message)
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || rigEntity_ == wi::ecs::INVALID_ENTITY)
                    return;
                auto& scene = session->Scenes().GetScene();
                auto after = bridge::CaptureHumanoidLookAtState(scene, rigEntity_);
                if (!after.componentExists)
                    return;
                mutator(after);
                if (session->Commands().Execute(
                    std::make_unique<bridge::SetHumanoidLookAtStateCommand>(
                        scene, rigEntity_, after)))
                {
                    SetStatus(std::string("PHASE 7C // ") + message);
                }
                RefreshLookAtControls();
                RequestRefresh();
            }

            template<typename Mutator>
            void CommitExpressionMaster(Mutator&& mutator, const char* message)
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || expressionEntity_ == wi::ecs::INVALID_ENTITY)
                    return;
                auto& scene = session->Scenes().GetScene();
                auto after = bridge::CaptureExpressionMasterState(scene, expressionEntity_);
                if (!after.componentExists)
                    return;
                mutator(after);
                if (session->Commands().Execute(
                    std::make_unique<bridge::SetExpressionMasterStateCommand>(
                        scene, expressionEntity_, after)))
                {
                    SetStatus(std::string("PHASE 7C // ") + message);
                }
                RefreshExpressionControls();
                RequestRefresh();
            }

            template<typename Mutator>
            void CommitExpressionEntry(Mutator&& mutator, const char* message)
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || expressionEntity_ == wi::ecs::INVALID_ENTITY)
                    return;
                auto& scene = session->Scenes().GetScene();
                auto after = bridge::CaptureExpressionEntryState(
                    scene, expressionEntity_, selectedExpression_);
                if (!after.valid)
                    return;
                mutator(after);
                if (session->Commands().Execute(
                    std::make_unique<bridge::SetExpressionEntryStateCommand>(
                        scene, expressionEntity_, selectedExpression_, after)))
                {
                    SetStatus(std::string("PHASE 7C // ") + message);
                }
                RefreshExpressionEntryControls();
                RequestRefresh();
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

            wi::ecs::Entity rigEntity_ = wi::ecs::INVALID_ENTITY;
            wi::ecs::Entity ikEntity_ = wi::ecs::INVALID_ENTITY;
            wi::ecs::Entity expressionEntity_ = wi::ecs::INVALID_ENTITY;
            std::size_t selectedExpression_ = 0;

            SceneInspectorButton header_;
            wi::gui::Label status_;

            wi::gui::Label ikLabel_;
            SceneInspectorComboBox ikBone_;
            SceneInspectorButton ikComponent_;
            SceneInspectorComboBox ikTarget_;
            SceneInspectorCheckBox ikDisabled_;
            SceneInspectorSlider ikChainLength_;
            SceneInspectorSlider ikIterations_;

            wi::gui::Label lookAtLabel_;
            SceneInspectorCheckBox lookAtEnabled_;
            SceneInspectorComboBox lookAtTarget_;
            SceneInspectorSlider headHorizontal_;
            SceneInspectorSlider headVertical_;
            SceneInspectorSlider headSpeed_;
            SceneInspectorSlider eyeHorizontal_;
            SceneInspectorSlider eyeVertical_;
            SceneInspectorSlider eyeSpeed_;

            wi::gui::Label expressionLabel_;
            SceneInspectorCheckBox forceTalking_;
            SceneInspectorSlider blinkFrequency_;
            SceneInspectorSlider blinkLength_;
            SceneInspectorSlider blinkCount_;
            SceneInspectorSlider lookFrequency_;
            SceneInspectorSlider lookLength_;
            SceneInspectorComboBox expressionList_;
            SceneInspectorCheckBox expressionBinary_;
            SceneInspectorSlider expressionWeight_;
            SceneInspectorComboBox overrideMouth_;
            SceneInspectorComboBox overrideBlink_;
            SceneInspectorComboBox overrideLook_;
        };

        bool CharacterControlsSectionProvider::IsVisible(
            const InspectorSectionContext& context) const
        {
            return owner_ != nullptr && owner_->IsVisible(context);
        }

        float CharacterControlsSectionProvider::MeasureContentHeight(
            const InspectorSectionContext&,
            float) const
        {
            return owner_ != nullptr ? owner_->Measure() : 0.0f;
        }

        void CharacterControlsSectionProvider::Refresh(const InspectorSectionContext&)
        {
            if (owner_ != nullptr)
                owner_->Refresh();
        }

        void CharacterControlsSectionProvider::ApplyLayout(
            const InspectorSectionContext&,
            const InspectorSectionLayout& layout)
        {
            if (owner_ != nullptr)
                owner_->Layout(layout);
        }

        std::unique_ptr<CharacterControlsInspector> activeInspector;
        StudioRenderPath* activeOwner = nullptr;
    }

    void RegisterPhase7Gate7CCharacterControlsInspector(
        StudioRenderPath& owner,
        wi::gui::Window& inspectorPanel,
        InspectorSectionRegistry& registry,
        std::function<void()> requestRefresh,
        std::function<void(std::string)> setStatus)
    {
        activeInspector.reset();
        activeOwner = &owner;
        activeInspector = std::make_unique<CharacterControlsInspector>(
            owner,
            inspectorPanel,
            registry,
            std::move(requestRefresh),
            std::move(setStatus));
        activeInspector->Register();
    }

    void PreparePhase7Gate7CCharacterControlsInspector(StudioRenderPath& owner)
    {
        if (activeOwner == &owner && activeInspector)
            activeInspector->PrepareForLayout();
    }
}
