#include "RuntimeScreen.h"
#include <cmath>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace
{
    constexpr std::size_t NoFocus = std::numeric_limits<std::size_t>::max();
}

namespace renegade::runtime
{
    bool RuntimeScreenController::Initialize(
        bridge::ScreenDocument document,
        std::string& error)
    {
        if (!document.enabledInGame)
        {
            error = "The requested Screen is disabled in game.";
            return false;
        }
        if (!bridge::ValidateScreenDocument(
                document,
                document.envelope.projectId,
                error))
        {
            return false;
        }

        authoredDocument_ = document;
        document_ = std::move(document);
        stateOverrides_.clear();
        displayTextOverrides_.clear();
        bindingSnapshot_ = {};
        bindingContext_ = {};
        bindingFrame_ = {};
        hasBindingSnapshot_ = false;
        widgetIndex_.clear();
        for (std::size_t index = 0; index < document_.widgets.size(); ++index)
        {
            widgetIndex_.emplace(document_.widgets[index].id, index);
        }
        focusIndex_ = NoFocus;
        initialized_ = true;
        bridge::ScreenBindingFrame initialFrame;
        if (!bridge::BuildScreenBindingFrame(authoredDocument_, {}, {}, initialFrame, error))
            return false;
        ApplyResolvedFrame(std::move(initialFrame));
        error.clear();
        return true;
    }

    bool RuntimeScreenController::SetWidgetState(
        const std::string& widgetId,
        const bool visible,
        const bool enabled,
        std::string& error)
    {
        const auto found = widgetIndex_.find(widgetId);
        if (found == widgetIndex_.end())
        {
            error = "Runtime screen widget does not exist: " + widgetId;
            return false;
        }

        const auto& current = document_.widgets[found->second];
        for (const auto& binding : authoredDocument_.widgets[found->second].bindings)
        {
            if ((binding.property == bridge::ScreenBindingProperty::Visible && visible != current.visible) ||
                (binding.property == bridge::ScreenBindingProperty::Enabled && enabled != current.enabled))
            { error = "Runtime state is controlled by an explicit binding."; return false; }
        }
        auto nextOverrides = stateOverrides_;
        nextOverrides[widgetId] = {visible, enabled};
        bridge::ScreenBindingFrame frame;
        if (!bridge::BuildScreenBindingFrame(authoredDocument_,bindingSnapshot_,bindingContext_,
                frame,error,&nextOverrides)) return false;
        stateOverrides_ = std::move(nextOverrides);
        ApplyResolvedFrame(std::move(frame));
        error.clear();
        return true;
    }

    bool RuntimeScreenController::SetWidgetDisplayText(
        const std::string& widgetId,std::string text,std::string& error)
    {
        const auto found=widgetIndex_.find(widgetId);
        if(!initialized_||found==widgetIndex_.end()||text.size()>4096)
        {error="Runtime Screen display text target or length is invalid.";return false;}
        auto& widget=document_.widgets[found->second];
        if(widget.kind!=bridge::ScreenWidgetKind::Text &&
           widget.kind!=bridge::ScreenWidgetKind::Button)
        {error="Runtime Screen display text requires a Text or Button widget.";return false;}
        if(!authoredDocument_.widgets[found->second].bindings.empty())
        {
            for(const auto& binding:authoredDocument_.widgets[found->second].bindings)
                if(binding.property==bridge::ScreenBindingProperty::Text)
                {error="Runtime Screen text is owned by a declared binding.";return false;}
        }
        // Keep runtime-provided display text as an explicit transient
        // override; SetWidgetState applies a full resolved binding frame and
        // would otherwise silently restore every other slot's authored text.
        displayTextOverrides_[widgetId]=std::move(text);
        widget.text=displayTextOverrides_.at(widgetId);
        error.clear();return true;
    }

