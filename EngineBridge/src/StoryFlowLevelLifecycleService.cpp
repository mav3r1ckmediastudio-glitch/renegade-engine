#include "renegade/bridge/StoryFlowLevelLifecycleService.h"

#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/SceneDocumentService.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include <wiArchive.h>
#include <wiScene.h>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    constexpr const char* Gate4Generator = "Renegade Story Flow Gate 4A";

    struct ScopedDirectory
    {
        fs::path path;

        ~ScopedDirectory()
        {
            if (path.empty())
                return;
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
    };

    std::string Trim(std::string value)
    {
        const auto isWhitespace = [](const unsigned char character)
        {
            return std::isspace(character) != 0;
        };
        while (!value.empty() && isWhitespace(value.front()))
            value.erase(value.begin());
        while (!value.empty() && isWhitespace(value.back()))
            value.pop_back();
        return value;
    }

    std::string LowerAscii(std::string value)
    {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](const unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
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
        return std::none_of(
            relative.begin(),
            relative.end(),
            [](const fs::path& part)
            {
                return part == "..";
            });
    }

    bool IsInsideRoot(const fs::path& root, const fs::path& candidate)
    {
        const fs::path relative = candidate.lexically_relative(root);
        if (relative.empty() || relative.is_absolute() ||
            relative.has_root_name() || relative.has_root_directory())
        {
            return false;
        }
        return std::none_of(
            relative.begin(),
            relative.end(),
            [](const fs::path& part)
            {
                return part == "..";
            });
    }

    bool NormalizeSceneHint(
        const std::string& input,
        std::string& normalized,
        std::string& error)
    {
        normalized.clear();
        const fs::path relative = fs::u8path(input).lexically_normal();
        if (!IsRelativeContainedPath(relative) || relative.filename().empty())
        {
            error = "The new Level Scene path must be project-relative.";
            return false;
        }

        auto part = relative.begin();
        if (part == relative.end() || part->generic_u8string() != "Content")
        {
            error = "New Level Scenes must be created under Content/Scenes/.";
            return false;
        }
        ++part;
        if (part == relative.end() || part->generic_u8string() != "Scenes")
        {
            error = "New Level Scenes must be created under Content/Scenes/.";
            return false;
        }

        if (LowerAscii(relative.extension().generic_u8string()) != ".wiscene")
        {
            error = "A new Level Scene must use the .wiscene extension.";
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
        if (!candidate.is_absolute())
            candidate = root / candidate;
        resolved = fs::absolute(candidate, pathError).lexically_normal();
        if (pathError || resolved.filename().empty())
        {
            error = "Could not resolve the project document path: " +
                pathError.message();
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
            error = "Could not read rendered document: " +
                path.generic_u8string();
            return false;
        }
        const std::streamoff size = stream.tellg();
        if (size < 0)
        {
            error = "Could not inspect rendered document size: " +
                path.generic_u8string();
            return false;
        }
        bytes.resize(static_cast<std::size_t>(size));
        stream.seekg(0, std::ios::beg);
        if (!bytes.empty())
        {
            stream.read(
                reinterpret_cast<char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
            if (stream.gcount() !=
                static_cast<std::streamsize>(bytes.size()))
            {
                bytes.clear();
                error = "Could not read complete rendered document: " +
                    path.generic_u8string();
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
        if (!ReadFileBytes(fs::u8path(path), actual, error))
            return false;
        if (actual != expected)
        {
            error = "The staged document bytes do not match the requested payload.";
            return false;
        }
        error.clear();
        return true;
    }

    bool ValidateWisceneArchive(
        const std::string& path,
        std::string& error)
    {
        try
        {
            wi::Archive archive(path, true, false);
            if (!archive.IsOpen())
            {
                error = "Could not open the staged WISCENE archive.";
                return false;
            }
            wi::scene::Scene scene;
            scene.Serialize(archive);
            if (archive.GetPos() != archive.GetSize())
            {
                error = "The staged WISCENE archive has incomplete or trailing data.";
                return false;
            }
        }
        catch (const std::exception& exception)
        {
            error = "WISCENE validation failed: " + std::string(exception.what());
            return false;
        }
        catch (...)
        {
            error = "WISCENE validation failed.";
            return false;
        }
        error.clear();
        return true;
    }

    bool RenderBlankWiscene(
        const fs::path& path,
        std::string& error)
    {
        try
        {
            std::error_code directoryError;
            fs::create_directories(path.parent_path(), directoryError);
            if (directoryError)
            {
                error = "Could not create the Level render directory: " +
                    directoryError.message();
                return false;
            }

            wi::scene::Scene scene;
            wi::Archive archive(path.generic_u8string(), false, false);
            if (!archive.IsOpen())
            {
                error = "Could not create the blank WISCENE archive.";
                return false;
            }
            archive.SetCompressionEnabled(true);
            scene.Serialize(archive);
            const bool written = archive.SaveFile(path.generic_u8string());
            archive = wi::Archive();
            if (!written || !fs::is_regular_file(path))
            {
                error = "Could not render the blank WISCENE archive.";
                return false;
            }
            return ValidateWisceneArchive(path.generic_u8string(), error);
        }
        catch (const std::exception& exception)
        {
            error = "Could not render the blank WISCENE archive: " +
                std::string(exception.what());
            return false;
        }
        catch (...)
        {
            error = "Could not render the blank WISCENE archive.";
            return false;
        }
    }

    const FlowNode* FindNode(
        const FlowDocument& document,
        const StableId& nodeId)
    {
        const auto found = std::find_if(
            document.nodes.begin(),
            document.nodes.end(),
            [&nodeId](const FlowNode& node)
            {
                return node.id == nodeId;
            });
        return found == document.nodes.end() ? nullptr : &*found;
    }

    bool SameSceneEnvelope(
        const DocumentEnvelope& envelope,
        const StableId& projectId,
        const StableId& sceneId,
        const std::string& sceneHint)
    {
        return envelope.documentId == sceneId &&
            envelope.projectId == projectId &&
            envelope.documentType == SceneDocumentType &&
            envelope.pathHint == sceneHint;
    }

    StoryFlowNewLevelResult Failure(
        StoryFlowLevelCreateCode code,
        std::string message)
    {
        StoryFlowNewLevelResult result;
        result.code = code;
        result.message = std::move(message);
        return result;
    }
}

namespace renegade::bridge
{
    const char* StoryFlowLevelCreateCodeName(
        const StoryFlowLevelCreateCode code) noexcept
    {
        switch (code)
        {
        case StoryFlowLevelCreateCode::Success: return "success";
        case StoryFlowLevelCreateCode::InvalidRequest: return "invalid_request";
        case StoryFlowLevelCreateCode::Conflict: return "conflict";
        case StoryFlowLevelCreateCode::RenderFailed: return "render_failed";
        case StoryFlowLevelCreateCode::TransactionFailed: return "transaction_failed";
        case StoryFlowLevelCreateCode::VerificationFailed: return "verification_failed";
        default: return "unknown";
        }
    }

    StoryFlowNewLevelResult StoryFlowLevelLifecycleService::CreateNewLevel(
        const StoryFlowNewLevelRequest& request) const
    {
        try
        {
            if (!IsValidStableId(request.projectId))
            {
                return Failure(
                    StoryFlowLevelCreateCode::InvalidRequest,
                    "Add New Level requires a valid owning project ID.");
            }
            if (request.projectRoot.empty())
            {
                return Failure(
                    StoryFlowLevelCreateCode::InvalidRequest,
                    "Add New Level requires an active project root.");
            }

            std::error_code pathError;
            const fs::path projectRoot =
                fs::absolute(fs::u8path(request.projectRoot), pathError)
                    .lexically_normal();
            if (pathError || !fs::is_directory(projectRoot, pathError))
            {
                return Failure(
                    StoryFlowLevelCreateCode::InvalidRequest,
                    "Add New Level could not resolve the active project root.");
            }

            std::string error;
            if (!ValidateFlowDocument(request.flow, request.projectId, error))
            {
                return Failure(
                    StoryFlowLevelCreateCode::InvalidRequest,
                    "The Story Flow snapshot is not valid: " + error);
            }

            fs::path flowPath;
            if (!ResolveProjectPath(
                    projectRoot,
                    request.flowPath,
                    flowPath,
                    error))
            {
                return Failure(
                    StoryFlowLevelCreateCode::InvalidRequest,
                    error);
            }
            if (!fs::is_regular_file(flowPath, pathError) || pathError)
            {
                return Failure(
                    StoryFlowLevelCreateCode::InvalidRequest,
                    "The Story Flow document to update does not exist: " +
                        flowPath.generic_u8string());
            }

            FlowDocument onDiskFlow;
            if (!ReadFlowDocument(
                    flowPath.generic_u8string(),
                    request.projectId,
                    onDiskFlow,
                    error))
            {
                return Failure(
                    StoryFlowLevelCreateCode::InvalidRequest,
                    "The current Story Flow document could not be read: " + error);
            }
            if (onDiskFlow.envelope.documentId !=
                request.flow.envelope.documentId)
            {
                return Failure(
                    StoryFlowLevelCreateCode::Conflict,
                    "The supplied Story Flow snapshot does not match the document on disk.");
            }

            const std::string levelName = Trim(request.levelName);
            if (levelName.empty())
            {
                return Failure(
                    StoryFlowLevelCreateCode::InvalidRequest,
                    "A new Level requires a visible name.");
            }

            std::string sceneHint;
            if (!NormalizeSceneHint(
                    request.scenePathHint,
                    sceneHint,
                    error))
            {
                return Failure(
                    StoryFlowLevelCreateCode::InvalidRequest,
                    error);
            }

            const fs::path scenePath =
                (projectRoot / fs::u8path(sceneHint)).lexically_normal();
            if (!IsInsideRoot(projectRoot, scenePath))
            {
                return Failure(
                    StoryFlowLevelCreateCode::InvalidRequest,
                    "The new Level Scene path escapes the active project root.");
            }
            fs::path sidecarPath = scenePath;
            sidecarPath += ".rmeta";

            const bool sceneExists = fs::exists(scenePath, pathError);
            if (pathError)
            {
                return Failure(
                    StoryFlowLevelCreateCode::Conflict,
                    "Could not inspect the new Level destination: " +
                        pathError.message());
            }
            const bool sidecarExists = fs::exists(sidecarPath, pathError);
            if (pathError)
            {
                return Failure(
                    StoryFlowLevelCreateCode::Conflict,
                    "Could not inspect the new Level identity destination: " +
                        pathError.message());
            }
            if (sceneExists || sidecarExists)
            {
                return Failure(
                    StoryFlowLevelCreateCode::Conflict,
                    "Add New Level will not overwrite an existing Scene or Scene identity sidecar.");
            }

            StoryFlowNewLevelResult result;
            result.levelNodeId = GenerateStableId();
            result.sceneDocumentId = GenerateStableId();
            result.scenePathHint = sceneHint;
            result.scenePath = scenePath.generic_u8string();

            FlowDocument candidate = request.flow;
            FlowNode level;
            level.id = result.levelNodeId;
            level.kind = FlowNodeKind::Level;
            level.name = levelName;
            level.sceneAssetId = result.sceneDocumentId;
            level.scenePathHint = sceneHint;
            candidate.nodes.push_back(std::move(level));
            if (!ValidateFlowDocument(candidate, request.projectId, error))
            {
                return Failure(
                    StoryFlowLevelCreateCode::InvalidRequest,
                    "The new Level would make Story Flow invalid: " + error);
            }

            DocumentEnvelope sceneEnvelope = CreateDocumentEnvelope(
                request.projectId,
                SceneDocumentType,
                sceneHint,
                Gate4Generator);
            sceneEnvelope.documentId = result.sceneDocumentId;
            if (!ValidateDocumentEnvelope(sceneEnvelope, error))
            {
                return Failure(
                    StoryFlowLevelCreateCode::InvalidRequest,
                    "The new Level identity envelope is invalid: " + error);
            }

            const std::string renderToken = GenerateStableId();
            ScopedDirectory renderDirectory{
                projectRoot / "Intermediate" / "Transactions" /
                "StoryFlowLevelCreate" / fs::u8path(renderToken)
            };
            fs::create_directories(renderDirectory.path, pathError);
            if (pathError)
            {
                return Failure(
                    StoryFlowLevelCreateCode::RenderFailed,
                    "Could not create the Level transaction render directory: " +
                        pathError.message());
            }

            const fs::path renderedScene =
                renderDirectory.path / "Level.wiscene";
            const fs::path renderedSidecar =
                renderDirectory.path / "Level.wiscene.rmeta";
            const fs::path renderedFlow =
                renderDirectory.path / "Flow.renegade-flow";

            if (!RenderBlankWiscene(renderedScene, error))
            {
                return Failure(
                    StoryFlowLevelCreateCode::RenderFailed,
                    error);
            }
            if (!WriteDocumentEnvelope(
                    renderedSidecar.generic_u8string(),
                    sceneEnvelope,
                    error))
            {
                return Failure(
                    StoryFlowLevelCreateCode::RenderFailed,
                    "Could not render the new Level Scene identity: " + error);
            }
            if (!WriteFlowDocument(
                    renderedFlow.generic_u8string(),
                    candidate,
                    error))
            {
                return Failure(
                    StoryFlowLevelCreateCode::RenderFailed,
                    "Could not render the updated Story Flow document: " + error);
            }

            std::vector<std::uint8_t> sceneBytes;
            std::vector<std::uint8_t> sidecarBytes;
            std::vector<std::uint8_t> flowBytes;
            std::vector<std::uint8_t> previousFlowBytes;
            if (!ReadFileBytes(renderedScene, sceneBytes, error) ||
                !ReadFileBytes(renderedSidecar, sidecarBytes, error) ||
                !ReadFileBytes(renderedFlow, flowBytes, error) ||
                !ReadFileBytes(flowPath, previousFlowBytes, error))
            {
                return Failure(
                    StoryFlowLevelCreateCode::RenderFailed,
                    error);
            }

            std::vector<ProjectDocumentWrite> writes;
            writes.reserve(4);

            ProjectDocumentWrite flowBackup;
            flowBackup.destinationPath =
                flowPath.generic_u8string() + ".bak";
            flowBackup.content = previousFlowBytes;
            flowBackup.validator = [previousFlowBytes](
                const std::string& stagedPath,
                std::string& validationError)
            {
                return FileMatchesBytes(
                    stagedPath,
                    previousFlowBytes,
                    validationError);
            };
            writes.push_back(std::move(flowBackup));

            ProjectDocumentWrite flowWrite;
            flowWrite.destinationPath = flowPath.generic_u8string();
            flowWrite.content = flowBytes;
            flowWrite.validator = [
                expectedBytes = flowBytes,
                projectId = request.projectId,
                flowId = candidate.envelope.documentId,
                levelNodeId = result.levelNodeId,
                sceneId = result.sceneDocumentId,
                sceneHint](
                    const std::string& stagedPath,
                    std::string& validationError)
            {
                if (!FileMatchesBytes(
                        stagedPath,
                        expectedBytes,
                        validationError))
                {
                    return false;
                }
                FlowDocument roundTrip;
                if (!ReadFlowDocument(
                        stagedPath,
                        projectId,
                        roundTrip,
                        validationError))
                {
                    return false;
                }
                if (roundTrip.envelope.documentId != flowId)
                {
                    validationError =
                        "The staged Story Flow changed document identity.";
                    return false;
                }
                const FlowNode* node = FindNode(roundTrip, levelNodeId);
                if (!node || node->kind != FlowNodeKind::Level ||
                    node->sceneAssetId != sceneId ||
                    node->scenePathHint != sceneHint)
                {
                    validationError =
                        "The staged Story Flow lost the new Level reference.";
                    return false;
                }
                validationError.clear();
                return true;
            };
            writes.push_back(std::move(flowWrite));

            ProjectDocumentWrite sceneWrite;
            sceneWrite.destinationPath = scenePath.generic_u8string();
            sceneWrite.content = sceneBytes;
            sceneWrite.validator = [expectedBytes = sceneBytes](
                const std::string& stagedPath,
                std::string& validationError)
            {
                return FileMatchesBytes(
                           stagedPath,
                           expectedBytes,
                           validationError) &&
                    ValidateWisceneArchive(stagedPath, validationError);
            };
            writes.push_back(std::move(sceneWrite));

            ProjectDocumentWrite sidecarWrite;
            sidecarWrite.destinationPath = sidecarPath.generic_u8string();
            sidecarWrite.content = sidecarBytes;
            sidecarWrite.validator = [
                expectedBytes = sidecarBytes,
                projectId = request.projectId,
                sceneId = result.sceneDocumentId,
                sceneHint](
                    const std::string& stagedPath,
                    std::string& validationError)
            {
                if (!FileMatchesBytes(
                        stagedPath,
                        expectedBytes,
                        validationError))
                {
                    return false;
                }
                DocumentEnvelope envelope;
                if (!ReadDocumentEnvelope(
                        stagedPath,
                        envelope,
                        validationError))
                {
                    return false;
                }
                if (!SameSceneEnvelope(
                        envelope,
                        projectId,
                        sceneId,
                        sceneHint))
                {
                    validationError =
                        "The staged Scene identity envelope does not match the new Level.";
                    return false;
                }
                validationError.clear();
                return true;
            };
            writes.push_back(std::move(sidecarWrite));

            ProjectDocumentTransactionOptions transactionOptions;
            transactionOptions.transactionId = request.transactionId.empty()
                ? GenerateStableId()
                : request.transactionId;
            transactionOptions.journalDirectory =
                (projectRoot / "Intermediate" / "Transactions")
                    .generic_u8string();
            transactionOptions.allowedRoot = projectRoot.generic_u8string();
            transactionOptions.operationHook = request.transactionHook;

            ProjectDocumentTransaction transaction;
            result.transaction = transaction.Execute(
                std::move(writes),
                std::move(transactionOptions));
            if (!result.transaction.success)
            {
                result.code = StoryFlowLevelCreateCode::TransactionFailed;
                result.message =
                    "Add New Level transaction failed [" +
                    result.transaction.code + "]: " +
                    result.transaction.message;
                return result;
            }

            FlowDocument committed;
            if (!ReadFlowDocument(
                    flowPath.generic_u8string(),
                    request.projectId,
                    committed,
                    error))
            {
                result.code = StoryFlowLevelCreateCode::VerificationFailed;
                result.message =
                    "The Level transaction committed, but Story Flow verification failed: " +
                    error;
                return result;
            }
            const FlowNode* committedLevel =
                FindNode(committed, result.levelNodeId);
            if (!committedLevel ||
                committedLevel->sceneAssetId != result.sceneDocumentId ||
                committedLevel->scenePathHint != sceneHint)
            {
                result.code = StoryFlowLevelCreateCode::VerificationFailed;
                result.message =
                    "The Level transaction committed, but the final Story Flow reference did not verify.";
                return result;
            }

            DocumentEnvelope committedSceneEnvelope;
            if (!ReadDocumentEnvelope(
                    sidecarPath.generic_u8string(),
                    committedSceneEnvelope,
                    error) ||
                !SameSceneEnvelope(
                    committedSceneEnvelope,
                    request.projectId,
                    result.sceneDocumentId,
                    sceneHint))
            {
                result.code = StoryFlowLevelCreateCode::VerificationFailed;
                result.message =
                    "The Level transaction committed, but the final Scene identity did not verify: " +
                    error;
                return result;
            }

            const PreparedSceneOpen prepared =
                PrepareWickedSceneOpen(scenePath.generic_u8string());
            if (!prepared.IsReady())
            {
                result.code = StoryFlowLevelCreateCode::VerificationFailed;
                result.message =
                    "The Level transaction committed, but the final WISCENE did not verify: " +
                    prepared.Error();
                return result;
            }

            std::string resolvedScenePath;
            if (!ResolveSceneDocumentPath(
                    projectRoot.generic_u8string(),
                    request.projectId,
                    result.sceneDocumentId,
                    sceneHint,
                    resolvedScenePath,
                    error))
            {
                result.code = StoryFlowLevelCreateCode::VerificationFailed;
                result.message =
                    "The Level transaction committed, but stable Scene resolution failed: " +
                    error;
                return result;
            }

            result.committedFlow = std::move(committed);
            result.succeeded = true;
            result.code = StoryFlowLevelCreateCode::Success;
            result.message =
                "Created governed Level and committed its Story Flow relationship atomically.";
            return result;
        }
        catch (const std::exception& exception)
        {
            return Failure(
                StoryFlowLevelCreateCode::RenderFailed,
                std::string("Add New Level failed: ") + exception.what());
        }
        catch (...)
        {
            return Failure(
                StoryFlowLevelCreateCode::RenderFailed,
                "Add New Level failed with an unknown error.");
        }
    }
}
