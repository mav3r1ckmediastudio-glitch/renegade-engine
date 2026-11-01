#include "renegade/screen/ScreenRenderer.h"
#include "renegade/screen/ScreenTextLayout.h"
#include "renegade/bridge/ScreenThemeResolver.h"

#include <WickedEngine.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
    bool VisibleInHierarchy(const renegade::bridge::ScreenDocument& document,
        const renegade::bridge::ScreenWidget& widget)
    {
        if(!widget.visible)return false;
        auto parent=widget.parentId;
        std::size_t remaining=document.widgets.size();
        while(!parent.empty()&&remaining--)
        {
            const auto p=std::find_if(document.widgets.begin(),document.widgets.end(),
                [&](const auto& item){return item.id==parent;});
            if(p==document.widgets.end()||!p->visible)return false;
            parent=p->parentId;
        }
        return parent.empty();
    }

    using renegade::bridge::ScreenColor;
    using renegade::bridge::ScreenHorizontalAlignment;
    using renegade::bridge::ScreenVerticalAlignment;
    using renegade::screen::ScreenInteractionState;

    wi::Color ToWickedColor(const ScreenColor& color, const float opacity)
    {
        return wi::Color(
            color.red,
            color.green,
            color.blue,
            static_cast<std::uint8_t>(std::clamp(
                static_cast<float>(color.alpha) * opacity, 0.0f, 255.0f)));
    }

    const renegade::bridge::ScreenVisualState& SelectVisual(
        const renegade::bridge::ScreenWidgetStyle& style,
        const ScreenInteractionState state)
    {
        switch (state)
        {
        case ScreenInteractionState::Hover: return style.hover;
        case ScreenInteractionState::Pressed: return style.pressed;
        case ScreenInteractionState::Focused: return style.focused;
        case ScreenInteractionState::Disabled: return style.disabled;
        case ScreenInteractionState::Normal:
        default: return style.normal;
        }
    }

    wi::font::Alignment ToWickedAlignment(
        const ScreenHorizontalAlignment alignment)
    {
        switch (alignment)
        {
        case ScreenHorizontalAlignment::Center: return wi::font::WIFALIGN_CENTER;
        case ScreenHorizontalAlignment::Right: return wi::font::WIFALIGN_RIGHT;
        case ScreenHorizontalAlignment::Left:
        default: return wi::font::WIFALIGN_LEFT;
        }
    }

    wi::font::Alignment ToWickedAlignment(
        const ScreenVerticalAlignment alignment)
    {
        switch (alignment)
        {
        case ScreenVerticalAlignment::Center: return wi::font::WIFALIGN_CENTER;
        case ScreenVerticalAlignment::Bottom: return wi::font::WIFALIGN_BOTTOM;
        case ScreenVerticalAlignment::Top:
        default: return wi::font::WIFALIGN_TOP;
        }
    }

    void ApplyRounding(wi::image::Params& params, const float radius)
    {
        if (radius <= 0.0f) return;
        params.enableCornerRounding();
        for (auto& corner : params.corners_rounding)
        {
            corner.radius = radius;
            corner.segments = 18;
        }
    }

    void DrawSurface(
        const wi::Resource* resource,
        const float x,
        const float y,
        const float width,
        const float height,
        const float radius,
        const wi::Color color,
        const wi::graphics::CommandList cmd)
    {
        if (width <= 0.0f || height <= 0.0f || color.getA() == 0) return;
        wi::image::Params params(x, y, width, height, color);
        params.blendFlag = wi::enums::BLENDMODE_ALPHA;
        ApplyRounding(params, std::min(radius, std::min(width, height) * 0.5f));
        const wi::graphics::Texture* texture =
            resource != nullptr && resource->IsValid() ?
                &resource->GetTexture() : nullptr;
        wi::image::Draw(texture, params, cmd);
    }

    // Keep native text editing, caret, selection and clipboard mechanics while
    // Renegade supplies the exact authored text/background appearance.
    class ScreenEditableText final : public wi::gui::TextInputField
    {
    public:
        void RenderEditor(const wi::Canvas& canvas,wi::graphics::CommandList cmd,
            wi::font::Params params,const wi::graphics::Rect& clip)
        {
            const auto width=wi::font::TextSize(font_input.GetText().c_str(),
                static_cast<int>(font_input.GetText().size()),params).x;
            if(params.h_align==wi::font::WIFALIGN_CENTER)params.posX-=width*.5f;
            else if(params.h_align==wi::font::WIFALIGN_RIGHT)params.posX-=width;
            params.h_align=wi::font::WIFALIGN_LEFT;
            font_input.params=params;
            const auto previous=scissorRect;
            scissorRect.left=std::max(scissorRect.left,clip.left);
            scissorRect.top=std::max(scissorRect.top,clip.top);
            scissorRect.right=std::min(scissorRect.right,clip.right);
            scissorRect.bottom=std::min(scissorRect.bottom,clip.bottom);
            wi::gui::TextInputField::Render(canvas,cmd);
            scissorRect=previous;
        }
    };

    class ScreenSurface final : public wi::gui::Button
    {
    public:
        renegade::bridge::ScreenWidget authored;
        std::unordered_map<ScreenInteractionState, wi::Resource> resources;
        wi::Resource progressFrameImage;
        wi::Resource progressFillImage;
        int fontStyle = 0;
        float scaleX = 1.0f;
        float scaleY = 1.0f;
        bool keyboardFocused = false;
        renegade::bridge::ScreenAccessibilitySettings accessibility;
        bool hasClip = false;
        renegade::bridge::ScreenRect clip;

        std::function<bool(const renegade::bridge::GameplayAttributeValue&)> changeControl;
        ScreenEditableText input;
        bool consumedText=false;
        std::string controlDiagnostic;
        mutable std::string wrappedText;
        mutable std::vector<renegade::screen::ScreenTextLine> wrappedLines;
        mutable float wrappedWidth=-1,wrappedMeasuredWidth=-1,wrappedSpacingX=0,wrappedSpacingY=0;
        mutable int wrappedFont=-1,wrappedSize=-1;
        void DrawWrappedText(const std::string& display,wi::font::Params params,
            const renegade::bridge::ScreenRect& content,
            renegade::bridge::ScreenHorizontalAlignment horizontal,
            renegade::bridge::ScreenVerticalAlignment vertical,
            wi::graphics::CommandList cmd) const
        {
            params.h_wrap=-1;params.h_align=wi::font::WIFALIGN_LEFT;
            params.v_align=wi::font::WIFALIGN_TOP;
            const auto measure=[&](const std::string& value)
                {return wi::font::TextSize(value,params).x;};
            // Font packing is asynchronous; remeasure total advance so newly
            // available glyph metrics invalidate the cached word layout.
            const auto measured=measure(display);
            if(wrappedText!=display||wrappedWidth!=content.width||wrappedMeasuredWidth!=measured||
                wrappedFont!=params.style||wrappedSize!=params.size||
                wrappedSpacingX!=params.spacingX||wrappedSpacingY!=params.spacingY)
            {
                wrappedText=display;wrappedWidth=content.width;wrappedMeasuredWidth=measured;
                wrappedFont=params.style;wrappedSize=params.size;
                wrappedSpacingX=params.spacingX;wrappedSpacingY=params.spacingY;
                wrappedLines=renegade::screen::WrapScreenText(display,content.width,measure);
            }
            const float advance=std::max(1.0f,float(params.size)+params.spacingY);
            const float height=advance*float(wrappedLines.size());
            for(std::size_t i=0;i<wrappedLines.size();++i)
            {
                const auto origin=renegade::screen::ResolveScreenTextLineOrigin(content,
                    wrappedLines[i].width,height,float(i)*advance,horizontal,vertical);
                if(origin.y>=content.y+content.height)break;
                params.posX=origin.x;params.posY=origin.y;
                wi::font::Draw(wrappedLines[i].text,params,cmd);
            }
        }
        void Update(const wi::Canvas& canvas,float dt) override
        {
            if(dt<=0){wi::gui::Widget::Update(canvas,dt);return;}
            consumedText=false;
            if(authored.kind!=renegade::bridge::ScreenWidgetKind::TextInput)
            {wi::gui::Button::Update(canvas,dt);return;}
            wi::gui::Widget::Update(canvas,dt);
            const bool before=input.GetState()==wi::gui::ACTIVE;
            if(before)
            {
                const auto draft=input.GetCurrentInputValue();
                if(draft.size()>4096)
                {
                    controlDiagnostic="Text entry exceeds the 4096-byte safety bound.";
                    input.SetValue(std::get<std::string>(authored.control.value));input.Deactivate();
                }
                else if(wi::input::Press(wi::input::KEYBOARD_BUTTON_ENTER))
                {
                    auto candidate=authored.control;candidate.value=draft;
                    if(renegade::bridge::ValidateScreenControl(renegade::bridge::ScreenControlKind::TextInput,
                        candidate,controlDiagnostic))
                    {
                        if(!changeControl||!changeControl(draft))
                            controlDiagnostic="UI value unavailable; previous value retained.";
                        else controlDiagnostic.clear();
                    }
                    // Native editor fields offer arithmetic entry. User-facing text
                    // is always literal: consume Enter before that optional mechanic.
                    input.SetValue(std::get<std::string>(authored.control.value));input.Deactivate();
                }
            }
            input.font.params.style=fontStyle;
            input.font.params.size=std::max(1,static_cast<int>(std::round(authored.style.text.fontSize*accessibility.textScale*std::min(scaleX,scaleY))));
            input.SetVisible(IsVisible());input.SetEnabled(IsEnabled());
            input.SetPos(XMFLOAT2(translation.x,translation.y));input.SetSize(XMFLOAT2(scale.x,scale.y));
            input.force_disable=force_disable;input.Update(canvas,dt);input.force_disable=false;
            state=input.GetState();priority_change=false;
            consumedText=before||state==wi::gui::ACTIVE;
        }
        void PointerControl()
        {
            using namespace renegade::bridge;
            if(!changeControl)return;
            if(authored.kind==ScreenWidgetKind::Slider)
            {
                const auto p=wi::input::GetPointer();const auto& c=authored.control;
                const double ratio=std::clamp(double((p.x-translation.x)/std::max(1.0f,scale.x)),0.0,1.0);
                (void)changeControl(QuantizeScreenSlider(c,c.minimum+ratio*(c.maximum-c.minimum)));
            }
        }
        bool Adjust(int direction)
        {
            using namespace renegade::bridge;
            if(!changeControl)return false;
            const auto& c=authored.control;
            if(authored.kind==ScreenWidgetKind::Slider)
                return changeControl(QuantizeScreenSlider(c,std::get<double>(c.value)+direction*c.step));
            if(authored.kind==ScreenWidgetKind::Selector)
            {
                const auto count=static_cast<int>(c.options.size());
                const int index=(static_cast<int>(std::get<double>(c.value))+direction+count)%count;
                return changeControl(double(index));
            }
            return false;
        }
        bool ActivateValue()
        {
            using namespace renegade::bridge;
            if(authored.kind==ScreenWidgetKind::Toggle)
                return changeControl&&changeControl(!std::get<bool>(authored.control.value));
            if(authored.kind==ScreenWidgetKind::Selector)return Adjust(1);
            if(authored.kind==ScreenWidgetKind::TextInput)
            {input.SetAsActive(false);state=wi::gui::ACTIVE;consumedText=true;return true;}
            return authored.kind==ScreenWidgetKind::Slider;
        }

        void CancelInteraction() noexcept
        {
            // Deactivate means click-release in native Button mechanics.
            // Dormant instances cancel the gesture without emitting an action.
            if(authored.kind==renegade::bridge::ScreenWidgetKind::TextInput)input.Deactivate();consumedText=false;
            state=wi::gui::IDLE;priority_change=false;keyboardFocused=false;
        }

        ScreenInteractionState Interaction() const noexcept
        {
            if (renegade::bridge::IsScreenInteractive(authored.kind) &&
                !authored.enabled)
                return ScreenInteractionState::Disabled;
            if (GetState() == wi::gui::ACTIVE)
                return ScreenInteractionState::Pressed;
            if (GetState() == wi::gui::FOCUS)
                return ScreenInteractionState::Hover;
            if (keyboardFocused) return ScreenInteractionState::Focused;
            return ScreenInteractionState::Normal;
        }

        void Render(
            const wi::Canvas& canvas,
            const wi::graphics::CommandList cmd) const override
        {
            wi::gui::Widget::Render(canvas, cmd);
            if (!IsVisible()) return;

            auto clipped = scissorRect;
            if (hasClip)
            {
                clipped.left = std::max(clipped.left, static_cast<int32_t>(std::ceil(clip.x)));
                clipped.top = std::max(clipped.top, static_cast<int32_t>(std::ceil(clip.y)));
                clipped.right = std::min(clipped.right, static_cast<int32_t>(std::floor(clip.x + clip.width)));
                clipped.bottom = std::min(clipped.bottom, static_cast<int32_t>(std::floor(clip.y + clip.height)));
            }
            ApplyScissor(canvas, clipped, cmd);
            const auto interaction = Interaction();
            const auto style = renegade::bridge::ResolveAccessibleScreenStyle(authored.style,authored.kind,accessibility);
            const auto& visual = SelectVisual(style, interaction);
            const float geometryScale = std::min(scaleX, scaleY);
            const float border = std::clamp(
                style.borderWidth * geometryScale,
                0.0f,
                std::min(scale.x, scale.y) * 0.5f);
            const float radius = style.cornerRadius * geometryScale;

            if (border > 0.0f)
            {
                DrawSurface(
                    nullptr, translation.x, translation.y, scale.x, scale.y,
                    radius, ToWickedColor(style.borderColor, style.opacity), cmd);
            }

            const auto resource = resources.find(interaction);
            const wi::Resource* image = (accessibility.contrast!=0 &&
                authored.kind!=renegade::bridge::ScreenWidgetKind::Image) || resource == resources.end() ?
                nullptr : &resource->second;
            const ScreenColor& surfaceColor = image != nullptr && image->IsValid() ?
                visual.imageTint : visual.background;
            renegade::bridge::ScreenRect imageRect{
                translation.x+border,translation.y+border,scale.x-border*2,scale.y-border*2};
            if(image&&image->IsValid()&&authored.kind==renegade::bridge::ScreenWidgetKind::Image) {
                const auto& desc=image->GetTexture().GetDesc();
                imageRect=renegade::bridge::FitScreenImage(imageRect,float(desc.width),float(desc.height),authored.imageFit);
            }
            DrawSurface(image,imageRect.x,imageRect.y,imageRect.width,imageRect.height,
                std::max(0.0f,radius-border),ToWickedColor(surfaceColor,style.opacity),cmd);

            if (authored.kind == renegade::bridge::ScreenWidgetKind::Progress)
            {
                const auto fillRect = renegade::bridge::ResolveScreenFillRect(
                    {translation.x, translation.y, scale.x, scale.y},
                    authored.fill, authored.fillDirection, border);
                if (progressFillImage.IsValid())
                {
                    // Clip a full-width textured bar instead of shrinking the artwork.
                    auto fillClip = clipped;
                    fillClip.left = std::max(fillClip.left, static_cast<int32_t>(std::ceil(fillRect.x)));
                    fillClip.top = std::max(fillClip.top, static_cast<int32_t>(std::ceil(fillRect.y)));
                    fillClip.right = std::min(fillClip.right, static_cast<int32_t>(std::floor(fillRect.x + fillRect.width)));
                    fillClip.bottom = std::min(fillClip.bottom, static_cast<int32_t>(std::floor(fillRect.y + fillRect.height)));
                    if (fillClip.right > fillClip.left && fillClip.bottom > fillClip.top)
                    {
                        ApplyScissor(canvas, fillClip, cmd);
                        DrawSurface(&progressFillImage, translation.x + border, translation.y + border,
                            scale.x - border * 2.0f, scale.y - border * 2.0f,
                            std::max(0.0f, radius - border),
                            ToWickedColor(visual.imageTint, style.opacity), cmd);
                        ApplyScissor(canvas, clipped, cmd);
                    }
                }
                else
                    DrawSurface(nullptr, fillRect.x, fillRect.y, fillRect.width, fillRect.height,
                        std::max(0.0f, radius - border),
                        ToWickedColor(visual.foreground, style.opacity), cmd);
                if (progressFrameImage.IsValid())
                    DrawSurface(&progressFrameImage, translation.x, translation.y, scale.x, scale.y,
                        radius, ToWickedColor(visual.imageTint, style.opacity), cmd);
            }

            if(authored.kind==renegade::bridge::ScreenWidgetKind::Slider)
            {
                const auto& c=authored.control;
                const float fraction=float((std::get<double>(c.value)-c.minimum)/(c.maximum-c.minimum));
                const float y=translation.y+scale.y*.78f;
                DrawSurface(nullptr,translation.x+border,y,scale.x-border*2,std::max(3.0f,scale.y*.08f),
                    0,ToWickedColor(visual.background,style.opacity),cmd);
                DrawSurface(nullptr,translation.x+border,y,(scale.x-border*2)*fraction,std::max(3.0f,scale.y*.08f),
                    0,ToWickedColor(visual.foreground,style.opacity),cmd);
                const float knob=std::max(6.0f,scale.y*.15f);
                DrawSurface(nullptr,translation.x+(scale.x-knob)*fraction,y-knob*.3f,knob,knob,
                    knob*.5f,ToWickedColor(visual.foreground,style.opacity),cmd);
            }
            if (authored.kind == renegade::bridge::ScreenWidgetKind::Text ||
                renegade::bridge::IsScreenInteractive(authored.kind))
            {
                wi::font::Params params;
                const auto& text = style.text;
                params.size = std::max(1, static_cast<int>(std::round(
                    text.fontSize * geometryScale)));
                params.style = fontStyle;
                params.spacingX = text.characterSpacing * geometryScale;
                params.spacingY = text.lineSpacing * geometryScale;
                params.softness = text.softness;
                params.bolden = text.bolden;
                params.shadowColor = ToWickedColor(
                    text.shadowColor, style.opacity);
                params.shadow_offset_x = text.shadowOffsetX * geometryScale;
                params.shadow_offset_y = text.shadowOffsetY * geometryScale;
                params.shadow_softness = text.shadowSoftness;
                params.shadow_bolden = text.shadowBolden;
                params.h_align = ToWickedAlignment(text.horizontalAlignment);
                params.v_align = ToWickedAlignment(text.verticalAlignment);
                params.color = ToWickedColor(visual.foreground, style.opacity);

                const float left = translation.x + border;
                const float top = translation.y + border;
                const float width = std::max(0.0f, scale.x - border * 2.0f);
                const float height = std::max(0.0f, scale.y - border * 2.0f);
                params.posX = text.horizontalAlignment ==
                        ScreenHorizontalAlignment::Center ? left + width * 0.5f :
                    (text.horizontalAlignment == ScreenHorizontalAlignment::Right ?
                        left + width : left);
                params.posY = text.verticalAlignment ==
                        ScreenVerticalAlignment::Center ? top + height * 0.5f :
                    (text.verticalAlignment == ScreenVerticalAlignment::Bottom ?
                        top + height : top);
                params.h_wrap = text.wrap ? width : -1.0f;
                std::string display=authored.text;
                if(renegade::bridge::IsScreenValueControl(authored.kind))
                {
                    using namespace renegade::bridge;
                    const auto value=ScreenControlDisplay(ScreenControlKindFor(authored.kind),authored.control);
                    if(authored.kind==ScreenWidgetKind::TextInput)
                    {
                        display=input.GetState()==wi::gui::ACTIVE?const_cast<ScreenSurface*>(this)->input.GetCurrentInputValue():value;
                        if(input.GetState()!=wi::gui::ACTIVE&&display.empty())display=authored.text;
                    }
                    else display+=(display.empty()?"":" // ")+value;
                }
                auto textClip=clipped;
                textClip.left=std::max(textClip.left,static_cast<int32_t>(std::ceil(left)));
                textClip.top=std::max(textClip.top,static_cast<int32_t>(std::ceil(top)));
                textClip.right=std::min(textClip.right,static_cast<int32_t>(std::floor(left+width)));
                textClip.bottom=std::min(textClip.bottom,static_cast<int32_t>(std::floor(top+height)));
                ApplyScissor(canvas,textClip,cmd);
                if(authored.kind==renegade::bridge::ScreenWidgetKind::TextInput&&input.GetState()==wi::gui::ACTIVE)
                    const_cast<ScreenSurface*>(this)->input.RenderEditor(canvas,cmd,params,textClip);
                else if(text.wrap&&authored.kind!=renegade::bridge::ScreenWidgetKind::TextInput)
                    DrawWrappedText(display,params,{left,top,width,height},
                        text.horizontalAlignment,text.verticalAlignment,cmd);
                else wi::font::Draw(display, params, cmd);
                ApplyScissor(canvas,clipped,cmd);
                if(!controlDiagnostic.empty())
                {
                    DrawSurface(nullptr,translation.x,translation.y+std::max(0.0f,scale.y-16.0f),
                        scale.x,std::min(16.0f,scale.y),0,wi::Color(12,16,22,240),cmd);
                    wi::font::Params issue;issue.posX=translation.x+4;issue.posY=translation.y+scale.y-13;
                    issue.size=10;issue.h_wrap=std::max(1.0f,scale.x-8);issue.color=wi::Color(255,100,100,255);
                    wi::font::Draw(controlDiagnostic,issue,cmd);
                }
            }
            // A focused semantic colour alone can be indistinguishable from
            // hover/normal creator styling. Always add a bounded dual-contrast
            // outline for keyboard/gamepad focus. Both strokes remain visible
            // against either light or dark surfaces without recolouring the
            // creator's content, and use the exact shared runtime geometry.
            if(keyboardFocused && IsEnabled() && authored.enabled &&
                renegade::bridge::IsScreenInteractive(authored.kind))
            {
                const float stroke=std::min(std::clamp(accessibility.focusWidth*geometryScale,1.0f,8.0f),
                    std::min(scale.x,scale.y)/6.0f);
                if(scale.x>stroke*5.0f && scale.y>stroke*5.0f)
                {
                    const auto outline=[&](const float inset,const wi::Color colour)
                    {
                        const float x=translation.x+inset,y=translation.y+inset;
                        const float w=scale.x-2.0f*inset,h=scale.y-2.0f*inset;
                        DrawSurface(nullptr,x,y,w,stroke,0,colour,cmd);
                        DrawSurface(nullptr,x,y+h-stroke,w,stroke,0,colour,cmd);
                        DrawSurface(nullptr,x,y+stroke,stroke,h-2.0f*stroke,0,colour,cmd);
                        DrawSurface(nullptr,x+w-stroke,y+stroke,stroke,h-2.0f*stroke,0,colour,cmd);
                    };
                    outline(0.0f,wi::Color(0,0,0,255));
                    outline(stroke,ToWickedColor(renegade::bridge::ScreenAccessibleFocusColor(accessibility),1));
                }
            }
        }

        const char* GetWidgetTypeName() const override
        {
            return "RenegadeScreenSurface";
        }
    };
}