    void RuntimeScreenController::ApplyResolvedFrame(bridge::ScreenBindingFrame frame)
    {
        for (const auto& value : frame.widgets)
        {
            const auto found = widgetIndex_.find(value.id);
            if (found == widgetIndex_.end()) continue;
            auto& widget = document_.widgets[found->second];
            widget.text = value.text;
            if (const auto overrideText = displayTextOverrides_.find(value.id);
                overrideText != displayTextOverrides_.end())
                widget.text = overrideText->second;
            widget.fill = value.fill;
            widget.visible = value.visible;
            widget.enabled = value.enabled;
        }
        bindingFrame_ = std::move(frame);
        RepairFocus();
    }

    bool RuntimeScreenController::ApplyBindings(
        const bridge::ScreenBindingSnapshot& snapshot,
        const bridge::ScreenBindingContext& context, std::string& error)
    {
        if (!initialized_ || context.session == 0 || snapshot.session != context.session ||
            (hasBindingSnapshot_ && (context.session < bindingContext_.session ||
                (context.session == bindingContext_.session && snapshot.revision <= bindingSnapshot_.revision))))
        { error = "Runtime UI rejected a missing, stale or repeated binding snapshot."; return false; }
        bridge::ScreenBindingFrame frame;
        if (!bridge::BuildScreenBindingFrame(authoredDocument_,snapshot,context,frame,error,&stateOverrides_))
            return false;
        bindingSnapshot_ = snapshot;
        bindingContext_ = context;
        hasBindingSnapshot_ = true;
        ApplyResolvedFrame(std::move(frame));
        error.clear();
        return true;
    }

    bool RuntimeScreenController::SetControlValue(const std::string& id,
        bridge::GameplayAttributeValue value,std::string& error)
    {
        const auto it=widgetIndex_.find(id);
        if(!initialized_||it==widgetIndex_.end()){error="Local UI value target is absent.";return false;}
        auto& widget=document_.widgets[it->second];
        if(!bridge::IsScreenValueControl(widget.kind)||!widget.visible||!widget.enabled)
        {error="Local UI value target is not an available control.";return false;}
        auto candidate=widget.control;candidate.value=std::move(value);
        if(!bridge::ValidateScreenControl(bridge::ScreenControlKindFor(widget.kind),candidate,error))return false;
        if(widget.control.value==candidate.value){error.clear();return true;}
        auto next=document_;next.widgets[it->second].control=std::move(candidate);
        bridge::ScreenInteractionPlan plan;
        if(!bridge::BuildScreenInteractionPlan(next,id,bridge::ScreenInteractionEvent::ValueChanged,plan,error))return false;
        return CommitInteractionPlan(std::move(plan),error);
    }

    bool RuntimeScreenController::CommitInteractionPlan(bridge::ScreenInteractionPlan plan,std::string& error)
    {
        auto nextOverrides=stateOverrides_;
        for(std::size_t i=0;i<document_.widgets.size();++i)
        {
            const auto& before=document_.widgets[i];const auto& after=plan.document.widgets[i];
            if(before.visible!=after.visible||before.enabled!=after.enabled)
                nextOverrides[after.id]={after.visible,after.enabled};
        }
        bridge::ScreenBindingFrame frame;
        if(!bridge::BuildScreenBindingFrame(authoredDocument_,bindingSnapshot_,bindingContext_,frame,error,&nextOverrides))return false;
        // Validate final focus against the production hierarchy/binding result.
        if(!plan.focusWidgetId.empty())
        {
            const auto found=std::find_if(frame.widgets.begin(),frame.widgets.end(),[&](const auto& w){return w.id==plan.focusWidgetId;});
            if(found==frame.widgets.end()||!found->visible||!found->enabled)
            {error="UI action focus target is unavailable after binding evaluation.";return false;}
        }
        document_=std::move(plan.document);stateOverrides_=std::move(nextOverrides);
        ApplyResolvedFrame(std::move(frame));
        if(!plan.focusWidgetId.empty())(void)FocusWidget(plan.focusWidgetId);
        error.clear();return true;
    }
    bool RuntimeScreenController::ExecuteInteractions(const std::string& id,bridge::ScreenInteractionEvent event,std::string& error)
    {
        if(!initialized_){error="UI interaction has no active Screen instance.";return false;}
        bridge::ScreenInteractionPlan plan;
        if(!bridge::BuildScreenInteractionPlan(document_,id,event,plan,error))return false;
        return CommitInteractionPlan(std::move(plan),error);
    }

