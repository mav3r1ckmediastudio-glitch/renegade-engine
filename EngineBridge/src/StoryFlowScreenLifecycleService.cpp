#include "renegade/bridge/StoryFlowScreenLifecycleService.h"

#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ScreenService.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    constexpr const char* Gate5Generator = "Renegade Story Flow Gate 5A";

    struct ScopedDirectory
    {
        fs::path path;
        ~ScopedDirectory()
        {
            if (path.empty()) return;
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
    };

    std::string Trim(std::string value)
    {
        const auto whitespace = [](const unsigned char c)
        {
            return std::isspace(c) != 0;
        };
        while (!value.empty() && whitespace(value.front())) value.erase(value.begin());
        while (!value.empty() && whitespace(value.back())) value.pop_back();
        return value;
    }

    std::string LowerAscii(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](const unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });
        return value;
    }

    bool IsRelativeContainedPath(const fs::path& relative)
    {
        if (relative.empty() || relative.is_absolute() ||
            relative.has_root_name() || relative.has_root_directory())
        {
            return false;
        }
        return std::none_of(relative.begin(), relative.end(),
            [](const fs::path& part) { return part == ".."; });
    }

    bool IsInsideRoot(const fs::path& root, const fs::path& candidate)
    {
        const fs::path relative = candidate.lexically_relative(root);
        return IsRelativeContainedPath(relative);
    }

    bool NormalizeScreenHint(
        const std::string& input,
        std::string& normalized,
        std::string& error)
    {
        normalized.clear();
        const fs::path relative = fs::u8path(input).lexically_normal();
        if (!IsRelativeContainedPath(relative) || relative.filename().empty())
        {
            error = "The new Screen path must be project-relative.";
            return false;
        }

        auto part = relative.begin();
        if (part == relative.end() || part->generic_u8string() != "Content")
        {
            error = "New Screens must be created under Content/Screens/.";
            return false;
        }
        ++part;
        if (part == relative.end() || part->generic_u8string() != "Screens")
        {
            error = "New Screens must be created under Content/Screens/.";
            return false;
        }

        if (LowerAscii(relative.extension().generic_u8string()) != ".renegade-screen")
        {
            error = "A new Screen must use the .renegade-screen extension.";
            return false;
        }

        normalized = relative.generic_u8string();
        error.clear();
        return true;
    }

    bool ResolveProjectPath(
        const fs::path& root,
        const std::string& value,
        fs::path& resolved,
        std::string& error)
    {
        if (value.empty())
        {
            error = "A project document path is required.";
            return false;
        }

        std::error_code pathError;
        fs::path candidate = fs::u8path(value);
        if (!candidate.is_absolute()) candidate = root / candidate;
        resolved = fs::absolute(candidate, pathError).lexically_normal();
        if (pathError || resolved.filename().empty())
        {
            error = "Could not resolve the project document path: " + pathError.message();
            return false;
        }
        if (!IsInsideRoot(root, resolved))
        {
            error = "The project document path escapes the active project root.";
            return false;
        }
        error.clear();
        return true;
    }

    bool ReadFileBytes(
        const fs::path& path,
        std::vector<std::uint8_t>& bytes,
        std::string& error)
    {
        bytes.clear();
        std::ifstream stream(path, std::ios::binary | std::ios::ate);
        if (!stream)
        {
            error = "Could not read rendered document: " + path.generic_u8string();
            return false;
        }
        const std::streamoff size = stream.tellg();
        if (size < 0)
        {
            error = "Could not inspect rendered document size: " + path.generic_u8string();
            return false;
        }
        bytes.resize(static_cast<std::size_t>(size));
        stream.seekg(0, std::ios::beg);
        if (!bytes.empty())
        {
            stream.read(reinterpret_cast<char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
            if (stream.gcount() != static_cast<std::streamsize>(bytes.size()))
            {
                bytes.clear();
                error = "Could not read complete rendered document: " + path.generic_u8string();
                return false;
            }
        }
        error.clear();
        return true;
    }

    bool FileMatchesBytes(
        const std::string& path,
        const std::vector<std::uint8_t>& expected,
        std::string& error)
    {
        std::vector<std::uint8_t> actual;
        if (!ReadFileBytes(fs::u8path(path), actual, error)) return false;
        if (actual != expected)
        {
            error = "The staged document bytes do not match the requested payload.";
            return false;
        }
        error.clear();
        return true;
    }

    const FlowNode* FindNode(const FlowDocument& document, const StableId& nodeId)
    {
        const auto found = std::find_if(document.nodes.begin(), document.nodes.end(),
            [&nodeId](const FlowNode& node) { return node.id == nodeId; });
        return found == document.nodes.end() ? nullptr : &*found;
    }

    void AddText(
        ScreenDocument& document,
        std::string name,
        std::string text,
        const float x,
        const float y,
        const float width,
        const float height)
    {
        ScreenWidget widget;
        widget.id = GenerateStableId();
        widget.kind = ScreenWidgetKind::Text;
        widget.name = std::move(name);
        widget.rect = {x, y, width, height};
        widget.visible = true;
        widget.enabled = false;
        widget.text = std::move(text);
        widget.style = MakeScreenWidgetStyleTemplate(widget.kind, height);
        document.widgets.push_back(std::move(widget));
    }

    void AddButton(
        ScreenDocument& document,
        std::string actionId,
        std::string label,
        const float y)
    {
        document.actions.push_back({actionId});

        ScreenWidget widget;
        widget.id = GenerateStableId();
        widget.kind = ScreenWidgetKind::Button;
        widget.name = label;
        widget.rect = {440.0f, y, 400.0f, 58.0f};
        widget.visible = true;
        widget.enabled = true;
        widget.text = std::move(label);
        widget.actionId = std::move(actionId);
        widget.style = MakeScreenWidgetStyleTemplate(
            widget.kind, widget.rect.height);
        document.focusOrder.push_back(widget.id);
        document.widgets.push_back(std::move(widget));
    }

    ScreenDocument BuildTemplate(
        const StableId& projectId,
        const StableId& documentId,
        const std::string& pathHint,
        const std::string& screenName,
        const StoryFlowScreenTemplate screenTemplate)
    {
        ScreenDocument document;
        document.envelope = CreateDocumentEnvelope(
            projectId,
            RuntimeScreenDocumentType,
            pathHint,
            Gate5Generator);
        document.envelope.documentId = documentId;
        document.designWidth = 1280.0f;
        document.designHeight = 720.0f;

        AddText(document, "Title", screenName, 240.0f, 90.0f, 800.0f, 80.0f);

        switch (screenTemplate)
        {
        case StoryFlowScreenTemplate::Title:
            AddButton(document, "new_game", "NEW GAME", 220.0f);
            AddButton(document, "load_game", "LOAD GAME", 292.0f);
            AddButton(document, "options", "OPTIONS", 364.0f);
            AddButton(document, "credits", "CREDITS", 436.0f);
            AddButton(document, "quit", "QUIT", 508.0f);
            break;
        case StoryFlowScreenTemplate::Loading:
            AddText(document, "LoadingLabel", "LOADING", 440.0f, 240.0f, 400.0f, 72.0f);
            AddButton(document, "continue", "CONTINUE", 400.0f);
            break;
        case StoryFlowScreenTemplate::Options:
            AddButton(document, "apply", "APPLY", 300.0f);
            AddButton(document, "back", "BACK", 380.0f);
            break;
        case StoryFlowScreenTemplate::Pause:
            AddButton(document, "resume", "RESUME", 260.0f);
            AddButton(document, "options", "OPTIONS", 340.0f);
            AddButton(document, "main_menu", "MAIN MENU", 420.0f);
            break;
        case StoryFlowScreenTemplate::Failure:
            AddButton(document, "retry", "RETRY", 300.0f);
            AddButton(document, "main_menu", "MAIN MENU", 380.0f);
            break;
        case StoryFlowScreenTemplate::Victory:
            AddButton(document, "continue", "CONTINUE", 300.0f);
            AddButton(document, "main_menu", "MAIN MENU", 380.0f);
            break;
        case StoryFlowScreenTemplate::SaveLoad:
            AddButton(document, "save", "SAVE", 260.0f);
            AddButton(document, "load", "LOAD", 340.0f);
            AddButton(document, "back", "BACK", 420.0f);
            break;
        case StoryFlowScreenTemplate::Credits:
            AddText(document, "Credits", "CREDITS", 340.0f, 240.0f, 600.0f, 72.0f);
            AddButton(document, "back", "BACK", 430.0f);
            break;
        case StoryFlowScreenTemplate::Custom:
        default:
            AddButton(document, "continue", "CONTINUE", 360.0f);
            break;
        }
        return document;
    }

    std::vector<std::string> ActionIds(const ScreenDocument& document)
    {
        std::vector<std::string> result;
        result.reserve(document.actions.size());
        for (const auto& action : document.actions) result.push_back(action.id);
        return result;
    }

    bool SameActionIds(
        const ScreenDocument& document,
        const std::vector<std::string>& expected)
    {
        return ActionIds(document) == expected;
    }

    StoryFlowNewScreenResult Failure(
        const StoryFlowScreenCreateCode code,
        std::string message)
    {
        StoryFlowNewScreenResult result;
        result.code = code;
        result.message = std::move(message);
        return result;
    }
}

