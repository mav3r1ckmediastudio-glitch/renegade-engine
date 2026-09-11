#include "Phase7Gate7AAnimationInspector.h"

#include "InspectorSectionFramework.h"
#include "RenegadeStudioChrome.h"
#include "StudioApplication.h"

#include "renegade/bridge/AnimationService.h"
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
        class AnimationInspector;

        class AnimationSectionProvider final : public IInspectorSectionProvider
        {
        public:
            explicit AnimationSectionProvider(AnimationInspector& owner) noexcept
                : owner_(&owner)
            {
                descriptor_.id = Phase7AnimationSectionId;
                descriptor_.title = "ANIMATION";
                descriptor_.order = 35;
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
            AnimationInspector* owner_ = nullptr;
            InspectorSectionDescriptor descriptor_;
        };

        class AnimationInspector final
        {
        public:
            AnimationInspector(
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
                header_.Create("Phase 7 Animation Section Header");
                header_.SetTooltip("Expose Wicked native AnimationComponent playback and authored clip settings.");
                header_.OnClick([this](const wi::gui::EventArgs&)
                {
                    const bool opening = !registry_->IsExpanded(Phase7AnimationSectionId);
                    if (opening)
                    {
                        if (owner_ != nullptr)
                            owner_->ResetS1BInspectorDisclosure();
                        (void)registry_->SetExpanded(Phase7AnimationSectionId, true);
                    }
                    else
                    {
                        (void)registry_->SetExpanded(Phase7AnimationSectionId, false);
                    }
                    RequestRefresh();
                });
                panel_->AddWidget(&header_);

                status_.Create("Phase 7 Animation Status");
                status_.SetText("Select an animated model.");
                status_.SetColor(wi::Color::Transparent());
                panel_->AddWidget(&status_);

                clips_.Create("Phase 7 Animation Clip");
                clips_.SetTooltip("Native Wicked animation clips related to the selected hierarchy.");
                clips_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    selectedClip_ = static_cast<wi::ecs::Entity>(args.userdata);
                    RefreshControls();
                    RequestRefresh();
                });
                panel_->AddWidget(&clips_);

                CreateButton(play_, "Animation Play", "PLAY", [this]() { Play(false); });
                CreateButton(playFromStart_, "Animation Play From Start", "FROM START", [this]() { Play(true); });
                CreateButton(pause_, "Animation Pause", "PAUSE", [this]() { Pause(); });
                CreateButton(stop_, "Animation Stop", "STOP", [this]() { Stop(); });

                mode_.Create("Animation Playback Mode");
                mode_.AddItem("LOOP", static_cast<std::uint64_t>(bridge::AnimationPlaybackMode::Loop));
                mode_.AddItem("PING-PONG", static_cast<std::uint64_t>(bridge::AnimationPlaybackMode::PingPong));
                mode_.AddItem("PLAY ONCE", static_cast<std::uint64_t>(bridge::AnimationPlaybackMode::PlayOnce));
                mode_.SetTooltip("Wicked-native loop, ping-pong or play-once mode.");
                mode_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    Commit([&](bridge::AnimationAuthoredState& state)
                    {
                        state.mode = static_cast<bridge::AnimationPlaybackMode>(args.userdata);
                    });
                });
                panel_->AddWidget(&mode_);

                CreateSlider(timer_, "Animation Timer", "TIME", "Scrub the native Wicked animation timer.", 0.0f, 1.0f, 10000.0f,
                    [this](const float value) { Scrub(value); });
                CreateSlider(speed_, "Animation Speed", "SPEED", "Authored native playback speed.", -4.0f, 4.0f, 10000.0f,
                    [this](const float value) { Commit([&](bridge::AnimationAuthoredState& state) { state.speed = value; }); });
                CreateSlider(amount_, "Animation Blend Amount", "BLEND", "Native Wicked AnimationComponent blend amount.", 0.0f, 1.0f, 10000.0f,
                    [this](const float value) { Commit([&](bridge::AnimationAuthoredState& state) { state.amount = value; }); });
                CreateSlider(start_, "Animation Start", "START", "Authored clip start time.", 0.0f, 600.0f, 10000.0f,
                    [this](const float value) { Commit([&](bridge::AnimationAuthoredState& state) { state.start = value; }); });
                CreateSlider(end_, "Animation End", "END", "Authored clip end time.", 0.0f, 600.0f, 10000.0f,
                    [this](const float value) { Commit([&](bridge::AnimationAuthoredState& state) { state.end = value; }); });

                rootMotion_.Create("Root motion: ");
                rootMotion_.SetTooltip("Use Wicked native root motion. Enabling requires a valid root-motion bone on the clip.");
                rootMotion_.OnClick([this](const wi::gui::EventArgs& args)
                {
                    auto* animation = CurrentAnimation();
                    if (animation == nullptr)
                        return;
                    if (args.bValue && animation->GetRootMotionBone() == wi::ecs::INVALID_ENTITY)
                    {
                        SetStatus("PHASE 7A // root motion needs a valid root-motion bone; setting rejected");
                        RequestRefresh();
                        return;
                    }
                    Commit([&](bridge::AnimationAuthoredState& state) { state.rootMotion = args.bValue; });
                });
                panel_->AddWidget(&rootMotion_);

                info_.Create("Phase 7 Animation Info");
                info_.SetColor(wi::Color::Transparent());
                panel_->AddWidget(&info_);

                std::string error;
                if (!registry_->Register(std::make_shared<AnimationSectionProvider>(*this), error))
                    SetStatus("PHASE 7A // " + error);
            }

            [[nodiscard]] bool IsVisible(const InspectorSectionContext& context)
            {
                if (!context.hasSelection)
                    return false;
                RefreshClipList(false);
                return !availableClips_.empty();
            }

            [[nodiscard]] float Measure() const noexcept
            {
                return 300.0f;
            }

            void Refresh()
            {
                RefreshClipList(true);
                RefreshControls();
            }

            void PrepareForLayout()
            {
                for (wi::gui::Widget* widget : Widgets())
                    widget->SetVisible(false);
            }

            void Layout(const InspectorSectionLayout& layout)
            {
                header_.SetVisible(true);
                header_.SetPos(XMFLOAT2(12.0f, layout.top));
                header_.SetSize(XMFLOAT2(layout.width, layout.headerHeight));
                header_.SetText(std::string(layout.expanded ? "▼  " : "▶  ") + "ANIMATION");
                if (!layout.expanded)
                    return;

                const float x = 12.0f;
                const float width = layout.width;
                float y = layout.contentTop;
                status_.SetVisible(true); status_.SetPos(XMFLOAT2(x, y)); status_.SetSize(XMFLOAT2(width, 20.0f)); y += 22.0f;
                clips_.SetVisible(true); clips_.SetPos(XMFLOAT2(x, y)); clips_.SetSize(XMFLOAT2(width, 28.0f)); y += 34.0f;

                const float half = (width - 8.0f) * 0.5f;
                play_.SetVisible(true); play_.SetPos(XMFLOAT2(x, y)); play_.SetSize(XMFLOAT2(half, 28.0f));
                playFromStart_.SetVisible(true); playFromStart_.SetPos(XMFLOAT2(x + half + 8.0f, y)); playFromStart_.SetSize(XMFLOAT2(half, 28.0f)); y += 32.0f;
                pause_.SetVisible(true); pause_.SetPos(XMFLOAT2(x, y)); pause_.SetSize(XMFLOAT2(half, 28.0f));
                stop_.SetVisible(true); stop_.SetPos(XMFLOAT2(x + half + 8.0f, y)); stop_.SetSize(XMFLOAT2(half, 28.0f)); y += 34.0f;

                mode_.SetVisible(true); mode_.SetPos(XMFLOAT2(x, y)); mode_.SetSize(XMFLOAT2(width, 28.0f)); y += 34.0f;
                for (SceneInspectorSlider* slider : {&timer_, &speed_, &amount_, &start_, &end_})
                {
                    slider->SetVisible(true); slider->SetPos(XMFLOAT2(x, y)); slider->SetSize(XMFLOAT2(width, 28.0f)); y += 32.0f;
                }
                rootMotion_.SetVisible(true); rootMotion_.SetPos(XMFLOAT2(x, y)); rootMotion_.SetSize(XMFLOAT2(width, 28.0f)); y += 32.0f;
                info_.SetVisible(true); info_.SetPos(XMFLOAT2(x, y)); info_.SetSize(XMFLOAT2(width, 20.0f));
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
            void CreateSlider(SceneInspectorSlider& slider, const char* name, const char* label, const char* tooltip,
                const float minimum, const float maximum, const float steps, Fn&& fn)
            {
                slider.Create(minimum, maximum, minimum, steps, name, label);
                slider.SetTooltip(tooltip);
                slider.OnValueCommitted([action = std::forward<Fn>(fn)](const float value) mutable { action(value); });
                panel_->AddWidget(&slider);
            }

            [[nodiscard]] std::vector<wi::gui::Widget*> Widgets()
            {
                return {&header_, &status_, &clips_, &play_, &playFromStart_, &pause_, &stop_, &mode_,
                    &timer_, &speed_, &amount_, &start_, &end_, &rootMotion_, &info_};
            }

            void RefreshClipList(const bool syncSelection)
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || !session->Selection().HasSelection())
                {
                    availableClips_.clear();
                    selectedClip_ = wi::ecs::INVALID_ENTITY;
                    return;
                }
                const auto selected = session->Selection().SelectedEntity();
                availableClips_ = bridge::CollectAnimationClips(session->Scenes().GetScene(), selected, true);
                if (!syncSelection)
                    return;

                const bool selectedStillExists = std::any_of(availableClips_.begin(), availableClips_.end(),
                    [this](const bridge::AnimationClipInfo& clip) { return clip.entity == selectedClip_; });
                if (!selectedStillExists)
                    selectedClip_ = availableClips_.empty() ? wi::ecs::INVALID_ENTITY : availableClips_.front().entity;

                clips_.ClearItems();
                for (const auto& clip : availableClips_)
                    clips_.AddItem(clip.name, static_cast<std::uint64_t>(clip.entity));
                if (selectedClip_ != wi::ecs::INVALID_ENTITY)
                    clips_.SetSelectedByUserdataWithoutCallback(static_cast<std::uint64_t>(selectedClip_));
            }

            [[nodiscard]] wi::scene::AnimationComponent* CurrentAnimation() const
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || selectedClip_ == wi::ecs::INVALID_ENTITY)
                    return nullptr;
                return session->Scenes().GetScene().animations.GetComponent(selectedClip_);
            }

            void RefreshControls()
            {
                auto* animation = CurrentAnimation();
                if (animation == nullptr)
                {
                    status_.SetText("No related native Wicked animation clips.");
                    return;
                }
                status_.SetText(animation->IsPlaying() ? "Native clip // PLAYING" : "Native clip // PAUSED");
                mode_.SetSelectedByUserdataWithoutCallback(static_cast<std::uint64_t>(bridge::AnimationMode(*animation)));
                timer_.SetRange(animation->start, std::max(animation->start + 0.0001f, animation->end));
                timer_.SetValue(animation->timer);
                speed_.SetValue(animation->speed);
                amount_.SetValue(animation->amount);
                start_.SetValue(animation->start);
                end_.SetValue(animation->end);
                rootMotion_.SetCheck(animation->IsRootMotion());
                const auto name = selectedClipName();
                info_.SetText(name + " // " + std::to_string(animation->channels.size()) + " channels // " +
                    std::to_string(animation->GetLength()) + " sec");
            }

            [[nodiscard]] std::string selectedClipName() const
            {
                for (const auto& clip : availableClips_)
                    if (clip.entity == selectedClip_)
                        return clip.name;
                return "Animation";
            }

            template<typename Mutator>
            void Commit(Mutator&& mutator)
            {
                auto* session = bridge::StudioSession::Current();
                auto* animation = CurrentAnimation();
                if (session == nullptr || animation == nullptr)
                    return;
                auto state = bridge::CaptureAnimationAuthoredState(*animation);
                mutator(state);
                if (session->Commands().Execute(std::make_unique<bridge::SetAnimationAuthoredStateCommand>(
                        session->Scenes().GetScene(), selectedClip_, state)))
                {
                    SetStatus("PHASE 7A // animation setting committed");
                }
                RefreshControls();
                RequestRefresh();
            }

            void Play(const bool fromStart)
            {
                auto* session = bridge::StudioSession::Current();
                if (session != nullptr && bridge::PlayAnimation(session->Scenes().GetScene(), selectedClip_, fromStart))
                    SetStatus(fromStart ? "PHASE 7A // playing from start" : "PHASE 7A // playing");
                RequestRefresh();
            }

            void Pause()
            {
                auto* session = bridge::StudioSession::Current();
                if (session != nullptr && bridge::PauseAnimation(session->Scenes().GetScene(), selectedClip_))
                    SetStatus("PHASE 7A // paused");
                RequestRefresh();
            }

            void Stop()
            {
                auto* session = bridge::StudioSession::Current();
                if (session != nullptr && bridge::StopAnimation(session->Scenes().GetScene(), selectedClip_))
                    SetStatus("PHASE 7A // stopped");
                RequestRefresh();
            }

            void Scrub(const float value)
            {
                auto* session = bridge::StudioSession::Current();
                if (session != nullptr && bridge::ScrubAnimation(session->Scenes().GetScene(), selectedClip_, value))
                    SetStatus("PHASE 7A // scrubbed native animation timer");
                RefreshControls();
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
            std::vector<bridge::AnimationClipInfo> availableClips_;
            wi::ecs::Entity selectedClip_ = wi::ecs::INVALID_ENTITY;

            SceneInspectorButton header_;
            wi::gui::Label status_;
            SceneInspectorComboBox clips_;
            SceneInspectorButton play_;
            SceneInspectorButton playFromStart_;
            SceneInspectorButton pause_;
            SceneInspectorButton stop_;
            SceneInspectorComboBox mode_;
            SceneInspectorSlider timer_;
            SceneInspectorSlider speed_;
            SceneInspectorSlider amount_;
            SceneInspectorSlider start_;
            SceneInspectorSlider end_;
            SceneInspectorCheckBox rootMotion_;
            wi::gui::Label info_;
        };

        bool AnimationSectionProvider::IsVisible(const InspectorSectionContext& context) const
        {
            return owner_ != nullptr && owner_->IsVisible(context);
        }

        float AnimationSectionProvider::MeasureContentHeight(const InspectorSectionContext&, float) const
        {
            return owner_ != nullptr ? owner_->Measure() : 0.0f;
        }

        void AnimationSectionProvider::Refresh(const InspectorSectionContext&)
        {
            if (owner_ != nullptr)
                owner_->Refresh();
        }

        void AnimationSectionProvider::ApplyLayout(const InspectorSectionContext&, const InspectorSectionLayout& layout)
        {
            if (owner_ != nullptr)
                owner_->Layout(layout);
        }

        std::unique_ptr<AnimationInspector> activeInspector;
        StudioRenderPath* activeOwner = nullptr;
    }

    void RegisterPhase7Gate7AAnimationInspector(
        StudioRenderPath& owner,
        wi::gui::Window& inspectorPanel,
        InspectorSectionRegistry& registry,
        std::function<void()> requestRefresh,
        std::function<void(std::string)> setStatus)
    {
        activeInspector.reset();
        activeOwner = &owner;
        activeInspector = std::make_unique<AnimationInspector>(
            owner, inspectorPanel, registry, std::move(requestRefresh), std::move(setStatus));
        activeInspector->Register();
    }

    void PreparePhase7Gate7AAnimationInspector(StudioRenderPath& owner)
    {
        if (activeOwner == &owner && activeInspector)
            activeInspector->PrepareForLayout();
    }
}