namespace renegade::screen
{
    bool BuildScreenRenderItems(
        const bridge::ScreenDocument& document,
        const float outputWidth,
        const float outputHeight,
        const std::unordered_map<bridge::StableId, ScreenInteractionState>&
            interactions,
        std::vector<ScreenRenderItem>& items,
        std::string& error,float layoutWidth)
    {
        bridge::ScreenCanvasTransform canvas;
        if (!bridge::ResolveScreenCanvasTransform(
                document, outputWidth, outputHeight, canvas, error))
        {
            return false;
        }
        const float metricScale = std::min(canvas.scaleX, canvas.scaleY);
        items.clear();
        items.reserve(document.widgets.size());
        std::vector<bridge::ScreenRect> designRects;
        if(!bridge::ResolveScreenWidgetRects(document,designRects,error,layoutWidth==0?outputWidth:layoutWidth))return false;
        std::size_t widgetIndex=0;
        for (const auto& widget : document.widgets)
        {
            const auto& designRect=designRects[widgetIndex++];
            ScreenInteractionState interaction = ScreenInteractionState::Normal;
            const auto requested = interactions.find(widget.id);
            if (requested != interactions.end()) interaction = requested->second;
            if (bridge::IsScreenInteractive(widget.kind) &&
                !widget.enabled)
                interaction = ScreenInteractionState::Disabled;

            ScreenRenderItem item;
            item.widgetId = widget.id;
            item.kind = widget.kind;
            item.logicalRect = {
                canvas.offsetX + designRect.x * canvas.scaleX,
                canvas.offsetY + designRect.y * canvas.scaleY,
                designRect.width * canvas.scaleX,
                designRect.height * canvas.scaleY,
            };
            item.scaleX = canvas.scaleX;
            item.scaleY = canvas.scaleY;
            item.interaction = interaction;
            item.visual = SelectVisual(widget.style, interaction);
            item.borderColor = widget.style.borderColor;
            item.borderWidth = widget.style.borderWidth * metricScale;
            item.cornerRadius = widget.style.cornerRadius * metricScale;
            item.opacity = widget.style.opacity;
            item.fill = widget.fill;
            item.fillDirection = widget.fillDirection;
            if(bridge::IsScreenValueControl(widget.kind))
            {
                if(!bridge::ValidateScreenControl(bridge::ScreenControlKindFor(widget.kind),widget.control,error))
                {items.clear();return false;}
                item.controlValue=widget.control.value;
                if(widget.kind==bridge::ScreenWidgetKind::Slider)
                    item.controlFraction=float((std::get<double>(widget.control.value)-widget.control.minimum)/
                        (widget.control.maximum-widget.control.minimum));
            }
            if (widget.kind == bridge::ScreenWidgetKind::Progress)
                item.fillRect = bridge::ResolveScreenFillRect(
                    item.logicalRect, widget.fill, widget.fillDirection, item.borderWidth);
            item.fontSize = widget.style.text.fontSize * metricScale;
            item.characterSpacing =
                widget.style.text.characterSpacing * metricScale;
            item.lineSpacing = widget.style.text.lineSpacing * metricScale;
            item.shadowOffsetX = widget.style.text.shadowOffsetX * metricScale;
            item.shadowOffsetY = widget.style.text.shadowOffsetY * metricScale;
            item.visible = VisibleInHierarchy(document,widget);
            item.enabled = widget.enabled;
            items.push_back(std::move(item));
        }
        error.clear();
        return true;
    }

