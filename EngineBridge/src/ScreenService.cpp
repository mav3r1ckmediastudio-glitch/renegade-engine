#include "renegade/bridge/ScreenService.h"
#include "ScreenBindingPersistence.h"
#include "ScreenAnimationPersistence.h"
#include "ScreenTransitionPersistence.h"
#include "ScreenControlPersistence.h"
#include "ScreenInteractionPersistence.h"
#include "renegade/bridge/ScreenThemeResolver.h"
#include "ScreenCompositionPersistence.h"
#include "renegade/bridge/DependencyService.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <filesystem>
#include <functional>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <wiConfig.h>

namespace
{
    namespace fs = std::filesystem;

    bool IsValidSymbol(const std::string& value)
    {
        if (value.empty() || value.front() < 'a' || value.front() > 'z')
        {
            return false;
        }

        return std::all_of(
            value.begin(),
            value.end(),
            [](const unsigned char character)
            {
                return (character >= 'a' && character <= 'z') ||
                    (character >= '0' && character <= '9') ||
                    character == '.' || character == '_' || character == '-';
            });
    }

    bool IsSafeRelativePath(const std::string& value)
    {
        if (value.empty())
        {
            return false;
        }

        const fs::path path = fs::u8path(value);
        if (path.is_absolute() || path.has_root_name() ||
            path.has_root_directory())
        {
            return false;
        }

        return std::none_of(
            path.begin(),
            path.end(),
            [](const fs::path& part)
            {
                return part == "..";
            });
    }

    bool IsContentRelativePath(const std::string& value)
    {
        if (!IsSafeRelativePath(value))
        {
            return false;
        }

        const fs::path path = fs::u8path(value);
        const auto first = path.begin();
        return first != path.end() && *first == "Content";
    }

    bool IsFiniteRect(
        const renegade::bridge::ScreenRect& rect,
        const float designWidth,
        const float designHeight)
    {
        return std::isfinite(rect.x) && std::isfinite(rect.y) &&
            std::isfinite(rect.width) && std::isfinite(rect.height) &&
            rect.x >= 0.0f && rect.y >= 0.0f &&
            rect.width > 0.0f && rect.height > 0.0f &&
            rect.x + rect.width <= designWidth &&
            rect.y + rect.height <= designHeight;
    }

    std::string IndexedSection(const char* prefix, const std::size_t index)
    {
        return std::string(prefix) + std::to_string(index);
    }

    bool TryParseWidgetKind(
        const std::string& value,
        renegade::bridge::ScreenWidgetKind& kind)
    {
        using renegade::bridge::ScreenWidgetKind;
        if(value=="toggle"){kind=ScreenWidgetKind::Toggle;return true;}
        if(value=="slider"){kind=ScreenWidgetKind::Slider;return true;}
        if(value=="selector"){kind=ScreenWidgetKind::Selector;return true;}
        if(value=="text_input"){kind=ScreenWidgetKind::TextInput;return true;}
        if (value == "image")
        {
            kind = ScreenWidgetKind::Image;
            return true;
        }
        if (value == "text")
        {
            kind = ScreenWidgetKind::Text;
            return true;
        }
        if (value == "progress")
        {
            kind = ScreenWidgetKind::Progress;
            return true;
        }
        if (value == "button")
        {
            kind = ScreenWidgetKind::Button;
            return true;
        }
        return false;
    }

    const char* HorizontalAlignmentName(
        const renegade::bridge::ScreenHorizontalAlignment value)
    {
        using renegade::bridge::ScreenHorizontalAlignment;
        switch (value)
        {
        case ScreenHorizontalAlignment::Left: return "left";
        case ScreenHorizontalAlignment::Center: return "center";
        case ScreenHorizontalAlignment::Right: return "right";
        default: return "unknown";
        }
    }

    bool TryParseHorizontalAlignment(
        const std::string& value,
        renegade::bridge::ScreenHorizontalAlignment& result)
    {
        using renegade::bridge::ScreenHorizontalAlignment;
        if (value == "left") result = ScreenHorizontalAlignment::Left;
        else if (value == "center") result = ScreenHorizontalAlignment::Center;
        else if (value == "right") result = ScreenHorizontalAlignment::Right;
        else return false;
        return true;
    }

    const char* VerticalAlignmentName(
        const renegade::bridge::ScreenVerticalAlignment value)
    {
        using renegade::bridge::ScreenVerticalAlignment;
        switch (value)
        {
        case ScreenVerticalAlignment::Top: return "top";
        case ScreenVerticalAlignment::Center: return "center";
        case ScreenVerticalAlignment::Bottom: return "bottom";
        default: return "unknown";
        }
    }

    bool TryParseVerticalAlignment(
        const std::string& value,
        renegade::bridge::ScreenVerticalAlignment& result)
    {
        using renegade::bridge::ScreenVerticalAlignment;
        if (value == "top") result = ScreenVerticalAlignment::Top;
        else if (value == "center") result = ScreenVerticalAlignment::Center;
        else if (value == "bottom") result = ScreenVerticalAlignment::Bottom;
        else return false;
        return true;
    }

    const char* CanvasScaleModeName(
        const renegade::bridge::ScreenCanvasScaleMode value)
    {
        using renegade::bridge::ScreenCanvasScaleMode;
        switch (value)
        {
        case ScreenCanvasScaleMode::Fit: return "fit";
        case ScreenCanvasScaleMode::Fill: return "fill";
        case ScreenCanvasScaleMode::Stretch: return "stretch";
        default: return "unknown";
        }
    }

    bool TryParseCanvasScaleMode(
        const std::string& value,
        renegade::bridge::ScreenCanvasScaleMode& result)
    {
        using renegade::bridge::ScreenCanvasScaleMode;
        if (value == "fit") result = ScreenCanvasScaleMode::Fit;
        else if (value == "fill") result = ScreenCanvasScaleMode::Fill;
        else if (value == "stretch") result = ScreenCanvasScaleMode::Stretch;
        else return false;
        return true;
    }

    const char* LayoutModeName(
        const renegade::bridge::ScreenLayoutMode value)
    {
        using renegade::bridge::ScreenLayoutMode;
        switch (value)
        {
        case ScreenLayoutMode::Absolute: return "absolute";
        case ScreenLayoutMode::Anchored: return "anchored";
        default: return "unknown";
        }
    }

    bool TryParseLayoutMode(
        const std::string& value,
        renegade::bridge::ScreenLayoutMode& result)
    {
        using renegade::bridge::ScreenLayoutMode;
        if (value == "absolute") result = ScreenLayoutMode::Absolute;
        else if (value == "anchored") result = ScreenLayoutMode::Anchored;
        else return false;
        return true;
    }

    std::string ColorText(const renegade::bridge::ScreenColor& color)
    {
        std::ostringstream text;
        // Wicked config treats '#' as a comment delimiter, so the canonical
        // serialized form is eight unprefixed hexadecimal RGBA digits.
        text << std::hex << std::uppercase << std::setfill('0')
            << std::setw(2) << static_cast<int>(color.red)
            << std::setw(2) << static_cast<int>(color.green)
            << std::setw(2) << static_cast<int>(color.blue)
            << std::setw(2) << static_cast<int>(color.alpha);
        return text.str();
    }

    bool TryParseColor(
        const std::string& value,
        renegade::bridge::ScreenColor& color)
    {
        const bool prefixed = value.size() == 9 && value.front() == '#';
        if (!prefixed && value.size() != 8) return false;
        unsigned long packed = 0;
        try
        {
            std::size_t consumed = 0;
            packed = std::stoul(value.substr(prefixed ? 1 : 0), &consumed, 16);
            if (consumed != 8) return false;
        }
        catch (const std::exception&)
        {
            return false;
        }
        color.red = static_cast<std::uint8_t>((packed >> 24) & 0xFF);
        color.green = static_cast<std::uint8_t>((packed >> 16) & 0xFF);
        color.blue = static_cast<std::uint8_t>((packed >> 8) & 0xFF);
        color.alpha = static_cast<std::uint8_t>(packed & 0xFF);
        return true;
    }

    void WriteVisualState(
        wi::config::Section& section,
        const char* prefix,
        const renegade::bridge::ScreenVisualState& state)
    {
        const std::string keyPrefix(prefix);
        section.Set((keyPrefix + "_background").c_str(), ColorText(state.background));
        section.Set((keyPrefix + "_foreground").c_str(), ColorText(state.foreground));
        section.Set((keyPrefix + "_image_tint").c_str(), ColorText(state.imageTint));
        section.Set((keyPrefix + "_image").c_str(), state.imageResourcePath);
    }

    bool ReadVisualState(
        const wi::config::Section& section,
        const char* prefix,
        renegade::bridge::ScreenVisualState& state)
    {
        const std::string keyPrefix(prefix);
        const std::string background = keyPrefix + "_background";
        const std::string foreground = keyPrefix + "_foreground";
        const std::string imageTint = keyPrefix + "_image_tint";
        const std::string image = keyPrefix + "_image";
        return section.Has(background.c_str()) &&
            section.Has(foreground.c_str()) && section.Has(imageTint.c_str()) &&
            section.Has(image.c_str()) &&
            TryParseColor(section.GetText(background.c_str()), state.background) &&
            TryParseColor(section.GetText(foreground.c_str()), state.foreground) &&
            TryParseColor(section.GetText(imageTint.c_str()), state.imageTint) &&
            (state.imageResourcePath = section.GetText(image.c_str()), true);
    }

    bool IsFiniteUnit(const float value)
    {
        return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
    }

    bool CanonicalWithinRoot(
        const fs::path& root,
        const fs::path& candidate,
        fs::path& canonicalCandidate,
        std::string& error)
    {
        std::error_code pathError;
        const fs::path canonicalRoot = fs::weakly_canonical(root, pathError);
        if (pathError)
        {
            error = "Could not canonicalize the project root: " +
                pathError.message();
            return false;
        }

        canonicalCandidate = fs::weakly_canonical(candidate, pathError);
        if (pathError)
        {
            error = "Could not canonicalize the Runtime screen path: " +
                pathError.message();
            return false;
        }

        const fs::path relative = fs::relative(
            canonicalCandidate,
            canonicalRoot,
            pathError);
        if (pathError || relative.empty() || relative.is_absolute() ||
            std::any_of(
                relative.begin(),
                relative.end(),
                [](const fs::path& part)
                {
                    return part == "..";
                }))
        {
            error = "A Runtime screen path resolved outside the project root.";
            return false;
        }

        error.clear();
        return true;
    }