namespace renegade::bridge
{
    const char* StoryFlowScreenTemplateName(
        const StoryFlowScreenTemplate value) noexcept
    {
        switch (value)
        {
        case StoryFlowScreenTemplate::Custom: return "custom";
        case StoryFlowScreenTemplate::Title: return "title";
        case StoryFlowScreenTemplate::Loading: return "loading";
        case StoryFlowScreenTemplate::Options: return "options";
        case StoryFlowScreenTemplate::Pause: return "pause";
        case StoryFlowScreenTemplate::Failure: return "failure";
        case StoryFlowScreenTemplate::Victory: return "victory";
        case StoryFlowScreenTemplate::SaveLoad: return "save-load";
        case StoryFlowScreenTemplate::Credits: return "credits";
        default: return "unknown";
        }
    }

    const char* StoryFlowScreenCreateCodeName(
        const StoryFlowScreenCreateCode code) noexcept
    {
        switch (code)
        {
        case StoryFlowScreenCreateCode::Success: return "success";
        case StoryFlowScreenCreateCode::InvalidRequest: return "invalid_request";
        case StoryFlowScreenCreateCode::Conflict: return "conflict";
        case StoryFlowScreenCreateCode::RenderFailed: return "render_failed";
        case StoryFlowScreenCreateCode::TransactionFailed: return "transaction_failed";
        case StoryFlowScreenCreateCode::VerificationFailed: return "verification_failed";
        default: return "unknown";
        }
    }