    bool BuildScreenRenderItemsInViewport(
        const bridge::ScreenDocument& document,
        const bridge::ScreenRect& viewport,
        const std::unordered_map<bridge::StableId, ScreenInteractionState>&
            interactions,
        std::vector<ScreenRenderItem>& items,
        std::string& error,float layoutWidth)
    {
        if (!std::isfinite(viewport.x) || !std::isfinite(viewport.y) ||
            !std::isfinite(viewport.width) ||
            !std::isfinite(viewport.height) || viewport.width <= 0.0f ||
            viewport.height <= 0.0f)
        {
            items.clear();
            error = "Screen render viewport must have finite positive geometry.";
            return false;
        }
        if (!BuildScreenRenderItems(
                document, viewport.width, viewport.height, interactions,
                items, error,layoutWidth))
        {
            return false;
        }
        for (auto& item : items)
        {
            item.logicalRect.x += viewport.x;
            item.logicalRect.y += viewport.y;
            if (item.kind == bridge::ScreenWidgetKind::Progress)
            {
                item.fillRect.x += viewport.x;
                item.fillRect.y += viewport.y;
            }
        }
        return true;
    }

    bool BuildAnimatedScreenRenderItems(const bridge::ScreenDocument& document,
        const bridge::ScreenRect& viewport,double seconds,bool reducedMotion,
        const std::unordered_map<bridge::StableId,ScreenInteractionState>& interactions,
        std::vector<ScreenRenderItem>& items,std::string& error,float layoutWidth)
    {
        if(!std::isfinite(seconds)||seconds<0)
        {items.clear();error="Animation clock must be finite and nonnegative.";return false;}
        if(!BuildScreenRenderItemsInViewport(document,viewport,interactions,items,error,layoutWidth))return false;
        const auto base=items;
        std::unordered_map<bridge::StableId,std::size_t> indices;
        for(std::size_t i=0;i<document.widgets.size();++i)indices.emplace(document.widgets[i].id,i);
        for(std::size_t i=0;i<items.size();++i)
        {
            auto& item=items[i];auto index=i;
            for(std::size_t depth=0;depth<=document.widgets.size();++depth)
            {
                const auto& widget=document.widgets[index];
                if(!bridge::ValidateScreenAnimations(widget.animations,
                    widget.kind==bridge::ScreenWidgetKind::Progress,error)){items.clear();return false;}
                float dx=0,dy=0,scale=1,opacity=1;
                for(const auto& track:widget.animations)
                {
                    const float v=bridge::EvaluateScreenAnimation(track,seconds,reducedMotion);
                    switch(track.property)
                    {
                    case bridge::ScreenAnimationProperty::Opacity:opacity=v;break;
                    case bridge::ScreenAnimationProperty::OffsetX:dx=v;break;
                    case bridge::ScreenAnimationProperty::OffsetY:dy=v;break;
                    case bridge::ScreenAnimationProperty::Scale:scale=v;break;
                    case bridge::ScreenAnimationProperty::Fill:if(index==i)item.fill=v;break;
                    }
                }
                const auto& bounds=base[index].logicalRect;
                const float cx=bounds.x+bounds.width*.5f,cy=bounds.y+bounds.height*.5f;
                auto& rect=item.logicalRect;
                rect.x=cx+(rect.x-cx)*scale+dx*base[index].scaleX;
                rect.y=cy+(rect.y-cy)*scale+dy*base[index].scaleY;
                rect.width*=scale;rect.height*=scale;
                item.scaleX*=scale;item.scaleY*=scale;
                item.fontSize*=scale;item.characterSpacing*=scale;item.lineSpacing*=scale;
                item.shadowOffsetX*=scale;item.shadowOffsetY*=scale;
                item.borderWidth*=scale;item.cornerRadius*=scale;item.opacity*=opacity;
                if(widget.parentId.empty())break;
                const auto parent=indices.find(widget.parentId);
                if(parent==indices.end()||depth==document.widgets.size())
                {items.clear();error="Animation parent hierarchy is invalid.";return false;}
                index=parent->second;
            }
            const auto& rect=item.logicalRect;
            if(!std::isfinite(rect.x)||!std::isfinite(rect.y)||!std::isfinite(rect.width)||
                !std::isfinite(rect.height)||!std::isfinite(item.fontSize))
            {items.clear();error="Animated geometry exceeded finite range.";return false;}
            if(item.kind==bridge::ScreenWidgetKind::Progress)
                item.fillRect=bridge::ResolveScreenFillRect(rect,item.fill,item.fillDirection,item.borderWidth);
        }
        error.clear();return true;
    }