    bool RuntimeScreenController::FocusNext() noexcept
    {
        if (!initialized_ || document_.focusOrder.empty())
        {
            return false;
        }

        const std::size_t start = focusIndex_ == NoFocus ?
            document_.focusOrder.size() - 1 : focusIndex_;
        for (std::size_t offset = 1; offset <= document_.focusOrder.size(); ++offset)
        {
            const std::size_t candidate =
                (start + offset) % document_.focusOrder.size();
            const auto* widget = FindWidget(document_.focusOrder[candidate]);
            if (widget != nullptr && IsFocusable(*widget))
            {
                focusIndex_ = candidate;
                return true;
            }
        }

        focusIndex_ = NoFocus;
        return false;
    }

    bool RuntimeScreenController::FocusPrevious() noexcept
    {
        if (!initialized_ || document_.focusOrder.empty())
        {
            return false;
        }

        const std::size_t start = focusIndex_ == NoFocus ?
            0 : focusIndex_;
        for (std::size_t offset = 1; offset <= document_.focusOrder.size(); ++offset)
        {
            const std::size_t candidate =
                (start + document_.focusOrder.size() - offset) %
                document_.focusOrder.size();
            const auto* widget = FindWidget(document_.focusOrder[candidate]);
            if (widget != nullptr && IsFocusable(*widget))
            {
                focusIndex_ = candidate;
                return true;
            }
        }

        focusIndex_ = NoFocus;
        return false;
    }

    bool RuntimeScreenController::FocusWidget(
        const std::string& widgetId) noexcept
    {
        const auto focus = std::find(
            document_.focusOrder.begin(),
            document_.focusOrder.end(),
            widgetId);
        if (focus == document_.focusOrder.end())
        {
            return false;
        }

        const auto* widget = FindWidget(widgetId);
        if (widget == nullptr || !IsFocusable(*widget))
        {
            return false;
        }

        focusIndex_ = static_cast<std::size_t>(
            std::distance(document_.focusOrder.begin(), focus));
        return true;
    }

    const bridge::ScreenWidget* RuntimeScreenController::FocusedWidget() const noexcept
    {
        if (focusIndex_ == NoFocus ||
            focusIndex_ >= document_.focusOrder.size())
        {
            return nullptr;
        }
        return FindWidget(document_.focusOrder[focusIndex_]);
    }

    const bridge::ScreenWidget* RuntimeScreenController::FindWidget(
        const std::string& widgetId) const noexcept
    {
        const auto found = widgetIndex_.find(widgetId);
        if (found == widgetIndex_.end())
        {
            return nullptr;
        }
        return &document_.widgets[found->second];
    }

    bool RuntimeScreenController::MakeFocusedActionRequest(
        const RuntimeInputSource source,
        const std::uint64_t sequence,
        RuntimeActionRequest& request,
        std::string& error) const
    {
        const auto* focused = FocusedWidget();
        if (focused == nullptr)
        {
            error = "The Runtime screen has no focused action control.";
            return false;
        }
        return MakeActionRequest(focused->id, source, sequence, request, error);
    }

    bool RuntimeScreenController::MakeActionRequest(
        const std::string& widgetId,
        const RuntimeInputSource source,
        const std::uint64_t sequence,
        RuntimeActionRequest& request,
        std::string& error) const
    {
        const auto* widget = FindWidget(widgetId);
        if (widget == nullptr || widget->kind != bridge::ScreenWidgetKind::Button)
        {
            error = "Runtime screen activation target is missing or is not a button.";
            return false;
        }
        if (!IsFocusable(*widget))
        {
            error = "Runtime screen activation target is hidden or disabled.";
            return false;
        }

        request.actionId = widget->actionId;
        request.widgetId = widget->id;
        request.inputSource = source;
        request.sequence = sequence;
        error.clear();
        return true;
    }

    const bridge::ScreenDocument& RuntimeScreenController::Document() const noexcept
    {
        return document_;
    }

    bool RuntimeScreenController::HasFocus() const noexcept
    {
        return FocusedWidget() != nullptr;
    }

