#include "renegade/bridge/ScreenService.h"
#include "renegade/bridge/DependencyService.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <filesystem>
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
            for (const auto& widget : screen.widgets)
            {
                if (widget.kind == ScreenWidgetKind::Image &&
                    !widget.resourcePath.empty())
                    document.imagePaths.push_back(widget.resourcePath);
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
        if (!std::isfinite(document.designWidth) ||
            !std::isfinite(document.designHeight) ||
            document.designWidth < 320.0f || document.designWidth > 16384.0f ||
            document.designHeight < 180.0f || document.designHeight > 16384.0f)
        {
            error = "The Runtime screen has invalid design dimensions.";
            return false;
        }
        if (document.actions.size() != 2)
        {
            error = "LP03 Runtime screens must declare exactly play and quit.";
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
        if (actionIds.count(RuntimeScreenPlayAction) != 1 ||
            actionIds.count(RuntimeScreenQuitAction) != 1)
        {
            error = "The Runtime screen must declare stable play and quit actions.";
            return false;
        }

        std::unordered_map<StableId, const ScreenWidget*> widgets;
        std::size_t imageCount = 0;
        std::size_t textCount = 0;
        std::size_t buttonCount = 0;
        std::size_t playButtonCount = 0;
        std::size_t quitButtonCount = 0;
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
            if (!IsFiniteRect(
                    widget.rect,
                    document.designWidth,
                    document.designHeight))
            {
                error = "Runtime screen widget '" + widget.name +
                    "' has an invalid design rectangle.";
                return false;
            }

            switch (widget.kind)
            {
            case ScreenWidgetKind::Image:
                ++imageCount;
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
                ++textCount;
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
                if (widget.actionId == RuntimeScreenPlayAction)
                {
                    ++playButtonCount;
                }
                if (widget.actionId == RuntimeScreenQuitAction)
                {
                    ++quitButtonCount;
                }
                break;
            }
        }

        if (imageCount != 1 || textCount < 1 || buttonCount != 2 ||
            playButtonCount != 1 || quitButtonCount != 1)
        {
            error = "The LP03 screen must contain one image, text, and one "
                "button for each of play and quit.";
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
                screen.Set("design_width", document.designWidth);
                screen.Set("design_height", document.designHeight);
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
            parsed.designWidth = screen.GetFloat("design_width");
            parsed.designHeight = screen.GetFloat("design_height");
            const int actionCount = screen.GetInt("action_count");
            const int widgetCount = screen.GetInt("widget_count");
            const int focusCount = screen.GetInt("focus_count");
            if (actionCount < 0 || actionCount > 64 ||
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