    bool ApplyScreenTransitionFrame(const bridge::ScreenDocument& document,
        const bridge::ScreenRect& viewport,const bridge::ScreenTransitionFrame& frame,
        std::vector<ScreenRenderItem>& items,std::string& error)
    {
        if(!std::isfinite(frame.opacity)||frame.opacity<0||frame.opacity>1||
            !std::isfinite(frame.scale)||frame.scale<=0||frame.scale>64||
            !std::isfinite(frame.offsetX)||!std::isfinite(frame.offsetY)||
            std::abs(frame.offsetX)>32768||std::abs(frame.offsetY)>32768)
        {error="Invalid Screen transition frame.";return false;}
        bridge::ScreenCanvasTransform canvas;
        if(!bridge::ResolveScreenCanvasTransform(document,viewport.width,viewport.height,canvas,error))return false;
        if(frame.opacity==1&&frame.scale==1&&frame.offsetX==0&&frame.offsetY==0)
        {error.clear();return true;} // exact legacy geometry; no active-frame transform work
        const float cx=viewport.x+viewport.width*.5f,cy=viewport.y+viewport.height*.5f;
        for(auto& item:items)
        {
            auto& rect=item.logicalRect;
            rect.x=cx+(rect.x-cx)*frame.scale+frame.offsetX*canvas.scaleX;
            rect.y=cy+(rect.y-cy)*frame.scale+frame.offsetY*canvas.scaleY;
            rect.width*=frame.scale;rect.height*=frame.scale;
            item.scaleX*=frame.scale;item.scaleY*=frame.scale;
            item.opacity*=frame.opacity;item.fontSize*=frame.scale;
            item.characterSpacing*=frame.scale;item.lineSpacing*=frame.scale;
            item.shadowOffsetX*=frame.scale;item.shadowOffsetY*=frame.scale;
            item.borderWidth*=frame.scale;item.cornerRadius*=frame.scale;
            if(item.kind==bridge::ScreenWidgetKind::Progress)
                item.fillRect=bridge::ResolveScreenFillRect(rect,item.fill,item.fillDirection,item.borderWidth);
        }
        error.clear();return true;
    }