    bool RuntimeScreenController::IsFocusable(
        const bridge::ScreenWidget& widget) const noexcept
    {
        if(!bridge::IsScreenInteractive(widget.kind)||!widget.visible||!widget.enabled)
            return false;
        // The renderer hides descendants of a hidden Group. Navigation must
        // use the same effective hierarchy visibility, not just the child's
        // local flag; a hidden control must never retain keyboard activation.
        auto parent=widget.parentId;
        auto remaining=document_.widgets.size();
        while(!parent.empty()&&remaining--)
        {
            const auto* ancestor=FindWidget(parent);
            if(!ancestor||!ancestor->visible)return false;
            parent=ancestor->parentId;
        }
        return parent.empty();
    }

    void RuntimeScreenController::RepairFocus() noexcept
    {
        if(focusIndex_!=NoFocus&&focusIndex_<document_.focusOrder.size())
        {
            const auto* current=FindWidget(document_.focusOrder[focusIndex_]);
            if(current&&IsFocusable(*current))return;
            // Prefer the next *available* control after the one hidden or
            // disabled, preserving user orientation across dynamic UI edits.
            const auto old=focusIndex_,count=document_.focusOrder.size();
            for(std::size_t offset=1;offset<count;++offset)
            {
                const auto candidate=(old+offset)%count;
                const auto* widget=FindWidget(document_.focusOrder[candidate]);
                if(widget&&IsFocusable(*widget))
                {focusIndex_=candidate;return;}
            }
        }
        focusIndex_=NoFocus;
        (void)FocusNext();
    }

    bool RuntimeScreenPresenter::Load(
        const bridge::ScreenDocument& document,
        const std::string& projectRoot,
        wi::RenderPath2D& renderPath,
        RuntimeScreenController& controller,
        RequestSink requestSink,
        std::string& error,PointerDragSink dragSink,bool inventoryHost)
    {
        Reset(renderPath);
        if (!document.enabledInGame)
        {
            error.clear();
            return true;
        }
        if (!requestSink)
        {
            error = "Runtime screen presenter requires an action request sink.";
            return false;
        }

        controller_ = &controller;
        inventoryHost_ = inventoryHost;
        requestSink_ = std::move(requestSink);
        if (!renderer_.Load(
                document,
                projectRoot,
                renderPath,
                [this](const bridge::StableId& widgetId)
                {
                    if (controller_ != nullptr)
                        (void)controller_->FocusWidget(widgetId);
                    QueueWidget(widgetId, RuntimeInputSource::Mouse);
                },
                error,
                [this](const bridge::StableId& id,const bridge::GameplayAttributeValue& value)
                {
                    std::string error;
                    if(!controller_||!controller_->SetControlValue(id,value,error))return false;
                    RefreshVisualFocus(*controller_);
                    return true;
                },
                std::move(dragSink)))
        {
            controller_ = nullptr;
            requestSink_ = {};
            return false;
        }
        animationTime_=0;presentationVisible_=true;
        transitionPolicy_=document.transitions;
        if(!transition_.Start(transitionPolicy_,reducedMotion_,error))
        {Reset(renderPath);return false;}
        renderer_.SetTransitionFrame(transition_.Frame());
        renderer_.SetInputEnabled(requestedInput_&&transition_.AcceptsInput());
        RefreshVisualFocus(controller);
        renderer_.ApplyLayout(renderPath);
        error.clear();
        return true;
    }

