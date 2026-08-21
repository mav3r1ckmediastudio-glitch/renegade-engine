#include "renegade/bridge/StoryFlowLevelReferenceService.h"

#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/SceneDocumentService.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    constexpr const char* Gate4BGenerator = "Renegade Story Flow Gate 4B";

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

    bool IsContainedRelative(const fs::path& value)
    {
        if (value.empty() || value.is_absolute() || value.has_root_name() ||
            value.has_root_directory())
        {
            return false;
        }
        return std::none_of(value.begin(), value.end(), [](const fs::path& part)
        {
            return part == "..";
        });
    }

    bool IsInsideRoot(const fs::path& root, const fs::path& candidate)
    {
        std::error_code rootError;
        std::error_code candidateError;
        const fs::path canonicalRoot = fs::weakly_canonical(root, rootError);
        const fs::path canonicalCandidate =
            fs::weakly_canonical(candidate, candidateError);
        if (rootError || candidateError)
            return false;
        const fs::path relative =
            canonicalCandidate.lexically_relative(canonicalRoot);
        return IsContainedRelative(relative);
    }

    bool ReadBytes(
        const fs::path& path,
        std::vector<std::uint8_t>& bytes,
        std::string& error)
    {
        bytes.clear();
        std::ifstream stream(path, std::ios::binary | std::ios::ate);
        if (!stream)
        {
            error = "Could not read project document: " + path.generic_u8string();
            return false;
        }
        const std::streamoff size = stream.tellg();
        if (size < 0)
        {
            error = "Could not inspect project document: " + path.generic_u8string();
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
                error = "Could not read complete project document: " +
                    path.generic_u8string();
                return false;
            }
        }
        error.clear();
        return true;
    }

    bool FileMatches(
        const std::string& path,
        const std::vector<std::uint8_t>& expected,
        std::string& error)
    {
        std::vector<std::uint8_t> actual;
        if (!ReadBytes(fs::u8path(path), actual, error))
            return false;
        if (actual != expected)
        {
            error = "The staged document bytes changed before commit.";
            return false;
        }
        error.clear();
        return true;
    }

    bool ResolveInsideProject(
        const fs::path& root,
        const std::string& value,
        fs::path& resolved,
        std::string& error)
    {
        if (value.empty())
        {
            error = "A project path is required.";
            return false;
        }
        std::error_code pathError;
        fs::path candidate = fs::u8path(value);
        if (!candidate.is_absolute()) candidate = root / candidate;
        resolved = fs::absolute(candidate, pathError).lexically_normal();
        if (pathError || resolved.filename().empty() || !IsInsideRoot(root, resolved))
        {
            error = "The selected path is not contained by the active project.";
            return false;
        }
        resolved = fs::weakly_canonical(resolved, pathError);
        if (pathError || resolved.filename().empty())
        {
            error = "The selected path could not be canonicalized inside the active project.";
            return false;
        }
        error.clear();
        return true;
    }

    const FlowNode* FindNode(const FlowDocument& flow, const StableId& id)
    {
        const auto found = std::find_if(flow.nodes.begin(), flow.nodes.end(),
            [&id](const FlowNode& node) { return node.id == id; });
        return found == flow.nodes.end() ? nullptr : &*found;
    }

    bool RenderEnvelope(
        const fs::path& temporary,
        const DocumentEnvelope& envelope,
        std::vector<std::uint8_t>& bytes,
        std::string& error)
    {
        std::error_code ignored;
        fs::remove(temporary, ignored);
        if (!WriteDocumentEnvelope(temporary.generic_u8string(), envelope, error))
            return false;
        const bool read = ReadBytes(temporary, bytes, error);
        fs::remove(temporary, ignored);
        return read;
    }

    bool RenderFlow(
        const fs::path& temporary,
        const FlowDocument& flow,
        std::vector<std::uint8_t>& bytes,
        std::string& error)
    {
        std::error_code ignored;
        fs::remove(temporary, ignored);
        if (!WriteFlowDocument(temporary.generic_u8string(), flow, error))
            return false;
        const bool read = ReadBytes(temporary, bytes, error);
        fs::remove(temporary, ignored);
        return read;
    }

    StoryFlowExistingLevelResult Failure(
        const StoryFlowLevelReferenceCode code,
        std::string message)
    {
        StoryFlowExistingLevelResult result;
        result.code = code;
        result.message = std::move(message);
        return result;
    }
}

