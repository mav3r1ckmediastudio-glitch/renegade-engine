#include "Phase7AnimationInspector.h"

#include "InspectorSectionFramework.h"
#include "RenegadeStudioChrome.h"
#include "StudioApplication.h"

#include "renegade/bridge/AnimationService.h"
#include "renegade/bridge/StudioSession.h"

#include <algorithm>
#include <cmath>
#include <functional>
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
        class LiveAnimationStatusLabel final : public wi::gui::Label
        {
        public:
            void SetTextProvider(std::function<std::string()> provider)
            {
                provider_ = std::move(provider);
            }

            void Update(const wi::Canvas& canvas, float dt) override
            {
                if (provider_)
                    SetText(provider_());
                wi::gui::Label::Update(canvas, dt);
            }

            const char* GetWidgetTypeName() const override
            {
                return "LiveAnimationStatusLabel";
            }

        private:
            std::function<std::string()> provider_;
        };

        [[nodiscard]] std::string FormatSeconds(const float seconds)
        {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(2) << seconds << "s";
            return stream.str();
        }

        [[nodiscard]] const char* PlaybackModeLabel(
            const bridge::AnimationPlaybackMode mode) noexcept
        {
            switch (mode)
            {
            case bridge::AnimationPlaybackMode::Loop:
                return "LOOP";
            case bridge::AnimationPlaybackMode::PingPong:
                return "PING-PONG";
            case bridge::AnimationPlaybackMode::PlayOnce:
                return "PLAY ONCE";
            }
            return "LOOP";
        }

        class AnimationInspector final : public IInspectorSectionProvider
        {
        public:
            AnimationInspector(
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
                descriptor_.id = Phase7AnimationSectionId;
                descriptor_.title = "ANIMATION";
                descriptor_.order = 35;
                descriptor_.defaultExpanded = false;
                descriptor_.headerHeight = 28.0f;
                descriptor_.spacingAfter = 6.0f;
                CreateControls();
            }

            [[nodiscard]] const InspectorSectionDescriptor& Descriptor()
                const noexcept override
            {
                return descriptor_;
            }

            [[nodiscard]] bool IsVisible(
                const InspectorSectionContext& context) const override
            {
                if (!context.hasSelection)
                    return false;
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || !session->Selection().HasSelection())
                    return false;
                const auto selected = session->Selection().SelectedEntity();
                if (selected == wi::ecs::INVALID_ENTITY)
                    return false;
                return !bridge::InspectAnimationsForSelection(
                    session->Scenes().GetScene(), selected).empty();
            }

            [[nodiscard]] float MeasureContentHeight(
                const InspectorSectionContext&,
                float) const override
            {
                return 382.0f;
            }

            void Refresh(const InspectorSectionContext&) override
            {
                RefreshState();
            }

            void ApplyLayout(
                const InspectorSectionContext&,
                const InspectorSectionLayout& layout) override
            {
                header_.SetVisible(true);
                header_.SetPos(XMFLOAT2(12.0f, layout.top));
                header_.SetSize(XMFLOAT2(layout.width, layout.headerHeight));
                header_.SetText(
                    std::string(layout.expanded ? "▼  " : "▶  ") +
                    descriptor_.title);

                if (!layout.expanded)
                {
                    SetContentVisible(false);
                    return;
                }

                SetContentVisible(true);
                const float x = 12.0f;
                const float width = layout.width;
                float y = layout.contentTop;
                constexpr float rowHeight = 28.0f;
                constexpr float gap = 6.0f;

                clip_.SetPos(XMFLOAT2(x, y));
                clip_.SetSize(XMFLOAT2(width, rowHeight));
                y += rowHeight + gap;

                liveStatus_.SetPos(XMFLOAT2(x, y));
                liveStatus_.SetSize(XMFLOAT2(width, 22.0f));
                y += 22.0f + gap;

                const float transportWidth = std::max(1.0f, (width - gap * 3.0f) / 4.0f);
                playFromStart_.SetPos(XMFLOAT2(x, y));
                playFromStart_.SetSize(XMFLOAT2(transportWidth, rowHeight));
                play_.SetPos(XMFLOAT2(x + (transportWidth + gap), y));
                play_.SetSize(XMFLOAT2(transportWidth, rowHeight));
                pause_.SetPos(XMFLOAT2(x + (transportWidth + gap) * 2.0f, y));
                pause_.SetSize(XMFLOAT2(transportWidth, rowHeight));
                stop_.SetPos(XMFLOAT2(x + (transportWidth + gap) * 3.0f, y));
                stop_.SetSize(XMFLOAT2(transportWidth, rowHeight));
                y += rowHeight + gap;

                timer_.SetPos(XMFLOAT2(x, y));
                timer_.SetSize(XMFLOAT2(width, rowHeight));
                y += rowHeight + gap;

                playbackMode_.SetPos(XMFLOAT2(x, y));
                playbackMode_.SetSize(XMFLOAT2(width, rowHeight));
                y += rowHeight + gap;

                blend_.SetPos(XMFLOAT2(x, y));
                blend_.SetSize(XMFLOAT2(width, rowHeight));
                y += rowHeight + gap;

                speed_.SetPos(XMFLOAT2(x, y));
                speed_.SetSize(XMFLOAT2(width, rowHeight));
                y += rowHeight + gap;

                const float rangeWidth = std::max(1.0f, (width - gap) * 0.5f);
                start_.SetPos(XMFLOAT2(x, y));
                start_.SetSize(XMFLOAT2(rangeWidth, rowHeight));
                end_.SetPos(XMFLOAT2(x + rangeWidth + gap, y));
                end_.SetSize(XMFLOAT2(rangeWidth, rowHeight));
                y += rowHeight + gap;

                rootMotion_.SetPos(XMFLOAT2(x, y));
                rootMotion_.SetSize(XMFLOAT2(width, rowHeight));
            }

            void PrepareForLayout()
            {
                header_.SetVisible(false);
                SetContentVisible(false);
            }

        private:
            [[nodiscard]] bridge::AnimationClipState* SelectedClip() noexcept
            {
                const auto it = std::find_if(
                    clips_.begin(), clips_.end(),
                    [this](const bridge::AnimationClipState& clip)
                    {
                        return clip.entity == selectedAnimation_;
                    });
                return it == clips_.end() ? nullptr : &*it;
            }

            [[nodiscard]] const bridge::AnimationClipState* SelectedClip() const noexcept
            {
                const auto it = std::find_if(
                    clips_.begin(), clips_.end(),
                    [this](const bridge::AnimationClipState& clip)
                    {
                        return clip.entity == selectedAnimation_;
                    });
                return it == clips_.end() ? nullptr : &*it;
            }

            void RefreshState()
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || !session->Selection().HasSelection())
                {
                    clips_.clear();
                    selectedAnimation_ = wi::ecs::INVALID_ENTITY;
                    return;
                }

                clips_ = bridge::InspectAnimationsForSelection(
                    session->Scenes().GetScene(),
                    session->Selection().SelectedEntity());
                if (clips_.empty())
                {
                    selectedAnimation_ = wi::ecs::INVALID_ENTITY;
                    return;
                }

                if (std::none_of(
                        clips_.begin(), clips_.end(),
                        [this](const bridge::AnimationClipState& clip)
                        {
                            return clip.entity == selectedAnimation_;
                        }))
                {
                    selectedAnimation_ = clips_.front().entity;
                }

                clip_.ClearItems();
                for (const auto& clip : clips_)
                {
                    clip_.AddItem(
                        clip.name,
                        static_cast<std::uint64_t>(clip.entity));
                }
                clip_.SetSelectedByUserdataWithoutCallback(
                    static_cast<std::uint64_t>(selectedAnimation_));

                const auto* selected = SelectedClip();
                if (selected == nullptr)
                    return;

                const float timerMaximum = std::max(
                    selected->authored.start + 0.001f,
                    selected->authored.end);
                timer_.SetRange(selected->authored.start, timerMaximum);
                timer_.SetValue(std::clamp(
                    selected->timer,
                    selected->authored.start,
                    timerMaximum));
                blend_.SetValue(selected->authored.amount);
                speed_.SetValue(std::clamp(
                    std::abs(selected->authored.speed), 0.0f, 4.0f));
                playbackMode_.SetSelectedByUserdataWithoutCallback(
                    static_cast<std::uint64_t>(selected->authored.playbackMode));
                start_.SetValue(selected->authored.start);
                end_.SetValue(selected->authored.end);
                rootMotion_.SetCheck(selected->authored.rootMotion);
            }

            [[nodiscard]] std::string LiveStatusText() const
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || selectedAnimation_ == wi::ecs::INVALID_ENTITY)
                    return "NO ANIMATION";
                const auto& scene = session->Scenes().GetScene();
                const auto* animation = scene.animations.GetComponent(selectedAnimation_);
                if (animation == nullptr)
                    return "ANIMATION UNAVAILABLE";

                std::string state = animation->IsPlaying()
                    ? "PLAYING"
                    : animation->IsEnded() ? "ENDED" : "PAUSED";
                return state + "  //  " + FormatSeconds(animation->timer) +
                    " / " + FormatSeconds(animation->GetLength()) +
                    "  //  " + std::to_string(clips_.size()) +
                    (clips_.size() == 1 ? " CLIP" : " CLIPS");
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

            void RunPreview(
                const char* action,
                const std::function<bool(wi::scene::Scene&, wi::ecs::Entity)>& operation)
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || selectedAnimation_ == wi::ecs::INVALID_ENTITY)
                    return;
                if (operation(session->Scenes().GetScene(), selectedAnimation_))
                {
                    SetStatus(std::string("ANIMATION // ") + action);
                    RequestRefresh();
                }
            }

            void CommitAuthored(
                const char* action,
                const std::function<void(bridge::AnimationAuthoredState&)>& edit)
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || selectedAnimation_ == wi::ecs::INVALID_ENTITY)
                    return;

                auto& scene = session->Scenes().GetScene();
                const auto before = bridge::CaptureAnimationAuthoredState(
                    scene, selectedAnimation_);
                auto after = before;
                edit(after);
                after = bridge::SanitizeAnimationAuthoredState(after);
                if (bridge::AnimationAuthoredStateEquals(before, after))
                {
                    RequestRefresh();
                    return;
                }

                auto command = std::make_unique<bridge::SetAnimationAuthoredStateCommand>(
                    scene, selectedAnimation_, before, after);
                if (session->Commands().Execute(std::move(command)))
                {
                    SetStatus(std::string("ANIMATION // ") + action);
                    RequestRefresh();
                }
            }

            void CreateControls()
            {
                header_.Create("Phase 7 Animation Header");
                header_.SetTooltip("Expand native Wicked animation playback controls.");
                header_.OnClick([this](const wi::gui::EventArgs&)
                {
                    (void)registry_->ToggleExpanded(Phase7AnimationSectionId);
                    RequestRefresh();
                });
                panel_->AddWidget(&header_);

                clip_.Create("Animation Clip");
                clip_.SetTooltip("Native Wicked AnimationComponent in the selected asset hierarchy.");
                clip_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    selectedAnimation_ = static_cast<wi::ecs::Entity>(args.userdata);
                    SetStatus("ANIMATION // CLIP SELECTED");
                    RequestRefresh();
                });
                panel_->AddWidget(&clip_);

                liveStatus_.Create("Animation Live Status");
                liveStatus_.SetTextProvider([this]() { return LiveStatusText(); });
                panel_->AddWidget(&liveStatus_);

                playFromStart_.Create("Animation Play From Start");
                playFromStart_.SetText("FROM START");
                playFromStart_.OnClick([this](const wi::gui::EventArgs&)
                {
                    RunPreview("PLAY FROM START", bridge::PlayAnimationPreviewFromStart);
                });
                panel_->AddWidget(&playFromStart_);

                play_.Create("Animation Play");
                play_.SetText("PLAY");
                play_.OnClick([this](const wi::gui::EventArgs&)
                {
                    RunPreview("PLAY", bridge::PlayAnimationPreview);
                });
                panel_->AddWidget(&play_);

                pause_.Create("Animation Pause");
                pause_.SetText("PAUSE");
                pause_.OnClick([this](const wi::gui::EventArgs&)
                {
                    RunPreview("PAUSE", bridge::PauseAnimationPreview);
                });
                panel_->AddWidget(&pause_);

                stop_.Create("Animation Stop");
                stop_.SetText("STOP");
                stop_.OnClick([this](const wi::gui::EventArgs&)
                {
                    RunPreview("STOP", bridge::StopAnimationPreview);
                });
                panel_->AddWidget(&stop_);

                timer_.Create(0.0f, 1.0f, 0.0f, 1000.0f,
                    "Animation Time", "TIME");
                timer_.SetTooltip("Preview scrub only. Scrubbing does not dirty the scene.");
                timer_.OnValuePreview([this](const float value)
                {
                    auto* session = bridge::StudioSession::Current();
                    if (session != nullptr && selectedAnimation_ != wi::ecs::INVALID_ENTITY)
                    {
                        (void)bridge::ScrubAnimationPreview(
                            session->Scenes().GetScene(), selectedAnimation_, value);
                    }
                });
                timer_.OnValueCommitted([this](const float value)
                {
                    auto* session = bridge::StudioSession::Current();
                    if (session != nullptr && selectedAnimation_ != wi::ecs::INVALID_ENTITY)
                    {
                        (void)bridge::ScrubAnimationPreview(
                            session->Scenes().GetScene(), selectedAnimation_, value);
                        SetStatus("ANIMATION // PREVIEW SCRUB");
                    }
                });
                panel_->AddWidget(&timer_);

                playbackMode_.Create("Animation Playback Mode");
                playbackMode_.AddItem("LOOP", static_cast<std::uint64_t>(bridge::AnimationPlaybackMode::Loop));
                playbackMode_.AddItem("PING-PONG", static_cast<std::uint64_t>(bridge::AnimationPlaybackMode::PingPong));
                playbackMode_.AddItem("PLAY ONCE", static_cast<std::uint64_t>(bridge::AnimationPlaybackMode::PlayOnce));
                playbackMode_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    const auto mode = static_cast<bridge::AnimationPlaybackMode>(args.userdata);
                    CommitAuthored(PlaybackModeLabel(mode),
                        [mode](bridge::AnimationAuthoredState& state)
                        {
                            state.playbackMode = mode;
                        });
                });
                panel_->AddWidget(&playbackMode_);

                blend_.Create(0.0f, 1.0f, 1.0f, 1000.0f,
                    "Animation Blend", "BLEND");
                blend_.SetTooltip("Wicked AnimationComponent blend amount.");
                blend_.OnValueCommitted([this](const float value)
                {
                    CommitAuthored("BLEND",
                        [value](bridge::AnimationAuthoredState& state)
                        {
                            state.amount = value;
                        });
                });
                panel_->AddWidget(&blend_);

                speed_.Create(0.0f, 4.0f, 1.0f, 4000.0f,
                    "Animation Speed", "SPEED");
                speed_.SetTooltip("Native animation playback speed multiplier.");
                speed_.OnValueCommitted([this](const float value)
                {
                    CommitAuthored("SPEED",
                        [value](bridge::AnimationAuthoredState& state)
                        {
                            state.speed = value;
                        });
                });
                panel_->AddWidget(&speed_);

                start_.Create("Animation Start");
                start_.SetFloatPrecision(3);
                start_.SetTooltip("Authored native animation start time. Press Enter to commit.");
                start_.OnInputAccepted([this](const wi::gui::EventArgs& args)
                {
                    const float value = std::isfinite(args.fValue) ? args.fValue : 0.0f;
                    CommitAuthored("START RANGE",
                        [value](bridge::AnimationAuthoredState& state)
                        {
                            state.start = value;
                            state.end = std::max(state.end, value);
                        });
                });
                panel_->AddWidget(&start_);

                end_.Create("Animation End");
                end_.SetFloatPrecision(3);
                end_.SetTooltip("Authored native animation end time. Press Enter to commit.");
                end_.OnInputAccepted([this](const wi::gui::EventArgs& args)
                {
                    const float value = std::isfinite(args.fValue) ? args.fValue : 0.0f;
                    CommitAuthored("END RANGE",
                        [value](bridge::AnimationAuthoredState& state)
                        {
                            state.end = std::max(state.start, value);
                        });
                });
                panel_->AddWidget(&end_);

                rootMotion_.Create("ROOT MOTION: ");
                rootMotion_.SetTooltip(
                    "Enable Wicked native root-motion evaluation. Root-motion bone ownership remains native.");
                rootMotion_.OnClick([this](const wi::gui::EventArgs& args)
                {
                    CommitAuthored(args.bValue ? "ROOT MOTION ON" : "ROOT MOTION OFF",
                        [enabled = args.bValue](bridge::AnimationAuthoredState& state)
                        {
                            state.rootMotion = enabled;
                        });
                });
                panel_->AddWidget(&rootMotion_);

                SetContentVisible(false);
                header_.SetVisible(false);
            }

            void SetContentVisible(const bool visible)
            {
                clip_.SetVisible(visible);
                liveStatus_.SetVisible(visible);
                playFromStart_.SetVisible(visible);
                play_.SetVisible(visible);
                pause_.SetVisible(visible);
                stop_.SetVisible(visible);
                timer_.SetVisible(visible);
                playbackMode_.SetVisible(visible);
                blend_.SetVisible(visible);
                speed_.SetVisible(visible);
                start_.SetVisible(visible);
                end_.SetVisible(visible);
                rootMotion_.SetVisible(visible);
            }

            StudioRenderPath* owner_ = nullptr;
            wi::gui::Window* panel_ = nullptr;
            InspectorSectionRegistry* registry_ = nullptr;
            std::function<void()> requestRefresh_;
            std::function<void(std::string)> setStatus_;
            InspectorSectionDescriptor descriptor_;
            std::vector<bridge::AnimationClipState> clips_;
            wi::ecs::Entity selectedAnimation_ = wi::ecs::INVALID_ENTITY;

            SceneInspectorButton header_;
            SceneInspectorComboBox clip_;
            LiveAnimationStatusLabel liveStatus_;
            SceneInspectorButton playFromStart_;
            SceneInspectorButton play_;
            SceneInspectorButton pause_;
            SceneInspectorButton stop_;
            SceneInspectorSlider timer_;
            SceneInspectorComboBox playbackMode_;
            SceneInspectorSlider blend_;
            SceneInspectorSlider speed_;
            SceneInspectorTextInputField start_;
            SceneInspectorTextInputField end_;
            SceneInspectorCheckBox rootMotion_;
        };

        std::unique_ptr<AnimationInspector> activeInspector;
        StudioRenderPath* activeOwner = nullptr;
    }

    void RegisterPhase7AnimationInspector(
        StudioRenderPath& owner,
        wi::gui::Window& inspectorPanel,
        InspectorSectionRegistry& registry,
        std::function<void()> requestRefresh,
        std::function<void(std::string)> setStatus)
    {
        activeInspector = std::make_unique<AnimationInspector>(
            owner,
            inspectorPanel,
            registry,
            std::move(requestRefresh),
            std::move(setStatus));
        activeOwner = &owner;

        std::string error;
        std::shared_ptr<IInspectorSectionProvider> provider(
            activeInspector.get(),
            [](IInspectorSectionProvider*) {});
        if (!registry.Register(std::move(provider), error) && setStatus)
            setStatus("PHASE 7A ANIMATION // " + error);
    }

    void PreparePhase7AnimationInspector(StudioRenderPath& owner)
    {
        if (activeOwner == &owner && activeInspector)
            activeInspector->PrepareForLayout();
    }
}