    void RuntimeScreenPresenter::UpdateInput(
        wi::RenderPath2D& renderPath,
        RuntimeScreenController& controller, const bool navigationEnabled, const float uiDeltaTime)
    {
        if (!renderer_.IsLoaded())
        {
            return;
        }

        if(presentationVisible_&&std::isfinite(uiDeltaTime)&&uiDeltaTime>0)
        {animationTime_+=uiDeltaTime;transition_.Tick(uiDeltaTime,reducedMotion_);}
        renderer_.SetTransitionFrame(transition_.Frame());
        renderer_.SetInputEnabled(requestedInput_&&transition_.AcceptsInput());
        renderer_.SetAnimationPreview({true,false,reducedMotion_,animationTime_});
        renderer_.ApplyLayout(renderPath);
        if (!renderer_.IsInputEnabled()) return;
        if(!navigationEnabled)
        {ApplyPointerFocus(controller);RefreshVisualFocus(controller);return;}

        if(renderer_.ConsumesTextInput()){RefreshVisualFocus(controller);return;}
        // Back is an explicitly authored, eligible Button action. Flow still
        // owns its destination; no guessed history or second navigation stack.
        const bool keyboardBack=wi::input::Press(wi::input::KEYBOARD_BUTTON_ESCAPE);
        const bool gamepadBack=wi::input::Press(wi::input::GAMEPAD_BUTTON_XBOX_B);
        if(keyboardBack||gamepadBack)
        {
            for(const auto& widget:controller.Document().widgets)
                if(widget.kind==bridge::ScreenWidgetKind::Button&&widget.actionId=="ui.back"&&
                    renderer_.IsWidgetInteractive(widget.id))
                {QueueWidget(widget.id,gamepadBack?RuntimeInputSource::Gamepad:RuntimeInputSource::Keyboard);break;}
            RefreshVisualFocus(controller);return;
        }
        const auto* valueControl=controller.FocusedWidget();
        const bool adjustable=valueControl&&(valueControl->kind==bridge::ScreenWidgetKind::Slider||
            valueControl->kind==bridge::ScreenWidgetKind::Selector);
        const bool left=wi::input::Press(wi::input::KEYBOARD_BUTTON_LEFT)||
            wi::input::Press(wi::input::GAMEPAD_BUTTON_LEFT)||
            wi::input::Press(wi::input::GAMEPAD_ANALOG_THUMBSTICK_L_AS_BUTTON_LEFT);
        const bool right=wi::input::Press(wi::input::KEYBOARD_BUTTON_RIGHT)||
            wi::input::Press(wi::input::GAMEPAD_BUTTON_RIGHT)||
            wi::input::Press(wi::input::GAMEPAD_ANALOG_THUMBSTICK_L_AS_BUTTON_RIGHT);
        if(adjustable&&(left||right))
        {(void)renderer_.AdjustControl(valueControl->id,right?1:-1);RefreshVisualFocus(controller);return;}

        const bool gamepadPrevious =
            wi::input::Press(wi::input::GAMEPAD_BUTTON_UP) ||
            wi::input::Press(wi::input::GAMEPAD_BUTTON_LEFT) ||
            wi::input::Press(
                wi::input::GAMEPAD_ANALOG_THUMBSTICK_L_AS_BUTTON_UP) ||
            wi::input::Press(
                wi::input::GAMEPAD_ANALOG_THUMBSTICK_L_AS_BUTTON_LEFT);
        const bool gamepadNext =
            wi::input::Press(wi::input::GAMEPAD_BUTTON_DOWN) ||
            wi::input::Press(wi::input::GAMEPAD_BUTTON_RIGHT) ||
            wi::input::Press(
                wi::input::GAMEPAD_ANALOG_THUMBSTICK_L_AS_BUTTON_DOWN) ||
            wi::input::Press(
                wi::input::GAMEPAD_ANALOG_THUMBSTICK_L_AS_BUTTON_RIGHT);
        const bool gamepadConfirm =
            wi::input::Press(wi::input::GAMEPAD_BUTTON_2);

        if (gamepadPrevious)
        {
            (void)controller.FocusPrevious();
        }
        else if (gamepadNext)
        {
            (void)controller.FocusNext();
        }
        else if (gamepadConfirm)
        {
            QueueFocused(RuntimeInputSource::Gamepad);
        }
        else
        {
            const bool reverseTab=wi::input::Press(wi::input::KEYBOARD_BUTTON_TAB)&&
                (wi::input::Down(wi::input::KEYBOARD_BUTTON_LSHIFT)||wi::input::Down(wi::input::KEYBOARD_BUTTON_RSHIFT));
            const bool keyboardPrevious =
                reverseTab || wi::input::Press(wi::input::KEYBOARD_BUTTON_UP) ||
                wi::input::Press(wi::input::KEYBOARD_BUTTON_LEFT);
            const bool keyboardNext =
                wi::input::Press(wi::input::KEYBOARD_BUTTON_DOWN) ||
                wi::input::Press(wi::input::KEYBOARD_BUTTON_RIGHT) ||
                (wi::input::Press(wi::input::KEYBOARD_BUTTON_TAB)&&!reverseTab);
            const bool keyboardConfirm =
                wi::input::Press(wi::input::KEYBOARD_BUTTON_ENTER) ||
                wi::input::Press(wi::input::KEYBOARD_BUTTON_SPACE);

            if (keyboardPrevious)
            {
                (void)controller.FocusPrevious();
            }
            else if (keyboardNext)
            {
                (void)controller.FocusNext();
            }
            else if (keyboardConfirm)
            {
                QueueFocused(RuntimeInputSource::Keyboard);
            }
            else
            {
                ApplyPointerFocus(controller);
            }
        }

        RefreshVisualFocus(controller);
    }