    // One native GUI root per Screen instance. Native focus must never
    // reorder authored surfaces or interleave widgets from different Screens.
    class ScreenInstanceRoot final : public wi::gui::Widget
    {
    public:
        std::vector<ScreenSurface*> surfaces;
        void Update(const wi::Canvas& canvas,float dt) override
        {
            Widget::Update(canvas,dt);
            state=wi::gui::IDLE;priority_change=false;
            if(!IsVisible())return;
            bool blocked=force_disable;
            for(auto it=surfaces.rbegin();it!=surfaces.rend();++it)
            {
                auto* surface=*it;
                surface->force_disable=blocked;
                surface->Update(canvas,dt);
                surface->force_disable=false;
                surface->priority_change=false;
                if(surface->IsVisible() && surface->IsEnabled() && surface->GetState()>wi::gui::IDLE)
                {blocked=true;state=wi::gui::FOCUS;}
            }
        }
        void Render(const wi::Canvas& canvas,wi::graphics::CommandList cmd) const override
        {
            if(!IsVisible())return;
            for(const auto* surface:surfaces)surface->Render(canvas,cmd);
        }
    };

    bool BuildAccessibleScreenRenderItems(const bridge::ScreenDocument& document,
        const bridge::ScreenRect& viewport,const bridge::ScreenAccessibilitySettings& settings,
        const bridge::ScreenAnimationPreview& animation,const bridge::ScreenTransitionFrame& transition,
        const std::unordered_map<bridge::StableId,ScreenInteractionState>& interactions,
        std::vector<ScreenRenderItem>& items,std::string& error,float layoutWidth)
    {
        if(!bridge::ValidateScreenAccessibilitySettings(settings))
        {error="Invalid accessibility geometry settings.";return false;}
        // The common/default path does not copy documents.
        bridge::ScreenDocument scaled;
        const bridge::ScreenDocument* layout=&document;
        if(settings.uiScale!=1)
        {
            scaled=document;
            scaled.designWidth/=settings.uiScale;scaled.designHeight/=settings.uiScale;
            scaled.safeArea.left/=settings.uiScale;scaled.safeArea.right/=settings.uiScale;
            scaled.safeArea.top/=settings.uiScale;scaled.safeArea.bottom/=settings.uiScale;
            layout=&scaled;
        }
        const float logicalWidth=(layoutWidth>0?layoutWidth:viewport.width)/settings.uiScale;
        const bool built=animation.active?
            BuildAnimatedScreenRenderItems(*layout,viewport,animation.seconds,animation.reducedMotion,
                interactions,items,error,logicalWidth):
            BuildScreenRenderItemsInViewport(*layout,viewport,interactions,items,error,logicalWidth);
        if(!built)return false;
        for(auto& item:items)
        {
            if(!bridge::IsScreenInteractive(item.kind)||settings.minimumTargetSize==0)continue;
            // Grow the visible surface and input surface together; never create
            // an invisible overlapping hit slop region. Ordering stays authored.
            const auto width=std::max(item.logicalRect.width,settings.minimumTargetSize);
            const auto height=std::max(item.logicalRect.height,settings.minimumTargetSize);
            item.logicalRect.x-=(width-item.logicalRect.width)*.5f;
            item.logicalRect.y-=(height-item.logicalRect.height)*.5f;
            item.logicalRect.width=width;item.logicalRect.height=height;
        }
        return ApplyScreenTransitionFrame(*layout,viewport,transition,items,error);
    }

    struct ScreenRenderer::Impl
    {
        bridge::ScreenDocument document;
        std::string projectRoot;
        ActivateSink activateSink;
        ControlSink controlSink;
        DragSink dragSink;
        ScreenInstanceRoot root;
        bool attached=false;
        std::vector<std::unique_ptr<ScreenSurface>> surfaces;
        std::unordered_map<bridge::StableId, ScreenSurface*> byId;
        std::unordered_map<bridge::StableId, bridge::ScreenRect> logicalRects;
        struct ScrollThumb {bridge::StableId trackId;float position=0;float fraction=1;};
        std::unordered_map<bridge::StableId,ScrollThumb> scrollThumbs;
        bridge::ScreenRect viewport;
        bool hasViewport = false;
        bridge::ScreenRect clip;
        bool hasClip = false;
        bool inputEnabled = true;
        bool loaded = false;
        // Static Screen geometry is stable between authored edits and viewport
        // changes. Avoid rebuilding hundreds of slot rects every editor frame.
        bool layoutDirty = true;
        float lastLayoutWidth = -1, lastLayoutHeight = -1;
        bridge::ScreenAnimationPreview animation;
        bridge::ScreenTransitionFrame transition;
        bridge::ScreenAccessibilitySettings accessibility;
    };

