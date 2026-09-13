#include "Phase7Gate7DNativeTimelineInspector.h"

#include "InspectorSectionFramework.h"
#include "Phase7Gate7AAnimationInspector.h"
#include "Phase7Gate7BHumanoidRetargetInspector.h"
#include "Phase7Gate7CCharacterControlsInspector.h"
#include "RenegadeStudioChrome.h"
#include "S4BScriptAttachmentInspector.h"
#include "S4DGlobalScriptInspector.h"
#include "StudioApplication.h"

#include "renegade/bridge/AnimationService.h"
#include "renegade/bridge/AnimationTimelineService.h"
#include "renegade/bridge/StudioSession.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_set>
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

        std::string KeyLabel(const std::size_t index, const float time)
        {
            std::ostringstream stream;
            stream << "KEY " << index << " // " << std::fixed << std::setprecision(3) << time << " s";
            return stream.str();
        }

        class NativeTimelineInspector;

        class NativeTimelineSectionProvider final : public IInspectorSectionProvider
        {
        public:
            explicit NativeTimelineSectionProvider(NativeTimelineInspector& owner) noexcept
                : owner_(&owner)
            {
                descriptor_.id = Phase7NativeTimelineSectionId;
                descriptor_.title = "TIMELINE / KEYFRAMES";
                descriptor_.order = 37;
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
            NativeTimelineInspector* owner_ = nullptr;
            InspectorSectionDescriptor descriptor_;
        };

        class NativeTimelineInspector final
        {
        public:
            NativeTimelineInspector(
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
                header_.Create("Phase 7D Timeline Section Header");
                header_.SetTooltip(
                    "Author Wicked-native animation channels and AnimationDataComponent keyframes without opening the stock Wicked Editor.");
                header_.OnClick([this](const wi::gui::EventArgs&)
                {
                    const bool opening = !registry_->IsExpanded(Phase7NativeTimelineSectionId);
                    for (const char* sectionId : {
                        "transform", "rendering", "materials",
                        S4BActionSectionId, S4BScriptSectionId, S4DGlobalScriptSectionId,
                        Phase7AnimationSectionId, Phase7HumanoidRetargetSectionId,
                        Phase7CharacterControlsSectionId, Phase7NativeTimelineSectionId})
                    {
                        (void)registry_->SetExpanded(sectionId, false);
                    }
                    if (opening)
                        (void)registry_->SetExpanded(Phase7NativeTimelineSectionId, true);
                    RequestRefresh();
                });
                panel_->AddWidget(&header_);

                status_.Create("Phase 7D Timeline Status");
                status_.SetColor(wi::Color::Transparent());
                panel_->AddWidget(&status_);

                CreateButton(
                    newClip_, "Timeline New Clip", "NEW CLIP", [this]() { CreateClip(); });
                newClip_.SetTooltip(
                    "Create an empty Wicked-native AnimationComponent so scene animation can be authored without importing a pre-existing clip.");

                clips_.Create("Timeline Clip");
                clips_.SetTooltip("Choose any native AnimationComponent in the current scene.");
                clips_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    selectedClip_ = static_cast<wi::ecs::Entity>(args.userdata);
                    selectedChannel_ = 0;
                    selectedKey_ = 0;
                    RefreshControls();
                    RequestRefresh();
                });
                panel_->AddWidget(&clips_);

                CreateSlider(
                    timer_, "Timeline Timer", "TIME",
                    "Scrub the selected Wicked AnimationComponent. Scrubbing is preview-only and is not added to Undo history.",
                    0.0f, 600.0f,
                    [this](const float value) { Scrub(value); });

                target_.Create("Timeline Record Target");
                target_.SetTooltip("Entity whose current native component state will be written into the selected clip.");
                target_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    selectedTarget_ = static_cast<wi::ecs::Entity>(args.userdata);
                    RefreshRecordStatus();
                });
                panel_->AddWidget(&target_);

                preset_.Create("Timeline Record Type");
                for (std::uint32_t value = static_cast<std::uint32_t>(bridge::TimelineRecordPreset::Transform);
                    value <= static_cast<std::uint32_t>(bridge::TimelineRecordPreset::MaterialTexMulAdd);
                    ++value)
                {
                    const auto recordPreset = static_cast<bridge::TimelineRecordPreset>(value);
                    if (recordPreset == bridge::TimelineRecordPreset::ScriptPlay ||
                        recordPreset == bridge::TimelineRecordPreset::ScriptStop)
                    {
                        continue;
                    }
                    preset_.AddItem(bridge::TimelineRecordPresetLabel(recordPreset), value);
                }
                preset_.SetTooltip(
                    "Wicked-native animation paths only. Renegade ACTION/SCRIPT/GLOBAL SCRIPT events use the governed .rscripts runtime and are not exposed here as legacy Wicked ScriptComponent events.");
                preset_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    selectedPreset_ = static_cast<bridge::TimelineRecordPreset>(args.userdata);
                    RefreshRecordStatus();
                });
                panel_->AddWidget(&preset_);

                CreateButton(record_, "Timeline Record Key", "RECORD KEY", [this]() { RecordKey(); });
                CreateButton(closeLoop_, "Timeline Close Loop", "CLOSE LOOP", [this]() { CloseLoop(); });
                closeLoop_.SetTooltip(
                    "Duplicate first VALUE keys at the current time to close a visual loop. Event channels such as SOUND PLAY/STOP are deliberately excluded.");

                channels_.Create("Timeline Channel");
                channels_.SetTooltip("Native AnimationComponent channel target and path.");
                channels_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    selectedChannel_ = static_cast<std::size_t>(args.userdata);
                    selectedKey_ = 0;
                    RefreshChannelControls();
                    RequestRefresh();
                });
                panel_->AddWidget(&channels_);

                interpolation_.Create("Timeline Sampling");
                interpolation_.AddItem("LINEAR", wi::scene::AnimationComponent::AnimationSampler::LINEAR);
                interpolation_.AddItem("STEP", wi::scene::AnimationComponent::AnimationSampler::STEP);
                interpolation_.AddItem("CUBIC SPLINE (IMPORTED)", wi::scene::AnimationComponent::AnimationSampler::CUBICSPLINE);
                interpolation_.SetTooltip(
                    "Linear and Step are authored natively. Imported cubic spline data is preserved; creating new cubic tangents is intentionally unsupported.");
                interpolation_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    SetSamplerMode(static_cast<bridge::AnimationSamplerMode>(args.userdata));
                });
                panel_->AddWidget(&interpolation_);

                keys_.Create("Timeline Key");
                keys_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    selectedKey_ = static_cast<std::size_t>(args.userdata);
                    RefreshKeyControls();
                    RequestRefresh();
                });
                panel_->AddWidget(&keys_);

                CreateSlider(
                    keyTime_, "Timeline Key Time", "KEY TIME",
                    "Move the selected native key and keep its data chunk paired while the channel is re-sorted.",
                    0.0f, 600.0f,
                    [this](const float value) { MoveKey(value); });

                CreateButton(deleteKey_, "Timeline Delete Key", "DELETE KEY", [this]() { DeleteKey(); });

                info_.Create("Phase 7D Timeline Info");
                info_.SetColor(wi::Color::Transparent());
                info_.SetFitTextEnabled(true);
                panel_->AddWidget(&info_);

                std::string error;
                if (!registry_->Register(std::make_shared<NativeTimelineSectionProvider>(*this), error))
                    SetStatus("PHASE 7D // " + error);
            }

            [[nodiscard]] bool IsVisible(const InspectorSectionContext&)
            {
                return bridge::StudioSession::Current() != nullptr;
            }

            [[nodiscard]] float Measure() const noexcept
            {
                return 472.0f;
            }

            void Refresh()
            {
                RefreshClipList(true);
                RefreshTargetList();
                RefreshControls();
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
                header_.SetText(std::string(layout.expanded ? "▼  " : "▶  ") + "TIMELINE / KEYFRAMES");
                if (!layout.expanded)
                    return;

                const float x = 12.0f;
                const float width = layout.width;
                float y = layout.contentTop;
                auto place = [&](wi::gui::Widget& widget, const float height = 28.0f)
                {
                    widget.SetVisible(true);
                    widget.SetPos(XMFLOAT2(x, y));
                    widget.SetSize(XMFLOAT2(width, height));
                    y += height + 6.0f;
                };

                place(status_, 20.0f);
                place(newClip_);
                place(clips_);
                place(timer_);
                place(target_);
                place(preset_);

                const float half = (width - 8.0f) * 0.5f;
                record_.SetVisible(true);
                record_.SetPos(XMFLOAT2(x, y));
                record_.SetSize(XMFLOAT2(half, 28.0f));
                closeLoop_.SetVisible(true);
                closeLoop_.SetPos(XMFLOAT2(x + half + 8.0f, y));
                closeLoop_.SetSize(XMFLOAT2(half, 28.0f));
                y += 34.0f;

                place(channels_);
                place(interpolation_);
                place(keys_);
                place(keyTime_);
                place(deleteKey_);
                place(info_, 42.0f);
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
            void CreateSlider(
                SceneInspectorSlider& slider,
                const char* name,
                const char* label,
                const char* tooltip,
                const float minimum,
                const float maximum,
                Fn&& fn)
            {
                slider.Create(minimum, maximum, minimum, 10000.0f, name, label);
                slider.SetTooltip(tooltip);
                slider.OnValueCommitted([action = std::forward<Fn>(fn)](const float value) mutable { action(value); });
                panel_->AddWidget(&slider);
            }

            [[nodiscard]] std::vector<wi::gui::Widget*> Widgets()
            {
                return {
                    &header_, &status_, &newClip_, &clips_, &timer_, &target_, &preset_, &record_, &closeLoop_,
                    &channels_, &interpolation_, &keys_, &keyTime_, &deleteKey_, &info_,
                };
            }

            void RefreshClipList(const bool repopulate)
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr)
                {
                    availableClips_.clear();
                    selectedClip_ = wi::ecs::INVALID_ENTITY;
                    return;
                }
                const auto selected = session->Selection().HasSelection()
                    ? session->Selection().SelectedEntity()
                    : wi::ecs::INVALID_ENTITY;
                availableClips_ = bridge::CollectAnimationClips(
                    session->Scenes().GetScene(), selected, false);
                if (!repopulate)
                    return;

                const bool keep = std::any_of(availableClips_.begin(), availableClips_.end(),
                    [this](const bridge::AnimationClipInfo& clip) { return clip.entity == selectedClip_; });
                if (!keep)
                    selectedClip_ = availableClips_.empty() ? wi::ecs::INVALID_ENTITY : availableClips_.front().entity;

                clips_.ClearItems();
                for (const auto& clip : availableClips_)
                    clips_.AddItem(clip.name, static_cast<std::uint64_t>(clip.entity));
                clips_.SetEnabled(!availableClips_.empty());
                if (selectedClip_ != wi::ecs::INVALID_ENTITY)
                    clips_.SetSelectedByUserdataWithoutCallback(static_cast<std::uint64_t>(selectedClip_));
            }

            void RefreshTargetList()
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr)
                    return;
                auto& scene = session->Scenes().GetScene();
                std::vector<wi::ecs::Entity> entities;
                std::unordered_set<wi::ecs::Entity> seen;
                auto append = [&](const auto& manager)
                {
                    for (std::size_t i = 0; i < manager.GetCount(); ++i)
                    {
                        const auto entity = manager.GetEntity(i);
                        if (seen.insert(entity).second)
                            entities.push_back(entity);
                    }
                };
                append(scene.transforms);
                append(scene.meshes);
                append(scene.objects);
                append(scene.lights);
                append(scene.sounds);
                append(scene.emitters);
                append(scene.cameras);
                append(scene.materials);

                const auto sceneSelection = session->Selection().HasSelection()
                    ? session->Selection().SelectedEntity()
                    : wi::ecs::INVALID_ENTITY;
                if (sceneSelection != wi::ecs::INVALID_ENTITY && seen.insert(sceneSelection).second)
                    entities.insert(entities.begin(), sceneSelection);

                if (selectedTarget_ == wi::ecs::INVALID_ENTITY || seen.find(selectedTarget_) == seen.end())
                    selectedTarget_ = sceneSelection != wi::ecs::INVALID_ENTITY
                        ? sceneSelection
                        : (entities.empty() ? wi::ecs::INVALID_ENTITY : entities.front());

                std::sort(entities.begin(), entities.end(),
                    [&scene](const wi::ecs::Entity left, const wi::ecs::Entity right)
                    {
                        return EntityName(scene, left) < EntityName(scene, right);
                    });
                target_.ClearItems();
                for (const auto entity : entities)
                    target_.AddItem(EntityName(scene, entity), static_cast<std::uint64_t>(entity));
                target_.SetEnabled(!entities.empty());
                if (selectedTarget_ != wi::ecs::INVALID_ENTITY)
                    target_.SetSelectedByUserdataWithoutCallback(static_cast<std::uint64_t>(selectedTarget_));
                preset_.SetSelectedByUserdataWithoutCallback(static_cast<std::uint64_t>(selectedPreset_));
            }

            [[nodiscard]] wi::scene::AnimationComponent* CurrentAnimation() const
            {
                auto* session = bridge::StudioSession::Current();
                return session == nullptr || selectedClip_ == wi::ecs::INVALID_ENTITY
                    ? nullptr
                    : session->Scenes().GetScene().animations.GetComponent(selectedClip_);
            }

            void RefreshControls()
            {
                auto* session = bridge::StudioSession::Current();
                auto* animation = CurrentAnimation();
                if (session == nullptr || animation == nullptr)
                {
                    status_.SetText("No native animation clips. Use NEW CLIP to begin authoring.");
                    timer_.SetEnabled(false);
                    record_.SetEnabled(false);
                    closeLoop_.SetEnabled(false);
                    channels_.ClearItems();
                    keys_.ClearItems();
                    info_.SetText("NEW CLIP creates an empty native Wicked AnimationComponent with Undo/Redo.");
                    return;
                }

                timer_.SetEnabled(true);
                closeLoop_.SetEnabled(true);
                status_.SetText("Native timeline // " + std::to_string(animation->channels.size()) + " channels");
                timer_.SetRange(animation->start, std::max(animation->start + 0.0001f, animation->end));
                timer_.SetValue(animation->timer);
                RefreshRecordStatus();
                RefreshChannelList();
            }

            void RefreshRecordStatus()
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr)
                    return;
                std::string reason;
                const bool canRecord = bridge::CanRecordTimelinePreset(
                    session->Scenes().GetScene(), selectedTarget_, selectedPreset_, &reason);
                record_.SetEnabled(canRecord && CurrentAnimation() != nullptr);
                if (!canRecord && CurrentAnimation() != nullptr)
                    info_.SetText("Record target: " + reason);
            }

            void RefreshChannelList()
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || selectedClip_ == wi::ecs::INVALID_ENTITY)
                    return;
                auto& scene = session->Scenes().GetScene();
                channelsData_ = bridge::CollectTimelineChannels(scene, selectedClip_);
                if (selectedChannel_ >= channelsData_.size())
                    selectedChannel_ = 0;

                channels_.ClearItems();
                for (const auto& channel : channelsData_)
                {
                    std::string label = EntityName(scene, channel.target) + " // " +
                        bridge::TimelinePathLabel(channel.path) + " // " +
                        std::to_string(channel.keyframeCount) + " keys";
                    channels_.AddItem(label, static_cast<std::uint64_t>(channel.index));
                }
                channels_.SetEnabled(!channelsData_.empty());
                if (!channelsData_.empty())
                    channels_.SetSelectedByUserdataWithoutCallback(static_cast<std::uint64_t>(selectedChannel_));
                RefreshChannelControls();
            }

            void RefreshChannelControls()
            {
                const bool hasChannel = selectedChannel_ < channelsData_.size();
                interpolation_.SetEnabled(hasChannel);
                keys_.SetEnabled(hasChannel);
                keyTime_.SetEnabled(false);
                deleteKey_.SetEnabled(false);
                if (!hasChannel)
                {
                    keys_.ClearItems();
                    info_.SetText("Record a key to create a native channel.");
                    return;
                }

                const auto& channel = channelsData_[selectedChannel_];
                interpolation_.SetSelectedByUserdataWithoutCallback(static_cast<std::uint64_t>(channel.mode));
                RefreshKeyList();
            }

            void RefreshKeyList()
            {
                auto* session = bridge::StudioSession::Current();
                auto* animation = CurrentAnimation();
                if (session == nullptr || animation == nullptr || selectedChannel_ >= channelsData_.size())
                    return;

                keysData_ = bridge::CollectTimelineKeys(
                    session->Scenes().GetScene(), selectedClip_, selectedChannel_);
                if (selectedKey_ >= keysData_.size())
                    selectedKey_ = keysData_.empty() ? 0 : keysData_.size() - 1;
                keys_.ClearItems();
                for (const auto& key : keysData_)
                    keys_.AddItem(KeyLabel(key.index, key.time), static_cast<std::uint64_t>(key.index));
                if (!keysData_.empty())
                    keys_.SetSelectedByUserdataWithoutCallback(static_cast<std::uint64_t>(selectedKey_));
                RefreshKeyControls();
            }

            void RefreshKeyControls()
            {
                auto* animation = CurrentAnimation();
                const bool hasKey = animation != nullptr && selectedKey_ < keysData_.size();
                keyTime_.SetEnabled(hasKey);
                deleteKey_.SetEnabled(hasKey);
                if (!hasKey)
                {
                    info_.SetText("Selected channel has no keys.");
                    return;
                }
                keyTime_.SetRange(animation->start, std::max(animation->start + 0.0001f, animation->end));
                keyTime_.SetValue(keysData_[selectedKey_].time);
                const auto& channel = channelsData_[selectedChannel_];
                info_.SetText(
                    std::string(bridge::TimelinePathLabel(channel.path)) + " // " +
                    std::to_string(channel.keyframeCount) + " native keys // data entity " +
                    std::to_string(channel.dataEntity));
            }

            template<typename Command>
            bool ExecuteAuthoredCommand(std::unique_ptr<Command> command, const char* successStatus)
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || command == nullptr)
                    return false;
                Command* executed = command.get();
                if (!executed->Execute())
                {
                    const std::string error = executed->Error();
                    SetStatus("PHASE 7D // " + (error.empty() ? std::string("no authored change") : error));
                    RefreshControls();
                    RequestRefresh();
                    return false;
                }
                if (!session->Commands().RecordExecuted(std::move(command)))
                {
                    SetStatus("PHASE 7D // command history rejected authored change");
                    RefreshControls();
                    RequestRefresh();
                    return false;
                }
                SetStatus(successStatus);
                RefreshControls();
                RequestRefresh();
                return true;
            }

            void CreateClip()
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr)
                    return;

                const std::string name = "Animation " + std::to_string(availableClips_.size() + 1);
                auto command = std::make_unique<bridge::CreateTimelineAnimationCommand>(
                    session->Scenes().GetScene(), name);
                auto* view = command.get();
                if (!session->Commands().Execute(std::move(command)))
                {
                    SetStatus("PHASE 7D // native clip creation failed");
                    return;
                }
                selectedClip_ = view->CreatedEntity();
                selectedChannel_ = 0;
                selectedKey_ = 0;
                SetStatus("PHASE 7D // NEW CLIP created");
                RefreshClipList(true);
                RefreshControls();
                RequestRefresh();
            }

            void RecordKey()
            {
                auto* session = bridge::StudioSession::Current();
                auto* animation = CurrentAnimation();
                if (session == nullptr || animation == nullptr)
                    return;
                auto command = std::make_unique<bridge::RecordTimelineKeyCommand>(
                    session->Scenes().GetScene(), selectedClip_, selectedTarget_, selectedPreset_, animation->timer);
                if (ExecuteAuthoredCommand(std::move(command), "PHASE 7D // native key recorded"))
                {
                    channelsData_ = bridge::CollectTimelineChannels(session->Scenes().GetScene(), selectedClip_);
                    if (!channelsData_.empty())
                        selectedChannel_ = std::min(selectedChannel_, channelsData_.size() - 1);
                }
            }

            void CloseLoop()
            {
                auto* session = bridge::StudioSession::Current();
                auto* animation = CurrentAnimation();
                if (session == nullptr || animation == nullptr)
                    return;
                ExecuteAuthoredCommand(
                    std::make_unique<bridge::CloseTimelineLoopCommand>(
                        session->Scenes().GetScene(), selectedClip_, animation->timer),
                    "PHASE 7D // value-channel loop seam closed at current time");
            }

            void MoveKey(const float time)
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || selectedChannel_ >= channelsData_.size() || selectedKey_ >= keysData_.size())
                    return;
                ExecuteAuthoredCommand(
                    std::make_unique<bridge::MoveTimelineKeyCommand>(
                        session->Scenes().GetScene(), selectedClip_, selectedChannel_, selectedKey_, time),
                    "PHASE 7D // key time committed");
            }

            void DeleteKey()
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || selectedChannel_ >= channelsData_.size() || selectedKey_ >= keysData_.size())
                    return;
                ExecuteAuthoredCommand(
                    std::make_unique<bridge::DeleteTimelineKeyCommand>(
                        session->Scenes().GetScene(), selectedClip_, selectedChannel_, selectedKey_),
                    "PHASE 7D // native key deleted");
            }

            void SetSamplerMode(const bridge::AnimationSamplerMode mode)
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || selectedChannel_ >= channelsData_.size())
                    return;
                ExecuteAuthoredCommand(
                    std::make_unique<bridge::SetTimelineSamplerModeCommand>(
                        session->Scenes().GetScene(), selectedClip_, selectedChannel_, mode),
                    "PHASE 7D // sampler mode committed");
            }

            void Scrub(const float value)
            {
                auto* session = bridge::StudioSession::Current();
                if (session != nullptr && bridge::ScrubAnimation(session->Scenes().GetScene(), selectedClip_, value))
                    SetStatus("PHASE 7D // timeline scrubbed");
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

            wi::ecs::Entity selectedClip_ = wi::ecs::INVALID_ENTITY;
            wi::ecs::Entity selectedTarget_ = wi::ecs::INVALID_ENTITY;
            bridge::TimelineRecordPreset selectedPreset_ = bridge::TimelineRecordPreset::Transform;
            std::size_t selectedChannel_ = 0;
            std::size_t selectedKey_ = 0;
            std::vector<bridge::AnimationClipInfo> availableClips_;
            std::vector<bridge::TimelineChannelInfo> channelsData_;
            std::vector<bridge::TimelineKeyInfo> keysData_;

            SceneInspectorButton header_;
            wi::gui::Label status_;
            SceneInspectorButton newClip_;
            SceneInspectorComboBox clips_;
            SceneInspectorSlider timer_;
            SceneInspectorComboBox target_;
            SceneInspectorComboBox preset_;
            SceneInspectorButton record_;
            SceneInspectorButton closeLoop_;
            SceneInspectorComboBox channels_;
            SceneInspectorComboBox interpolation_;
            SceneInspectorComboBox keys_;
            SceneInspectorSlider keyTime_;
            SceneInspectorButton deleteKey_;
            wi::gui::Label info_;
        };

        bool NativeTimelineSectionProvider::IsVisible(const InspectorSectionContext& context) const
        {
            return owner_ != nullptr && owner_->IsVisible(context);
        }

        float NativeTimelineSectionProvider::MeasureContentHeight(
            const InspectorSectionContext&,
            float) const
        {
            return owner_ != nullptr ? owner_->Measure() : 0.0f;
        }

        void NativeTimelineSectionProvider::Refresh(const InspectorSectionContext&)
        {
            if (owner_ != nullptr)
                owner_->Refresh();
        }

        void NativeTimelineSectionProvider::ApplyLayout(
            const InspectorSectionContext&,
            const InspectorSectionLayout& layout)
        {
            if (owner_ != nullptr)
                owner_->Layout(layout);
        }

        std::unique_ptr<NativeTimelineInspector> activeInspector;
        StudioRenderPath* activeOwner = nullptr;
    }

    void RegisterPhase7Gate7DNativeTimelineInspector(
        StudioRenderPath& owner,
        wi::gui::Window& inspectorPanel,
        InspectorSectionRegistry& registry,
        std::function<void()> requestRefresh,
        std::function<void(std::string)> setStatus)
    {
        activeInspector.reset();
        activeOwner = &owner;
        activeInspector = std::make_unique<NativeTimelineInspector>(
            owner,
            inspectorPanel,
            registry,
            std::move(requestRefresh),
            std::move(setStatus));
        activeInspector->Register();
    }

    void PreparePhase7Gate7DNativeTimelineInspector(StudioRenderPath& owner)
    {
        if (activeOwner == &owner && activeInspector)
            activeInspector->PrepareForLayout();
    }
}