    bool RuntimeScreenPresenter::BeginExit(
        const std::vector<bridge::ScreenAnimationTrack>* overrideTracks,std::string& error)
    {
        if(!transition_.BeginExit(overrideTracks,reducedMotion_,error))return false;
        renderer_.SetInputEnabled(false);
        renderer_.SetTransitionFrame(transition_.Frame());
        return true;
    }
    bool RuntimeScreenPresenter::OverrideEnterTransition(
        const std::vector<bridge::ScreenAnimationTrack>& tracks,wi::RenderPath2D& renderPath,std::string& error)
    {
        auto policy=transitionPolicy_;policy.enter=tracks;
        if(!transition_.Start(std::move(policy),reducedMotion_,error))return false;
        renderer_.SetTransitionFrame(transition_.Frame());
        renderer_.SetInputEnabled(requestedInput_&&transition_.AcceptsInput());
        renderer_.ApplyLayout(renderPath);return true;
    }
    void RuntimeScreenPresenter::CancelExit() noexcept
    {
        transition_.CancelExit();renderer_.SetTransitionFrame({});
        renderer_.SetInputEnabled(requestedInput_);
    }

    void RuntimeScreenPresenter::SetPresentationVisible(const bool visible) noexcept
    {presentationVisible_=visible;renderer_.SetPresentationVisible(visible);}
    void RuntimeScreenPresenter::AttachPresentation(wi::RenderPath2D& renderPath)
    {renderer_.AttachPresentation(renderPath);}
    void RuntimeScreenPresenter::DetachPresentation(wi::RenderPath2D& renderPath) noexcept
    {renderer_.DetachPresentation(renderPath);}

    void RuntimeScreenPresenter::SetInputEnabled(const bool enabled) noexcept
    {
        requestedInput_=enabled;renderer_.SetInputEnabled(enabled&&transition_.AcceptsInput());
    }

    void RuntimeScreenPresenter::RefreshVisualFocus(
        const RuntimeScreenController& controller)
    {
        const auto* focused = controller.FocusedWidget();
        renderer_.SetFocusedWidget(
            focused == nullptr ? bridge::StableId{} : focused->id);
        for (const auto& widget : controller.Document().widgets)
        {
            renderer_.SetWidgetState(widget.id, widget.visible, widget.enabled);
            renderer_.SetWidgetFill(widget.id,widget.fill);
            renderer_.SetWidgetText(widget.id,widget.text);
            if(bridge::IsScreenValueControl(widget.kind))
                (void)renderer_.SetWidgetControlValue(widget.id,widget.control.value);
        }
    }

    void RuntimeScreenPresenter::Reset(wi::RenderPath2D& renderPath) noexcept
    {
        renderer_.Reset(renderPath);
        requestSink_ = {};
        controller_ = nullptr;
        nextSequence_ = 1;
        animationTime_=0;presentationVisible_=true;requestedInput_=true;
        transition_.Reset();transitionPolicy_={};
    }

