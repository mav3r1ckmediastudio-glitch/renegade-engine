#pragma once

#include "renegade/bridge/FlowService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/StoryFlowLevelReferenceService.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace renegade::bridge
{
    struct StoryFlowProjectHomeResult
    {
        bool succeeded = false;
        bool createdFlow = false;
        bool adoptedStartupScene = false;
        bool updatedProjectDescriptor = false;
        StableId flowDocumentId;
        std::string flowPathHint;
        std::string flowPath;
        std::string message;
    };

    // Project-home invariant for Renegade Studio.
    //
    // Every governed project must have a usable Story Flow before creator
    // workspace routing is decided. Legacy/scene-first projects are migrated by
    // creating a canonical Story Flow and adopting their existing startup scene
    // as the first Level. The project descriptor is then updated transactionally.
    // Existing valid Story Flow metadata is never rewritten.
    class StoryFlowProjectHomeService final
    {
    public:
        [[nodiscard]] StoryFlowProjectHomeResult Ensure(
            const ProjectMetadata& project) const
        {
            namespace fs = std::filesystem;
            StoryFlowProjectHomeResult result;

            if (!IsValidStableId(project.projectId) || project.rootPath.empty() ||
                project.descriptorPath.empty())
            {
                result.message = "Project home requires a governed active project.";
                return result;
            }

            const bool hasFlowId = IsValidStableId(project.startupFlowId);
            const bool hasFlowHint = !project.startupFlow.empty();
            if (hasFlowId && hasFlowHint)
            {
                std::string resolved;
                std::string error;
                FlowDocument flow;
                if (ResolveStoryFlowDocumentPath(
                        project.rootPath, project.projectId,
                        project.startupFlowId, project.startupFlow,
                        resolved, error) &&
                    ReadFlowDocument(resolved, project.projectId, flow, error))
                {
                    result.succeeded = true;
                    result.flowDocumentId = flow.envelope.documentId;
                    result.flowPathHint = project.startupFlow;
                    result.flowPath = std::move(resolved);
                    result.message = "Existing Story Flow project home is valid.";
                    return result;
                }

                result.message = "Configured startup Story Flow is invalid: " + error;
                return result;
            }
            if (hasFlowId != hasFlowHint)
            {
                result.message = "Project descriptor contains incomplete startup Story Flow metadata.";
                return result;
            }

            try
            {
                const fs::path root = fs::weakly_canonical(fs::u8path(project.rootPath));
                if (!fs::is_directory(root))
                {
                    result.message = "Project root is unavailable.";
                    return result;
                }

                constexpr const char* CanonicalFlowHint =
                    "Content/StoryFlow/Main.renegade-flow";
                const fs::path flowPath = root / fs::u8path(CanonicalFlowHint);
                std::error_code pathError;
                fs::create_directories(flowPath.parent_path(), pathError);
                if (pathError)
                {
                    result.message = "Could not create the Story Flow project-home folder: " +
                        pathError.message();
                    return result;
                }

                FlowDocument flow;
                std::string error;
                if (fs::is_regular_file(flowPath, pathError) && !pathError)
                {
                    if (!ReadFlowDocument(
                            flowPath.generic_u8string(), project.projectId,
                            flow, error))
                    {
                        result.message =
                            "A canonical Story Flow file already exists but is invalid: " + error;
                        return result;
                    }
                }
                else
                {
                    flow.envelope = CreateDocumentEnvelope(
                        project.projectId,
                        StoryFlowDocumentType,
                        CanonicalFlowHint,
                        "renegade-story-flow-project-home-v1");

                    FlowNode start;
                    start.id = GenerateStableId();
                    start.kind = FlowNodeKind::GameStart;
                    start.name = "Game Start";
                    flow.startNodeId = start.id;
                    flow.nodes.push_back(std::move(start));

                    if (!WriteFlowDocument(
                            flowPath.generic_u8string(), flow, error))
                    {
                        result.message = "Could not create the project Story Flow: " + error;
                        return result;
                    }
                    result.createdFlow = true;
                }

                const fs::path startupScene =
                    root / fs::u8path(project.startupScene);
                if (!project.startupScene.empty() &&
                    fs::is_regular_file(startupScene, pathError) && !pathError)
                {
                    const bool alreadyPresent = std::any_of(
                        flow.nodes.begin(), flow.nodes.end(),
                        [&project](const FlowNode& node)
                        {
                            return node.kind == FlowNodeKind::Level &&
                                node.scenePathHint == project.startupScene;
                        });

                    if (!alreadyPresent)
                    {
                        StoryFlowExistingLevelRequest adopt;
                        adopt.projectRoot = project.rootPath;
                        adopt.projectId = project.projectId;
                        adopt.flowPath = flowPath.generic_u8string();
                        adopt.flow = flow;
                        adopt.levelName = "Main Level";
                        adopt.scenePath = startupScene.generic_u8string();
                        adopt.transactionId =
                            "story-flow-project-home-adopt-" + GenerateStableId();

                        StoryFlowLevelReferenceService levels;
                        const auto adopted = levels.AddExistingLevel(adopt);
                        if (!adopted.succeeded)
                        {
                            result.message =
                                "Could not adopt the project's startup scene into Story Flow: " +
                                adopted.message;
                            return result;
                        }
                        flow = adopted.committedFlow;
                        result.adoptedStartupScene = true;

                        const bool startAlreadyRouted = std::any_of(
                            flow.routes.begin(), flow.routes.end(),
                            [&flow](const FlowRoute& route)
                            {
                                return route.sourceNodeId == flow.startNodeId &&
                                    route.outcome == GameStartOutcome;
                            });
                        if (!startAlreadyRouted)
                        {
                            FlowRoute route;
                            route.id = GenerateStableId();
                            route.sourceNodeId = flow.startNodeId;
                            route.outcome = GameStartOutcome;
                            route.destinationNodeId = adopted.levelNodeId;
                            route.destinationEntry = "default";
                            flow.routes.push_back(std::move(route));
                            if (!WriteFlowDocument(
                                    flowPath.generic_u8string(), flow, error))
                            {
                                result.message =
                                    "Startup Level was adopted but the Game Start route could not be committed: " +
                                    error;
                                return result;
                            }
                        }
                    }
                }

                std::ifstream input(
                    fs::u8path(project.descriptorPath),
                    std::ios::binary);
                if (!input)
                {
                    result.message = "Could not read the project descriptor for Story Flow migration.";
                    return result;
                }
                std::ostringstream descriptorStream;
                descriptorStream << input.rdbuf();
                std::string descriptor = descriptorStream.str();

                const auto setKey = [](std::string& text,
                    const std::string& key,
                    const std::string& value)
                {
                    const std::string prefix = key + " = ";
                    const std::size_t found = text.find(prefix);
                    if (found != std::string::npos)
                    {
                        const std::size_t end = text.find('\n', found);
                        text.replace(
                            found,
                            end == std::string::npos ? text.size() - found : end - found,
                            prefix + value);
                        return;
                    }

                    const std::string anchor = "startup_scene = ";
                    const std::size_t scene = text.find(anchor);
                    if (scene != std::string::npos)
                    {
                        const std::size_t end = text.find('\n', scene);
                        const std::size_t insertion =
                            end == std::string::npos ? text.size() : end + 1;
                        text.insert(insertion, prefix + value + "\n");
                        return;
                    }
                    text += "\n" + prefix + value + "\n";
                };

                setKey(descriptor, "startup_flow_id", flow.envelope.documentId);
                setKey(descriptor, "startup_flow", CanonicalFlowHint);

                ProjectDocumentWrite descriptorWrite;
                descriptorWrite.destinationPath = project.descriptorPath;
                descriptorWrite.content.assign(descriptor.begin(), descriptor.end());
                descriptorWrite.validator =
                    [expectedId = flow.envelope.documentId](
                        const std::string& stagedPath,
                        std::string& validationError)
                    {
                        std::ifstream staged(fs::u8path(stagedPath), std::ios::binary);
                        if (!staged)
                        {
                            validationError = "Could not reopen staged project descriptor.";
                            return false;
                        }
                        std::ostringstream stream;
                        stream << staged.rdbuf();
                        const std::string text = stream.str();
                        if (text.find("startup_flow_id = " + expectedId) == std::string::npos ||
                            text.find("startup_flow = Content/StoryFlow/Main.renegade-flow") ==
                                std::string::npos)
                        {
                            validationError =
                                "Staged project descriptor did not retain Story Flow project-home metadata.";
                            return false;
                        }
                        validationError.clear();
                        return true;
                    };

                ProjectDocumentTransactionOptions options;
                options.transactionId =
                    "story-flow-project-home-descriptor-" + GenerateStableId();
                options.allowedRoot = root.generic_u8string();
                options.journalDirectory =
                    (root / "Intermediate" / "Transactions").generic_u8string();

                ProjectDocumentTransaction transaction;
                auto committed = transaction.Execute(
                    {std::move(descriptorWrite)}, std::move(options));
                if (!committed.success || !committed.committed)
                {
                    result.message =
                        "Could not bind Story Flow as the project home: " + committed.message;
                    return result;
                }

                result.succeeded = true;
                result.updatedProjectDescriptor = true;
                result.flowDocumentId = flow.envelope.documentId;
                result.flowPathHint = CanonicalFlowHint;
                result.flowPath = flowPath.generic_u8string();
                result.message = result.adoptedStartupScene
                    ? "Story Flow project home created and startup Level adopted."
                    : "Story Flow project home created.";
                return result;
            }
            catch (const std::exception& exception)
            {
                result.message =
                    std::string("Story Flow project-home migration failed: ") +
                    exception.what();
                return result;
            }
        }
    };
}
