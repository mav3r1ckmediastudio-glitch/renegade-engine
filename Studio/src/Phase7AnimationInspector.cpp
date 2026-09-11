#include "Phase7NativeInspector.h"

#include "InspectorSectionFramework.h"
#include "RenegadeStudioChrome.h"
#include "StudioApplication.h"

#include "renegade/bridge/AnimationCreationService.h"
#include "renegade/bridge/AnimationService.h"
#include "renegade/bridge/StudioSession.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace renegade::studio
{
    // Gate 7E is implemented in its own translation unit but shares the same
    // registration/reset lifecycle with A-D.
    void RegisterPhase7SpecialistInspector(
        StudioRenderPath& owner,
        wi::gui::Window& inspectorPanel,
        InspectorSectionRegistry& registry,
        std::function<void()> requestRefresh,
        std::function<void(std::string)> setStatus);
    void PreparePhase7SpecialistInspector(StudioRenderPath& owner);

    namespace
    {
        using AnimationPath =
            wi::scene::AnimationComponent::AnimationChannel::Path;
        using HumanoidBone = wi::scene::HumanoidComponent::HumanoidBone;

        constexpr std::array<const char*, 55> HumanoidBoneNames = {
            "Hips", "Spine", "Chest", "Upper Chest", "Neck", "Head",
            "Left Eye", "Right Eye", "Jaw",
            "Left Upper Leg", "Left Lower Leg", "Left Foot", "Left Toes",
            "Right Upper Leg", "Right Lower Leg", "Right Foot", "Right Toes",
            "Left Shoulder", "Left Upper Arm", "Left Lower Arm", "Left Hand",
            "Right Shoulder", "Right Upper Arm", "Right Lower Arm", "Right Hand",
            "Left Thumb Metacarpal", "Left Thumb Proximal", "Left Thumb Distal",
            "Left Index Proximal", "Left Index Intermediate", "Left Index Distal",
            "Left Middle Proximal", "Left Middle Intermediate", "Left Middle Distal",
            "Left Ring Proximal", "Left Ring Intermediate", "Left Ring Distal",
            "Left Little Proximal", "Left Little Intermediate", "Left Little Distal",
            "Right Thumb Metacarpal", "Right Thumb Proximal", "Right Thumb Distal",
            "Right Index Proximal", "Right Index Intermediate", "Right Index Distal",
            "Right Middle Proximal", "Right Middle Intermediate", "Right Middle Distal",
            "Right Ring Proximal", "Right Ring Intermediate", "Right Ring Distal",
            "Right Little Proximal", "Right Little Intermediate", "Right Little Distal",
        };
        static_assert(
            HumanoidBoneNames.size() ==
                static_cast<std::size_t>(HumanoidBone::Count));

        struct PathChoice
        {
            const char* label;
            AnimationPath path;
        };

        constexpr std::array<PathChoice, 25> TimelinePaths = {{
            {"Position", AnimationPath::TRANSLATION},
            {"Rotation", AnimationPath::ROTATION},
            {"Scale", AnimationPath::SCALE},
            {"Morph Weights", AnimationPath::WEIGHTS},
            {"Light Color", AnimationPath::LIGHT_COLOR},
            {"Light Intensity", AnimationPath::LIGHT_INTENSITY},
            {"Light Range", AnimationPath::LIGHT_RANGE},
            {"Light Inner Cone", AnimationPath::LIGHT_INNERCONE},
            {"Light Outer Cone", AnimationPath::LIGHT_OUTERCONE},
            {"Sound Play", AnimationPath::SOUND_PLAY},
            {"Sound Stop", AnimationPath::SOUND_STOP},
            {"Sound Volume", AnimationPath::SOUND_VOLUME},
            {"Emitter Emit Count", AnimationPath::EMITTER_EMITCOUNT},
            {"Camera FOV", AnimationPath::CAMERA_FOV},
            {"Camera Focal Length", AnimationPath::CAMERA_FOCAL_LENGTH},
            {"Camera Aperture Size", AnimationPath::CAMERA_APERTURE_SIZE},
            {"Camera Aperture Shape", AnimationPath::CAMERA_APERTURE_SHAPE},
            {"Script Play", AnimationPath::SCRIPT_PLAY},
            {"Script Stop", AnimationPath::SCRIPT_STOP},
            {"Material Color", AnimationPath::MATERIAL_COLOR},
            {"Material Emissive", AnimationPath::MATERIAL_EMISSIVE},
            {"Material Roughness", AnimationPath::MATERIAL_ROUGHNESS},
            {"Material Metalness", AnimationPath::MATERIAL_METALNESS},
            {"Material Reflectance", AnimationPath::MATERIAL_REFLECTANCE},
            {"Material UV Mul/Add", AnimationPath::MATERIAL_TEXMULADD},
        }};

        std::string EntityName(
            const wi::scene::Scene& scene,
            const wi::ecs::Entity entity)
        {
            if (entity == wi::ecs::INVALID_ENTITY)
                return "NONE";
            const auto* name = scene.names.GetComponent(entity);
            if (name != nullptr && !name->name.empty())
                return name->name;
            return "Entity " + std::to_string(entity);
        }

        bool InSubtree(
            const wi::scene::Scene& scene,
            const wi::ecs::Entity entity,
            const wi::ecs::Entity root) noexcept
        {
            return root == wi::ecs::INVALID_ENTITY || entity == root ||
                scene.Entity_IsDescendant(entity, root);
        }

        wi::ecs::Entity RelatedRoot(
            const wi::scene::Scene& scene,
            const wi::ecs::Entity selected) noexcept
        {
            const auto animationRoot = bridge::ResolveAnimationOwner(scene, selected);
            if (animationRoot != wi::ecs::INVALID_ENTITY)
                return animationRoot;

            wi::ecs::Entity probe = selected;
            wi::ecs::Entity last = selected;
            while (probe != wi::ecs::INVALID_ENTITY)
            {
                last = probe;
                const auto* hierarchy = scene.hierarchy.GetComponent(probe);
                if (hierarchy == nullptr || hierarchy->parentID == probe)
                    break;
                probe = hierarchy->parentID;
            }
            return last;
        }

        wi::ecs::Entity RelatedArmature(
            const wi::scene::Scene& scene,
            const wi::ecs::Entity selected) noexcept
        {
            if (scene.armatures.Contains(selected))
                return selected;
            const auto root = RelatedRoot(scene, selected);
            for (std::size_t index = 0; index < scene.armatures.GetCount(); ++index)
            {
                const auto entity = scene.armatures.GetEntity(index);
                if (InSubtree(scene, entity, root))
                    return entity;
            }
            return wi::ecs::INVALID_ENTITY;
        }

        wi::ecs::Entity RelatedHumanoid(
            const wi::scene::Scene& scene,
            const wi::ecs::Entity selected) noexcept
        {
            if (scene.humanoids.Contains(selected))
                return selected;
            const auto root = RelatedRoot(scene, selected);
            for (std::size_t index = 0; index < scene.humanoids.GetCount(); ++index)
            {
                const auto entity = scene.humanoids.GetEntity(index);
                if (InSubtree(scene, entity, root))
                    return entity;
            }
            return wi::ecs::INVALID_ENTITY;
        }

        wi::ecs::Entity RelatedExpression(
            const wi::scene::Scene& scene,
            const wi::ecs::Entity selected) noexcept
        {
            if (scene.expressions.Contains(selected))
                return selected;
            const auto root = RelatedRoot(scene, selected);
            for (std::size_t index = 0; index < scene.expressions.GetCount(); ++index)
            {
                const auto entity = scene.expressions.GetEntity(index);
                if (InSubtree(scene, entity, root))
                    return entity;
            }
            return wi::ecs::INVALID_ENTITY;
        }

        std::vector<wi::ecs::Entity> RelatedTransforms(
            const wi::scene::Scene& scene,
            const wi::ecs::Entity selected)
        {
            std::vector<wi::ecs::Entity> result;
            const auto root = RelatedRoot(scene, selected);
            for (std::size_t index = 0; index < scene.transforms.GetCount(); ++index)
            {
                const auto entity = scene.transforms.GetEntity(index);
                if (InSubtree(scene, entity, root))
                    result.push_back(entity);
            }
            if (result.empty())
            {
                for (std::size_t index = 0; index < scene.transforms.GetCount(); ++index)
                    result.push_back(scene.transforms.GetEntity(index));
            }
            return result;
        }

        std::vector<wi::ecs::Entity> AllNamedSceneEntities(
            const wi::scene::Scene& scene)
        {
            wi::unordered_set<wi::ecs::Entity> unique;
            scene.FindAllEntities(unique);
            std::vector<wi::ecs::Entity> result(unique.begin(), unique.end());
            std::sort(result.begin(), result.end());
            return result;
        }

        const char* PathLabel(const AnimationPath path) noexcept
        {
            for (const auto& choice : TimelinePaths)
            {
                if (choice.path == path)
                    return choice.label;
            }
            return "Native Channel";
        }

        InspectorSectionDescriptor MakeSection(
            std::string id,
            std::string title,
            const int order)
        {
            InspectorSectionDescriptor descriptor;
            descriptor.id = std::move(id);
            descriptor.title = std::move(title);
            descriptor.order = order;
            descriptor.defaultExpanded = false;
            descriptor.headerHeight = 28.0f;
            descriptor.spacingAfter = 6.0f;
            return descriptor;
        }

        class Phase7AnimationInspector;

        enum class Phase7Section
        {
            Animation,
            Rig,
            IkExpression,
            Timeline,
        };

        class Phase7Provider final : public IInspectorSectionProvider
        {
        public:
            Phase7Provider(
                Phase7AnimationInspector& owner,
                Phase7Section section,
                InspectorSectionDescriptor descriptor)
                : owner_(&owner)
                , section_(section)
                , descriptor_(std::move(descriptor))
            {
            }

            [[nodiscard]] const InspectorSectionDescriptor& Descriptor()
                const noexcept override
            {
                return descriptor_;
            }
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
            Phase7AnimationInspector* owner_ = nullptr;
            Phase7Section section_ = Phase7Section::Animation;
            InspectorSectionDescriptor descriptor_;
        };

        struct AnimationControls
        {
            SceneInspectorButton header;
            wi::gui::Label status;
            SceneInspectorComboBox clip;
            SceneInspectorButton fromStart;
            SceneInspectorButton playPause;
            SceneInspectorButton stop;
            SceneInspectorComboBox mode;
            SceneInspectorSlider timer;
            SceneInspectorSlider amount;
            SceneInspectorSlider speed;
            SceneInspectorSlider start;
            SceneInspectorSlider end;
            SceneInspectorCheckBox rootMotion;
            SceneInspectorComboBox rootBone;
        };

        struct RigControls
        {
            SceneInspectorButton header;
            wi::gui::Label status;
            SceneInspectorButton autoMap;
            SceneInspectorButton resetPose;
            SceneInspectorButton retarget;
            SceneInspectorComboBox boneSlot;
            SceneInspectorComboBox boneTarget;
        };

        struct IkExpressionControls
        {
            SceneInspectorButton header;
            wi::gui::Label status;
            SceneInspectorButton ensureIk;
            SceneInspectorCheckBox ikEnabled;
            SceneInspectorComboBox ikTarget;
            SceneInspectorSlider ikChain;
            SceneInspectorSlider ikIterations;

            SceneInspectorCheckBox lookEnabled;
            SceneInspectorComboBox lookTarget;
            SceneInspectorSlider headHorizontal;
            SceneInspectorSlider headVertical;
            SceneInspectorSlider headSpeed;
            SceneInspectorSlider eyeHorizontal;
            SceneInspectorSlider eyeVertical;
            SceneInspectorSlider eyeSpeed;

            SceneInspectorButton ensureExpressions;
            SceneInspectorCheckBox forceTalking;
            SceneInspectorSlider blinkFrequency;
            SceneInspectorSlider blinkLength;
            SceneInspectorSlider blinkCount;
            SceneInspectorSlider lookFrequency;
            SceneInspectorSlider lookLength;
            SceneInspectorComboBox expression;
            SceneInspectorSlider expressionWeight;
            SceneInspectorCheckBox expressionBinary;
            SceneInspectorComboBox mouthOverride;
            SceneInspectorComboBox blinkOverride;
            SceneInspectorComboBox lookOverride;
        };

        struct TimelineControls
        {
            SceneInspectorButton header;
            wi::gui::Label status;
            SceneInspectorComboBox animation;
            SceneInspectorButton create;
            SceneInspectorComboBox target;
            SceneInspectorComboBox path;
            SceneInspectorComboBox interpolation;
            SceneInspectorSlider time;
            SceneInspectorButton record;
            SceneInspectorComboBox channel;
        };

        class Phase7AnimationInspector final
        {
        public:
            Phase7AnimationInspector(
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
                CreateAnimationControls();
                CreateRigControls();
                CreateIkExpressionControls();
                CreateTimelineControls();

                std::string error;
                auto registerOne = [&](Phase7Section section, const char* id,
                    const char* title, const int order)
                {
                    error.clear();
                    auto provider = std::make_shared<Phase7Provider>(
                        *this, section, MakeSection(id, title, order));
                    if (!registry_->Register(std::move(provider), error))
                        SetStatus("PHASE 7 // " + error);
                };
                registerOne(Phase7Section::Animation,
                    Phase7AnimationSectionId, "ANIMATION", 34);
                registerOne(Phase7Section::Rig,
                    Phase7RigSectionId, "RIG / RETARGET", 35);
                registerOne(Phase7Section::IkExpression,
                    Phase7IkExpressionSectionId, "IK / EXPRESSION", 36);
                registerOne(Phase7Section::Timeline,
                    Phase7TimelineSectionId, "TIMELINE", 37);
            }

            void Prepare()
            {
                SetAnimationVisible(false);
                SetRigVisible(false);
                SetIkExpressionVisible(false);
                SetTimelineVisible(false);
            }

            [[nodiscard]] bool Visible(
                const Phase7Section section,
                const InspectorSectionContext& context) const
            {
                if (!context.hasSelection)
                    return false;
                const auto* session = bridge::StudioSession::Current();
                if (session == nullptr || !session->Selection().HasSelection())
                    return false;
                const auto selected = session->Selection().SelectedEntity();
                const auto& scene = session->Scenes().GetScene();
                switch (section)
                {
                case Phase7Section::Animation:
                case Phase7Section::Timeline:
                    return !bridge::CollectAnimationClips(scene, selected).empty() ||
                        RelatedArmature(scene, selected) != wi::ecs::INVALID_ENTITY ||
                        RelatedHumanoid(scene, selected) != wi::ecs::INVALID_ENTITY;
                case Phase7Section::Rig:
                    return RelatedArmature(scene, selected) != wi::ecs::INVALID_ENTITY ||
                        RelatedHumanoid(scene, selected) != wi::ecs::INVALID_ENTITY;
                case Phase7Section::IkExpression:
                    return scene.transforms.Contains(selected) ||
                        RelatedHumanoid(scene, selected) != wi::ecs::INVALID_ENTITY ||
                        RelatedExpression(scene, selected) != wi::ecs::INVALID_ENTITY;
                }
                return false;
            }

            [[nodiscard]] float Measure(const Phase7Section section) const
            {
                switch (section)
                {
                case Phase7Section::Animation:
                    return 394.0f;
                case Phase7Section::Rig:
                    return 174.0f;
                case Phase7Section::IkExpression:
                    return 726.0f;
                case Phase7Section::Timeline:
                    return 280.0f;
                }
                return 0.0f;
            }

            void Refresh(const Phase7Section section)
            {
                switch (section)
                {
                case Phase7Section::Animation:
                    RefreshAnimation();
                    break;
                case Phase7Section::Rig:
                    RefreshRig();
                    break;
                case Phase7Section::IkExpression:
                    RefreshIkExpression();
                    break;
                case Phase7Section::Timeline:
                    RefreshTimeline();
                    break;
                }
            }

            void Layout(const Phase7Section section, const InspectorSectionLayout& layout)
            {
                switch (section)
                {
                case Phase7Section::Animation:
                    LayoutAnimation(layout);
                    break;
                case Phase7Section::Rig:
                    LayoutRig(layout);
                    break;
                case Phase7Section::IkExpression:
                    LayoutIkExpression(layout);
                    break;
                case Phase7Section::Timeline:
                    LayoutTimeline(layout);
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
                if (requestRefresh_)
                    requestRefresh_();
            }

            void SetStatus(std::string status)
            {
                if (setStatus_)
                    setStatus_(std::move(status));
            }

            void CloseAll(const char* opening)
            {
                for (const char* id : {
                    "transform", "rendering", "materials", "action", "script",
                    "global_script", Phase7AnimationSectionId, Phase7RigSectionId,
                    Phase7IkExpressionSectionId, Phase7TimelineSectionId,
                    Phase7SpecialistSectionId})
                {
                    (void)registry_->SetExpanded(id, false);
                }
                if (opening != nullptr)
                    (void)registry_->SetExpanded(opening, true);
            }

            void WireHeader(SceneInspectorButton& button, const char* id,
                const char* label)
            {
                button.Create(std::string("Phase7 ") + label + " Header");
                button.OnClick([this, id](const wi::gui::EventArgs&)
                {
                    const bool opening = !registry_->IsExpanded(id);
                    CloseAll(opening ? id : nullptr);
                    RequestRefresh();
                });
                panel_->AddWidget(&button);
                button.SetVisible(false);
            }

            void CreateAnimationControls()
            {
                WireHeader(animation_.header, Phase7AnimationSectionId, "Animation");
                animation_.status.Create("Phase7 Animation Status");
                animation_.status.SetWrapEnabled(true);
                panel_->AddWidget(&animation_.status);

                animation_.clip.Create("CLIP");
                animation_.clip.SetTooltip("Native Wicked AnimationComponent clip in the selected model hierarchy.");
                animation_.clip.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    selectedAnimation_ = static_cast<wi::ecs::Entity>(args.userdata);
                    selectedTimelineAnimation_ = selectedAnimation_;
                    RequestRefresh();
                });
                panel_->AddWidget(&animation_.clip);

                animation_.fromStart.Create("FROM START");
                animation_.fromStart.OnClick([this](const wi::gui::EventArgs&)
                {
                    Preview(bridge::AnimationPreviewAction::PlayFromStart);
                });
                panel_->AddWidget(&animation_.fromStart);
                animation_.playPause.Create("PLAY");
                animation_.playPause.OnClick([this](const wi::gui::EventArgs&)
                {
                    auto* session = Session();
                    if (session == nullptr)
                        return;
                    const auto* clip = session->Scenes().GetScene().animations.GetComponent(selectedAnimation_);
                    Preview(clip != nullptr && clip->IsPlaying()
                        ? bridge::AnimationPreviewAction::Pause
                        : bridge::AnimationPreviewAction::Play);
                });
                panel_->AddWidget(&animation_.playPause);
                animation_.stop.Create("STOP");
                animation_.stop.OnClick([this](const wi::gui::EventArgs&)
                {
                    Preview(bridge::AnimationPreviewAction::Stop);
                });
                panel_->AddWidget(&animation_.stop);

                animation_.mode.Create("PLAYBACK MODE");
                animation_.mode.AddItem("Loop", static_cast<std::uint64_t>(bridge::AnimationPlaybackMode::Loop));
                animation_.mode.AddItem("Ping Pong", static_cast<std::uint64_t>(bridge::AnimationPlaybackMode::PingPong));
                animation_.mode.AddItem("Play Once", static_cast<std::uint64_t>(bridge::AnimationPlaybackMode::Once));
                animation_.mode.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    CommitAnimation([&](bridge::AnimationAuthoringState& state)
                    {
                        state.playbackMode = static_cast<bridge::AnimationPlaybackMode>(args.userdata);
                    });
                });
                panel_->AddWidget(&animation_.mode);

                animation_.timer.Create(0.0f, 1.0f, 0.0f, 10000.0f,
                    "Phase7 Animation Timer", "TIME");
                animation_.timer.OnValuePreview([this](const float value)
                {
                    PreviewTimer(value);
                });
                animation_.timer.OnValueCommitted([this](const float value)
                {
                    PreviewTimer(value);
                });
                panel_->AddWidget(&animation_.timer);

                auto authoredSlider = [this](SceneInspectorSlider& slider,
                    const float min, const float max, const float initial,
                    const char* name, const char* label,
                    auto setter)
                {
                    slider.Create(min, max, initial, 10000.0f, name, label);
                    slider.OnValueCommitted([this, setter](const float value)
                    {
                        CommitAnimation([&](bridge::AnimationAuthoringState& state)
                        {
                            setter(state, value);
                        });
                    });
                    panel_->AddWidget(&slider);
                };
                authoredSlider(animation_.amount, 0.0f, 1.0f, 1.0f,
                    "Phase7 Animation Blend", "BLEND",
                    [](auto& state, float value) { state.amount = value; });
                authoredSlider(animation_.speed, -8.0f, 8.0f, 1.0f,
                    "Phase7 Animation Speed", "SPEED",
                    [](auto& state, float value) { state.speed = value; });
                authoredSlider(animation_.start, 0.0f, 120.0f, 0.0f,
                    "Phase7 Animation Start", "START",
                    [](auto& state, float value) { state.start = value; });
                authoredSlider(animation_.end, 0.0f, 120.0f, 1.0f,
                    "Phase7 Animation End", "END",
                    [](auto& state, float value) { state.end = value; });

                animation_.rootMotion.Create("ROOT MOTION: ");
                animation_.rootMotion.OnClick([this](const wi::gui::EventArgs& args)
                {
                    CommitAnimation([&](bridge::AnimationAuthoringState& state)
                    {
                        state.rootMotion = args.bValue;
                    });
                });
                panel_->AddWidget(&animation_.rootMotion);
                animation_.rootBone.Create("ROOT MOTION BONE");
                animation_.rootBone.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    CommitAnimation([&](bridge::AnimationAuthoringState& state)
                    {
                        state.rootMotionBone = static_cast<wi::ecs::Entity>(args.userdata);
                        if (state.rootMotionBone == wi::ecs::INVALID_ENTITY)
                            state.rootMotion = false;
                    });
                });
                panel_->AddWidget(&animation_.rootBone);
                SetAnimationVisible(false);
            }

            void CreateRigControls()
            {
                WireHeader(rig_.header, Phase7RigSectionId, "Rig");
                rig_.status.Create("Phase7 Rig Status");
                rig_.status.SetWrapEnabled(true);
                panel_->AddWidget(&rig_.status);
                rig_.autoMap.Create("AUTO MAP HUMANOID");
                rig_.autoMap.SetTooltip("Use the pinned Wicked VRM/Mixamo/UE-style bone-name mapper.");
                rig_.autoMap.OnClick([this](const wi::gui::EventArgs&) { AutoMapHumanoid(); });
                panel_->AddWidget(&rig_.autoMap);
                rig_.resetPose.Create("RESET POSE");
                rig_.resetPose.OnClick([this](const wi::gui::EventArgs&) { ResetPose(); });
                panel_->AddWidget(&rig_.resetPose);
                rig_.retarget.Create("IMPORT + RETARGET ANIMATION");
                rig_.retarget.SetTooltip("Native Wicked retarget: WISCENE, GLTF/GLB, FBX, VRM or VRMA.");
                rig_.retarget.OnClick([this](const wi::gui::EventArgs&) { ChooseRetargetSource(); });
                panel_->AddWidget(&rig_.retarget);

                rig_.boneSlot.Create("HUMANOID BONE");
                for (std::size_t index = 0; index < HumanoidBoneNames.size(); ++index)
                    rig_.boneSlot.AddItem(HumanoidBoneNames[index], index);
                rig_.boneSlot.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    selectedHumanoidBone_ = static_cast<std::size_t>(args.userdata);
                    RequestRefresh();
                });
                panel_->AddWidget(&rig_.boneSlot);
                rig_.boneTarget.Create("BONE TARGET");
                rig_.boneTarget.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    SetHumanoidBone(static_cast<wi::ecs::Entity>(args.userdata));
                });
                panel_->AddWidget(&rig_.boneTarget);
                SetRigVisible(false);
            }

            void CreateIkExpressionControls()
            {
                WireHeader(ikexpr_.header, Phase7IkExpressionSectionId, "IK Expression");
                ikexpr_.status.Create("Phase7 IK Expression Status");
                ikexpr_.status.SetWrapEnabled(true);
                panel_->AddWidget(&ikexpr_.status);

                ikexpr_.ensureIk.Create("ADD / RESET IK");
                ikexpr_.ensureIk.OnClick([this](const wi::gui::EventArgs&) { EnsureIk(); });
                panel_->AddWidget(&ikexpr_.ensureIk);
                ikexpr_.ikEnabled.Create("IK ENABLED: ");
                ikexpr_.ikEnabled.OnClick([this](const wi::gui::EventArgs& args)
                {
                    CommitIk([&](auto& state) { state.enabled = args.bValue; });
                });
                panel_->AddWidget(&ikexpr_.ikEnabled);
                ikexpr_.ikTarget.Create("IK TARGET");
                ikexpr_.ikTarget.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    CommitIk([&](auto& state) { state.target = static_cast<wi::ecs::Entity>(args.userdata); });
                });
                panel_->AddWidget(&ikexpr_.ikTarget);
                ikexpr_.ikChain.Create(0.0f, 64.0f, 0.0f, 64.0f,
                    "Phase7 IK Chain", "IK CHAIN");
                ikexpr_.ikChain.OnValueCommitted([this](const float value)
                {
                    CommitIk([&](auto& state) { state.chainLength = static_cast<std::uint32_t>(std::lround(value)); });
                });
                panel_->AddWidget(&ikexpr_.ikChain);
                ikexpr_.ikIterations.Create(1.0f, 64.0f, 1.0f, 63.0f,
                    "Phase7 IK Iterations", "IK ITERATIONS");
                ikexpr_.ikIterations.OnValueCommitted([this](const float value)
                {
                    CommitIk([&](auto& state) { state.iterations = static_cast<std::uint32_t>(std::lround(value)); });
                });
                panel_->AddWidget(&ikexpr_.ikIterations);

                ikexpr_.lookEnabled.Create("HUMANOID LOOK AT: ");
                ikexpr_.lookEnabled.OnClick([this](const wi::gui::EventArgs& args)
                {
                    CommitLook([&](auto& state) { state.enabled = args.bValue; });
                });
                panel_->AddWidget(&ikexpr_.lookEnabled);
                ikexpr_.lookTarget.Create("LOOK TARGET");
                ikexpr_.lookTarget.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    CommitLook([&](auto& state) { state.target = static_cast<wi::ecs::Entity>(args.userdata); });
                });
                panel_->AddWidget(&ikexpr_.lookTarget);

                auto lookSlider = [this](SceneInspectorSlider& slider,
                    float min, float max, float initial,
                    const char* name, const char* label, auto setter)
                {
                    slider.Create(min, max, initial, 1000.0f, name, label);
                    slider.OnValueCommitted([this, setter](const float value)
                    {
                        CommitLook([&](auto& state) { setter(state, value); });
                    });
                    panel_->AddWidget(&slider);
                };
                lookSlider(ikexpr_.headHorizontal, 0.0f, 90.0f, 60.0f,
                    "Phase7 Head Horizontal", "HEAD H", [](auto& s, float v) { s.headHorizontalDegrees = v; });
                lookSlider(ikexpr_.headVertical, 0.0f, 60.0f, 30.0f,
                    "Phase7 Head Vertical", "HEAD V", [](auto& s, float v) { s.headVerticalDegrees = v; });
                lookSlider(ikexpr_.headSpeed, 0.01f, 1.0f, 0.1f,
                    "Phase7 Head Speed", "HEAD SPEED", [](auto& s, float v) { s.headSpeed = v; });
                lookSlider(ikexpr_.eyeHorizontal, 0.0f, 40.0f, 9.0f,
                    "Phase7 Eye Horizontal", "EYE H", [](auto& s, float v) { s.eyeHorizontalDegrees = v; });
                lookSlider(ikexpr_.eyeVertical, 0.0f, 30.0f, 9.0f,
                    "Phase7 Eye Vertical", "EYE V", [](auto& s, float v) { s.eyeVerticalDegrees = v; });
                lookSlider(ikexpr_.eyeSpeed, 0.01f, 1.0f, 0.1f,
                    "Phase7 Eye Speed", "EYE SPEED", [](auto& s, float v) { s.eyeSpeed = v; });

                ikexpr_.ensureExpressions.Create("ADD EXPRESSIONS");
                ikexpr_.ensureExpressions.OnClick([this](const wi::gui::EventArgs&) { EnsureExpressions(); });
                panel_->AddWidget(&ikexpr_.ensureExpressions);
                ikexpr_.forceTalking.Create("FORCE TALKING: ");
                ikexpr_.forceTalking.OnClick([this](const wi::gui::EventArgs& args)
                {
                    CommitExpressionMaster([&](auto& state) { state.forceTalking = args.bValue; });
                });
                panel_->AddWidget(&ikexpr_.forceTalking);

                auto expressionSlider = [this](SceneInspectorSlider& slider,
                    float min, float max, float initial, float steps,
                    const char* name, const char* label, auto setter)
                {
                    slider.Create(min, max, initial, steps, name, label);
                    slider.OnValueCommitted([this, setter](const float value)
                    {
                        CommitExpressionMaster([&](auto& state) { setter(state, value); });
                    });
                    panel_->AddWidget(&slider);
                };
                expressionSlider(ikexpr_.blinkFrequency, 0.0f, 1.0f, 0.0f, 1000.0f,
                    "Phase7 Blink Frequency", "BLINKS / SEC", [](auto& s, float v) { s.blinkFrequency = v; });
                expressionSlider(ikexpr_.blinkLength, 0.0f, 1.0f, 0.1f, 1000.0f,
                    "Phase7 Blink Length", "BLINK LENGTH", [](auto& s, float v) { s.blinkLength = v; });
                expressionSlider(ikexpr_.blinkCount, 1.0f, 4.0f, 2.0f, 3.0f,
                    "Phase7 Blink Count", "BLINK COUNT", [](auto& s, float v) { s.blinkCount = static_cast<int>(std::lround(v)); });
                expressionSlider(ikexpr_.lookFrequency, 0.0f, 1.0f, 0.0f, 1000.0f,
                    "Phase7 Look Frequency", "LOOKS / SEC", [](auto& s, float v) { s.lookFrequency = v; });
                expressionSlider(ikexpr_.lookLength, 0.0f, 1.0f, 0.6f, 1000.0f,
                    "Phase7 Look Length", "LOOK LENGTH", [](auto& s, float v) { s.lookLength = v; });

                ikexpr_.expression.Create("EXPRESSION");
                ikexpr_.expression.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    selectedExpression_ = static_cast<std::size_t>(args.userdata);
                    RequestRefresh();
                });
                panel_->AddWidget(&ikexpr_.expression);
                ikexpr_.expressionWeight.Create(0.0f, 1.0f, 0.0f, 10000.0f,
                    "Phase7 Expression Weight", "WEIGHT");
                ikexpr_.expressionWeight.OnValueCommitted([this](const float value)
                {
                    CommitExpressionItem([&](auto& state) { state.weight = value; });
                });
                panel_->AddWidget(&ikexpr_.expressionWeight);
                ikexpr_.expressionBinary.Create("BINARY: ");
                ikexpr_.expressionBinary.OnClick([this](const wi::gui::EventArgs& args)
                {
                    CommitExpressionItem([&](auto& state) { state.binary = args.bValue; });
                });
                panel_->AddWidget(&ikexpr_.expressionBinary);

                auto overrideCombo = [this](SceneInspectorComboBox& combo,
                    const char* name, auto setter)
                {
                    combo.Create(name);
                    combo.AddItem("None", static_cast<std::uint64_t>(wi::scene::ExpressionComponent::Override::None));
                    combo.AddItem("Block", static_cast<std::uint64_t>(wi::scene::ExpressionComponent::Override::Block));
                    combo.AddItem("Blend", static_cast<std::uint64_t>(wi::scene::ExpressionComponent::Override::Blend));
                    combo.OnSelect([this, setter](const wi::gui::EventArgs& args)
                    {
                        CommitExpressionItem([&](auto& state)
                        {
                            setter(state, static_cast<wi::scene::ExpressionComponent::Override>(args.userdata));
                        });
                    });
                    panel_->AddWidget(&combo);
                };
                overrideCombo(ikexpr_.mouthOverride, "MOUTH OVERRIDE",
                    [](auto& s, auto v) { s.mouth = v; });
                overrideCombo(ikexpr_.blinkOverride, "BLINK OVERRIDE",
                    [](auto& s, auto v) { s.blink = v; });
                overrideCombo(ikexpr_.lookOverride, "LOOK OVERRIDE",
                    [](auto& s, auto v) { s.look = v; });
                SetIkExpressionVisible(false);
            }

            void CreateTimelineControls()
            {
                WireHeader(timeline_.header, Phase7TimelineSectionId, "Timeline");
                timeline_.status.Create("Phase7 Timeline Status");
                timeline_.status.SetWrapEnabled(true);
                panel_->AddWidget(&timeline_.status);
                timeline_.animation.Create("TIMELINE / CLIP");
                timeline_.animation.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    selectedTimelineAnimation_ = static_cast<wi::ecs::Entity>(args.userdata);
                    selectedAnimation_ = selectedTimelineAnimation_;
                    RequestRefresh();
                });
                panel_->AddWidget(&timeline_.animation);
                timeline_.create.Create("NEW NATIVE TIMELINE");
                timeline_.create.OnClick([this](const wi::gui::EventArgs&) { CreateTimeline(); });
                panel_->AddWidget(&timeline_.create);
                timeline_.target.Create("RECORD TARGET");
                timeline_.target.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    selectedTimelineTarget_ = static_cast<wi::ecs::Entity>(args.userdata);
                });
                panel_->AddWidget(&timeline_.target);
                timeline_.path.Create("CHANNEL PATH");
                for (std::size_t index = 0; index < TimelinePaths.size(); ++index)
                    timeline_.path.AddItem(TimelinePaths[index].label, index);
                timeline_.path.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    selectedTimelinePath_ = static_cast<std::size_t>(args.userdata);
                });
                panel_->AddWidget(&timeline_.path);
                timeline_.interpolation.Create("SAMPLING");
                timeline_.interpolation.AddItem("Step", static_cast<std::uint64_t>(bridge::AnimationInterpolation::Step));
                timeline_.interpolation.AddItem("Linear", static_cast<std::uint64_t>(bridge::AnimationInterpolation::Linear));
                timeline_.interpolation.SetTooltip(
                    "Imported cubic-spline channels remain readable/playable. Pinned Wicked does not author spline tangent data, so creator recording is deliberately Step/Linear only.");
                timeline_.interpolation.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    timelineInterpolation_ = static_cast<bridge::AnimationInterpolation>(args.userdata);
                });
                panel_->AddWidget(&timeline_.interpolation);
                timeline_.time.Create(0.0f, 120.0f, 0.0f, 12000.0f,
                    "Phase7 Timeline Time", "KEY TIME");
                timeline_.time.OnValuePreview([this](const float value)
                {
                    if (auto* session = Session())
                        (void)bridge::PreviewAnimationTimer(session->Scenes().GetScene(), selectedTimelineAnimation_, value);
                });
                timeline_.time.OnValueCommitted([this](const float value)
                {
                    timelineTime_ = value;
                });
                panel_->AddWidget(&timeline_.time);
                timeline_.record.Create("RECORD / REPLACE CURRENT VALUE");
                timeline_.record.OnClick([this](const wi::gui::EventArgs&) { RecordTimelineKey(); });
                panel_->AddWidget(&timeline_.record);
                timeline_.channel.Create("NATIVE CHANNELS");
                panel_->AddWidget(&timeline_.channel);
                SetTimelineVisible(false);
            }

            void Preview(const bridge::AnimationPreviewAction action)
            {
                auto* session = Session();
                if (session == nullptr || selectedAnimation_ == wi::ecs::INVALID_ENTITY)
                    return;
                if (bridge::PreviewAnimation(session->Scenes().GetScene(), selectedAnimation_, action))
                    RequestRefresh();
            }

            void PreviewTimer(const float value)
            {
                auto* session = Session();
                if (session == nullptr || selectedAnimation_ == wi::ecs::INVALID_ENTITY)
                    return;
                (void)bridge::PreviewAnimationTimer(
                    session->Scenes().GetScene(), selectedAnimation_, value);
            }

            template<typename Fn>
            void CommitAnimation(Fn fn)
            {
                auto* session = Session();
                if (session == nullptr)
                    return;
                auto& scene = session->Scenes().GetScene();
                const auto* animation = scene.animations.GetComponent(selectedAnimation_);
                if (animation == nullptr)
                    return;
                auto state = bridge::CaptureAnimationAuthoring(*animation);
                fn(state);
                if (session->Commands().Execute(
                        std::make_unique<bridge::SetAnimationAuthoringCommand>(
                            scene, selectedAnimation_, state)))
                {
                    SetStatus("PHASE 7A // animation state updated");
                }
                RequestRefresh();
            }

            void AutoMapHumanoid()
            {
                auto* session = Session();
                if (session == nullptr)
                    return;
                auto& scene = session->Scenes().GetScene();
                const auto selected = session->Selection().SelectedEntity();
                const auto armature = RelatedArmature(scene, selected);
                bridge::HumanoidMappingState mapping;
                std::string error;
                if (armature == wi::ecs::INVALID_ENTITY ||
                    !bridge::BuildAutomaticHumanoidMapping(scene, armature, mapping, error))
                {
                    SetStatus("PHASE 7B // " + error);
                    return;
                }
                if (session->Commands().Execute(
                        std::make_unique<bridge::SetHumanoidMappingCommand>(scene, armature, mapping)))
                {
                    selectedHumanoid_ = armature;
                    SetStatus("PHASE 7B // native humanoid mapping created");
                }
                RequestRefresh();
            }

            void ResetPose()
            {
                auto* session = Session();
                if (session == nullptr)
                    return;
                auto& scene = session->Scenes().GetScene();
                const auto selected = session->Selection().SelectedEntity();
                auto entity = RelatedHumanoid(scene, selected);
                if (entity == wi::ecs::INVALID_ENTITY)
                    entity = RelatedArmature(scene, selected);
                if (bridge::ResetHumanoidPosePreview(scene, entity))
                    SetStatus("PHASE 7B // pose reset through Wicked");
            }

            void ChooseRetargetSource()
            {
                auto* session = Session();
                if (session == nullptr)
                    return;
                const auto humanoid = RelatedHumanoid(
                    session->Scenes().GetScene(),
                    session->Selection().SelectedEntity());
                if (humanoid == wi::ecs::INVALID_ENTITY)
                {
                    SetStatus("PHASE 7B // create/map a Humanoid first");
                    return;
                }

                wi::helper::FileDialogParams params;
                params.type = wi::helper::FileDialogParams::OPEN;
                params.description = "Animation sources (wiscene, gltf, glb, fbx, vrm, vrma)";
                params.extensions = {"WISCENE", "GLTF", "GLB", "FBX", "VRM", "VRMA"};
                wi::helper::FileDialog(params, [this, humanoid](std::string filename)
                {
                    wi::eventhandler::Subscribe_Once(
                        wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                        [this, humanoid, filename = std::move(filename)](std::uint64_t)
                        {
                            auto* current = Session();
                            if (current == nullptr)
                                return;
                            auto command = std::make_unique<bridge::RetargetAnimationsCommand>(
                                current->Scenes().GetScene(), humanoid, filename);
                            if (current->Commands().Execute(std::move(command)))
                                SetStatus("PHASE 7B // native retarget completed");
                            else
                                SetStatus("PHASE 7B // retarget failed: no compatible humanoid animation channels were produced");
                            RequestRefresh();
                        });
                });
            }

            void SetHumanoidBone(const wi::ecs::Entity target)
            {
                auto* session = Session();
                if (session == nullptr || selectedHumanoidBone_ >= HumanoidBoneNames.size())
                    return;
                auto& scene = session->Scenes().GetScene();
                const auto humanoid = RelatedHumanoid(scene, session->Selection().SelectedEntity());
                if (humanoid == wi::ecs::INVALID_ENTITY)
                    return;
                (void)session->Commands().Execute(
                    std::make_unique<bridge::SetHumanoidBoneCommand>(
                        scene, humanoid,
                        static_cast<HumanoidBone>(selectedHumanoidBone_), target));
                RequestRefresh();
            }

            void EnsureIk()
            {
                auto* session = Session();
                if (session == nullptr || !session->Selection().HasSelection())
                    return;
                auto& scene = session->Scenes().GetScene();
                const auto selected = session->Selection().SelectedEntity();
                if (!scene.transforms.Contains(selected))
                    return;
                bridge::InverseKinematicsState state;
                state.enabled = true;
                state.chainLength = 2;
                state.iterations = 4;
                if (session->Commands().Execute(
                        std::make_unique<bridge::SetInverseKinematicsCommand>(scene, selected, state, true)))
                    SetStatus("PHASE 7C // native IK component ready");
                RequestRefresh();
            }

            template<typename Fn>
            void CommitIk(Fn fn)
            {
                auto* session = Session();
                if (session == nullptr || !session->Selection().HasSelection())
                    return;
                auto& scene = session->Scenes().GetScene();
                const auto selected = session->Selection().SelectedEntity();
                const auto* ik = scene.inverse_kinematics.GetComponent(selected);
                if (ik == nullptr)
                    return;
                auto state = bridge::CaptureInverseKinematics(*ik);
                fn(state);
                (void)session->Commands().Execute(
                    std::make_unique<bridge::SetInverseKinematicsCommand>(scene, selected, state, false));
                RequestRefresh();
            }

            template<typename Fn>
            void CommitLook(Fn fn)
            {
                auto* session = Session();
                if (session == nullptr)
                    return;
                auto& scene = session->Scenes().GetScene();
                const auto humanoid = RelatedHumanoid(scene, session->Selection().SelectedEntity());
                const auto* component = scene.humanoids.GetComponent(humanoid);
                if (component == nullptr)
                    return;
                auto state = bridge::CaptureHumanoidLook(*component);
                fn(state);
                (void)session->Commands().Execute(
                    std::make_unique<bridge::SetHumanoidLookCommand>(scene, humanoid, state));
                RequestRefresh();
            }

            void EnsureExpressions()
            {
                auto* session = Session();
                if (session == nullptr || !session->Selection().HasSelection())
                    return;
                auto& scene = session->Scenes().GetScene();
                auto entity = RelatedHumanoid(scene, session->Selection().SelectedEntity());
                if (entity == wi::ecs::INVALID_ENTITY)
                    entity = session->Selection().SelectedEntity();
                if (session->Commands().Execute(
                        std::make_unique<bridge::EnsureExpressionComponentCommand>(scene, entity)))
                    SetStatus("PHASE 7C // native ExpressionComponent added");
                RequestRefresh();
            }

            template<typename Fn>
            void CommitExpressionMaster(Fn fn)
            {
                auto* session = Session();
                if (session == nullptr)
                    return;
                auto& scene = session->Scenes().GetScene();
                const auto entity = RelatedExpression(scene, session->Selection().SelectedEntity());
                const auto* expression = scene.expressions.GetComponent(entity);
                if (expression == nullptr)
                    return;
                auto state = bridge::CaptureExpressionMaster(*expression);
                fn(state);
                (void)session->Commands().Execute(
                    std::make_unique<bridge::SetExpressionMasterCommand>(scene, entity, state));
                RequestRefresh();
            }

            template<typename Fn>
            void CommitExpressionItem(Fn fn)
            {
                auto* session = Session();
                if (session == nullptr)
                    return;
                auto& scene = session->Scenes().GetScene();
                const auto entity = RelatedExpression(scene, session->Selection().SelectedEntity());
                const auto* expression = scene.expressions.GetComponent(entity);
                if (expression == nullptr || selectedExpression_ >= expression->expressions.size())
                    return;
                auto state = bridge::CaptureExpressionItem(*expression, selectedExpression_);
                fn(state);
                (void)session->Commands().Execute(
                    std::make_unique<bridge::SetExpressionItemCommand>(scene, entity, state));
                RequestRefresh();
            }

            void CreateTimeline()
            {
                auto* session = Session();
                if (session == nullptr || !session->Selection().HasSelection())
                    return;
                auto& scene = session->Scenes().GetScene();
                const auto selected = session->Selection().SelectedEntity();
                auto owner = RelatedRoot(scene, selected);
                if (owner == wi::ecs::INVALID_ENTITY)
                    owner = selected;
                auto command = std::make_unique<bridge::CreateNativeAnimationCommand>(
                    scene, owner, "Renegade Timeline");
                auto* raw = command.get();
                if (session->Commands().Execute(std::move(command)))
                {
                    selectedTimelineAnimation_ = raw->CreatedEntity();
                    selectedAnimation_ = selectedTimelineAnimation_;
                    SetStatus("PHASE 7D // native Wicked timeline created");
                }
                RequestRefresh();
            }

            void RecordTimelineKey()
            {
                auto* session = Session();
                if (session == nullptr || selectedTimelineAnimation_ == wi::ecs::INVALID_ENTITY ||
                    selectedTimelineTarget_ == wi::ecs::INVALID_ENTITY ||
                    selectedTimelinePath_ >= TimelinePaths.size())
                {
                    return;
                }
                auto& scene = session->Scenes().GetScene();
                std::vector<float> values;
                std::string error;
                const auto path = TimelinePaths[selectedTimelinePath_].path;
                if (!bridge::CaptureCurrentAnimationValue(
                        scene, selectedTimelineTarget_, path, values, error))
                {
                    SetStatus("PHASE 7D // " + error);
                    return;
                }
                auto command = std::make_unique<bridge::RecordAnimationKeyCommand>(
                    scene, selectedTimelineAnimation_, selectedTimelineTarget_, path,
                    timelineInterpolation_, timelineTime_, values);
                if (session->Commands().Execute(std::move(command)))
                    SetStatus("PHASE 7D // native key recorded / replaced");
                else
                    SetStatus("PHASE 7D // key record rejected by native path validation");
                RequestRefresh();
            }

            void RefreshAnimation()
            {
                auto* session = Session();
                if (session == nullptr)
                    return;
                auto& scene = session->Scenes().GetScene();
                const auto selected = session->Selection().SelectedEntity();
                const auto clips = bridge::CollectAnimationClips(scene, selected);
                animation_.clip.ClearItems();
                if (clips.empty())
                {
                    selectedAnimation_ = wi::ecs::INVALID_ENTITY;
                    animation_.status.SetText("No native Wicked clips are attached to this hierarchy.");
                    return;
                }
                bool selectedFound = false;
                for (std::size_t index = 0; index < clips.size(); ++index)
                {
                    const auto& clip = clips[index];
                    animation_.clip.AddItem(clip.name, clip.entity);
                    if (clip.entity == selectedAnimation_)
                    {
                        animation_.clip.SetSelectedWithoutCallback(static_cast<int>(index));
                        selectedFound = true;
                    }
                }
                if (!selectedFound)
                {
                    selectedAnimation_ = clips.front().entity;
                    selectedTimelineAnimation_ = selectedAnimation_;
                    animation_.clip.SetSelectedWithoutCallback(0);
                }
                const auto* native = scene.animations.GetComponent(selectedAnimation_);
                if (native == nullptr)
                    return;
                const auto state = bridge::CaptureAnimationAuthoring(*native);
                animation_.mode.SetSelectedByUserdataWithoutCallback(static_cast<std::uint64_t>(state.playbackMode));
                const float timerMax = std::max(0.01f, state.end);
                animation_.timer.SetRange(state.start, timerMax);
                animation_.timer.SetValue(native->timer);
                animation_.amount.SetValue(state.amount);
                animation_.speed.SetValue(state.speed);
                const float rangeMax = std::max(10.0f, state.end * 2.0f + 1.0f);
                animation_.start.SetRange(0.0f, rangeMax);
                animation_.end.SetRange(0.0f, rangeMax);
                animation_.start.SetValue(state.start);
                animation_.end.SetValue(state.end);
                animation_.rootMotion.SetCheck(state.rootMotion);
                animation_.playPause.SetText(native->IsPlaying() ? "PAUSE" : "PLAY");

                animation_.rootBone.ClearItems();
                animation_.rootBone.AddItem("None", wi::ecs::INVALID_ENTITY);
                const auto root = RelatedRoot(scene, selected);
                int rootSelection = 0;
                int item = 1;
                for (std::size_t index = 0; index < scene.transforms.GetCount(); ++index)
                {
                    const auto entity = scene.transforms.GetEntity(index);
                    if (!InSubtree(scene, entity, root))
                        continue;
                    animation_.rootBone.AddItem(EntityName(scene, entity), entity);
                    if (entity == state.rootMotionBone)
                        rootSelection = item;
                    ++item;
                }
                animation_.rootBone.SetSelectedWithoutCallback(rootSelection);
                animation_.status.SetText(
                    std::to_string(clips.size()) + " clip(s) // " +
                    std::to_string(native->channels.size()) + " channels // " +
                    (state.rootMotion ? "root motion ON" : "root motion OFF"));
            }

            void RefreshRig()
            {
                auto* session = Session();
                if (session == nullptr)
                    return;
                auto& scene = session->Scenes().GetScene();
                const auto selected = session->Selection().SelectedEntity();
                const auto armature = RelatedArmature(scene, selected);
                selectedHumanoid_ = RelatedHumanoid(scene, selected);
                rig_.autoMap.SetEnabled(armature != wi::ecs::INVALID_ENTITY);
                rig_.resetPose.SetEnabled(
                    armature != wi::ecs::INVALID_ENTITY || selectedHumanoid_ != wi::ecs::INVALID_ENTITY);
                rig_.retarget.SetEnabled(selectedHumanoid_ != wi::ecs::INVALID_ENTITY);

                rig_.boneTarget.ClearItems();
                rig_.boneTarget.AddItem("None", wi::ecs::INVALID_ENTITY);
                int selectedTarget = 0;
                int item = 1;
                const auto* armatureComponent = scene.armatures.GetComponent(armature);
                const auto* humanoid = scene.humanoids.GetComponent(selectedHumanoid_);
                const auto mapped = humanoid != nullptr && selectedHumanoidBone_ < HumanoidBoneNames.size()
                    ? humanoid->bones[selectedHumanoidBone_]
                    : wi::ecs::INVALID_ENTITY;
                if (armatureComponent != nullptr)
                {
                    for (const auto bone : armatureComponent->boneCollection)
                    {
                        rig_.boneTarget.AddItem(EntityName(scene, bone), bone);
                        if (bone == mapped)
                            selectedTarget = item;
                        ++item;
                    }
                }
                rig_.boneSlot.SetSelectedWithoutCallback(static_cast<int>(selectedHumanoidBone_));
                rig_.boneTarget.SetSelectedWithoutCallback(selectedTarget);
                rig_.boneTarget.SetEnabled(humanoid != nullptr);

                std::size_t mappedCount = 0;
                if (humanoid != nullptr)
                {
                    for (const auto bone : humanoid->bones)
                        if (bone != wi::ecs::INVALID_ENTITY)
                            ++mappedCount;
                }
                rig_.status.SetText(
                    "ARMATURE: " + (armature == wi::ecs::INVALID_ENTITY ? std::string("NONE") : EntityName(scene, armature)) +
                    " // HUMANOID: " + (selectedHumanoid_ == wi::ecs::INVALID_ENTITY ? std::string("NONE") : EntityName(scene, selectedHumanoid_)) +
                    " // " + std::to_string(mappedCount) + "/" +
                    std::to_string(HumanoidBoneNames.size()) + " mapped");
            }

            void PopulateTransformCombo(
                SceneInspectorComboBox& combo,
                const wi::scene::Scene& scene,
                const wi::ecs::Entity selected,
                const wi::ecs::Entity current)
            {
                combo.ClearItems();
                combo.AddItem("None", wi::ecs::INVALID_ENTITY);
                int selectedIndex = 0;
                int item = 1;
                for (const auto entity : RelatedTransforms(scene, selected))
                {
                    combo.AddItem(EntityName(scene, entity), entity);
                    if (entity == current)
                        selectedIndex = item;
                    ++item;
                }
                combo.SetSelectedWithoutCallback(selectedIndex);
            }

            void RefreshIkExpression()
            {
                auto* session = Session();
                if (session == nullptr || !session->Selection().HasSelection())
                    return;
                auto& scene = session->Scenes().GetScene();
                const auto selected = session->Selection().SelectedEntity();
                const auto* ik = scene.inverse_kinematics.GetComponent(selected);
                ikexpr_.ensureIk.SetEnabled(scene.transforms.Contains(selected));
                ikexpr_.ikEnabled.SetEnabled(ik != nullptr);
                ikexpr_.ikTarget.SetEnabled(ik != nullptr);
                ikexpr_.ikChain.SetEnabled(ik != nullptr);
                ikexpr_.ikIterations.SetEnabled(ik != nullptr);
                if (ik != nullptr)
                {
                    const auto state = bridge::CaptureInverseKinematics(*ik);
                    ikexpr_.ikEnabled.SetCheck(state.enabled);
                    ikexpr_.ikChain.SetValue(static_cast<float>(state.chainLength));
                    ikexpr_.ikIterations.SetValue(static_cast<float>(state.iterations));
                    PopulateTransformCombo(ikexpr_.ikTarget, scene, selected, state.target);
                }
                else
                {
                    PopulateTransformCombo(ikexpr_.ikTarget, scene, selected, wi::ecs::INVALID_ENTITY);
                }

                const auto humanoidEntity = RelatedHumanoid(scene, selected);
                const auto* humanoid = scene.humanoids.GetComponent(humanoidEntity);
                const bool hasHumanoid = humanoid != nullptr;
                for (wi::gui::Widget* widget : {
                    static_cast<wi::gui::Widget*>(&ikexpr_.lookEnabled),
                    static_cast<wi::gui::Widget*>(&ikexpr_.lookTarget),
                    static_cast<wi::gui::Widget*>(&ikexpr_.headHorizontal),
                    static_cast<wi::gui::Widget*>(&ikexpr_.headVertical),
                    static_cast<wi::gui::Widget*>(&ikexpr_.headSpeed),
                    static_cast<wi::gui::Widget*>(&ikexpr_.eyeHorizontal),
                    static_cast<wi::gui::Widget*>(&ikexpr_.eyeVertical),
                    static_cast<wi::gui::Widget*>(&ikexpr_.eyeSpeed)})
                {
                    widget->SetEnabled(hasHumanoid);
                }
                if (humanoid != nullptr)
                {
                    const auto look = bridge::CaptureHumanoidLook(*humanoid);
                    ikexpr_.lookEnabled.SetCheck(look.enabled);
                    PopulateTransformCombo(ikexpr_.lookTarget, scene, selected, look.target);
                    ikexpr_.headHorizontal.SetValue(look.headHorizontalDegrees);
                    ikexpr_.headVertical.SetValue(look.headVerticalDegrees);
                    ikexpr_.headSpeed.SetValue(look.headSpeed);
                    ikexpr_.eyeHorizontal.SetValue(look.eyeHorizontalDegrees);
                    ikexpr_.eyeVertical.SetValue(look.eyeVerticalDegrees);
                    ikexpr_.eyeSpeed.SetValue(look.eyeSpeed);
                }
                else
                {
                    PopulateTransformCombo(ikexpr_.lookTarget, scene, selected, wi::ecs::INVALID_ENTITY);
                }

                const auto expressionEntity = RelatedExpression(scene, selected);
                const auto* expressions = scene.expressions.GetComponent(expressionEntity);
                ikexpr_.ensureExpressions.SetEnabled(expressions == nullptr);
                const bool hasExpressions = expressions != nullptr;
                for (wi::gui::Widget* widget : {
                    static_cast<wi::gui::Widget*>(&ikexpr_.forceTalking),
                    static_cast<wi::gui::Widget*>(&ikexpr_.blinkFrequency),
                    static_cast<wi::gui::Widget*>(&ikexpr_.blinkLength),
                    static_cast<wi::gui::Widget*>(&ikexpr_.blinkCount),
                    static_cast<wi::gui::Widget*>(&ikexpr_.lookFrequency),
                    static_cast<wi::gui::Widget*>(&ikexpr_.lookLength),
                    static_cast<wi::gui::Widget*>(&ikexpr_.expression),
                    static_cast<wi::gui::Widget*>(&ikexpr_.expressionWeight),
                    static_cast<wi::gui::Widget*>(&ikexpr_.expressionBinary),
                    static_cast<wi::gui::Widget*>(&ikexpr_.mouthOverride),
                    static_cast<wi::gui::Widget*>(&ikexpr_.blinkOverride),
                    static_cast<wi::gui::Widget*>(&ikexpr_.lookOverride)})
                {
                    widget->SetEnabled(hasExpressions);
                }
                ikexpr_.expression.ClearItems();
                if (expressions != nullptr)
                {
                    const auto master = bridge::CaptureExpressionMaster(*expressions);
                    ikexpr_.forceTalking.SetCheck(master.forceTalking);
                    ikexpr_.blinkFrequency.SetValue(master.blinkFrequency);
                    ikexpr_.blinkLength.SetValue(master.blinkLength);
                    ikexpr_.blinkCount.SetValue(static_cast<float>(master.blinkCount));
                    ikexpr_.lookFrequency.SetValue(master.lookFrequency);
                    ikexpr_.lookLength.SetValue(master.lookLength);
                    for (std::size_t index = 0; index < expressions->expressions.size(); ++index)
                    {
                        const auto& expression = expressions->expressions[index];
                        ikexpr_.expression.AddItem(
                            expression.name.empty() ? "Expression " + std::to_string(index) : expression.name,
                            index);
                    }
                    if (!expressions->expressions.empty())
                    {
                        selectedExpression_ = std::min(selectedExpression_, expressions->expressions.size() - 1);
                        ikexpr_.expression.SetSelectedWithoutCallback(static_cast<int>(selectedExpression_));
                        const auto item = bridge::CaptureExpressionItem(*expressions, selectedExpression_);
                        ikexpr_.expressionWeight.SetValue(item.weight);
                        ikexpr_.expressionBinary.SetCheck(item.binary);
                        ikexpr_.mouthOverride.SetSelectedByUserdataWithoutCallback(static_cast<std::uint64_t>(item.mouth));
                        ikexpr_.blinkOverride.SetSelectedByUserdataWithoutCallback(static_cast<std::uint64_t>(item.blink));
                        ikexpr_.lookOverride.SetSelectedByUserdataWithoutCallback(static_cast<std::uint64_t>(item.look));
                    }
                }

                ikexpr_.status.SetText(
                    std::string(ik != nullptr ? "IK" : "NO IK") + " // " +
                    (hasHumanoid ? "HUMANOID LOOK" : "NO HUMANOID") + " // " +
                    (hasExpressions ? "EXPRESSIONS" : "NO EXPRESSIONS"));
            }

            void RefreshTimeline()
            {
                auto* session = Session();
                if (session == nullptr || !session->Selection().HasSelection())
                    return;
                auto& scene = session->Scenes().GetScene();
                const auto selected = session->Selection().SelectedEntity();
                const auto clips = bridge::CollectAnimationClips(scene, selected);
                timeline_.animation.ClearItems();
                bool selectedFound = false;
                for (std::size_t index = 0; index < clips.size(); ++index)
                {
                    timeline_.animation.AddItem(clips[index].name, clips[index].entity);
                    if (clips[index].entity == selectedTimelineAnimation_)
                    {
                        timeline_.animation.SetSelectedWithoutCallback(static_cast<int>(index));
                        selectedFound = true;
                    }
                }
                if (!selectedFound && !clips.empty())
                {
                    selectedTimelineAnimation_ = clips.front().entity;
                    timeline_.animation.SetSelectedWithoutCallback(0);
                }

                timeline_.target.ClearItems();
                const auto entities = AllNamedSceneEntities(scene);
                int targetSelection = 0;
                int item = 0;
                for (const auto entity : entities)
                {
                    timeline_.target.AddItem(EntityName(scene, entity), entity);
                    if (entity == selectedTimelineTarget_)
                        targetSelection = item;
                    ++item;
                }
                if (selectedTimelineTarget_ == wi::ecs::INVALID_ENTITY && !entities.empty())
                {
                    selectedTimelineTarget_ = selected;
                    const auto found = std::find(entities.begin(), entities.end(), selectedTimelineTarget_);
                    if (found != entities.end())
                        targetSelection = static_cast<int>(found - entities.begin());
                }
                if (!entities.empty())
                    timeline_.target.SetSelectedWithoutCallback(targetSelection);
                timeline_.path.SetSelectedWithoutCallback(static_cast<int>(selectedTimelinePath_));
                timeline_.interpolation.SetSelectedByUserdataWithoutCallback(static_cast<std::uint64_t>(timelineInterpolation_));

                timeline_.channel.ClearItems();
                const auto channels = bridge::CollectTimelineChannels(scene, selectedTimelineAnimation_);
                for (std::size_t index = 0; index < channels.size(); ++index)
                {
                    const auto& channel = channels[index];
                    std::string mode = channel.interpolation == bridge::AnimationInterpolation::CubicSpline
                        ? "CUBIC" : channel.interpolation == bridge::AnimationInterpolation::Step ? "STEP" : "LINEAR";
                    timeline_.channel.AddItem(
                        std::string(PathLabel(channel.path)) + " // " +
                        EntityName(scene, channel.target) + " // " + mode +
                        " // " + std::to_string(channel.keyCount) + " keys",
                        index);
                }
                const auto* animation = scene.animations.GetComponent(selectedTimelineAnimation_);
                if (animation != nullptr)
                {
                    const float maxTime = std::max(10.0f, animation->end * 2.0f + 1.0f);
                    timeline_.time.SetRange(0.0f, maxTime);
                    timelineTime_ = std::clamp(animation->timer, 0.0f, maxTime);
                    timeline_.time.SetValue(timelineTime_);
                }
                timeline_.record.SetEnabled(animation != nullptr && selectedTimelineTarget_ != wi::ecs::INVALID_ENTITY);
                timeline_.status.SetText(
                    std::to_string(channels.size()) + " native channel(s) // Step/Linear recording // imported Cubic preserved");
            }

            static void Full(wi::gui::Widget& widget, const float x,
                const float y, const float width, const float height = 28.0f)
            {
                widget.SetPos(XMFLOAT2(x, y));
                widget.SetSize(XMFLOAT2(width, height));
                widget.SetVisible(true);
            }

            void LayoutAnimation(const InspectorSectionLayout& layout)
            {
                animation_.header.SetVisible(true);
                Full(animation_.header, 12.0f, layout.top, layout.width, layout.headerHeight);
                animation_.header.SetText(std::string(layout.expanded ? "▼  " : "▶  ") + "ANIMATION");
                if (!layout.expanded)
                {
                    SetAnimationVisible(false, true);
                    return;
                }
                float y = layout.contentTop;
                Full(animation_.status, 12.0f, y, layout.width, 42.0f); y += 46.0f;
                Full(animation_.clip, 12.0f, y, layout.width); y += 34.0f;
                const float third = (layout.width - 12.0f) / 3.0f;
                Full(animation_.fromStart, 12.0f, y, third);
                Full(animation_.playPause, 18.0f + third, y, third);
                Full(animation_.stop, 24.0f + third * 2.0f, y, layout.width - third * 2.0f - 12.0f);
                y += 34.0f;
                for (wi::gui::Widget* widget : {
                    static_cast<wi::gui::Widget*>(&animation_.mode),
                    static_cast<wi::gui::Widget*>(&animation_.timer),
                    static_cast<wi::gui::Widget*>(&animation_.amount),
                    static_cast<wi::gui::Widget*>(&animation_.speed),
                    static_cast<wi::gui::Widget*>(&animation_.start),
                    static_cast<wi::gui::Widget*>(&animation_.end),
                    static_cast<wi::gui::Widget*>(&animation_.rootMotion),
                    static_cast<wi::gui::Widget*>(&animation_.rootBone)})
                {
                    Full(*widget, 12.0f, y, layout.width);
                    y += 34.0f;
                }
            }

            void LayoutRig(const InspectorSectionLayout& layout)
            {
                rig_.header.SetVisible(true);
                Full(rig_.header, 12.0f, layout.top, layout.width, layout.headerHeight);
                rig_.header.SetText(std::string(layout.expanded ? "▼  " : "▶  ") + "RIG / RETARGET");
                if (!layout.expanded)
                {
                    SetRigVisible(false, true);
                    return;
                }
                float y = layout.contentTop;
                Full(rig_.status, 12.0f, y, layout.width, 42.0f); y += 46.0f;
                const float half = (layout.width - 6.0f) * 0.5f;
                Full(rig_.autoMap, 12.0f, y, half);
                Full(rig_.resetPose, 18.0f + half, y, half); y += 34.0f;
                Full(rig_.retarget, 12.0f, y, layout.width); y += 34.0f;
                Full(rig_.boneSlot, 12.0f, y, layout.width); y += 34.0f;
                Full(rig_.boneTarget, 12.0f, y, layout.width);
            }

            void LayoutIkExpression(const InspectorSectionLayout& layout)
            {
                ikexpr_.header.SetVisible(true);
                Full(ikexpr_.header, 12.0f, layout.top, layout.width, layout.headerHeight);
                ikexpr_.header.SetText(std::string(layout.expanded ? "▼  " : "▶  ") + "IK / EXPRESSION");
                if (!layout.expanded)
                {
                    SetIkExpressionVisible(false, true);
                    return;
                }
                float y = layout.contentTop;
                Full(ikexpr_.status, 12.0f, y, layout.width, 42.0f); y += 46.0f;
                for (wi::gui::Widget* widget : {
                    static_cast<wi::gui::Widget*>(&ikexpr_.ensureIk),
                    static_cast<wi::gui::Widget*>(&ikexpr_.ikEnabled),
                    static_cast<wi::gui::Widget*>(&ikexpr_.ikTarget),
                    static_cast<wi::gui::Widget*>(&ikexpr_.ikChain),
                    static_cast<wi::gui::Widget*>(&ikexpr_.ikIterations),
                    static_cast<wi::gui::Widget*>(&ikexpr_.lookEnabled),
                    static_cast<wi::gui::Widget*>(&ikexpr_.lookTarget),
                    static_cast<wi::gui::Widget*>(&ikexpr_.headHorizontal),
                    static_cast<wi::gui::Widget*>(&ikexpr_.headVertical),
                    static_cast<wi::gui::Widget*>(&ikexpr_.headSpeed),
                    static_cast<wi::gui::Widget*>(&ikexpr_.eyeHorizontal),
                    static_cast<wi::gui::Widget*>(&ikexpr_.eyeVertical),
                    static_cast<wi::gui::Widget*>(&ikexpr_.eyeSpeed),
                    static_cast<wi::gui::Widget*>(&ikexpr_.ensureExpressions),
                    static_cast<wi::gui::Widget*>(&ikexpr_.forceTalking),
                    static_cast<wi::gui::Widget*>(&ikexpr_.blinkFrequency),
                    static_cast<wi::gui::Widget*>(&ikexpr_.blinkLength),
                    static_cast<wi::gui::Widget*>(&ikexpr_.blinkCount),
                    static_cast<wi::gui::Widget*>(&ikexpr_.lookFrequency),
                    static_cast<wi::gui::Widget*>(&ikexpr_.lookLength),
                    static_cast<wi::gui::Widget*>(&ikexpr_.expression),
                    static_cast<wi::gui::Widget*>(&ikexpr_.expressionWeight),
                    static_cast<wi::gui::Widget*>(&ikexpr_.expressionBinary),
                    static_cast<wi::gui::Widget*>(&ikexpr_.mouthOverride),
                    static_cast<wi::gui::Widget*>(&ikexpr_.blinkOverride),
                    static_cast<wi::gui::Widget*>(&ikexpr_.lookOverride)})
                {
                    Full(*widget, 12.0f, y, layout.width);
                    y += 34.0f;
                }
            }

            void LayoutTimeline(const InspectorSectionLayout& layout)
            {
                timeline_.header.SetVisible(true);
                Full(timeline_.header, 12.0f, layout.top, layout.width, layout.headerHeight);
                timeline_.header.SetText(std::string(layout.expanded ? "▼  " : "▶  ") + "TIMELINE");
                if (!layout.expanded)
                {
                    SetTimelineVisible(false, true);
                    return;
                }
                float y = layout.contentTop;
                Full(timeline_.status, 12.0f, y, layout.width, 42.0f); y += 46.0f;
                for (wi::gui::Widget* widget : {
                    static_cast<wi::gui::Widget*>(&timeline_.animation),
                    static_cast<wi::gui::Widget*>(&timeline_.create),
                    static_cast<wi::gui::Widget*>(&timeline_.target),
                    static_cast<wi::gui::Widget*>(&timeline_.path),
                    static_cast<wi::gui::Widget*>(&timeline_.interpolation),
                    static_cast<wi::gui::Widget*>(&timeline_.time),
                    static_cast<wi::gui::Widget*>(&timeline_.record),
                    static_cast<wi::gui::Widget*>(&timeline_.channel)})
                {
                    Full(*widget, 12.0f, y, layout.width);
                    y += 34.0f;
                }
            }

            void SetAnimationVisible(const bool visible, const bool keepHeader = false)
            {
                if (!keepHeader) animation_.header.SetVisible(visible);
                for (wi::gui::Widget* widget : {
                    static_cast<wi::gui::Widget*>(&animation_.status),
                    static_cast<wi::gui::Widget*>(&animation_.clip),
                    static_cast<wi::gui::Widget*>(&animation_.fromStart),
                    static_cast<wi::gui::Widget*>(&animation_.playPause),
                    static_cast<wi::gui::Widget*>(&animation_.stop),
                    static_cast<wi::gui::Widget*>(&animation_.mode),
                    static_cast<wi::gui::Widget*>(&animation_.timer),
                    static_cast<wi::gui::Widget*>(&animation_.amount),
                    static_cast<wi::gui::Widget*>(&animation_.speed),
                    static_cast<wi::gui::Widget*>(&animation_.start),
                    static_cast<wi::gui::Widget*>(&animation_.end),
                    static_cast<wi::gui::Widget*>(&animation_.rootMotion),
                    static_cast<wi::gui::Widget*>(&animation_.rootBone)}) widget->SetVisible(visible);
            }

            void SetRigVisible(const bool visible, const bool keepHeader = false)
            {
                if (!keepHeader) rig_.header.SetVisible(visible);
                for (wi::gui::Widget* widget : {
                    static_cast<wi::gui::Widget*>(&rig_.status),
                    static_cast<wi::gui::Widget*>(&rig_.autoMap),
                    static_cast<wi::gui::Widget*>(&rig_.resetPose),
                    static_cast<wi::gui::Widget*>(&rig_.retarget),
                    static_cast<wi::gui::Widget*>(&rig_.boneSlot),
                    static_cast<wi::gui::Widget*>(&rig_.boneTarget)}) widget->SetVisible(visible);
            }

            void SetIkExpressionVisible(const bool visible, const bool keepHeader = false)
            {
                if (!keepHeader) ikexpr_.header.SetVisible(visible);
                for (wi::gui::Widget* widget : {
                    static_cast<wi::gui::Widget*>(&ikexpr_.status),
                    static_cast<wi::gui::Widget*>(&ikexpr_.ensureIk),
                    static_cast<wi::gui::Widget*>(&ikexpr_.ikEnabled),
                    static_cast<wi::gui::Widget*>(&ikexpr_.ikTarget),
                    static_cast<wi::gui::Widget*>(&ikexpr_.ikChain),
                    static_cast<wi::gui::Widget*>(&ikexpr_.ikIterations),
                    static_cast<wi::gui::Widget*>(&ikexpr_.lookEnabled),
                    static_cast<wi::gui::Widget*>(&ikexpr_.lookTarget),
                    static_cast<wi::gui::Widget*>(&ikexpr_.headHorizontal),
                    static_cast<wi::gui::Widget*>(&ikexpr_.headVertical),
                    static_cast<wi::gui::Widget*>(&ikexpr_.headSpeed),
                    static_cast<wi::gui::Widget*>(&ikexpr_.eyeHorizontal),
                    static_cast<wi::gui::Widget*>(&ikexpr_.eyeVertical),
                    static_cast<wi::gui::Widget*>(&ikexpr_.eyeSpeed),
                    static_cast<wi::gui::Widget*>(&ikexpr_.ensureExpressions),
                    static_cast<wi::gui::Widget*>(&ikexpr_.forceTalking),
                    static_cast<wi::gui::Widget*>(&ikexpr_.blinkFrequency),
                    static_cast<wi::gui::Widget*>(&ikexpr_.blinkLength),
                    static_cast<wi::gui::Widget*>(&ikexpr_.blinkCount),
                    static_cast<wi::gui::Widget*>(&ikexpr_.lookFrequency),
                    static_cast<wi::gui::Widget*>(&ikexpr_.lookLength),
                    static_cast<wi::gui::Widget*>(&ikexpr_.expression),
                    static_cast<wi::gui::Widget*>(&ikexpr_.expressionWeight),
                    static_cast<wi::gui::Widget*>(&ikexpr_.expressionBinary),
                    static_cast<wi::gui::Widget*>(&ikexpr_.mouthOverride),
                    static_cast<wi::gui::Widget*>(&ikexpr_.blinkOverride),
                    static_cast<wi::gui::Widget*>(&ikexpr_.lookOverride)}) widget->SetVisible(visible);
            }

            void SetTimelineVisible(const bool visible, const bool keepHeader = false)
            {
                if (!keepHeader) timeline_.header.SetVisible(visible);
                for (wi::gui::Widget* widget : {
                    static_cast<wi::gui::Widget*>(&timeline_.status),
                    static_cast<wi::gui::Widget*>(&timeline_.animation),
                    static_cast<wi::gui::Widget*>(&timeline_.create),
                    static_cast<wi::gui::Widget*>(&timeline_.target),
                    static_cast<wi::gui::Widget*>(&timeline_.path),
                    static_cast<wi::gui::Widget*>(&timeline_.interpolation),
                    static_cast<wi::gui::Widget*>(&timeline_.time),
                    static_cast<wi::gui::Widget*>(&timeline_.record),
                    static_cast<wi::gui::Widget*>(&timeline_.channel)}) widget->SetVisible(visible);
            }

            StudioRenderPath* owner_ = nullptr;
            wi::gui::Window* panel_ = nullptr;
            InspectorSectionRegistry* registry_ = nullptr;
            std::function<void()> requestRefresh_;
            std::function<void(std::string)> setStatus_;
            AnimationControls animation_;
            RigControls rig_;
            IkExpressionControls ikexpr_;
            TimelineControls timeline_;
            wi::ecs::Entity selectedAnimation_ = wi::ecs::INVALID_ENTITY;
            wi::ecs::Entity selectedTimelineAnimation_ = wi::ecs::INVALID_ENTITY;
            wi::ecs::Entity selectedTimelineTarget_ = wi::ecs::INVALID_ENTITY;
            wi::ecs::Entity selectedHumanoid_ = wi::ecs::INVALID_ENTITY;
            std::size_t selectedHumanoidBone_ = 0;
            std::size_t selectedExpression_ = 0;
            std::size_t selectedTimelinePath_ = 0;
            bridge::AnimationInterpolation timelineInterpolation_ =
                bridge::AnimationInterpolation::Linear;
            float timelineTime_ = 0.0f;
        };

        bool Phase7Provider::IsVisible(const InspectorSectionContext& context) const
        {
            return owner_ != nullptr && owner_->Visible(section_, context);
        }
        float Phase7Provider::MeasureContentHeight(
            const InspectorSectionContext&, const float) const
        {
            return owner_ == nullptr ? 0.0f : owner_->Measure(section_);
        }
        void Phase7Provider::Refresh(const InspectorSectionContext&)
        {
            if (owner_ != nullptr) owner_->Refresh(section_);
        }
        void Phase7Provider::ApplyLayout(
            const InspectorSectionContext&,
            const InspectorSectionLayout& layout)
        {
            if (owner_ != nullptr) owner_->Layout(section_, layout);
        }

        std::unique_ptr<Phase7AnimationInspector> activeInspector;
        StudioRenderPath* activeOwner = nullptr;
    }

    void RegisterPhase7NativeInspector(
        StudioRenderPath& owner,
        wi::gui::Window& inspectorPanel,
        InspectorSectionRegistry& registry,
        std::function<void()> requestRefresh,
        std::function<void(std::string)> setStatus)
    {
        activeInspector.reset();
        activeOwner = &owner;
        activeInspector = std::make_unique<Phase7AnimationInspector>(
            owner, inspectorPanel, registry, requestRefresh, setStatus);
        activeInspector->Register();
        RegisterPhase7SpecialistInspector(
            owner, inspectorPanel, registry,
            std::move(requestRefresh), std::move(setStatus));
    }

    void PreparePhase7NativeInspector(StudioRenderPath& owner)
    {
        if (activeOwner == &owner && activeInspector)
            activeInspector->Prepare();
        PreparePhase7SpecialistInspector(owner);
    }
}