    bool RuntimeScreenPresenter::IsLoaded() const noexcept
    {
        return renderer_.IsLoaded();
    }
    bool RuntimeScreenPresenter::SetVisualImageResource(
        const bridge::StableId& widgetId,const std::string& path,
        std::string& error) {
        return renderer_.SetWidgetImageResource(widgetId,path,error);
    }
    bool RuntimeScreenPresenter::PagedScrollbarGeometry(
        std::size_t panel,bridge::ScreenRect& track,bridge::ScreenRect& thumb) const {
        if(panel>1 || !presentationVisible_ || !renderer_.IsLoaded() ||
           !controller_)return false;
        const bridge::ScreenWidget* trackWidget=nullptr;
        const bridge::ScreenWidget* thumbWidget=nullptr;
        const std::string trackName="Paged Scroll Track "+std::to_string(panel);
        const std::string thumbName="Paged Scroll Thumb "+std::to_string(panel);
        for(const auto& w:controller_->Document().widgets) {
            if(w.name==trackName && w.visible)trackWidget=&w;
            if(w.name==thumbName && w.visible)thumbWidget=&w;
        }
        if(!trackWidget || !thumbWidget)return false;
        const auto* a=renderer_.LogicalRect(trackWidget->id);
        const auto* b=renderer_.LogicalRect(thumbWidget->id);
        if(!a || !b || a->height<=0 || b->height<=0)return false;
        track=*a;thumb=*b;return true;
    }
    void RuntimeScreenPresenter::SetPagedScrollbar(
        std::size_t panel,float position,float fraction) {
        if(panel>1 || !controller_ || !renderer_.IsLoaded())return;
        bridge::StableId track,thumb;
        const std::string trackName="Paged Scroll Track "+std::to_string(panel);
        const std::string thumbName="Paged Scroll Thumb "+std::to_string(panel);
        for(const auto& w:controller_->Document().widgets) {
            if(w.name==trackName)track=w.id;
            if(w.name==thumbName)thumb=w.id;
        }
        if(!track.empty() && !thumb.empty())
            renderer_.SetScrollbarThumb(thumb,track,position,fraction);
    }
    std::string RuntimeScreenPresenter::InventorySlotAt(float x,float y) const {
        if(!presentationVisible_ || !renderer_.IsLoaded() || !controller_ ||
           !std::isfinite(x) || !std::isfinite(y)) return {};
        // Use the ScreenRenderer's actual responsive output geometry, not
        // authored 1280x720 design coordinates. Empty slots are valid drops.
        for(const auto& widget:controller_->Document().widgets) {
            if(widget.kind!=bridge::ScreenWidgetKind::Button ||
               (widget.actionId.rfind("inventory.slot.",0)!=0 &&
                !(widget.actionId.rfind("inventory.paged.",0)==0 &&
                  widget.actionId.find(".slot.")!=std::string::npos)) ||
               !widget.visible)continue;
            const auto* rect=renderer_.LogicalRect(widget.id);
            if(!rect)continue;
            if(x>=rect->x && y>=rect->y &&
               x<=rect->x+rect->width && y<=rect->y+rect->height)
                return widget.actionId;
        }
        return {};
    }

    std::string RuntimeScreenPresenter::InventoryContextAt(float x,float y) const {
        if(!presentationVisible_ || !renderer_.IsLoaded() || !controller_ ||
           !std::isfinite(x) || !std::isfinite(y))return {};
        for(const auto& widget:controller_->Document().widgets) {
            if(widget.kind!=bridge::ScreenWidgetKind::Button ||
               widget.actionId.rfind("inventory.context.",0)!=0 ||
               !widget.visible)continue;
            const auto* rect=renderer_.LogicalRect(widget.id);
            if(rect && x>=rect->x && x<=rect->x+rect->width &&
               y>=rect->y && y<=rect->y+rect->height)
                return widget.actionId;
        }
        return {};
    }
    void RuntimeScreenPresenter::QueueFocused(const RuntimeInputSource source)
    {
        if (controller_ == nullptr || !requestSink_ || !renderer_.IsInputEnabled())
        {
            return;
        }

        const auto* focused=controller_->FocusedWidget();
        if(!focused||!renderer_.IsWidgetInteractive(focused->id))return;
        if(bridge::IsScreenValueControl(focused->kind))
        {(void)renderer_.ActivateControl(focused->id);return;}
        RuntimeActionRequest request;
        std::string error;
        if (controller_->MakeFocusedActionRequest(
                source,
                nextSequence_++,
                request,
                error))
        {
            if(!controller_->ExecuteInteractions(request.widgetId,bridge::ScreenInteractionEvent::Activate,error))
            {wi::backlog::post("UI interaction rejected: "+error,wi::backlog::LogLevel::Warning);return;}
            RefreshVisualFocus(*controller_);
            if(const auto* widget=controller_->FindWidget(request.widgetId);widget&&!ShouldDispatchScreenAction(*widget,inventoryHost_))
                return; // Atomic local UI actions succeeded; do not dispatch Flow.
            requestSink_(std::move(request));
        }
    }

