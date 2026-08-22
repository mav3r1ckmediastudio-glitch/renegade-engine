#include "renegade/screen/ScreenRenderer.h"

#include <WickedEngine.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
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

    class ScreenSurface final : public wi::gui::Button
    {
    public:
        renegade::bridge::ScreenWidget authored;
        std::unordered_map<ScreenInteractionState, wi::Resource> resources;
        int fontStyle = 0;
        float scaleX = 1.0f;
        float scaleY = 1.0f;
        bool keyboardFocused = false;

        ScreenInteractionState Interaction() const noexcept
        {
            if (authored.kind == renegade::bridge::ScreenWidgetKind::Button &&
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

            ApplyScissor(canvas, scissorRect, cmd);
            const auto interaction = Interaction();
            const auto& style = authored.style;
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
            const wi::Resource* image = resource == resources.end() ?
                nullptr : &resource->second;
            const ScreenColor& surfaceColor = image != nullptr && image->IsValid() ?
                visual.imageTint : visual.background;
            DrawSurface(
                image,
                translation.x + border,
                translation.y + border,
                scale.x - border * 2.0f,
                scale.y - border * 2.0f,
                std::max(0.0f, radius - border),
                ToWickedColor(surfaceColor, style.opacity),
                cmd);

            if (authored.kind == renegade::bridge::ScreenWidgetKind::Text ||
                authored.kind == renegade::bridge::ScreenWidgetKind::Button)
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
                wi::font::Draw(authored.text, params, cmd);
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
        std::string& error)
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
        for (const auto& widget : document.widgets)
        {
            bridge::ScreenRect designRect;
            if (!bridge::ResolveScreenWidgetRect(
                    document, widget.id, designRect, error))
            {
                items.clear();
                return false;
            }
            ScreenInteractionState interaction = ScreenInteractionState::Normal;
            const auto requested = interactions.find(widget.id);
            if (requested != interactions.end()) interaction = requested->second;
            if (widget.kind == bridge::ScreenWidgetKind::Button &&
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
            item.fontSize = widget.style.text.fontSize * metricScale;
            item.characterSpacing =
                widget.style.text.characterSpacing * metricScale;
            item.lineSpacing = widget.style.text.lineSpacing * metricScale;
            item.shadowOffsetX = widget.style.text.shadowOffsetX * metricScale;
            item.shadowOffsetY = widget.style.text.shadowOffsetY * metricScale;
            item.visible = widget.visible;
            item.enabled = widget.enabled;
            items.push_back(std::move(item));
        }
        error.clear();
        return true;
    }

    struct ScreenRenderer::Impl
    {
        bridge::ScreenDocument document;
        ActivateSink activateSink;
        std::vector<std::unique_ptr<ScreenSurface>> surfaces;
        std::unordered_map<bridge::StableId, ScreenSurface*> byId;
        std::unordered_map<bridge::StableId, bridge::ScreenRect> logicalRects;
        bool loaded = false;
    };

    ScreenRenderer::ScreenRenderer() : impl_(std::make_unique<Impl>()) {}
    ScreenRenderer::~ScreenRenderer() = default;
    ScreenRenderer::ScreenRenderer(ScreenRenderer&&) noexcept = default;
    ScreenRenderer& ScreenRenderer::operator=(ScreenRenderer&&) noexcept = default;

    bool ScreenRenderer::Load(
        const bridge::ScreenDocument& document,
        const std::string& projectRoot,
        wi::RenderPath2D& renderPath,
        ActivateSink activateSink,
        std::string& error)
    {
        Reset(renderPath);
        if (!bridge::ValidateScreenDocument(
                document, document.envelope.projectId, error))
        {
            return false;
        }

        impl_->document = document;
        impl_->activateSink = std::move(activateSink);

        const auto loadResource = [&projectRoot, &error](
            const std::string& resourcePath, wi::Resource& resource)
        {
            if (resourcePath.empty()) return true;
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
            return true;
        };

        // Wicked renders GUI storage in reverse. Reverse insertion preserves
        // authored back-to-front order without mutating the Screen document.
        for (auto iterator = document.widgets.rbegin();
             iterator != document.widgets.rend(); ++iterator)
        {
            const auto& widget = *iterator;
            auto surface = std::make_unique<ScreenSurface>();
            surface->Create(widget.name + "#" + widget.id);
            surface->authored = widget;
            surface->SetVisible(widget.visible);
            surface->SetEnabled(
                widget.kind == bridge::ScreenWidgetKind::Button && widget.enabled);
            surface->SetText(widget.text);
            surface->SetShadowRadius(0.0f);

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
                widget.kind == bridge::ScreenWidgetKind::Button)
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

            if (widget.kind == bridge::ScreenWidgetKind::Button)
            {
                const auto widgetId = widget.id;
                surface->OnClick([this, widgetId](const wi::gui::EventArgs&)
                {
                    if (impl_->activateSink) impl_->activateSink(widgetId);
                });
            }

            auto* raw = surface.get();
            renderPath.GetGUI().AddWidget(raw);
            impl_->byId.emplace(widget.id, raw);
            impl_->surfaces.push_back(std::move(surface));
        }

        impl_->loaded = true;
        ApplyLayout(renderPath);
        error.clear();
        return true;
    }

    void ScreenRenderer::ApplyLayout(wi::RenderPath2D& renderPath)
    {
        if (!impl_->loaded) return;
        std::unordered_map<bridge::StableId, ScreenInteractionState> states;
        for (const auto& [id, surface] : impl_->byId)
            states.emplace(id, surface->Interaction());
        std::vector<ScreenRenderItem> items;
        std::string error;
        if (!BuildScreenRenderItems(
                impl_->document, renderPath.GetLogicalWidth(),
                renderPath.GetLogicalHeight(), states, items, error))
            return;

        impl_->logicalRects.clear();
        for (const auto& item : items)
        {
            const auto found = impl_->byId.find(item.widgetId);
            if (found == impl_->byId.end()) continue;
            auto* surface = found->second;
            surface->SetPos(XMFLOAT2(item.logicalRect.x, item.logicalRect.y));
            surface->SetSize(XMFLOAT2(
                item.logicalRect.width, item.logicalRect.height));
            surface->scaleX = item.scaleX;
            surface->scaleY = item.scaleY;
            impl_->logicalRects.emplace(item.widgetId, item.logicalRect);
        }
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
        found->second->authored.visible = visible;
        found->second->authored.enabled = enabled;
        found->second->SetVisible(visible);
        found->second->SetEnabled(
            found->second->authored.kind == bridge::ScreenWidgetKind::Button &&
            enabled);
    }

    void ScreenRenderer::Reset(wi::RenderPath2D& renderPath) noexcept
    {
        for (const auto& surface : impl_->surfaces)
            renderPath.GetGUI().RemoveWidget(surface.get());
        impl_->surfaces.clear();
        impl_->byId.clear();
        impl_->logicalRects.clear();
        impl_->activateSink = {};
        impl_->document = {};
        impl_->loaded = false;
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