    void AddUniquePath(std::vector<fs::path>& paths, const fs::path& candidate)
    {
        if (std::find(paths.begin(), paths.end(), candidate) == paths.end())
        {
            paths.push_back(candidate);
        }
    }
}

namespace renegade::bridge
{
    RuntimeScreenDependencyReader MakeRuntimeScreenDependencyReader(
        std::string expectedProjectId)
    {
        return [projectId = std::move(expectedProjectId)](
            const std::string& path, RuntimeScreenDependencyDocument& document,
            std::string& error)
        {
            ScreenDocument screen;
            if (!ReadScreenDocument(path, projectId, screen, error))
                return false;
            document.projectId = screen.envelope.projectId;
            document.imagePaths.clear();
            document.fontPaths.clear();
            document.screenPaths.clear();
            std::vector<ScreenThemeReference> sources;
            if(screen.theme)sources.push_back(*screen.theme);
            for(const auto& widget:screen.widgets)if(widget.component)
                sources.push_back({widget.component->documentId,widget.component->pathHint});
            if(!sources.empty())
            {
                auto ancestor=fs::u8path(path).parent_path();
                while(!ancestor.empty()&&ancestor.filename()!="Content"&&ancestor!=ancestor.parent_path())
                    ancestor=ancestor.parent_path();
                if(ancestor.filename()!="Content"){error="UI dependencies require a project Content root.";return false;}
                const auto root=ancestor.parent_path();ScreenDocument resolved;
                if(!ResolveScreenTheme(root.generic_u8string(),screen,resolved,error))return false;
                for(const auto& source:sources)
                {
                    std::string sourcePath;
                    if(!ResolveRuntimeScreenDocumentPath(root.generic_u8string(),projectId,source.documentId,
                        source.pathHint,sourcePath,error))return false;
                    const auto relative=fs::relative(fs::u8path(sourcePath),root).generic_u8string();
                    if(std::find(document.screenPaths.begin(),document.screenPaths.end(),relative)==document.screenPaths.end())
                        document.screenPaths.push_back(relative);
                }
            }
            const auto addUnique = [](std::vector<std::string>& paths,
                                      const std::string& path)
            {
                if (std::find(paths.begin(), paths.end(), path) == paths.end())
                    paths.push_back(path);
            };
            for (const auto& widget : screen.widgets)
            {
                if (widget.kind == ScreenWidgetKind::Image &&
                    !widget.resourcePath.empty())
                    addUnique(document.imagePaths, widget.resourcePath);
                const ScreenVisualState* states[] = {
                    &widget.style.normal, &widget.style.hover,
                    &widget.style.pressed, &widget.style.focused,
                    &widget.style.disabled,
                };
                for (const auto* state : states)
                {
                    if (!state->imageResourcePath.empty())
                        addUnique(document.imagePaths, state->imageResourcePath);
                }
                if ((widget.kind == ScreenWidgetKind::Text ||
                     IsScreenInteractive(widget.kind)) &&
                    widget.style.text.fontResource != BuiltinScreenFont)
                {
                    addUnique(document.fontPaths, widget.style.text.fontResource);
                }
            }
            error.clear();
            return true;
        };
    }

    const char* ScreenWidgetKindName(const ScreenWidgetKind kind) noexcept
    {
        switch (kind)
        {
        case ScreenWidgetKind::Image:
            return "image";
        case ScreenWidgetKind::Text:
            return "text";
        case ScreenWidgetKind::Button:
            return "button";
        case ScreenWidgetKind::Toggle:return "toggle";
        case ScreenWidgetKind::Slider:return "slider";
        case ScreenWidgetKind::Selector:return "selector";
        case ScreenWidgetKind::TextInput:return "text_input";
        case ScreenWidgetKind::Progress:
            return "progress";
        default:
            return "unknown";
        }
    }

    ScreenWidgetStyle MakeScreenWidgetStyleTemplate(
        const ScreenWidgetKind kind,
        const float widgetHeight)
    {
        ScreenWidgetStyle style;
        style.normal.background = {0, 0, 0, 0};
        style.hover = style.normal;
        style.pressed = style.normal;
        style.focused = style.normal;
        style.disabled = style.normal;

        if(IsScreenValueControl(kind))return MakeScreenWidgetStyleTemplate(ScreenWidgetKind::Button,widgetHeight);
        if (kind == ScreenWidgetKind::Progress)
        {
            style.normal.background = {18, 27, 42, 235};
            style.normal.foreground = {0, 196, 235, 255};
            style.hover = style.normal;
            style.pressed = style.normal;
            style.focused = style.normal;
            style.disabled = style.normal;
            return style;
        }

        if (kind == ScreenWidgetKind::Image)
        {
            return style;
        }

        style.text.fontResource = BuiltinScreenFont;
        style.text.wrap = kind == ScreenWidgetKind::Text;
        style.text.horizontalAlignment = ScreenHorizontalAlignment::Center;
        style.text.verticalAlignment = ScreenVerticalAlignment::Center;
        if (kind == ScreenWidgetKind::Text)
        {
            style.text.fontSize = std::clamp(widgetHeight * 0.55f, 18.0f, 64.0f);
            style.text.shadowColor = {0, 0, 0, 210};
            style.normal.foreground = {238, 250, 255, 255};
            style.hover.foreground = style.normal.foreground;
            style.pressed.foreground = style.normal.foreground;
            style.focused.foreground = style.normal.foreground;
            style.disabled.foreground = style.normal.foreground;
            return style;
        }

        style.text.fontSize = std::clamp(widgetHeight * 0.42f, 18.0f, 42.0f);
        style.normal.background = {18, 27, 42, 235};
        style.hover.background = {0, 196, 235, 255};
        style.focused.background = {0, 196, 235, 255};
        style.pressed.background = {220, 250, 255, 255};
        style.disabled.background = {18, 27, 42, 120};
        style.normal.foreground = {240, 252, 255, 255};
        style.hover.foreground = style.normal.foreground;
        style.focused.foreground = style.normal.foreground;
        style.pressed.foreground = {18, 27, 42, 255};
        style.disabled.foreground = {150, 160, 170, 200};
        return style;
    }

    ScreenRect ResolveScreenFillRect(const ScreenRect& bounds, const float fraction,
        const ScreenFillDirection direction, const float borderWidth) noexcept
    {
        if (!std::isfinite(bounds.x) || !std::isfinite(bounds.y) ||
            !std::isfinite(bounds.width) || !std::isfinite(bounds.height) ||
            !std::isfinite(borderWidth) || bounds.width <= 0 || bounds.height <= 0)
            return {};
        const float border = std::clamp(borderWidth, 0.0f,
            std::max(0.0f, std::min(bounds.width, bounds.height) * 0.5f));
        ScreenRect result{bounds.x + border, bounds.y + border,
            std::max(0.0f, bounds.width - border * 2),
            std::max(0.0f, bounds.height - border * 2)};
        const float value = std::isfinite(fraction) ? std::clamp(fraction, 0.0f, 1.0f) : 0.0f;
        if (direction == ScreenFillDirection::BottomToTop || direction == ScreenFillDirection::TopToBottom)
        {
            if (direction == ScreenFillDirection::BottomToTop) result.y += result.height * (1 - value);
            result.height *= value;
        }
        else
        {
            if (direction == ScreenFillDirection::RightToLeft) result.x += result.width * (1 - value);
            result.width *= value;
        }
        return result;
    }

    const char* ScreenChildLayoutName(ScreenChildLayout mode) noexcept
    {
        switch(mode)
        {
        case ScreenChildLayout::Free:return "FREE";
        case ScreenChildLayout::Horizontal:return "HORIZONTAL";
        case ScreenChildLayout::Vertical:return "VERTICAL";
        case ScreenChildLayout::Grid:return "GRID";
        }
        return "UNKNOWN";
    }
    bool ValidateScreenContainerLayout(const ScreenContainerLayout& layout,std::string& error)
    {
        if(static_cast<int>(layout.mode)<0||static_cast<int>(layout.mode)>3||
           !std::isfinite(layout.padding)||layout.padding<0||layout.padding>16384||
           !std::isfinite(layout.gap)||layout.gap<0||layout.gap>16384||
           layout.columns<1||layout.columns>64||
           !std::isfinite(layout.compactBelowWidth)||layout.compactBelowWidth<0||
           layout.compactBelowWidth>32768||
           static_cast<int>(layout.compactMode)<0||static_cast<int>(layout.compactMode)>3||
           layout.compactColumns<1||layout.compactColumns>64)
        {error="Container requires a valid mode, nonnegative finite padding/spacing and 1..64 columns.";return false;}
        error.clear();return true;
    }

    bool ValidateScreenSafeArea(const ScreenSafeArea& area,float designWidth,
        float designHeight,std::string& error)
    {
        if(!std::isfinite(designWidth)||!std::isfinite(designHeight)||
           !std::isfinite(area.left)||!std::isfinite(area.top)||
           !std::isfinite(area.right)||!std::isfinite(area.bottom)||
           area.left<0||area.top<0||area.right<0||area.bottom<0||
           area.left+area.right>=designWidth||area.top+area.bottom>=designHeight)
        {error="Safe-area insets must be finite, nonnegative and leave a positive design viewport.";return false;}
        error.clear();return true;
    }

    bool ValidateScreenSizeConstraints(
        const ScreenSizeConstraints& limits,std::string& error)
    {
        const auto valid=[](float value)
            {return std::isfinite(value)&&value>=0.0f&&value<=16384.0f;};
        if(!valid(limits.minimumWidth)||!valid(limits.minimumHeight)||
           !valid(limits.maximumWidth)||!valid(limits.maximumHeight)||
           (limits.maximumWidth>0&&limits.maximumWidth<limits.minimumWidth)||
           (limits.maximumHeight>0&&limits.maximumHeight<limits.minimumHeight))
        {error="Widget size constraints require finite, nonnegative bounds, with each nonzero maximum >= its minimum.";return false;}
        error.clear();return true;
    }