    void RuntimeScreenPresenter::QueueWidget(
        const std::string& widgetId,
        const RuntimeInputSource source)
    {
        const auto* clicked = controller_ ? controller_->FindWidget(widgetId) : nullptr;
        const bool inventorySlot = clicked &&
            (clicked->actionId.rfind("inventory.slot.",0)==0 ||
             (clicked->actionId.rfind("inventory.paged.",0)==0 &&
              clicked->actionId.find(".slot.")!=std::string::npos));
        // Inventory slot clicks are owned by RuntimeApplication's authoritative
        // pointer gesture (same responsive hit-test as drags and potion use).
        // A native Wicked Button can be covered by icon artwork and silently
        // miss a click, or fire after a drag and transfer twice. Other authored
        // Screen controls still use the standard native callback.
        if (inventoryHost_ && source==RuntimeInputSource::Mouse && inventorySlot)
            return;
        if (controller_ == nullptr || !requestSink_ || !renderer_.IsInputEnabled())
        {
            if(inventorySlot)
                wi::backlog::post("Inventory Screen click ignored: no input ownership",
                    wi::backlog::LogLevel::Warning);
            return;
        }

        RuntimeActionRequest request;
        std::string error;
        if(!renderer_.IsWidgetInteractive(widgetId)) {
            if(inventorySlot)
                wi::backlog::post("Inventory Screen click ignored: widget not interactive",
                    wi::backlog::LogLevel::Warning);
            return;
        }
        if (!controller_->MakeActionRequest(
                widgetId,source,nextSequence_++,request,error)) {
            if(inventorySlot)
                wi::backlog::post("Inventory Screen click rejected: "+error,
                    wi::backlog::LogLevel::Warning);
            return;
        }
        if(!controller_->ExecuteInteractions(
                request.widgetId,bridge::ScreenInteractionEvent::Activate,error)) {
            wi::backlog::post("UI interaction rejected: "+error,
                wi::backlog::LogLevel::Warning);
            return;
        }
        RefreshVisualFocus(*controller_);
        if(const auto* widget=controller_->FindWidget(request.widgetId);
           widget && !ShouldDispatchScreenAction(*widget,inventoryHost_)) {
            if(inventorySlot)
                wi::backlog::post("Inventory Screen click stopped by localOnly",
                    wi::backlog::LogLevel::Warning);
            return;
        }
        requestSink_(std::move(request));
    }

    void RuntimeScreenPresenter::ApplyPointerFocus(
        RuntimeScreenController& controller)
    {
        const auto& mouse = wi::input::GetMouseState();
        if (mouse.delta_position.x == 0.0f && mouse.delta_position.y == 0.0f)
        {
            return;
        }

        const XMFLOAT4 pointer = wi::input::GetPointer();
        const auto& document = controller.Document();
        for (auto iterator = document.focusOrder.rbegin();
            iterator != document.focusOrder.rend();
            ++iterator)
        {
            const auto* rect = renderer_.LogicalRect(*iterator);
            const auto* widget = controller.FindWidget(*iterator);
            if (rect == nullptr || widget == nullptr ||
                !widget->visible || !widget->enabled)
            {
                continue;
            }

            const auto& area = *rect;
            if (pointer.x >= area.x && pointer.x <= area.x + area.width &&
                pointer.y >= area.y && pointer.y <= area.y + area.height)
            {
                (void)controller.FocusWidget(*iterator);
                return;
            }
        }
    }
}