    StoryFlowNewScreenResult StoryFlowScreenLifecycleService::CreateNewScreen(
        const StoryFlowNewScreenRequest& request) const
    {
        try
        {
            if (!IsValidStableId(request.projectId))
            {
                return Failure(StoryFlowScreenCreateCode::InvalidRequest,
                    "Add Screen requires a valid owning project ID.");
            }
            if (request.projectRoot.empty())
            {
                return Failure(StoryFlowScreenCreateCode::InvalidRequest,
                    "Add Screen requires an active project root.");
            }

            std::error_code pathError;
            const fs::path projectRoot =
                fs::absolute(fs::u8path(request.projectRoot), pathError).lexically_normal();
            if (pathError || !fs::is_directory(projectRoot, pathError))
            {
                return Failure(StoryFlowScreenCreateCode::InvalidRequest,
                    "Add Screen could not resolve the active project root.");
            }

            std::string error;
            if (!ValidateFlowDocument(request.flow, request.projectId, error))
            {
                return Failure(StoryFlowScreenCreateCode::InvalidRequest,
                    "The Story Flow snapshot is not valid: " + error);
            }

            fs::path flowPath;
            if (!ResolveProjectPath(projectRoot, request.flowPath, flowPath, error))
            {
                return Failure(StoryFlowScreenCreateCode::InvalidRequest, error);
            }
            if (!fs::is_regular_file(flowPath, pathError) || pathError)
            {
                return Failure(StoryFlowScreenCreateCode::InvalidRequest,
                    "The Story Flow document to update does not exist: " +
                    flowPath.generic_u8string());
            }

            FlowDocument onDiskFlow;
            if (!ReadFlowDocument(flowPath.generic_u8string(), request.projectId,
                    onDiskFlow, error))
            {
                return Failure(StoryFlowScreenCreateCode::InvalidRequest,
                    "The current Story Flow document could not be read: " + error);
            }
            if (onDiskFlow.envelope.documentId != request.flow.envelope.documentId)
            {
                return Failure(StoryFlowScreenCreateCode::Conflict,
                    "The supplied Story Flow snapshot does not match the document on disk.");
            }

            const std::string screenName = Trim(request.screenName);
            if (screenName.empty())
            {
                return Failure(StoryFlowScreenCreateCode::InvalidRequest,
                    "A new Screen requires a visible name.");
            }

            std::string screenHint;
            if (!NormalizeScreenHint(request.screenPathHint, screenHint, error))
            {
                return Failure(StoryFlowScreenCreateCode::InvalidRequest, error);
            }

            const fs::path screenPath =
                (projectRoot / fs::u8path(screenHint)).lexically_normal();
            if (!IsInsideRoot(projectRoot, screenPath))
            {
                return Failure(StoryFlowScreenCreateCode::InvalidRequest,
                    "The new Screen path escapes the active project root.");
            }

            pathError.clear();
            const bool screenExists = fs::exists(screenPath, pathError);
            if (pathError)
            {
                return Failure(StoryFlowScreenCreateCode::Conflict,
                    "Could not inspect the new Screen destination: " + pathError.message());
            }
            if (screenExists)
            {
                return Failure(StoryFlowScreenCreateCode::Conflict,
                    "Add Screen will not overwrite an existing Runtime Screen document.");
            }

            StoryFlowNewScreenResult result;
            result.screenNodeId = GenerateStableId();
            result.screenDocumentId = GenerateStableId();
            result.screenPathHint = screenHint;
            result.screenPath = screenPath.generic_u8string();

            ScreenDocument screen = BuildTemplate(
                request.projectId,
                result.screenDocumentId,
                screenHint,
                screenName,
                request.screenTemplate);
            if (!ValidateScreenDocument(screen, request.projectId, error))
            {
                return Failure(StoryFlowScreenCreateCode::InvalidRequest,
                    "The selected Screen template is invalid: " + error);
            }
            result.actionIds = ActionIds(screen);

            FlowDocument candidate = request.flow;
            FlowNode screenNode;
            screenNode.id = result.screenNodeId;
            screenNode.kind = FlowNodeKind::Screen;
            screenNode.name = screenName;
            screenNode.screenDocumentId = result.screenDocumentId;
            screenNode.screenPathHint = screenHint;
            candidate.nodes.push_back(std::move(screenNode));
            if (!ValidateFlowDocument(candidate, request.projectId, error))
            {
                return Failure(StoryFlowScreenCreateCode::InvalidRequest,
                    "The new Screen would make Story Flow invalid: " + error);
            }

            pathError.clear();
            const fs::path tempRoot = fs::temp_directory_path(pathError);
            if (pathError)
            {
                return Failure(StoryFlowScreenCreateCode::RenderFailed,
                    "Could not resolve the Screen transaction render root: " +
                    pathError.message());
            }
            ScopedDirectory renderDirectory{
                tempRoot / "renegade-story-flow" / "screen-create" /
                fs::u8path(GenerateStableId())
            };
            fs::create_directories(renderDirectory.path, pathError);
            if (pathError)
            {
                return Failure(StoryFlowScreenCreateCode::RenderFailed,
                    "Could not create the Screen transaction render directory: " +
                    pathError.message());
            }

            const fs::path renderedScreen =
                renderDirectory.path / "Screen.renegade-screen";
            const fs::path renderedFlow =
                renderDirectory.path / "Flow.renegade-flow";
            if (!WriteScreenDocument(renderedScreen.generic_u8string(), screen, error))
            {
                return Failure(StoryFlowScreenCreateCode::RenderFailed,
                    "Could not render the new Runtime Screen document: " + error);
            }
            if (!WriteFlowDocument(renderedFlow.generic_u8string(), candidate, error))
            {
                return Failure(StoryFlowScreenCreateCode::RenderFailed,
                    "Could not render the updated Story Flow document: " + error);
            }

            std::vector<std::uint8_t> screenBytes;
            std::vector<std::uint8_t> flowBytes;
            std::vector<std::uint8_t> previousFlowBytes;
            if (!ReadFileBytes(renderedScreen, screenBytes, error) ||
                !ReadFileBytes(renderedFlow, flowBytes, error) ||
                !ReadFileBytes(flowPath, previousFlowBytes, error))
            {
                return Failure(StoryFlowScreenCreateCode::RenderFailed, error);
            }

            std::vector<ProjectDocumentWrite> writes;
            writes.reserve(3);

            ProjectDocumentWrite flowBackup;
            flowBackup.destinationPath = flowPath.generic_u8string() + ".bak";
            flowBackup.content = previousFlowBytes;
            flowBackup.validator = [previousFlowBytes](
                const std::string& stagedPath, std::string& validationError)
            {
                return FileMatchesBytes(stagedPath, previousFlowBytes, validationError);
            };
            writes.push_back(std::move(flowBackup));

            ProjectDocumentWrite flowWrite;
            flowWrite.destinationPath = flowPath.generic_u8string();
            flowWrite.content = flowBytes;
            flowWrite.validator = [
                expectedBytes = flowBytes,
                projectId = request.projectId,
                flowId = candidate.envelope.documentId,
                screenNodeId = result.screenNodeId,
                screenId = result.screenDocumentId,
                screenHint](const std::string& stagedPath, std::string& validationError)
            {
                if (!FileMatchesBytes(stagedPath, expectedBytes, validationError))
                    return false;
                FlowDocument roundTrip;
                if (!ReadFlowDocument(stagedPath, projectId, roundTrip, validationError))
                    return false;
                if (roundTrip.envelope.documentId != flowId)
                {
                    validationError = "The staged Story Flow changed document identity.";
                    return false;
                }
                const FlowNode* node = FindNode(roundTrip, screenNodeId);
                if (!node || node->kind != FlowNodeKind::Screen ||
                    node->screenDocumentId != screenId ||
                    node->screenPathHint != screenHint)
                {
                    validationError = "The staged Story Flow lost the new Screen reference.";
                    return false;
                }
                validationError.clear();
                return true;
            };
            writes.push_back(std::move(flowWrite));

            ProjectDocumentWrite screenWrite;
            screenWrite.destinationPath = screenPath.generic_u8string();
            screenWrite.content = screenBytes;
            screenWrite.validator = [
                expectedBytes = screenBytes,
                projectId = request.projectId,
                screenId = result.screenDocumentId,
                screenHint,
                expectedActions = result.actionIds](
                    const std::string& stagedPath, std::string& validationError)
            {
                if (!FileMatchesBytes(stagedPath, expectedBytes, validationError))
                    return false;
                ScreenDocument roundTrip;
                if (!ReadScreenDocument(stagedPath, projectId, roundTrip, validationError))
                    return false;
                if (roundTrip.envelope.documentId != screenId ||
                    roundTrip.envelope.pathHint != screenHint ||
                    !SameActionIds(roundTrip, expectedActions))
                {
                    validationError =
                        "The staged Runtime Screen did not retain its stable identity and actions.";
                    return false;
                }
                validationError.clear();
                return true;
            };
            writes.push_back(std::move(screenWrite));

            ProjectDocumentTransactionOptions transactionOptions;
            transactionOptions.transactionId = request.transactionId.empty()
                ? GenerateStableId() : request.transactionId;
            transactionOptions.journalDirectory =
                (projectRoot / "Intermediate" / "Transactions").generic_u8string();
            transactionOptions.allowedRoot = projectRoot.generic_u8string();
            transactionOptions.operationHook = request.transactionHook;

            ProjectDocumentTransaction transaction;
            result.transaction = transaction.Execute(
                std::move(writes), std::move(transactionOptions));
            if (!result.transaction.success)
            {
                result.code = StoryFlowScreenCreateCode::TransactionFailed;
                result.message = "Add Screen transaction failed [" +
                    result.transaction.code + "]: " + result.transaction.message;
                return result;
            }

            FlowDocument committed;
            if (!ReadFlowDocument(flowPath.generic_u8string(), request.projectId,
                    committed, error))
            {
                result.code = StoryFlowScreenCreateCode::VerificationFailed;
                result.message =
                    "The Screen transaction committed, but Story Flow verification failed: " +
                    error;
                return result;
            }
            const FlowNode* committedScreen = FindNode(committed, result.screenNodeId);
            if (!committedScreen || committedScreen->kind != FlowNodeKind::Screen ||
                committedScreen->screenDocumentId != result.screenDocumentId ||
                committedScreen->screenPathHint != screenHint)
            {
                result.code = StoryFlowScreenCreateCode::VerificationFailed;
                result.message =
                    "The Screen transaction committed, but the final Story Flow reference did not verify.";
                return result;
            }

            ScreenDocument committedScreenDocument;
            if (!ReadScreenDocument(screenPath.generic_u8string(), request.projectId,
                    committedScreenDocument, error) ||
                committedScreenDocument.envelope.documentId != result.screenDocumentId ||
                committedScreenDocument.envelope.pathHint != screenHint ||
                !SameActionIds(committedScreenDocument, result.actionIds))
            {
                result.code = StoryFlowScreenCreateCode::VerificationFailed;
                result.message =
                    "The Screen transaction committed, but the final Runtime Screen did not verify: " +
                    error;
                return result;
            }

            std::string resolvedScreenPath;
            if (!ResolveRuntimeScreenDocumentPath(
                    projectRoot.generic_u8string(), request.projectId,
                    result.screenDocumentId, screenHint, resolvedScreenPath, error))
            {
                result.code = StoryFlowScreenCreateCode::VerificationFailed;
                result.message =
                    "The Screen transaction committed, but stable Screen resolution failed: " +
                    error;
                return result;
            }

            result.committedFlow = std::move(committed);
            result.succeeded = true;
            result.code = StoryFlowScreenCreateCode::Success;
            result.message =
                "Created governed Screen and committed its Story Flow relationship atomically.";
            return result;
        }
        catch (const std::exception& exception)
        {
            return Failure(StoryFlowScreenCreateCode::RenderFailed,
                std::string("Add Screen failed: ") + exception.what());
        }
        catch (...)
        {
            return Failure(StoryFlowScreenCreateCode::RenderFailed,
                "Add Screen failed with an unknown error.");
        }
    }
}