    static bool ResolveScreenWidgetRectsInternal(
        const ScreenDocument& document,
        const StableId* widgetId,
        std::vector<ScreenRect>& rects,
        std::string& error,float layoutWidth)
    {
        if(layoutWidth==0)layoutWidth=document.designWidth;
        if(!std::isfinite(layoutWidth)||layoutWidth<=0)
        {error="Responsive layout width must be finite and positive.";return false;}
        std::unordered_map<StableId, const ScreenWidget*> widgets;
        std::unordered_map<StableId,std::size_t> childCounts,childIndices;
        widgets.reserve(document.widgets.size());
        for (const auto& widget : document.widgets)
        {
            if(!widgets.emplace(widget.id, &widget).second)
            {error="Screen layout contains duplicate widget identities.";return false;}
            childIndices.emplace(widget.id,childCounts[widget.parentId]++);
        }

        std::unordered_set<StableId> resolving;
        std::unordered_map<StableId, ScreenRect> resolved;
        std::function<bool(const StableId&, ScreenRect&)> resolve =
            [&](const StableId& id, ScreenRect& output)
        {
            const auto cached = resolved.find(id);
            if (cached != resolved.end())
            {
                output = cached->second;
                return true;
            }
            const auto found = widgets.find(id);
            if (found == widgets.end())
            {
                error = "The Runtime screen layout references a missing widget: " + id;
                return false;
            }
            if (!resolving.insert(id).second)
            {
                error = "The Runtime screen widget parent graph contains a cycle.";
                return false;
            }

            const auto& widget = *found->second;
            ScreenRect parent{0.0f, 0.0f, document.designWidth, document.designHeight};
            if (!widget.parentId.empty() && !resolve(widget.parentId, parent))
            {
                resolving.erase(id);
                return false;
            }
            // Safe-area margins constrain only root-anchored elements, not
            // fixed absolute art or nested local geometry. The resolved rect
            // is consumed by both shared rendering and pointer hit testing.
            if(widget.parentId.empty() && widget.layoutMode==ScreenLayoutMode::Anchored)
            {
                if(!ValidateScreenSafeArea(document.safeArea,
                    document.designWidth,document.designHeight,error))
                {resolving.erase(id);return false;}
                parent.x=document.safeArea.left;
                parent.y=document.safeArea.top;
                parent.width-=document.safeArea.left+document.safeArea.right;
                parent.height-=document.safeArea.top+document.safeArea.bottom;
            }

            const auto parentWidget=widgets.find(widget.parentId);
            auto layout=parentWidget!=widgets.end()?parentWidget->second->container:ScreenContainerLayout{};
            if(layout.compactBelowWidth>0&&layoutWidth<layout.compactBelowWidth)
            {layout.mode=layout.compactMode;layout.columns=layout.compactColumns;}
            if(parentWidget!=widgets.end()&&layout.mode!=ScreenChildLayout::Free)
            {
                if(!ValidateScreenContainerLayout(layout,error)){resolving.erase(id);return false;}
                const std::size_t count=childCounts.at(widget.parentId);
                const std::size_t index=childIndices.at(id);
                const std::size_t columns=layout.mode==ScreenChildLayout::Horizontal?count:
                    (layout.mode==ScreenChildLayout::Vertical?1:
                     std::min(count,static_cast<std::size_t>(layout.columns)));
                const std::size_t rows=(count+columns-1)/columns;
                const float width=(parent.width-2*layout.padding-layout.gap*float(columns-1))/float(columns);
                const float height=(parent.height-2*layout.padding-layout.gap*float(rows-1))/float(rows);
                if(!std::isfinite(width)||!std::isfinite(height)||width<=0||height<=0)
                {error="Container is too small for its padding, spacing and children.";resolving.erase(id);return false;}
                output={parent.x+layout.padding+float(index%columns)*(width+layout.gap),
                    parent.y+layout.padding+float(index/columns)*(height+layout.gap),width,height};
                // Derive final cell edges from the actual parent edge to avoid
                // rounding a zero-padding fractional layout beyond the canvas.
                if(index%columns==columns-1)output.width=parent.x+parent.width-layout.padding-output.x;
                if(index/columns==rows-1)output.height=parent.y+parent.height-layout.padding-output.y;

            }
            else if (widget.layoutMode == ScreenLayoutMode::Absolute)
            {
                output = {
                    parent.x + widget.rect.x,
                    parent.y + widget.rect.y,
                    widget.rect.width,
                    widget.rect.height,
                };
            }
            else
            {
                const auto& anchors = widget.anchors;
                const float left = parent.x +
                    anchors.minimumX * parent.width + anchors.offsetMinimumX;
                const float top = parent.y +
                    anchors.minimumY * parent.height + anchors.offsetMinimumY;
                const float right = parent.x +
                    anchors.maximumX * parent.width + anchors.offsetMaximumX;
                const float bottom = parent.y +
                    anchors.maximumY * parent.height + anchors.offsetMaximumY;
                output = {left, top, right - left, bottom - top};
            }

            // Final size constraints are shared by edit-time rect resolution,
            // native rendering and pointer hit geometry. Preserve top/left
            // placement and authored fallback dimensions on viewport changes.
            const auto& limits=widget.sizeConstraints;
            if(!ValidateScreenSizeConstraints(limits,error))
            {resolving.erase(id);return false;}
            output.width=std::max(output.width,limits.minimumWidth);
            output.height=std::max(output.height,limits.minimumHeight);
            if(limits.maximumWidth>0)output.width=std::min(output.width,limits.maximumWidth);
            if(limits.maximumHeight>0)output.height=std::min(output.height,limits.maximumHeight);
            if(!std::isfinite(output.width)||!std::isfinite(output.height)||
                output.width<=0||output.height<=0)
            {error="Widget constraints produce an invalid size.";resolving.erase(id);return false;}
            resolving.erase(id);
            resolved.emplace(id, output);
            return true;
        };

        std::vector<ScreenRect> candidate;
        candidate.reserve(widgetId?1:document.widgets.size());
        const auto append=[&](const StableId& id)
        {
            ScreenRect rect;
            if(!resolve(id,rect))return false;
            if(!IsFiniteRect(rect,document.designWidth,document.designHeight))
            {error="Runtime screen widget layout resolves outside the design canvas.";return false;}
            candidate.push_back(rect);return true;
        };
        if(widgetId){if(!append(*widgetId))return false;}
        else for(const auto& widget:document.widgets)if(!append(widget.id))return false;
        rects=std::move(candidate);error.clear();return true;
    }

    bool ResolveScreenWidgetRect(const ScreenDocument& document,
        const StableId& widgetId,ScreenRect& rect,std::string& error)
    {
        std::vector<ScreenRect> candidate;
        if(!ResolveScreenWidgetRectsInternal(document,&widgetId,candidate,error,document.designWidth))return false;
        rect=candidate.front();return true;
    }
    bool ResolveScreenWidgetRects(const ScreenDocument& document,
        std::vector<ScreenRect>& rects,std::string& error,float layoutWidth)
    {return ResolveScreenWidgetRectsInternal(document,nullptr,rects,error,layoutWidth);}

    bool ResolveScreenCanvasTransform(
        const ScreenDocument& document,
        const float outputWidth,
        const float outputHeight,
        ScreenCanvasTransform& transform,
        std::string& error)
    {
        if (!std::isfinite(outputWidth) || !std::isfinite(outputHeight) ||
            outputWidth <= 0.0f || outputHeight <= 0.0f ||
            !std::isfinite(document.designWidth) ||
            !std::isfinite(document.designHeight) ||
            document.designWidth <= 0.0f || document.designHeight <= 0.0f)
        {
            error = "The Runtime screen cannot resolve invalid canvas dimensions.";
            return false;
        }

        transform.scaleX = outputWidth / document.designWidth;
        transform.scaleY = outputHeight / document.designHeight;
        if (document.scaleMode != ScreenCanvasScaleMode::Stretch)
        {
            const float uniform = document.scaleMode ==
                ScreenCanvasScaleMode::Fill ?
                std::max(transform.scaleX, transform.scaleY) :
                std::min(transform.scaleX, transform.scaleY);
            transform.scaleX = uniform;
            transform.scaleY = uniform;
        }
        transform.offsetX =
            (outputWidth - document.designWidth * transform.scaleX) * 0.5f;
        transform.offsetY =
            (outputHeight - document.designHeight * transform.scaleY) * 0.5f;
        error.clear();
        return true;
    }