    ScreenRenderer::ScreenRenderer() : impl_(std::make_unique<Impl>()) {}
    ScreenRenderer::~ScreenRenderer() = default;
    ScreenRenderer::ScreenRenderer(ScreenRenderer&&) noexcept = default;
    ScreenRenderer& ScreenRenderer::operator=(ScreenRenderer&&) noexcept = default;

    bool ScreenRenderer::Load(
        const bridge::ScreenDocument& authoredDocument,
        const std::string& projectRoot,
        wi::RenderPath2D& renderPath,
        ActivateSink activateSink,
        std::string& error, ControlSink controlSink,DragSink dragSink)
    {
        bridge::ScreenDocument document;
        if(!bridge::ResolveScreenTheme(projectRoot,authoredDocument,document,error))return false;
        Reset(renderPath);
        if (!bridge::ValidateScreenDocument(
                document, document.envelope.projectId, error))
        {
            return false;
        }

        impl_->document = document;
        impl_->projectRoot = projectRoot;
        impl_->activateSink = std::move(activateSink);
        impl_->controlSink=std::move(controlSink);
        impl_->dragSink=std::move(dragSink);

        // A grid can reference identical artwork hundreds of times across
        // normal/hover/pressed/focused/disabled states. Resolve/load once per
        // preview reload, then share the retained Wicked resource handles.
        std::unordered_map<std::string, wi::Resource> resourceCache;
        resourceCache.reserve(32);
        const auto loadResource = [&projectRoot, &error, &resourceCache](
            const std::string& resourcePath, wi::Resource& resource)
        {
            if (resourcePath.empty()) return true;
            const auto cached = resourceCache.find(resourcePath);
            if (cached != resourceCache.end()) {
                resource = cached->second;
                return true;
            }
            std::string resolved;
            if (!bridge::ResolveScreenResourcePath(
                    projectRoot, resourcePath, resolved, error))
                return false;
            resource = wi::resourcemanager::Load(resolved);
            if (!resource.IsValid())
            {
                error = "Wicked could not load Screen resource: " + resolved;
                return false;
            }
            resourceCache.emplace(resourcePath, resource);
            return true;
        };

        // Wicked renders GUI storage in reverse. Reverse insertion preserves
        // authored back-to-front order without mutating the Screen document.
        for (auto iterator = document.widgets.begin();
             iterator != document.widgets.end(); ++iterator)
        {
            const auto& widget = *iterator;
            auto surface = std::make_unique<ScreenSurface>();
            surface->Create(widget.name + "#" + widget.id);
            surface->authored = widget;
            surface->SetVisible(VisibleInHierarchy(document,widget));
            surface->SetEnabled(
                impl_->inputEnabled &&
                bridge::IsScreenInteractive(widget.kind) && widget.enabled);
            surface->SetText(widget.text);
            surface->SetShadowRadius(0.0f);
            if (widget.kind == bridge::ScreenWidgetKind::Progress)
            {
                if (!loadResource(widget.progressFrameResourcePath, surface->progressFrameImage) ||
                    !loadResource(widget.progressFillResourcePath, surface->progressFillImage))
                { Reset(renderPath); return false; }
            }

            std::string primary = widget.style.normal.imageResourcePath;
            if (primary.empty() && widget.kind == bridge::ScreenWidgetKind::Image)
                primary = widget.resourcePath;
            for (const auto interaction : {
                    ScreenInteractionState::Normal,
                    ScreenInteractionState::Hover,
                    ScreenInteractionState::Pressed,
                    ScreenInteractionState::Focused,
                    ScreenInteractionState::Disabled})
            {
                const auto& visual = SelectVisual(widget.style, interaction);
                const std::string path = visual.imageResourcePath.empty() ?
                    primary : visual.imageResourcePath;
                wi::Resource resource;
                if (!loadResource(path, resource))
                {
                    Reset(renderPath);
                    return false;
                }
                surface->resources.emplace(interaction, std::move(resource));
            }

            if (widget.kind == bridge::ScreenWidgetKind::Text ||
                bridge::IsScreenInteractive(widget.kind))
            {
                if (widget.style.text.fontResource == bridge::BuiltinScreenFont)
                {
                    surface->fontStyle = 0;
                }
                else
                {
                    std::string fontPath;
                    if (!bridge::ResolveScreenResourcePath(
                            projectRoot, widget.style.text.fontResource,
                            fontPath, error))
                    {
                        Reset(renderPath);
                        return false;
                    }
                    surface->fontStyle = wi::font::AddFontStyle(fontPath);
                    if (surface->fontStyle < 0)
                    {
                        error = "Wicked could not load Screen font: " + fontPath;
                        Reset(renderPath);
                        return false;
                    }
                    // ScreenSurface draws through wi::font directly, so the
                    // inherited SpriteFont must not reset the authored style.
                    surface->font.SetDisableUpdate(true);
                }
            }

            if(bridge::IsScreenValueControl(widget.kind))
            {
                const auto id=widget.id;auto* rawSurface=surface.get();
                surface->changeControl=[this,id](const bridge::GameplayAttributeValue& value){
                    return IsWidgetInteractive(id)&&impl_->controlSink&&impl_->controlSink(id,value);
                };
                if(widget.kind==bridge::ScreenWidgetKind::TextInput)
                {
                    surface->input.Create("Screen text value "+id);
                    surface->input.SetValue(std::get<std::string>(widget.control.value));
                    surface->input.SetCancelInputEnabled(true);
                    surface->input.SetShadowRadius(0);surface->input.SetDescription("");
                    surface->input.SetColor(wi::Color::Transparent());
                }
                else
                {
                    surface->OnClick([rawSurface](const wi::gui::EventArgs&){
                        // Native Button can revisit DEACTIVATING while held.
                        // Value changes are one release gesture, not frame repeats.
                        if(wi::input::Down(wi::input::MOUSE_BUTTON_LEFT))return;
                        if(rawSurface->authored.kind==bridge::ScreenWidgetKind::Slider)rawSurface->PointerControl();
                        else (void)rawSurface->ActivateValue();
                    });
                    if(widget.kind==bridge::ScreenWidgetKind::Slider)
                    {
                        surface->OnDragStart([rawSurface](const wi::gui::EventArgs&){rawSurface->PointerControl();});
                        surface->OnDrag([rawSurface](const wi::gui::EventArgs&){rawSurface->PointerControl();});
                    }
                }
            }
            if (widget.kind == bridge::ScreenWidgetKind::Button)
            {
                const auto widgetId = widget.id;
                const bool inventorySlot =
                    widget.actionId.rfind("inventory.slot.",0)==0 ||
                  widget.actionId.rfind("inventory.paged.",0)==0;
                if (inventorySlot) {
                    surface->OnDragStart([this,widgetId](
                        const wi::gui::EventArgs& args) {
                        if (impl_->dragSink)
                            impl_->dragSink(widgetId,true,
                                args.startPos.x,args.startPos.y);
                    });
                    surface->OnDragEnd([this,widgetId](
                        const wi::gui::EventArgs& args) {
                        if (impl_->dragSink)
                            impl_->dragSink(widgetId,false,
                                args.clickPos.x,args.clickPos.y);
                    });
                }
                surface->OnClick([this, widgetId, inventorySlot](
                    const wi::gui::EventArgs&)
                {
                    // Wicked Button::Update invokes OnClick on release after
                    // DEACTIVATING, so this is a single gesture. The legacy
                    // held-button guard can discard that event on the same
                    // input frame. Exempt only the opt-in inventory slot
                    // callbacks; all existing UI actions retain their guard.
                    if(!inventorySlot &&
                       wi::input::Down(wi::input::MOUSE_BUTTON_LEFT))return;
                    if (impl_->activateSink) impl_->activateSink(widgetId);
                });
            }

            auto* raw = surface.get();
            impl_->root.surfaces.push_back(raw);
            impl_->byId.emplace(widget.id, raw);
            impl_->surfaces.push_back(std::move(surface));
        }

        impl_->root.SetName("Renegade Screen instance");
        impl_->root.SetSize(XMFLOAT2(0,0));
        impl_->root.SetVisible(true);
        impl_->root.priority_change=false;
        impl_->loaded = true;
        AttachPresentation(renderPath);
        ApplyLayout(renderPath);
        error.clear();
        return true;
    }

