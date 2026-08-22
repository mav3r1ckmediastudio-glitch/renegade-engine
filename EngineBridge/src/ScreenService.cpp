#include "renegade/bridge/ScreenService.h"
#include "renegade/bridge/DependencyService.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <filesystem>
#include <functional>
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
        text << '#' << std::hex << std::uppercase << std::setfill('0')
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
        if (value.size() != 9 || value.front() != '#') return false;
        unsigned long packed = 0;
        try
        {
            std::size_t consumed = 0;
            packed = std::stoul(value.substr(1), &consumed, 16);
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
                     widget.kind == ScreenWidgetKind::Button) &&
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

    bool ResolveScreenWidgetRect(
        const ScreenDocument& document,
        const StableId& widgetId,
        ScreenRect& rect,
        std::string& error)
    {
        std::unordered_map<StableId, const ScreenWidget*> widgets;
        for (const auto& widget : document.widgets)
        {
            widgets.emplace(widget.id, &widget);
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

            if (widget.layoutMode == ScreenLayoutMode::Absolute)
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

            resolving.erase(id);
            resolved.emplace(id, output);
            return true;
        };

        if (!resolve(widgetId, rect)) return false;
        if (!IsFiniteRect(rect, document.designWidth, document.designHeight))
        {
            error = "Runtime screen widget layout resolves outside the design canvas.";
            return false;
        }
        error.clear();
        return true;
    }

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
        if (document.schemaVersion != ScreenDocument::CurrentSchemaVersion ||
            document.migratedFromVersion > 1)
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
        if (std::string(CanvasScaleModeName(document.scaleMode)) == "unknown")
        {
            error = "The Runtime screen has an invalid canvas scale mode.";
            return false;
        }
        if (document.actions.empty() || document.actions.size() > 64)
        {
            error = "The Runtime screen must declare between 1 and 64 actions.";
            return false;
        }
        if (document.widgets.empty() || document.widgets.size() > 256)
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
            ScreenRect resolvedRect;
            if (!ResolveScreenWidgetRect(document, widget.id, resolvedRect, error))
            {
                error = "Runtime screen widget '" + widget.name +
                    "' has invalid layout: " + error;
                return false;
            }

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

            if (widget.kind == ScreenWidgetKind::Text ||
                widget.kind == ScreenWidgetKind::Button)
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

            switch (widget.kind)
            {
            case ScreenWidgetKind::Image:
                if (!widget.visible ||
                    !IsContentRelativePath(widget.resourcePath) ||
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
            }
        }

        if (buttonCount == 0)
        {
            error = "The Runtime screen must contain at least one action button.";
            return false;
        }
        if (availableButtonCount == 0)
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
                found->second->kind != ScreenWidgetKind::Button)
            {
                error = "The Runtime screen focus order references a missing or non-button widget.";
                return false;
            }
        }
        for (const auto& [widgetId, widget] : widgets)
        {
            if (widget->kind == ScreenWidgetKind::Button &&
                focusIds.count(widgetId) != 1)
            {
                error = "The Runtime screen focus order omits button: " +
                    widget->name;
                return false;
            }
        }

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
                auto& screen = file.GetSection("screen");
                screen.Set("schema_version", document.schemaVersion);
                screen.Set("migrated_from", document.migratedFromVersion);
                screen.Set("design_width", document.designWidth);
                screen.Set("design_height", document.designHeight);
                screen.Set("scale_mode", CanvasScaleModeName(document.scaleMode));
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
                    section.Set("visible", widget.visible);
                    section.Set("enabled", widget.enabled);
                    section.Set("text", widget.text);
                    section.Set("resource", widget.resourcePath);
                    section.Set("action", widget.actionId);
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
            if (sourceSchema != 1 &&
                sourceSchema != ScreenDocument::CurrentSchemaVersion)
            {
                error = "The Runtime screen document uses unsupported Screen schema " +
                    std::to_string(sourceSchema) + '.';
                return false;
            }
            parsed.schemaVersion = ScreenDocument::CurrentSchemaVersion;
            parsed.migratedFromVersion = sourceSchema == 1 ? 1 :
                (screen.Has("migrated_from") ? screen.GetUint("migrated_from") : 0);
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
            const int actionCount = screen.GetInt("action_count");
            const int widgetCount = screen.GetInt("widget_count");
            const int focusCount = screen.GetInt("focus_count");
            if (actionCount <= 0 || actionCount > 64 ||
                widgetCount <= 0 || widgetCount > 256 ||
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
                widget.visible = section.GetBool("visible");
                widget.enabled = section.GetBool("enabled");
                widget.text = section.GetText("text");
                widget.resourcePath = section.GetText("resource");
                widget.actionId = section.GetText("action");
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