    bool ValidateScreenDocument(
        const ScreenDocument& document,
        const StableId& expectedProjectId,
        std::string& error)
    {
        if(document.theme&&(document.schemaVersion<3||!IsValidStableId(document.theme->documentId)||
            document.theme->documentId==document.envelope.documentId||!IsContentRelativePath(document.theme->pathHint)))
        {error="UI theme reference must be a different valid project Screen.";return false;}

        if (!ValidateDocumentEnvelope(document.envelope, error))
        {
            return false;
        }
        if (document.envelope.documentType != RuntimeScreenDocumentType)
        {
            error = "The document envelope is not a Runtime screen document.";
            return false;
        }
        if (!expectedProjectId.empty() &&
            document.envelope.projectId != expectedProjectId)
        {
            error = "The Runtime screen document belongs to a different project.";
            return false;
        }
        if ((document.schemaVersion != 2 && document.schemaVersion != ScreenDocument::CurrentSchemaVersion) ||
            document.migratedFromVersion > 2)
        {
            error = "The Runtime screen has an unsupported Screen schema version.";
            return false;
        }
        if (!std::isfinite(document.designWidth) ||
            !std::isfinite(document.designHeight) ||
            document.designWidth < 320.0f || document.designWidth > 16384.0f ||
            document.designHeight < 180.0f || document.designHeight > 16384.0f)
        {
            error = "The Runtime screen has invalid design dimensions.";
            return false;
        }
        if(!ValidateScreenTransition(document.transitions.enter,true,error)||
            !ValidateScreenTransition(document.transitions.exit,false,error))return false;
        if(document.schemaVersion<3&&(!document.transitions.enter.empty()||!document.transitions.exit.empty()))
        {error="Screen transitions require an explicit schema-v3 upgrade.";return false;}
        if(!ValidateScreenSafeArea(document.safeArea,document.designWidth,document.designHeight,error))
            return false;
        if(document.schemaVersion<3 && (document.safeArea.left!=0||document.safeArea.top!=0||
           document.safeArea.right!=0||document.safeArea.bottom!=0))
        {error="Safe-area insets require an explicit Screen v3 upgrade.";return false;}
        if (std::string(CanvasScaleModeName(document.scaleMode)) == "unknown")
        {
            error = "The Runtime screen has an invalid canvas scale mode.";
            return false;
        }
        if (document.schemaVersion < 3 &&
            (document.usage != ScreenUsage::Menu || !document.enabledInGame || document.composition))
        {
            error = "Changing Screen usage or availability requires an explicit schema-v3 upgrade.";
            return false;
        }
        if (document.composition && !ValidateScreenCompositionPolicy(*document.composition,error)) return false;
        const bool needsControl = document.usage == ScreenUsage::Menu ||
            document.usage == ScreenUsage::Modal;
        if (std::string(ScreenUsageName(document.usage)) == "unknown")
        {
            error = "The Screen has an invalid usage.";
            return false;
        }
        if ((needsControl && document.actions.empty()) || document.actions.size() > ScreenDocument::MaximumActions)
        {
            error = "The Runtime screen must declare between 1 and 128 actions.";
            return false;
        }
        if ((needsControl && document.widgets.empty()) || document.widgets.size() > 256)
        {
            error = "The Runtime screen contains an invalid widget count.";
            return false;
        }

        std::unordered_set<std::string> actionIds;
        for (const auto& action : document.actions)
        {
            if (!IsValidSymbol(action.id))
            {
                error = "The Runtime screen contains a malformed action ID.";
                return false;
            }
            if (!actionIds.insert(action.id).second)
            {
                error = "The Runtime screen contains duplicate action ID: " +
                    action.id;
                return false;
            }
        }

        std::unordered_map<StableId, const ScreenWidget*> widgets;
        std::size_t buttonCount = 0;
        std::size_t availableButtonCount = 0;
        std::vector<ScreenRect> layoutRects;
        std::string layoutError;
        const bool sharedLayoutReady=ResolveScreenWidgetRects(document,layoutRects,layoutError);
        std::size_t layoutIndex=0;

        for (const auto& widget : document.widgets)
        {
            if (!IsValidStableId(widget.id))
            {
                error = "The Runtime screen contains a widget with a malformed stable ID.";
                return false;
            }
            if (!widgets.emplace(widget.id, &widget).second)
            {
                error = "The Runtime screen contains duplicate widget ID: " +
                    widget.id;
                return false;
            }
            if (widget.name.empty())
            {
                error = "The Runtime screen contains an unnamed widget: " +
                    widget.id;
                return false;
            }
            if (widget.layoutMode == ScreenLayoutMode::Anchored &&
                (!IsFiniteUnit(widget.anchors.minimumX) ||
                 !IsFiniteUnit(widget.anchors.minimumY) ||
                 !IsFiniteUnit(widget.anchors.maximumX) ||
                 !IsFiniteUnit(widget.anchors.maximumY) ||
                 widget.anchors.minimumX > widget.anchors.maximumX ||
                 widget.anchors.minimumY > widget.anchors.maximumY ||
                 !std::isfinite(widget.anchors.offsetMinimumX) ||
                 !std::isfinite(widget.anchors.offsetMinimumY) ||
                 !std::isfinite(widget.anchors.offsetMaximumX) ||
                 !std::isfinite(widget.anchors.offsetMaximumY)))
            {
                error = "Runtime screen widget '" + widget.name +
                    "' has invalid anchors.";
                return false;
            }
            if(widget.layoutMode==ScreenLayoutMode::Absolute&&
                (!std::isfinite(widget.rect.x)||!std::isfinite(widget.rect.y)||
                 !std::isfinite(widget.rect.width)||!std::isfinite(widget.rect.height)||
                 widget.rect.width<=0||widget.rect.height<=0))
            {error="Screen widget has invalid authored fallback geometry.";return false;}
            if(!ValidateScreenSizeConstraints(widget.sizeConstraints,error))
                return false;
            if(document.schemaVersion<3 &&
                (widget.sizeConstraints.minimumWidth!=0||widget.sizeConstraints.minimumHeight!=0||
                 widget.sizeConstraints.maximumWidth!=0||widget.sizeConstraints.maximumHeight!=0))
            {error="Widget size constraints require an explicit Screen v3 upgrade.";return false;}
            if(widget.component&&(document.schemaVersion<3||
                !IsValidStableId(widget.component->documentId)||
                widget.component->documentId==document.envelope.documentId||
                !IsValidStableId(widget.component->widgetId)||
                !IsContentRelativePath(widget.component->pathHint)||widget.component->overrides>1023||
                (!widget.component->treeRootId.empty()&&
                 !IsValidStableId(widget.component->treeRootId))))
            {error="Invalid linked component source, override mask or schema.";return false;}
            if(widget.transitionOverride)
            {
                if(document.schemaVersion<3||widget.kind!=ScreenWidgetKind::Button||
                    !ValidateScreenTransition(widget.transitionOverride->enter,true,error)||
                    !ValidateScreenTransition(widget.transitionOverride->exit,false,error))
                {if(error.empty())error="Transition overrides require a schema-v3 Button.";return false;}
            }
            if(!ValidateScreenInteractions(document,widget,error))return false;
            if(widget.themeOverrides>63||(document.schemaVersion<3&&widget.themeOverrides!=63))
            {error="Invalid UI theme override mask/schema.";return false;}
            if(!ValidateScreenAnimations(widget.animations,widget.kind==ScreenWidgetKind::Progress,error))return false;
            if(document.schemaVersion<3&&!widget.animations.empty())
            {error="Animations require explicit Screen v3 upgrade.";return false;}
            if(!ValidateScreenContainerLayout(widget.container,error))return false;
            if((widget.container.mode!=ScreenChildLayout::Free||widget.container.compactBelowWidth>0)&&widget.kind!=ScreenWidgetKind::Image)
            {error="Only an Image/container may allocate child layout.";return false;}
            if(document.schemaVersion<3&&(widget.container.mode!=ScreenChildLayout::Free||widget.container.compactBelowWidth>0||
                widget.container.padding!=8||widget.container.gap!=8||widget.container.columns!=2))
            {error="Container configuration requires Screen schema v3.";return false;}
            ScreenRect resolvedRect;
            if(sharedLayoutReady)resolvedRect=layoutRects[layoutIndex];
            ++layoutIndex;
            // Valid frames share one layout pass. Invalid authoring retains
            // the existing named-widget diagnostic and validation ordering.
            if (!sharedLayoutReady&&!ResolveScreenWidgetRect(document, widget.id, resolvedRect, error))
            {
                error = "Runtime screen widget '" + widget.name +
                    "' has invalid layout: " + error;
                return false;
            }

            if(widget.container.mode!=ScreenChildLayout::Free&&
                (resolvedRect.width<=2*widget.container.padding||resolvedRect.height<=2*widget.container.padding))
            {error="Container padding leaves no space for child content.";return false;}
            const auto& style = widget.style;
            if (!std::isfinite(style.opacity) || style.opacity < 0.0f ||
                style.opacity > 1.0f || !std::isfinite(style.borderWidth) ||
                style.borderWidth < 0.0f || !std::isfinite(style.cornerRadius) ||
                style.cornerRadius < 0.0f)
            {
                error = "Runtime screen widget '" + widget.name +
                    "' has invalid authored appearance.";
                return false;
            }
            const ScreenVisualState* states[] = {
                &style.normal, &style.hover, &style.pressed,
                &style.focused, &style.disabled,
            };
            for (const auto* state : states)
            {
                if (!state->imageResourcePath.empty() &&
                    !IsContentRelativePath(state->imageResourcePath))
                {
                    error = "Runtime screen widget '" + widget.name +
                        "' has an unsafe state image resource.";
                    return false;
                }
            }

            if (widget.kind == ScreenWidgetKind::Text || IsScreenInteractive(widget.kind))
            {
                const auto& text = style.text;
                const bool builtinFont = text.fontResource == BuiltinScreenFont;
                const fs::path fontPath = fs::u8path(text.fontResource);
                if ((!builtinFont &&
                     (!IsContentRelativePath(text.fontResource) ||
                      fontPath.extension() != ".ttf")) ||
                    !std::isfinite(text.fontSize) || text.fontSize <= 0.0f ||
                    text.fontSize > 1024.0f ||
                    !std::isfinite(text.characterSpacing) ||
                    !std::isfinite(text.lineSpacing) ||
                    !IsFiniteUnit(text.softness) || !IsFiniteUnit(text.bolden) ||
                    !std::isfinite(text.shadowOffsetX) ||
                    !std::isfinite(text.shadowOffsetY) ||
                    !IsFiniteUnit(text.shadowSoftness) ||
                    !IsFiniteUnit(text.shadowBolden))
                {
                    error = "Runtime screen widget '" + widget.name +
                        "' has invalid authored typography.";
                    return false;
                }
            }

            if (widget.bindings.size() > 4 ||
                (document.schemaVersion < 3 && !widget.bindings.empty()))
            {
                error = "Screen bindings require schema v3 and at most one binding per target property.";
                return false;
            }
            std::unordered_set<int> bindingProperties;
            for (const auto& binding : widget.bindings)
            {
                if (!ValidateScreenBinding(binding, error)) return false;
                if (!bindingProperties.insert(static_cast<int>(binding.property)).second)
                { error = "Screen element has duplicate bindings for one property."; return false; }
                if (binding.property == ScreenBindingProperty::Text &&
                    widget.kind != ScreenWidgetKind::Text && widget.kind != ScreenWidgetKind::Button)
                { error = "Text binding requires a text-capable element."; return false; }
                if (binding.property == ScreenBindingProperty::Fill &&
                    widget.kind != ScreenWidgetKind::Progress)
                { error = "Fill binding requires a Progress element."; return false; }
            }

            if(static_cast<int>(widget.imageFit)<0||static_cast<int>(widget.imageFit)>2)
            {error="Screen image fitting is invalid.";return false;}
            if(widget.localOnly&&(document.schemaVersion<3||widget.kind!=ScreenWidgetKind::Button))
            {error="Local-only Flow suppression is only available for v3 Buttons.";return false;}
            switch (widget.kind)
            {
            case ScreenWidgetKind::Toggle:
            case ScreenWidgetKind::Slider:
            case ScreenWidgetKind::Selector:
            case ScreenWidgetKind::TextInput:
                if(document.schemaVersion<3||!widget.actionId.empty()||!widget.resourcePath.empty()||
                   !ValidateScreenControl(ScreenControlKindFor(widget.kind),widget.control,error))
                {if(error.empty())error="UI value controls require v3 and no implicit action/resource.";return false;}
                ++buttonCount;if(widget.visible&&widget.enabled)++availableButtonCount;
                break;
            case ScreenWidgetKind::Progress:
                if (document.schemaVersion < 3 || !std::isfinite(widget.fill) ||
                    widget.fill < 0.0f || widget.fill > 1.0f ||
                    static_cast<int>(widget.fillDirection) < 0 ||
                    static_cast<int>(widget.fillDirection) > 3 ||
                    !widget.resourcePath.empty() || !widget.text.empty() || !widget.actionId.empty() ||
                    (!widget.progressFrameResourcePath.empty() && !IsContentRelativePath(widget.progressFrameResourcePath)) ||
                    (!widget.progressFillResourcePath.empty() && !IsContentRelativePath(widget.progressFillResourcePath)))
                { error = "Progress element has invalid schema, fill or data fields."; return false; }
                break;

            case ScreenWidgetKind::Image:
                if ((!widget.resourcePath.empty() &&
                     !IsContentRelativePath(widget.resourcePath)) ||
                    !widget.text.empty() || !widget.actionId.empty())
                {
                    error = "Runtime screen image '" + widget.name +
                        "' has invalid image fields.";
                    return false;
                }
                break;

            case ScreenWidgetKind::Text:
                if (widget.text.empty() || !widget.resourcePath.empty() ||
                    !widget.actionId.empty())
                {
                    error = "Runtime screen text '" + widget.name +
                        "' has invalid text fields.";
                    return false;
                }
                break;

            case ScreenWidgetKind::Button:
                ++buttonCount;
                if (widget.text.empty() || !widget.resourcePath.empty() ||
                    actionIds.count(widget.actionId) != 1)
                {
                    error = "Runtime screen button '" + widget.name +
                        "' has an invalid action reference.";
                    return false;
                }
                if (widget.visible && widget.enabled)
                {
                    ++availableButtonCount;
                }
                break;
            default:
                error = "Screen element kind is unsupported.";
                return false;
            }
        }

        if (needsControl && buttonCount == 0)
        {
            error = "The Runtime screen must contain at least one action button.";
            return false;
        }
        if (needsControl && availableButtonCount == 0)
        {
            error = "The Runtime screen has no visible enabled control.";
            return false;
        }
        if (document.focusOrder.size() != buttonCount)
        {
            error = "The Runtime screen focus order must contain every button exactly once.";
            return false;
        }

        std::unordered_set<StableId> focusIds;
        for (const auto& widgetId : document.focusOrder)
        {
            if (!IsValidStableId(widgetId) ||
                !focusIds.insert(widgetId).second)
            {
                error = "The Runtime screen contains a malformed or duplicate focus target.";
                return false;
            }
            const auto found = widgets.find(widgetId);
            if (found == widgets.end() ||
                !IsScreenInteractive(found->second->kind))
            {
                error = "The Runtime screen focus order references a missing or non-button widget.";
                return false;
            }
        }
        for (const auto& [widgetId, widget] : widgets)
        {
            if (IsScreenInteractive(widget->kind) &&
                focusIds.count(widgetId) != 1)
            {
                error = "The Runtime screen focus order omits button: " +
                    widget->name;
                return false;
            }
        }

        // Linked-tree membership must not silently alias a source child or
        // refer to a vanished/foreign instance root. Legacy single-widget
        // links have an empty treeRootId and are unaffected.
        std::unordered_map<StableId,std::unordered_set<StableId>> linkedSourceIds;
        for(const auto& widget:document.widgets)
        {
            if(!widget.component||widget.component->treeRootId.empty())continue;
            const auto& link=*widget.component;
            const auto root=widgets.find(link.treeRootId);
            if(root==widgets.end()||!root->second->component||
                root->second->component->treeRootId!=link.treeRootId||
                root->second->component->documentId!=link.documentId||
                root->second->component->pathHint!=link.pathHint||
                !linkedSourceIds[link.treeRootId].insert(link.widgetId).second)
            {error="Linked UI tree has a missing root, mixed source or duplicate member.";return false;}
            if(widget.id!=link.treeRootId)
            {
                StableId parent=widget.parentId;
                bool within=false;
                for(unsigned depth=0;depth<256&&!parent.empty();++depth)
                {
                    if(parent==link.treeRootId){within=true;break;}
                    const auto found=widgets.find(parent);
                    if(found==widgets.end())break;
                    parent=found->second->parentId;
                }
                if(!within)
                {error="Linked UI tree member is outside its instance root.";return false;}
            }
        }
        std::unordered_set<float> widths;
        for(const auto& widget:document.widgets)if(widget.container.compactBelowWidth>0)
        {
            widths.insert(std::nextafter(widget.container.compactBelowWidth,0.0f));
            widths.insert(widget.container.compactBelowWidth);
        }
        std::vector<ScreenRect> intervalRects;
        for(float width:widths)if(!ResolveScreenWidgetRects(document,intervalRects,error,width))
        {error="Responsive layout is invalid: "+error;return false;}
        error.clear();
        return true;
    }