namespace renegade::bridge
{
    const char* StoryFlowLevelReferenceCodeName(
        const StoryFlowLevelReferenceCode code) noexcept
    {
        switch (code)
        {
        case StoryFlowLevelReferenceCode::Success: return "success";
        case StoryFlowLevelReferenceCode::InvalidRequest: return "invalid_request";
        case StoryFlowLevelReferenceCode::Missing: return "missing";
        case StoryFlowLevelReferenceCode::Conflict: return "conflict";
        case StoryFlowLevelReferenceCode::WrongProject: return "wrong_project";
        case StoryFlowLevelReferenceCode::WrongDocumentType: return "wrong_document_type";
        case StoryFlowLevelReferenceCode::InvalidScene: return "invalid_scene";
        case StoryFlowLevelReferenceCode::TransactionFailed: return "transaction_failed";
        case StoryFlowLevelReferenceCode::VerificationFailed: return "verification_failed";
        default: return "unknown";
        }
    }

    StoryFlowExistingLevelResult StoryFlowLevelReferenceService::AddExistingLevel(
        const StoryFlowExistingLevelRequest& request) const
    {
        try
        {
            if (!IsValidStableId(request.projectId) || request.projectRoot.empty())
            {
                return Failure(StoryFlowLevelReferenceCode::InvalidRequest,
                    "Add Existing Level requires an active governed project.");
            }

            std::error_code pathError;
            const fs::path projectRoot =
                fs::weakly_canonical(fs::u8path(request.projectRoot), pathError);
            if (pathError || !fs::is_directory(projectRoot, pathError))
            {
                return Failure(StoryFlowLevelReferenceCode::InvalidRequest,
                    "The active project root could not be resolved.");
            }

            std::string error;
            if (!ValidateFlowDocument(request.flow, request.projectId, error))
            {
                return Failure(StoryFlowLevelReferenceCode::InvalidRequest,
                    "The Story Flow snapshot is invalid: " + error);
            }

            fs::path flowPath;
            if (!ResolveInsideProject(projectRoot, request.flowPath, flowPath, error) ||
                !fs::is_regular_file(flowPath, pathError) || pathError)
            {
                return Failure(StoryFlowLevelReferenceCode::InvalidRequest,
                    "The Story Flow document to update is unavailable.");
            }

            fs::path scenePath;
            if (!ResolveInsideProject(projectRoot, request.scenePath, scenePath, error))
            {
                return Failure(StoryFlowLevelReferenceCode::InvalidRequest, error);
            }
            if (LowerAscii(scenePath.extension().generic_u8string()) != ".wiscene")
            {
                return Failure(StoryFlowLevelReferenceCode::InvalidRequest,
                    "Add Existing Level only accepts .wiscene files.");
            }
            if (!fs::is_regular_file(scenePath, pathError) || pathError)
            {
                return Failure(StoryFlowLevelReferenceCode::Missing,
                    "The selected WISCENE file does not exist.");
            }

            auto prepared = PrepareWickedSceneOpen(scenePath.generic_u8string());
            if (!prepared.IsReady())
            {
                return Failure(StoryFlowLevelReferenceCode::InvalidScene,
                    "The selected WISCENE could not be validated: " +
                    prepared.Error());
            }

            const fs::path relativeScene = scenePath.lexically_relative(projectRoot);
            if (!IsContainedRelative(relativeScene))
            {
                return Failure(StoryFlowLevelReferenceCode::InvalidRequest,
                    "The selected WISCENE is outside the active project.");
            }
            const std::string sceneHint = relativeScene.generic_u8string();

            FlowDocument onDiskFlow;
            if (!ReadFlowDocument(flowPath.generic_u8string(), request.projectId,
                    onDiskFlow, error) ||
                onDiskFlow.envelope.documentId != request.flow.envelope.documentId)
            {
                return Failure(StoryFlowLevelReferenceCode::Conflict,
                    "The Story Flow on disk no longer matches the authoring session.");
            }

            fs::path sidecarPath = scenePath;
            sidecarPath += ".rmeta";
            pathError.clear();
            const fs::file_status sidecarStatus = fs::status(sidecarPath, pathError);
            bool sidecarExists = false;
            if (pathError)
            {
                if (pathError == std::errc::no_such_file_or_directory)
                {
                    pathError.clear();
                }
                else
                {
                    return Failure(StoryFlowLevelReferenceCode::Conflict,
                        "The Scene identity sidecar could not be inspected.");
                }
            }
            else if (fs::exists(sidecarStatus))
            {
                if (!fs::is_regular_file(sidecarStatus))
                {
                    return Failure(StoryFlowLevelReferenceCode::Conflict,
                        "The Scene identity sidecar is not a regular file.");
                }
                sidecarExists = true;
            }

            DocumentEnvelope sceneEnvelope;
            bool createdMetadata = false;
            if (sidecarExists)
            {
                if (!ReadDocumentEnvelope(sidecarPath.generic_u8string(),
                        sceneEnvelope, error))
                {
                    return Failure(StoryFlowLevelReferenceCode::Conflict,
                        "The existing Scene identity sidecar is invalid: " + error);
                }
                if (sceneEnvelope.projectId != request.projectId)
                {
                    return Failure(StoryFlowLevelReferenceCode::WrongProject,
                        "The selected Scene belongs to a different Renegade project.");
                }
                if (sceneEnvelope.documentType != SceneDocumentType)
                {
                    return Failure(StoryFlowLevelReferenceCode::WrongDocumentType,
                        "The selected WISCENE sidecar is not a Scene document identity.");
                }
                if (sceneEnvelope.pathHint != sceneHint &&
                    !RetargetDocumentEnvelope(sceneEnvelope, sceneHint, error))
                {
                    return Failure(StoryFlowLevelReferenceCode::Conflict,
                        "The Scene identity path hint could not be repaired: " + error);
                }
            }
            else
            {
                sceneEnvelope = CreateDocumentEnvelope(request.projectId,
                    SceneDocumentType, sceneHint, Gate4BGenerator);
                createdMetadata = true;
            }

            if (!IsValidStableId(sceneEnvelope.documentId))
            {
                return Failure(StoryFlowLevelReferenceCode::Conflict,
                    "The selected Scene is missing a valid stable document identity.");
            }
            for (const auto& node : request.flow.nodes)
            {
                if (node.kind == FlowNodeKind::Level &&
                    node.sceneAssetId == sceneEnvelope.documentId)
                {
                    return Failure(StoryFlowLevelReferenceCode::Conflict,
                        "This governed Scene is already present in Story Flow as '" +
                        node.name + "'.");
                }
            }

            const std::string levelName = Trim(request.levelName);
            if (levelName.empty())
            {
                return Failure(StoryFlowLevelReferenceCode::InvalidRequest,
                    "Add Existing Level requires a visible Level name.");
            }

            StoryFlowExistingLevelResult result;
            result.levelNodeId = GenerateStableId();
            result.sceneDocumentId = sceneEnvelope.documentId;
            result.scenePath = scenePath.generic_u8string();
            result.scenePathHint = sceneHint;
            result.createdMetadata = createdMetadata;

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
                return Failure(StoryFlowLevelReferenceCode::InvalidRequest,
                    "The adopted Level would make Story Flow invalid: " + error);
            }

            // Render-only payloads are scratch data, not project mutations.
            // Keep them in the OS temp root so nested transaction artifact
            // names cannot amplify a deep project path past Windows limits.
            // The final Flow/sidecar writes remain governed below.
            pathError.clear();
            const fs::path temporaryRoot = fs::temp_directory_path(pathError);
            if (pathError)
            {
                return Failure(StoryFlowLevelReferenceCode::TransactionFailed,
                    "Could not resolve the Level adoption render root.");
            }
            const fs::path renderRoot = temporaryRoot / "renegade-story-flow" /
                "level-adopt" / fs::u8path(GenerateStableId());
            fs::create_directories(renderRoot, pathError);
            if (pathError)
            {
                return Failure(StoryFlowLevelReferenceCode::TransactionFailed,
                    "Could not prepare the Level adoption transaction.");
            }
            const fs::path renderedMeta = renderRoot / "Level.wiscene.rmeta";
            const fs::path renderedFlow = renderRoot / "Flow.renegade-flow";

            std::vector<std::uint8_t> metaBytes;
            std::vector<std::uint8_t> flowBytes;
            std::vector<std::uint8_t> previousFlowBytes;
            const bool rendered =
                RenderEnvelope(renderedMeta, sceneEnvelope, metaBytes, error) &&
                RenderFlow(renderedFlow, candidate, flowBytes, error) &&
                ReadBytes(flowPath, previousFlowBytes, error);
            std::error_code ignored;
            fs::remove_all(renderRoot, ignored);
            if (!rendered)
            {
                return Failure(StoryFlowLevelReferenceCode::TransactionFailed,
                    "Could not prepare Level adoption payloads: " + error);
            }

            std::vector<ProjectDocumentWrite> writes;
            ProjectDocumentWrite backup;
            backup.destinationPath = flowPath.generic_u8string() + ".bak";
            backup.content = previousFlowBytes;
            backup.validator = [expected = previousFlowBytes](
                const std::string& path, std::string& validationError)
            {
                return FileMatches(path, expected, validationError);
            };
            writes.push_back(std::move(backup));

            ProjectDocumentWrite flowWrite;
            flowWrite.destinationPath = flowPath.generic_u8string();
            flowWrite.content = flowBytes;
            flowWrite.validator = [expected = flowBytes,
                projectId = request.projectId,
                flowId = candidate.envelope.documentId,
                nodeId = result.levelNodeId,
                sceneId = result.sceneDocumentId](
                const std::string& path, std::string& validationError)
            {
                if (!FileMatches(path, expected, validationError)) return false;
                FlowDocument roundTrip;
                if (!ReadFlowDocument(path, projectId, roundTrip, validationError) ||
                    roundTrip.envelope.documentId != flowId)
                    return false;
                const FlowNode* node = FindNode(roundTrip, nodeId);
                if (!node || node->kind != FlowNodeKind::Level ||
                    node->sceneAssetId != sceneId)
                {
                    validationError = "The staged Flow lost the adopted Level reference.";
                    return false;
                }
                validationError.clear();
                return true;
            };
            writes.push_back(std::move(flowWrite));

            ProjectDocumentWrite metaWrite;
            metaWrite.destinationPath = sidecarPath.generic_u8string();
            metaWrite.content = metaBytes;
            metaWrite.validator = [expected = metaBytes,
                projectId = request.projectId,
                sceneId = result.sceneDocumentId,
                sceneHint](const std::string& path, std::string& validationError)
            {
                if (!FileMatches(path, expected, validationError)) return false;
                DocumentEnvelope envelope;
                if (!ReadDocumentEnvelope(path, envelope, validationError)) return false;
                if (envelope.projectId != projectId ||
                    envelope.documentId != sceneId ||
                    envelope.documentType != SceneDocumentType ||
                    envelope.pathHint != sceneHint)
                {
                    validationError = "The staged Scene identity does not match the adopted Level.";
                    return false;
                }
                validationError.clear();
                return true;
            };
            writes.push_back(std::move(metaWrite));

            ProjectDocumentTransactionOptions options;
            options.transactionId = request.transactionId.empty()
                ? GenerateStableId() : request.transactionId;
            options.journalDirectory =
                (projectRoot / "Intermediate" / "Transactions").generic_u8string();
            options.allowedRoot = projectRoot.generic_u8string();
            options.operationHook = request.transactionHook;

            ProjectDocumentTransaction transaction;
            result.transaction = transaction.Execute(std::move(writes), options);
            if (!result.transaction.success)
            {
                result.code = StoryFlowLevelReferenceCode::TransactionFailed;
                result.message = "Add Existing Level transaction failed [" +
                    result.transaction.code + "]: " + result.transaction.message;
                return result;
            }

            FlowDocument committed;
            DocumentEnvelope committedMeta;
            if (!ReadFlowDocument(flowPath.generic_u8string(), request.projectId,
                    committed, error) ||
                !ReadDocumentEnvelope(sidecarPath.generic_u8string(),
                    committedMeta, error))
            {
                result.code = StoryFlowLevelReferenceCode::VerificationFailed;
                result.message = "Level adoption committed but final verification failed: " + error;
                return result;
            }
            const FlowNode* committedNode = FindNode(committed, result.levelNodeId);
            if (!committedNode || committedNode->sceneAssetId != result.sceneDocumentId ||
                committedMeta.documentId != result.sceneDocumentId ||
                committedMeta.projectId != request.projectId ||
                committedMeta.documentType != SceneDocumentType)
            {
                result.code = StoryFlowLevelReferenceCode::VerificationFailed;
                result.message = "Level adoption committed but stable identity verification failed.";
                return result;
            }

            result.succeeded = true;
            result.code = StoryFlowLevelReferenceCode::Success;
            result.message = createdMetadata
                ? "Existing WISCENE adopted and governed by a new Scene identity."
                : "Existing governed Scene added to Story Flow.";
            result.committedFlow = std::move(committed);
            return result;
        }
        catch (const std::exception& exception)
        {
            return Failure(StoryFlowLevelReferenceCode::InvalidRequest,
                std::string("Add Existing Level failed: ") + exception.what());
        }
        catch (...)
        {
            return Failure(StoryFlowLevelReferenceCode::InvalidRequest,
                "Add Existing Level failed unexpectedly.");
        }
    }

    StoryFlowLevelResolution StoryFlowLevelReferenceService::ResolveLevel(
        const std::string& projectRoot,
        const StableId& projectId,
        const FlowDocument& flow,
        const StableId& levelNodeId) const
    {
        StoryFlowLevelResolution result;
        result.levelNodeId = levelNodeId;

        if (!IsValidStableId(projectId) || projectRoot.empty() ||
            !IsValidStableId(levelNodeId))
        {
            result.message = "Level resolution requires an active project and stable Level node ID.";
            return result;
        }

        std::string validationError;
        if (!ValidateFlowDocument(flow, projectId, validationError))
        {
            result.message = "Story Flow is invalid: " + validationError;
            return result;
        }

        const FlowNode* level = FindNode(flow, levelNodeId);
        if (!level || level->kind != FlowNodeKind::Level)
        {
            result.message = "The selected Story Flow node is not a Level.";
            return result;
        }

        result.sceneDocumentId = level->sceneAssetId;
        result.requestedPathHint = level->scenePathHint;
        std::string resolved;
        std::string error;
        if (!ResolveSceneDocumentPath(projectRoot, projectId,
                level->sceneAssetId, level->scenePathHint, resolved, error))
        {
            result.code = StoryFlowLevelReferenceCode::Missing;
            if (error.find("different project") != std::string::npos)
                result.code = StoryFlowLevelReferenceCode::WrongProject;
            else if (error.find("document type") != std::string::npos ||
                     error.find("Scene") != std::string::npos &&
                     error.find("type") != std::string::npos)
                result.code = StoryFlowLevelReferenceCode::WrongDocumentType;
            result.message = error;
            return result;
        }

        std::error_code rootError;
        std::error_code pathError;
        const fs::path root =
            fs::weakly_canonical(fs::u8path(projectRoot), rootError);
        const fs::path path =
            fs::weakly_canonical(fs::u8path(resolved), pathError);
        if (rootError || pathError || !IsInsideRoot(root, path))
        {
            result.message = "Resolved Level escaped the active project root.";
            return result;
        }

        result.resolvedPath = path.generic_u8string();
        result.resolvedPathHint = path.lexically_relative(root).generic_u8string();
        result.pathHintMoved =
            fs::u8path(result.requestedPathHint).lexically_normal() !=
            fs::u8path(result.resolvedPathHint).lexically_normal();
        result.succeeded = true;
        result.code = StoryFlowLevelReferenceCode::Success;
        result.message = result.pathHintMoved
            ? "Level resolved by stable Scene identity after a move; the stored path hint is stale."
            : "Level resolved by stable Scene identity.";
        return result;
    }
}