    void ScreenRenderer::SetViewport(
        const bridge::ScreenRect& viewport) noexcept
    {
        impl_->layoutDirty = true;
        impl_->viewport = viewport;
        impl_->hasViewport = std::isfinite(viewport.x) &&
            std::isfinite(viewport.y) && std::isfinite(viewport.width) &&
            std::isfinite(viewport.height) && viewport.width > 0.0f &&
            viewport.height > 0.0f;
    }

    void ScreenRenderer::ClearViewport() noexcept
    {
        impl_->layoutDirty = true;
        impl_->viewport = {};
        impl_->hasViewport = false;
    }

    void ScreenRenderer::SetClipViewport(const bridge::ScreenRect& clip) noexcept
    {
        impl_->layoutDirty = true;
        impl_->clip = clip;
        impl_->hasClip = std::isfinite(clip.x) && std::isfinite(clip.y) &&
            std::isfinite(clip.width) && std::isfinite(clip.height) &&
            clip.width > 0 && clip.height > 0;
    }

    void ScreenRenderer::ClearClipViewport() noexcept
    {
        impl_->layoutDirty = true;
        impl_->clip = {};
        impl_->hasClip = false;
    }

    void ScreenRenderer::ApplyLayout(wi::RenderPath2D& renderPath)
    {
        if (!impl_->loaded) return;
        const float width=renderPath.GetLogicalWidth(), height=renderPath.GetLogicalHeight();
        if (!impl_->layoutDirty && !impl_->animation.active &&
            impl_->lastLayoutWidth==width && impl_->lastLayoutHeight==height) return;
        std::unordered_map<bridge::StableId, ScreenInteractionState> states;
        for (const auto& [id, surface] : impl_->byId)
            states.emplace(id, surface->Interaction());
        std::vector<ScreenRenderItem> items;
        std::string error;
        const bridge::ScreenRect animationViewport=impl_->hasViewport?impl_->viewport:
            bridge::ScreenRect{0,0,renderPath.GetLogicalWidth(),renderPath.GetLogicalHeight()};
        if(!BuildAccessibleScreenRenderItems(impl_->document,animationViewport,impl_->accessibility,
            impl_->animation,impl_->transition,states,items,error,
            impl_->hasViewport?impl_->document.designWidth:animationViewport.width))return;

        // Thumb location follows the resolved track, including responsive
        // design scaling and accessibility. Its saved Screen rect stays intact.
        for(auto& item:items) {
            const auto override=impl_->scrollThumbs.find(item.widgetId);
            if(override==impl_->scrollThumbs.end())continue;
            const auto track=std::find_if(items.begin(),items.end(),
                [&](const auto& candidate){
                    return candidate.widgetId==override->second.trackId;});
            if(track==items.end()||track->logicalRect.height<=0)continue;
            const float minThumb=std::min(track->logicalRect.height,
                std::max(14.0f,track->logicalRect.width*1.5f));
            const float thumbHeight=std::clamp(
                track->logicalRect.height*override->second.fraction,
                minThumb,track->logicalRect.height);
            item.logicalRect.height=thumbHeight;
            item.logicalRect.y=track->logicalRect.y+
                (track->logicalRect.height-thumbHeight)*override->second.position;
        }
        impl_->logicalRects.clear();
        for (const auto& item : items)
        {
            const auto found = impl_->byId.find(item.widgetId);
            if (found == impl_->byId.end()) continue;
            auto* surface = found->second;
            surface->SetPos(XMFLOAT2(
                item.logicalRect.x, item.logicalRect.y));
            surface->SetSize(XMFLOAT2(
                item.logicalRect.width, item.logicalRect.height));
            surface->accessibility = impl_->accessibility;
            surface->hasClip = impl_->hasClip;
            surface->clip = impl_->clip;
            surface->scaleX = item.scaleX;
            surface->scaleY = item.scaleY;
            surface->authored.style.opacity=item.opacity;
            surface->authored.fill=item.fill;
            // Each frame uses resolved hierarchy visibility; otherwise a
            // child can reappear when a dynamic binding hides its Group.
            if(surface->IsVisible()!=item.visible)surface->SetVisible(item.visible);
            surface->SetEnabled(impl_->inputEnabled&&item.enabled&&item.visible&&item.opacity>0.001f&&
                bridge::IsScreenInteractive(item.kind));
            impl_->logicalRects.emplace(item.widgetId, item.logicalRect);
        }
        impl_->layoutDirty = false;
        impl_->lastLayoutWidth=width;
        impl_->lastLayoutHeight=height;
    }

    bool ScreenRenderer::SetWidgetControlValue(const bridge::StableId& id,const bridge::GameplayAttributeValue& value)
    {
        const auto it=impl_->byId.find(id);
        if(it==impl_->byId.end()||!bridge::IsScreenValueControl(it->second->authored.kind))return false;
        auto control=it->second->authored.control;control.value=value;std::string error;
        if(!bridge::ValidateScreenControl(bridge::ScreenControlKindFor(it->second->authored.kind),control,error))return false;
        const bool changed=it->second->authored.control.value!=value;it->second->authored.control=control;
        if(changed)impl_->layoutDirty=true;
        if(changed&&it->second->authored.kind==bridge::ScreenWidgetKind::TextInput)
            it->second->input.SetValue(std::get<std::string>(value));
        for(auto& w:impl_->document.widgets)if(w.id==id){w.control=control;break;}
        return true;
    }
    bool ScreenRenderer::ActivateControl(const bridge::StableId& id)
    {
        const auto it=impl_->byId.find(id);
        return IsWidgetInteractive(id)&&it!=impl_->byId.end()&&it->second->ActivateValue();
    }
    bool ScreenRenderer::AdjustControl(const bridge::StableId& id,int direction)
    {
        const auto it=impl_->byId.find(id);
        return (direction==-1||direction==1)&&IsWidgetInteractive(id)&&it!=impl_->byId.end()&&it->second->Adjust(direction);
    }
    bool ScreenRenderer::ConsumesTextInput() const noexcept
    {
        for(const auto& entry:impl_->byId)if(entry.second->consumedText)return true;
        return false;
    }

    void ScreenRenderer::SetAccessibilitySettings(bridge::ScreenAccessibilitySettings settings) noexcept
    {
        if(!bridge::ValidateScreenAccessibilitySettings(settings))return;
        impl_->accessibility=settings;
        impl_->layoutDirty=true;
        for(auto& surface:impl_->surfaces)surface->accessibility=settings;
    }

    void ScreenRenderer::SetTransitionFrame(bridge::ScreenTransitionFrame frame) noexcept
    {
        const auto& previous=impl_->transition;
        if(previous.opacity!=frame.opacity || previous.scale!=frame.scale ||
           previous.offsetX!=frame.offsetX || previous.offsetY!=frame.offsetY)
            impl_->layoutDirty=true;
        impl_->transition=frame;
    }