    bool PreserveScreenUpgradeOriginal(
        const std::string& filePath, const ScreenDocument& candidate, std::string& error)
    {
        std::error_code ec;
        const auto path = fs::u8path(filePath);
        if (!fs::exists(path, ec)) { error.clear(); return !ec; }
        wi::config::File original;
        if (!original.Open(filePath) || !original.HasSection("screen"))
        {
            error = "Cannot inspect existing Screen before save.";
            return false;
        }
        const auto& settings = original.GetSection("screen");
        const auto version = settings.Has("schema_version") ?
            settings.GetUint("schema_version") : 1;
        if (version > ScreenDocument::CurrentSchemaVersion)
        {
            error = "Refusing to overwrite a Screen written by a newer schema.";
            return false;
        }
        if (version >= candidate.schemaVersion) { error.clear(); return true; }
        ScreenDocument previous;
        if (!ReadScreenDocument(filePath, candidate.envelope.projectId, previous, error) ||
            previous.envelope.documentId != candidate.envelope.documentId)
        {
            error = "Existing Screen identity cannot be verified before upgrade: " + error;
            return false;
        }
        std::ifstream in(path, std::ios::binary);
        std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(in), {}};
        if (!in || bytes.empty()) { error = "Cannot read original Screen bytes."; return false; }
        const std::string backupStem = filePath + ".pre-ui-schema" + std::to_string(version);
        auto backup = fs::u8path(backupStem);
        const auto matches = [&bytes](const fs::path& location)
        {
            std::ifstream saved(location, std::ios::binary);
            const std::vector<std::uint8_t> old{std::istreambuf_iterator<char>(saved), {}};
            return saved && old == bytes;
        };
        if (fs::exists(backup, ec))
        {
            if (matches(backup)) { error.clear(); return true; }
            // Undo and a new branch may upgrade a different legacy revision.
            // Preserve both originals; never replace the first archive.
            std::uint64_t hash = 14695981039346656037ull;
            for (const auto byte : bytes) { hash ^= byte; hash *= 1099511628211ull; }
            std::ostringstream suffix;
            suffix << std::hex << std::setw(16) << std::setfill('0') << hash;
            backup = fs::u8path(backupStem + "-" + suffix.str());
            if (fs::exists(backup, ec))
            {
                if (!matches(backup))
                { error = "Screen original archive identity collision."; return false; }
                error.clear();
                return true;
            }
        }
        ProjectDocumentWrite write;
        write.destinationPath = backup.generic_u8string();
        write.content = bytes;
        write.validator = [bytes](const std::string& staged, std::string& failure)
        {
            std::ifstream check(fs::u8path(staged), std::ios::binary);
            const std::vector<std::uint8_t> actual{std::istreambuf_iterator<char>(check), {}};
            if (!check || actual != bytes) { failure = "Migration original byte mismatch."; return false; }
            return true;
        };
        ProjectDocumentTransactionOptions options;
        // Match the existing document writer's LF02 journal convention so
        // ordinary project-open recovery also sees interrupted archive writes.
        const auto destination = fs::absolute(path, ec).lexically_normal();
        auto transactionRoot = destination;
        const auto hint = fs::u8path(candidate.envelope.pathHint).lexically_normal();
        for (const auto& part : hint) { (void)part; transactionRoot = transactionRoot.parent_path(); }
        const bool projectLocation = IsSafeRelativePath(candidate.envelope.pathHint) &&
            !transactionRoot.empty() &&
            (transactionRoot / hint).lexically_normal() == destination;
        if (!projectLocation) transactionRoot = destination.parent_path();
        options.allowedRoot = transactionRoot.generic_u8string();
        options.journalDirectory = projectLocation
            ? (transactionRoot / "Intermediate" / "Transactions").generic_u8string()
            : (transactionRoot / ".renegade-transactions").generic_u8string();
        const auto result = ProjectDocumentTransaction().Execute({std::move(write)}, std::move(options));
        if (!result.success) { error = "Cannot preserve Screen migration original: " + result.message; return false; }
        error.clear();
        return true;
    }

    bool WriteScreenDocument(
        const std::string& filePath,
        const ScreenDocument& document,
        std::string& error)
    {
        if (!ValidateScreenDocument(
                document,
                document.envelope.projectId,
                error))
        {
            return false;
        }

        if (!PreserveScreenUpgradeOriginal(filePath, document, error))
            return false;
        const StableId expectedDocumentId = document.envelope.documentId;
        const StableId expectedProjectId = document.envelope.projectId;
        const std::size_t expectedActionCount = document.actions.size();
        const std::size_t expectedWidgetCount = document.widgets.size();
        const std::size_t expectedFocusCount = document.focusOrder.size();

        return WriteTransactionalDocument(
            filePath,
            document.envelope,
            false,
            [&document](wi::config::File& file)
            {
                if(document.schemaVersion>=3)detail::WriteScreenTransitionPair(file,"screen",document.transitions);
                auto& screen = file.GetSection("screen");
                screen.Set("schema_version", document.schemaVersion);
                if (document.schemaVersion >= 3)
                {
                    screen.Set("usage", ScreenUsageName(document.usage));
                    screen.Set("enabled_in_game", document.enabledInGame);
                    detail::WriteScreenComposition(screen,document.composition);
                }
                screen.Set("migrated_from", document.migratedFromVersion);
                screen.Set("design_width", document.designWidth);
                screen.Set("design_height", document.designHeight);
                screen.Set("scale_mode", CanvasScaleModeName(document.scaleMode));
                if(document.schemaVersion>=3)
                {
                    screen.Set("safe_left",document.safeArea.left);
                    screen.Set("safe_top",document.safeArea.top);
                    screen.Set("safe_right",document.safeArea.right);
                    screen.Set("safe_bottom",document.safeArea.bottom);
                }
                screen.Set(
                    "action_count",
                    static_cast<int>(document.actions.size()));
                screen.Set(
                    "widget_count",
                    static_cast<int>(document.widgets.size()));
                screen.Set(
                    "focus_count",
                    static_cast<int>(document.focusOrder.size()));

                for (std::size_t index = 0;
                    index < document.actions.size(); ++index)
                {
                    auto& section = file.GetSection(
                        IndexedSection("action_", index).c_str());
                    section.Set("id", document.actions[index].id);
                }

                for (std::size_t index = 0;
                    index < document.widgets.size(); ++index)
                {
                    const auto& widget = document.widgets[index];
                    auto& section = file.GetSection(
                        IndexedSection("widget_", index).c_str());
                    section.Set("id", widget.id);
                    section.Set(
                        "kind",
                        ScreenWidgetKindName(widget.kind));
                    section.Set("name", widget.name);
                    section.Set("x", widget.rect.x);
                    section.Set("y", widget.rect.y);
                    section.Set("width", widget.rect.width);
                    section.Set("height", widget.rect.height);
                    if(document.schemaVersion>=3)
                    {
                        section.Set("minimum_width",widget.sizeConstraints.minimumWidth);
                        section.Set("minimum_height",widget.sizeConstraints.minimumHeight);
                        section.Set("maximum_width",widget.sizeConstraints.maximumWidth);
                        section.Set("maximum_height",widget.sizeConstraints.maximumHeight);
                    }
                    section.Set("visible", widget.visible);
                    section.Set("enabled", widget.enabled);
                    section.Set("text", widget.text);
                    section.Set("resource", widget.resourcePath);
                    section.Set("action", widget.actionId);
                    if(document.schemaVersion>=3)
                    {
                        section.Set("local_only",widget.localOnly);
                        section.Set("image_fit",static_cast<int>(widget.imageFit));
                        if(widget.transitionOverride)section.Set("transition_override",true);
                    }
                    if(IsScreenValueControl(widget.kind))detail::WriteScreenControl(section,widget);
                    if (widget.kind == ScreenWidgetKind::Progress)
                    {
                        section.Set("fill", widget.fill);
                        section.Set("fill_direction", static_cast<int>(widget.fillDirection));
                        if (!widget.progressFrameResourcePath.empty())
                            section.Set("progress_frame_resource", widget.progressFrameResourcePath);
                        if (!widget.progressFillResourcePath.empty())
                            section.Set("progress_fill_resource", widget.progressFillResourcePath);
                    }

                    section.Set("parent", widget.parentId);
                    section.Set("layout_mode", LayoutModeName(widget.layoutMode));
                    section.Set("anchor_min_x", widget.anchors.minimumX);
                    section.Set("anchor_min_y", widget.anchors.minimumY);
                    section.Set("anchor_max_x", widget.anchors.maximumX);
                    section.Set("anchor_max_y", widget.anchors.maximumY);
                    section.Set("offset_min_x", widget.anchors.offsetMinimumX);
                    section.Set("offset_min_y", widget.anchors.offsetMinimumY);
                    section.Set("offset_max_x", widget.anchors.offsetMaximumX);
                    section.Set("offset_max_y", widget.anchors.offsetMaximumY);
                    if(document.schemaVersion>=3)
                    {
                        section.Set("theme_overrides",static_cast<int>(widget.themeOverrides));
                        if(widget.component)
                        {
                            section.Set("component_document",widget.component->documentId);
                            section.Set("component_path",widget.component->pathHint);
                            section.Set("component_widget",widget.component->widgetId);
                            section.Set("component_overrides",static_cast<int>(widget.component->overrides));
                            if(!widget.component->treeRootId.empty())
                                section.Set("component_tree_root",widget.component->treeRootId);
                        }
                        section.Set("child_layout",static_cast<int>(widget.container.mode));
                        section.Set("child_padding",widget.container.padding);
                        section.Set("child_gap",widget.container.gap);
                        section.Set("child_columns",widget.container.columns);
                        section.Set("compact_below_width",widget.container.compactBelowWidth);
                        section.Set("compact_child_layout",static_cast<int>(widget.container.compactMode));
                        section.Set("compact_child_columns",widget.container.compactColumns);
                    }


                    const auto& style = widget.style;
                    WriteVisualState(section, "normal", style.normal);
                    WriteVisualState(section, "hover", style.hover);
                    WriteVisualState(section, "pressed", style.pressed);
                    WriteVisualState(section, "focused", style.focused);
                    WriteVisualState(section, "disabled", style.disabled);
                    section.Set("border_color", ColorText(style.borderColor));
                    section.Set("border_width", style.borderWidth);
                    section.Set("corner_radius", style.cornerRadius);
                    section.Set("opacity", style.opacity);

                    const auto& text = style.text;
                    section.Set("font_resource", text.fontResource);
                    section.Set("font_size", text.fontSize);
                    section.Set("character_spacing", text.characterSpacing);
                    section.Set("line_spacing", text.lineSpacing);
                    section.Set("font_softness", text.softness);
                    section.Set("font_bolden", text.bolden);
                    section.Set("shadow_color", ColorText(text.shadowColor));
                    section.Set("shadow_offset_x", text.shadowOffsetX);
                    section.Set("shadow_offset_y", text.shadowOffsetY);
                    section.Set("shadow_softness", text.shadowSoftness);
                    section.Set("shadow_bolden", text.shadowBolden);
                    section.Set("horizontal_alignment",
                        HorizontalAlignmentName(text.horizontalAlignment));
                    section.Set("vertical_alignment",
                        VerticalAlignmentName(text.verticalAlignment));
                    section.Set("wrap", text.wrap);
                    // Adding sections can invalidate File-owned references.
                    // Finish all widget fields before inserting child sections.
                    if (document.schemaVersion >= 3)
                    {
                        section.Set("binding_count", static_cast<int>(widget.bindings.size()));
                        for (std::size_t bindingIndex = 0; bindingIndex < widget.bindings.size(); ++bindingIndex)
                        {
                            const auto bindingSection = IndexedSection("widget_", index) + "_" +
                                IndexedSection("binding_", bindingIndex);
                            detail::WriteScreenBinding(file.GetSection(bindingSection.c_str()),
                                widget.bindings[bindingIndex]);
                        }
                    }
                    if(widget.transitionOverride)
                        detail::WriteScreenTransitionPair(file,IndexedSection("widget_",index),*widget.transitionOverride);
                    if(document.schemaVersion>=3)detail::WriteWidgetInteractions(file,IndexedSection("widget_",index),widget.interactions);
                    if(document.schemaVersion>=3)
                        detail::WriteWidgetAnimations(file,IndexedSection("widget_",index),widget.animations);
                }

                if(document.theme)
                {
                    auto& theme=file.GetSection("ui_theme");
                    theme.Set("document_id",document.theme->documentId);theme.Set("path_hint",document.theme->pathHint);
                }
                for (std::size_t index = 0;
                    index < document.focusOrder.size(); ++index)
                {
                    auto& section = file.GetSection(
                        IndexedSection("focus_", index).c_str());
                    section.Set(
                        "widget_id",
                        document.focusOrder[index]);
                }
            },
            [
                expectedDocumentId,
                expectedProjectId,
                expectedActionCount,
                expectedWidgetCount,
                expectedFocusCount](
                    const std::string& path,
                    std::string& validationError)
            {
                ScreenDocument roundTrip;
                if (!ReadScreenDocument(
                        path,
                        expectedProjectId,
                        roundTrip,
                        validationError))
                {
                    return false;
                }
                if (roundTrip.envelope.documentId != expectedDocumentId ||
                    roundTrip.actions.size() != expectedActionCount ||
                    roundTrip.widgets.size() != expectedWidgetCount ||
                    roundTrip.focusOrder.size() != expectedFocusCount)
                {
                    validationError =
                        "The Runtime screen document did not round-trip exactly.";
                    return false;
                }
                validationError.clear();
                return true;
            },
            error);
    }

    bool ReadScreenDocument(
        const std::string& filePath,
        const StableId& expectedProjectId,
        ScreenDocument& document,
        std::string& error)
    {
        DocumentEnvelope envelope;
        if (!ReadDocumentEnvelope(filePath, envelope, error))
        {
            return false;
        }

        try
        {
            wi::config::File file;
            if (!file.Open(filePath))
            {
                error = "Could not read Runtime screen document: " + filePath;
                return false;
            }
            if (!file.HasSection("screen"))
            {
                error = "The Runtime screen document is missing its screen section.";
                return false;
            }

            ScreenDocument parsed;
            parsed.envelope = std::move(envelope);
            const auto& screen = file.GetSection("screen");
            const std::uint32_t sourceSchema = screen.Has("schema_version") ?
                screen.GetUint("schema_version") : 1;
            if (sourceSchema != 1 && sourceSchema != 2 &&
                sourceSchema != ScreenDocument::CurrentSchemaVersion)
            {
                error = "The Runtime screen document uses unsupported Screen schema " +
                    std::to_string(sourceSchema) + '.';
                return false;
            }
            if(!detail::ReadScreenTransitionPair(file,"screen",sourceSchema,parsed.transitions,error))return false;
            parsed.schemaVersion = sourceSchema == 1 ? 2 : sourceSchema;
            parsed.migratedFromVersion = sourceSchema == 1 ? 1 :
                (screen.Has("migrated_from") ? screen.GetUint("migrated_from") : 0);
            if (sourceSchema >= 3)
            {
                if (!screen.Has("usage") || !screen.Has("enabled_in_game"))
                {
                    error = "Screen schema v3 is missing its explicit usage/enabled policy.";
                    return false;
                }
                const auto usage = screen.GetText("usage");
                bool matched = false;
                for (auto candidate : {ScreenUsage::Menu, ScreenUsage::Hud,
                        ScreenUsage::Overlay, ScreenUsage::Modal})
                    if (usage == ScreenUsageName(candidate))
                    { parsed.usage = candidate; matched = true; break; }
                if (!matched)
                {
                    error = "The Screen has an unknown usage.";
                    return false;
                }
                parsed.enabledInGame = screen.GetBool("enabled_in_game");
                if (!detail::ReadScreenComposition(screen,parsed.composition,error,ScreenLayerForUsage(parsed.usage))) return false;
            }
            parsed.designWidth = screen.GetFloat("design_width");
            parsed.designHeight = screen.GetFloat("design_height");
            if (sourceSchema == 1)
            {
                parsed.scaleMode = ScreenCanvasScaleMode::Fit;
            }
            else if (!screen.Has("scale_mode") ||
                !TryParseCanvasScaleMode(
                    screen.GetText("scale_mode"), parsed.scaleMode))
            {
                error = "The Runtime screen document has an invalid canvas scale mode.";
                return false;
            }
            const bool hasSafeArea=screen.Has("safe_left")||screen.Has("safe_top")||
                screen.Has("safe_right")||screen.Has("safe_bottom");
            if(hasSafeArea)
            {
                if(sourceSchema<3||!screen.Has("safe_left")||!screen.Has("safe_top")||
                    !screen.Has("safe_right")||!screen.Has("safe_bottom"))
                {error="Screen safe-area insets require all four v3 fields.";return false;}
                const auto readInset=[&](const char* key,float& result)
                {
                    std::istringstream stream(screen.GetText(key));
                    stream.imbue(std::locale::classic());
                    stream>>std::noskipws>>result;
                    return !stream.fail()&&stream.peek()==std::char_traits<char>::eof();
                };
                if(!readInset("safe_left",parsed.safeArea.left)||
                   !readInset("safe_top",parsed.safeArea.top)||
                   !readInset("safe_right",parsed.safeArea.right)||
                   !readInset("safe_bottom",parsed.safeArea.bottom)||
                   !ValidateScreenSafeArea(parsed.safeArea,parsed.designWidth,parsed.designHeight,error))
                {if(error.empty())error="Screen safe-area values are malformed.";return false;}
            }
            const int actionCount = screen.GetInt("action_count");
            const int widgetCount = screen.GetInt("widget_count");
            const int focusCount = screen.GetInt("focus_count");
            const bool needsControl = parsed.usage == ScreenUsage::Menu ||
                parsed.usage == ScreenUsage::Modal;
            if ((needsControl && actionCount == 0) || actionCount < 0 || actionCount > static_cast<int>(ScreenDocument::MaximumActions) ||
                (needsControl && widgetCount == 0) || widgetCount < 0 || widgetCount > 256 ||
                focusCount < 0 || focusCount > 256)
            {
                error = "The Runtime screen document contains invalid section counts.";
                return false;
            }

            parsed.actions.reserve(static_cast<std::size_t>(actionCount));
            for (int index = 0; index < actionCount; ++index)
            {
                const std::string sectionName = IndexedSection(
                    "action_",
                    static_cast<std::size_t>(index));
                if (!file.HasSection(sectionName.c_str()))
                {
                    error = "The Runtime screen document is missing section " +
                        sectionName + '.';
                    return false;
                }
                parsed.actions.push_back({
                    file.GetSection(sectionName.c_str()).GetText("id")
                });
            }

            parsed.widgets.reserve(static_cast<std::size_t>(widgetCount));
            for (int index = 0; index < widgetCount; ++index)
            {
                const std::string sectionName = IndexedSection(
                    "widget_",
                    static_cast<std::size_t>(index));
                if (!file.HasSection(sectionName.c_str()))
                {
                    error = "The Runtime screen document is missing section " +
                        sectionName + '.';
                    return false;
                }

                const auto& section = file.GetSection(sectionName.c_str());
                ScreenWidget widget;
                widget.id = section.GetText("id");
                if (!TryParseWidgetKind(section.GetText("kind"), widget.kind))
                {
                    error = "The Runtime screen contains an unsupported widget kind.";
                    return false;
                }
                widget.name = section.GetText("name");
                widget.rect.x = section.GetFloat("x");
                widget.rect.y = section.GetFloat("y");
                widget.rect.width = section.GetFloat("width");
                widget.rect.height = section.GetFloat("height");
                const char* sizeKeys[]={"minimum_width","minimum_height",
                    "maximum_width","maximum_height"};
                const bool anySize=std::any_of(std::begin(sizeKeys),std::end(sizeKeys),
                    [&](const char* key){return section.Has(key);});
                if(anySize)
                {
                    const auto scalar=[&](const char* key,float& target)
                    {
                        if(!section.Has(key))return false;
                        std::istringstream value(section.GetText(key));
                        value.imbue(std::locale::classic());
                        value>>std::noskipws>>target;
                        return !value.fail()&&
                            value.peek()==std::char_traits<char>::eof();
                    };
                    if(sourceSchema<3||
                       !scalar("minimum_width",widget.sizeConstraints.minimumWidth)||
                       !scalar("minimum_height",widget.sizeConstraints.minimumHeight)||
                       !scalar("maximum_width",widget.sizeConstraints.maximumWidth)||
                       !scalar("maximum_height",widget.sizeConstraints.maximumHeight)||
                       !ValidateScreenSizeConstraints(widget.sizeConstraints,error))
                    {error="Widget size constraint fields are incomplete or malformed.";return false;}
                }
                const auto transitionPrefix=IndexedSection("widget_",index);
                if(!section.Has("transition_override")&&
                    (file.HasSection((transitionPrefix+"_enter").c_str())||file.HasSection((transitionPrefix+"_exit").c_str())))
                {error="Widget transition tracks lack an explicit Button override.";return false;}
                if(section.Has("transition_override"))
                {
                    const auto flag=section.GetText("transition_override");
                    if(sourceSchema<3||(flag!="true"&&flag!="false"))
                    {error="Malformed Button transition override flag.";return false;}
                    if(flag=="false"&&
                        (file.HasSection((transitionPrefix+"_enter").c_str())||file.HasSection((transitionPrefix+"_exit").c_str())))
                    {error="Disabled Button transition override still contains tracks.";return false;}
                    if(flag=="true")
                    {
                        ScreenTransitionPair pair;
                        if(!detail::ReadScreenTransitionPair(file,IndexedSection("widget_",index),sourceSchema,pair,error))return false;
                        widget.transitionOverride=std::move(pair);
                    }
                }
                widget.visible = section.GetBool("visible");
                widget.enabled = section.GetBool("enabled");
                widget.text = section.GetText("text");
                widget.resourcePath = section.GetText("resource");
                widget.actionId = section.GetText("action");
                if(sourceSchema>=3&&section.Has("image_fit")) {
                    const int fit=section.GetInt("image_fit");
                    if(fit<0||fit>2){error="Screen image fitting is invalid.";return false;}
                    widget.imageFit=static_cast<ScreenImageFit>(fit);
                }
                if(sourceSchema>=3&&section.Has("local_only"))
                    widget.localOnly=section.GetBool("local_only");
                if(IsScreenValueControl(widget.kind)&&!detail::ReadScreenControl(section,widget,error))return false;
                if (widget.kind == ScreenWidgetKind::Progress)
                {
                    if (sourceSchema < 3 || !section.Has("fill") || !section.Has("fill_direction"))
                    { error = "Progress element is missing its authored fill fields."; return false; }
                    const auto fillText = section.GetText("fill");
                    std::istringstream fillStream(fillText);
                    fillStream.imbue(std::locale::classic());
                    fillStream >> std::noskipws >> widget.fill;
                    int direction = -1;
                    std::istringstream directionStream(section.GetText("fill_direction"));
                    directionStream >> std::noskipws >> direction;
                    if (fillStream.fail() || fillStream.peek() != std::char_traits<char>::eof() ||
                        directionStream.fail() || directionStream.peek() != std::char_traits<char>::eof() ||
                        direction < 0 || direction > 3)
                    { error = "Progress element fill fields are malformed."; return false; }
                    widget.fillDirection = static_cast<ScreenFillDirection>(direction);
                    if (section.Has("progress_frame_resource"))
                        widget.progressFrameResourcePath = section.GetText("progress_frame_resource");
                    if (section.Has("progress_fill_resource"))
                        widget.progressFillResourcePath = section.GetText("progress_fill_resource");
                }
                if (sourceSchema == 1)
                {
                    widget.style = MakeScreenWidgetStyleTemplate(
                        widget.kind, widget.rect.height);
                }
                else
                {
                    const char* requiredKeys[] = {
                        "parent", "layout_mode", "anchor_min_x", "anchor_min_y",
                        "anchor_max_x", "anchor_max_y", "offset_min_x",
                        "offset_min_y", "offset_max_x", "offset_max_y",
                        "border_color", "border_width", "corner_radius", "opacity",
                        "font_resource", "font_size", "character_spacing",
                        "line_spacing", "font_softness", "font_bolden",
                        "shadow_color", "shadow_offset_x", "shadow_offset_y",
                        "shadow_softness", "shadow_bolden",
                        "horizontal_alignment", "vertical_alignment", "wrap",
                    };
                    if (std::any_of(std::begin(requiredKeys), std::end(requiredKeys),
                        [&section](const char* key) { return !section.Has(key); }) ||
                        !TryParseLayoutMode(
                            section.GetText("layout_mode"), widget.layoutMode) ||
                        !ReadVisualState(section, "normal", widget.style.normal) ||
                        !ReadVisualState(section, "hover", widget.style.hover) ||
                        !ReadVisualState(section, "pressed", widget.style.pressed) ||
                        !ReadVisualState(section, "focused", widget.style.focused) ||
                        !ReadVisualState(section, "disabled", widget.style.disabled) ||
                        !TryParseColor(
                            section.GetText("border_color"), widget.style.borderColor) ||
                        !TryParseColor(
                            section.GetText("shadow_color"),
                            widget.style.text.shadowColor) ||
                        !TryParseHorizontalAlignment(
                            section.GetText("horizontal_alignment"),
                            widget.style.text.horizontalAlignment) ||
                        !TryParseVerticalAlignment(
                            section.GetText("vertical_alignment"),
                            widget.style.text.verticalAlignment))
                    {
                        error = "The Runtime screen contains incomplete or malformed authored style data.";
                        return false;
                    }
                    widget.parentId = section.GetText("parent");
                    widget.anchors.minimumX = section.GetFloat("anchor_min_x");
                    widget.anchors.minimumY = section.GetFloat("anchor_min_y");
                    widget.anchors.maximumX = section.GetFloat("anchor_max_x");
                    widget.anchors.maximumY = section.GetFloat("anchor_max_y");
                    widget.anchors.offsetMinimumX = section.GetFloat("offset_min_x");
                    widget.anchors.offsetMinimumY = section.GetFloat("offset_min_y");
                    widget.anchors.offsetMaximumX = section.GetFloat("offset_max_x");
                    widget.anchors.offsetMaximumY = section.GetFloat("offset_max_y");
                    widget.style.borderWidth = section.GetFloat("border_width");
                    widget.style.cornerRadius = section.GetFloat("corner_radius");
                    widget.style.opacity = section.GetFloat("opacity");
                    auto& textStyle = widget.style.text;
                    textStyle.fontResource = section.GetText("font_resource");
                    textStyle.fontSize = section.GetFloat("font_size");
                    textStyle.characterSpacing = section.GetFloat("character_spacing");
                    textStyle.lineSpacing = section.GetFloat("line_spacing");
                    textStyle.softness = section.GetFloat("font_softness");
                    textStyle.bolden = section.GetFloat("font_bolden");
                    textStyle.shadowOffsetX = section.GetFloat("shadow_offset_x");
                    textStyle.shadowOffsetY = section.GetFloat("shadow_offset_y");
                    textStyle.shadowSoftness = section.GetFloat("shadow_softness");
                    textStyle.shadowBolden = section.GetFloat("shadow_bolden");
                    textStyle.wrap = section.GetBool("wrap");
                }
                const bool hasContainer=section.Has("child_layout")||section.Has("child_padding")||
                    section.Has("child_gap")||section.Has("child_columns")||section.Has("compact_below_width")||
                    section.Has("compact_child_layout")||section.Has("compact_child_columns");
                if(hasContainer)
                {
                    const auto scalar=[&](const char* key,auto& value)
                    {
                        if(!section.Has(key))return false;
                        std::istringstream stream(section.GetText(key));stream.imbue(std::locale::classic());
                        stream>>std::noskipws>>value;
                        return !stream.fail()&&stream.peek()==std::char_traits<char>::eof();
                    };
                    int mode=-1;
                    if(sourceSchema<3||!scalar("child_layout",mode)||mode<0||mode>3||
                        !scalar("child_padding",widget.container.padding)||
                        !scalar("child_gap",widget.container.gap)||
                        !scalar("child_columns",widget.container.columns))
                    {error="Container layout fields are missing or malformed.";return false;}
                    widget.container.mode=static_cast<ScreenChildLayout>(mode);
                    if(section.Has("compact_below_width")||section.Has("compact_child_layout")||
                        section.Has("compact_child_columns"))
                    {
                        int compact=-1;
                        if(!scalar("compact_below_width",widget.container.compactBelowWidth)||
                            !scalar("compact_child_layout",compact)||compact<0||compact>3||
                            !scalar("compact_child_columns",widget.container.compactColumns))
                        {error="Compact layout fields are missing or malformed.";return false;}
                        widget.container.compactMode=static_cast<ScreenChildLayout>(compact);
                    }
                    if(!ValidateScreenContainerLayout(widget.container,error))return false;
                }
                if (section.Has("binding_count"))
                {
                    const auto countText = section.GetText("binding_count");
                    int bindingCount = -1;
                    std::istringstream countStream(countText);
                    countStream >> std::noskipws >> bindingCount;
                    if (sourceSchema < 3 || countStream.fail() ||
                        countStream.peek() != std::char_traits<char>::eof() ||
                        bindingCount < 0 || bindingCount > 4)
                    { error = "Screen element binding count is malformed."; return false; }
                    for (int bindingIndex = 0; bindingIndex < bindingCount; ++bindingIndex)
                    {
                        const auto bindingSection = sectionName + "_" +
                            IndexedSection("binding_", static_cast<std::size_t>(bindingIndex));
                        ScreenBinding binding;
                        if (!file.HasSection(bindingSection.c_str()))
                        { error = "Screen element binding section is missing."; return false; }
                        if (!detail::ReadScreenBinding(file.GetSection(bindingSection.c_str()), binding, error))
                            return false;
                        widget.bindings.push_back(std::move(binding));
                    }
                }
                if(!detail::ReadWidgetInteractions(file,sectionName,sourceSchema,widget.interactions,error))return false;
                if(!detail::ReadWidgetAnimations(file,sectionName,sourceSchema,widget.animations,error))return false;
                if(section.Has("theme_overrides"))
                {
                    int mask=-1;
                    if(sourceSchema<3||!detail::ReadAnimationScalar(section,"theme_overrides",mask)||mask<0||mask>63)
                    {error="Malformed UI theme override mask.";return false;}
                    widget.themeOverrides=static_cast<unsigned>(mask);
                }
                if(section.Has("component_document")||section.Has("component_path")||
                    section.Has("component_widget")||section.Has("component_overrides")||
                    section.Has("component_tree_root"))
                {
                    int mask=-1;
                    if(sourceSchema<3||!section.Has("component_document")||!section.Has("component_path")||
                        !section.Has("component_widget")||
                        !detail::ReadAnimationScalar(section,"component_overrides",mask)||mask<0||mask>1023)
                    {error="Malformed linked component reference.";return false;}
                    widget.component=ScreenComponentReference{section.GetText("component_document"),
                        section.GetText("component_path"),section.GetText("component_widget"),static_cast<unsigned>(mask)};
                    if(section.Has("component_tree_root"))
                        widget.component->treeRootId=section.GetText("component_tree_root");
                }
                parsed.widgets.push_back(std::move(widget));
            }

            parsed.focusOrder.reserve(static_cast<std::size_t>(focusCount));
            for (int index = 0; index < focusCount; ++index)
            {
                const std::string sectionName = IndexedSection(
                    "focus_",
                    static_cast<std::size_t>(index));
                if (!file.HasSection(sectionName.c_str()))
                {
                    error = "The Runtime screen document is missing section " +
                        sectionName + '.';
                    return false;
                }
                parsed.focusOrder.push_back(
                    file.GetSection(sectionName.c_str()).GetText("widget_id"));
            }

            if(file.HasSection("ui_theme"))
            {
                const auto& theme=file.GetSection("ui_theme");
                if(sourceSchema<3||!theme.Has("document_id")||!theme.Has("path_hint"))
                {error="Malformed UI theme reference.";return false;}
                parsed.theme=ScreenThemeReference{theme.GetText("document_id"),theme.GetText("path_hint")};
            }
            if (!ValidateScreenDocument(parsed, expectedProjectId, error))
            {
                return false;
            }

            document = std::move(parsed);
            error.clear();
            return true;
        }
        catch (const std::exception& exception)
        {
            error = std::string("Could not read Runtime screen document: ") +
                exception.what();
            return false;
        }
    }

    bool ResolveRuntimeScreenDocumentPath(
        const std::string& projectRoot,
        const StableId& expectedProjectId,
        const StableId& documentId,
        const std::string& pathHint,
        std::string& resolvedPath,
        std::string& error)
    {
        if (!IsValidStableId(expectedProjectId) ||
            !IsValidStableId(documentId))
        {
            error = "Runtime screen reference is missing a valid project or document ID.";
            return false;
        }
        if (!IsContentRelativePath(pathHint))
        {
            error = "Runtime screen path hint must be project-relative inside Content.";
            return false;
        }

        const fs::path root = fs::u8path(projectRoot);
        std::vector<fs::path> matches;
        bool foundWrongOwner = false;
        bool foundWrongType = false;

        const auto consider = [&](const fs::path& candidate)
        {
            std::error_code fileError;
            if (!fs::is_regular_file(candidate, fileError) || fileError)
            {
                return;
            }

            DocumentEnvelope envelope;
            std::string readError;
            if (!ReadDocumentEnvelope(
                    candidate.generic_u8string(),
                    envelope,
                    readError) ||
                envelope.documentId != documentId)
            {
                return;
            }
            if (envelope.projectId != expectedProjectId)
            {
                foundWrongOwner = true;
                return;
            }
            if (envelope.documentType != RuntimeScreenDocumentType)
            {
                foundWrongType = true;
                return;
            }

            fs::path canonical;
            std::string containmentError;
            if (CanonicalWithinRoot(root, candidate, canonical, containmentError))
            {
                AddUniquePath(matches, canonical);
            }
        };

        consider((root / fs::u8path(pathHint)).lexically_normal());

        std::error_code scanError;
        const fs::path contentRoot = root / "Content";
        if (fs::is_directory(contentRoot, scanError) && !scanError)
        {
            fs::recursive_directory_iterator iterator(
                contentRoot,
                fs::directory_options::skip_permission_denied,
                scanError);
            const fs::recursive_directory_iterator end;
            while (!scanError && iterator != end)
            {
                if (iterator->is_regular_file(scanError) && !scanError &&
                    iterator->path().extension() == ".renegade-screen")
                {
                    consider(iterator->path());
                }
                iterator.increment(scanError);
            }
        }

        std::sort(matches.begin(), matches.end());
        if (matches.empty())
        {
            if (foundWrongOwner)
            {
                error = "Runtime screen document ID belongs to a different project.";
            }
            else if (foundWrongType)
            {
                error = "Runtime screen document ID has the wrong document type.";
            }
            else
            {
                error = "Could not resolve Runtime screen document ID " +
                    documentId + " inside the project.";
            }
            return false;
        }
        if (matches.size() > 1)
        {
            error = "Runtime screen document ID " + documentId +
                " is ambiguous: " + std::to_string(matches.size()) +
                " matching project documents were found.";
            return false;
        }

        resolvedPath = matches.front().generic_u8string();
        error.clear();
        return true;
    }

    bool ResolveScreenResourcePath(
        const std::string& projectRoot,
        const std::string& resourcePath,
        std::string& resolvedPath,
        std::string& error)
    {
        if (!IsContentRelativePath(resourcePath))
        {
            error = "Runtime screen resources must be project-relative inside Content.";
            return false;
        }

        const fs::path root = fs::u8path(projectRoot);
        const fs::path candidate =
            (root / fs::u8path(resourcePath)).lexically_normal();
        std::error_code fileError;
        if (!fs::is_regular_file(candidate, fileError) || fileError)
        {
            error = "Runtime screen resource is missing: " +
                candidate.generic_u8string();
            return false;
        }

        fs::path canonical;
        if (!CanonicalWithinRoot(root, candidate, canonical, error))
        {
            return false;
        }

        std::error_code pathError;
        const fs::path canonicalContent =
            fs::weakly_canonical(root / "Content", pathError);
        if (pathError)
        {
            error = "Could not canonicalize the project Content tree: " +
                pathError.message();
            return false;
        }
        const fs::path relativeToContent = fs::relative(
            canonical,
            canonicalContent,
            pathError);
        if (pathError || relativeToContent.empty() ||
            relativeToContent.is_absolute() ||
            std::any_of(
                relativeToContent.begin(),
                relativeToContent.end(),
                [](const fs::path& part)
                {
                    return part == "..";
                }))
        {
            error = "Runtime screen resource resolved outside the project Content tree.";
            return false;
        }

        resolvedPath = canonical.generic_u8string();
        error.clear();
        return true;
    }
}