    void ScreenRenderer::SetAnimationPreview(bridge::ScreenAnimationPreview preview) noexcept
    {
        if(impl_->animation.active || preview.active)impl_->layoutDirty=true;
        impl_->animation=preview;
    }
    bool ScreenRenderer::IsWidgetInteractive(const bridge::StableId& id) const noexcept
    {
        const auto it=impl_->byId.find(id);
        return impl_->inputEnabled&&it!=impl_->byId.end()&&impl_->root.IsVisible()&&
            it->second->IsVisible()&&it->second->IsEnabled()&&it->second->authored.enabled&&it->second->authored.style.opacity>0.001f;
    }

    void ScreenRenderer::SetInputEnabled(const bool enabled) noexcept
    {
        impl_->inputEnabled = enabled;
        for (const auto& [id, surface] : impl_->byId)
        {
            surface->SetEnabled(
                enabled &&
                bridge::IsScreenInteractive(surface->authored.kind) &&
                surface->authored.enabled);
        }
    }

    void ScreenRenderer::SetPresentationVisible(const bool visible) noexcept
    {
        if(impl_->root.IsVisible()==visible)return;
        impl_->root.SetVisible(visible);
        if(!visible)for(auto* surface:impl_->root.surfaces)
        {surface->CancelInteraction();}
    }
    void ScreenRenderer::AttachPresentation(wi::RenderPath2D& renderPath)
    {
        if(!impl_->loaded || impl_->attached)return;
        impl_->root.priority_change=false;
        renderPath.GetGUI().AddWidget(&impl_->root);impl_->attached=true;
    }
    void ScreenRenderer::DetachPresentation(wi::RenderPath2D& renderPath) noexcept
    {
        if(!impl_->attached)return;
        renderPath.GetGUI().RemoveWidget(&impl_->root);impl_->attached=false;
    }

    bool ScreenRenderer::IsInputEnabled() const noexcept
    {
        return impl_->inputEnabled;
    }

    void ScreenRenderer::SetFocusedWidget(const bridge::StableId& widgetId)
    {
        for (const auto& [id, surface] : impl_->byId)
            surface->keyboardFocused = !widgetId.empty() && id == widgetId;
    }

    void ScreenRenderer::SetWidgetState(
        const bridge::StableId& widgetId,
        const bool visible,
        const bool enabled)
    {
        const auto found = impl_->byId.find(widgetId);
        if (found == impl_->byId.end()) return;
        if(found->second->authored.visible!=visible ||
           found->second->authored.enabled!=enabled)impl_->layoutDirty=true;
        // This document is the renderer instance's transient layout source.
        // Keep it in sync, or ApplyLayout re-disables initially-disabled UP
        // buttons every frame and destroys their release-click state.
        for(auto& widget:impl_->document.widgets)if(widget.id==widgetId) {
            widget.visible=visible;widget.enabled=enabled;break;
        }
        found->second->authored.visible = visible;
        found->second->authored.enabled = enabled;
        found->second->SetVisible(visible);
        found->second->SetEnabled(
            impl_->inputEnabled &&
            bridge::IsScreenInteractive(found->second->authored.kind) &&
            enabled);
    }

    void ScreenRenderer::SetScrollbarThumb(const bridge::StableId& thumbId,
        const bridge::StableId& trackId,float position,float fraction)
    {
        if(!impl_->loaded || !std::isfinite(position) || !std::isfinite(fraction) ||
           impl_->byId.find(thumbId)==impl_->byId.end() ||
           impl_->byId.find(trackId)==impl_->byId.end())return;
        impl_->layoutDirty=true;
        impl_->scrollThumbs[thumbId]={
            trackId,std::clamp(position,0.0f,1.0f),std::clamp(fraction,0.0f,1.0f)};
    }

    void ScreenRenderer::SetWidgetFill(const bridge::StableId& widgetId, const float fill)
    {
        if (!std::isfinite(fill)) return;
        const auto found = impl_->byId.find(widgetId);
        if (found == impl_->byId.end() ||
            found->second->authored.kind != bridge::ScreenWidgetKind::Progress) return;
        const float value = std::clamp(fill, 0.0f, 1.0f);
        if(found->second->authored.fill!=value)impl_->layoutDirty=true;
        if(!impl_->animation.active)found->second->authored.fill = value;
        for (auto& widget : impl_->document.widgets)
            if (widget.id == widgetId) { widget.fill = value; break; }
    }

    void ScreenRenderer::SetWidgetText(const bridge::StableId& widgetId, const std::string& text)
    {
        const auto found = impl_->byId.find(widgetId);
        if (found == impl_->byId.end() ||
            (found->second->authored.kind != bridge::ScreenWidgetKind::Text &&
             !bridge::IsScreenInteractive(found->second->authored.kind))) return;
        if (found->second->authored.text == text) return;
        // Keep the authored document as the validated layout/resource baseline.
        // Dynamic empty text is a valid transient presentation value.
        found->second->authored.text = text;
        found->second->SetText(text);
    }

    bool ScreenRenderer::SetWidgetImageResource(
        const bridge::StableId& widgetId,
        const std::string& resourcePath,std::string& error)
    {
        const auto it=impl_->byId.find(widgetId);
        if(!impl_->loaded||it==impl_->byId.end()||
           it->second->authored.kind!=bridge::ScreenWidgetKind::Image) {
            error="Runtime Screen icon target is not a loaded Image widget.";
            return false;
        }
        if(it->second->authored.resourcePath==resourcePath) {
            error.clear();return true;
        }
        std::string resolved;
        if(!bridge::ResolveScreenResourcePath(
                impl_->projectRoot,resourcePath,resolved,error))return false;
        auto resource=wi::resourcemanager::Load(resolved);
        if(!resource.IsValid()) {
            error="Inventory icon resource unavailable: "+resourcePath;
            return false;
        }
        // UI icons have no alternate interaction-image states; changing a
        // transient icon never modifies a creator-authored layout or item.
        const auto& style=it->second->authored.style;
        if(!style.normal.imageResourcePath.empty()||
           !style.hover.imageResourcePath.empty()||
           !style.pressed.imageResourcePath.empty()||
           !style.focused.imageResourcePath.empty()||
           !style.disabled.imageResourcePath.empty()) {
            error="State-owned Screen image must not be overridden.";
            return false;
        }
        for(const auto state:{ScreenInteractionState::Normal,
                ScreenInteractionState::Hover,ScreenInteractionState::Pressed,
                ScreenInteractionState::Focused,ScreenInteractionState::Disabled})
            it->second->resources[state]=resource;
        it->second->authored.resourcePath=resourcePath;
        for(auto& widget:impl_->document.widgets)
            if(widget.id==widgetId) {
                widget.resourcePath=resourcePath;break;
            }
        error.clear();return true;
    }

    void ScreenRenderer::Reset(wi::RenderPath2D& renderPath) noexcept
    {
        DetachPresentation(renderPath);
        for(auto& surface:impl_->surfaces)surface->CancelInteraction();
        impl_->root.surfaces.clear();
        impl_->surfaces.clear();
        impl_->byId.clear();
        impl_->logicalRects.clear();
        impl_->scrollThumbs.clear();
        impl_->activateSink = {};
        impl_->controlSink={};
        impl_->dragSink={};
        impl_->document = {};
        impl_->projectRoot.clear();
        impl_->loaded = false;
        impl_->layoutDirty = true;
        impl_->animation={};impl_->transition={};
    }

    const bridge::ScreenRect* ScreenRenderer::LogicalRect(
        const bridge::StableId& widgetId) const noexcept
    {
        const auto found = impl_->logicalRects.find(widgetId);
        return found == impl_->logicalRects.end() ? nullptr : &found->second;
    }

    bool ScreenRenderer::IsLoaded() const noexcept
    {
        return impl_->loaded;
    }
}
