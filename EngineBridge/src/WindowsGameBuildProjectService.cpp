#include "renegade/bridge/WindowsGameBuildProjectService.h"

#include "renegade/bridge/FlowService.h"

#include <filesystem>
#include <sstream>
#include <utility>

namespace renegade::bridge
{
    namespace
    {
        namespace fs = std::filesystem;

        std::string TraceLine(const FlowStepResult& step)
        {
            if (step.previousNodeId.empty())
            {
                return "ENTER " + step.currentNodeName;
            }

            std::string line = step.previousNodeId + " --" + step.outcome +
                "--> " + step.currentNodeId + " [" + step.currentNodeName + ']';
            if (!step.destinationEntry.empty())
            {
                line += " entry=" + step.destinationEntry;
            }
            return line;
        }

        bool CollectCurrentDependencies(
            const ProjectMetadata& project,
            DependencyGraph& graph,
            std::string& error)
        {
            if (project.rootPath.empty() || project.descriptorPath.empty() ||
                project.projectId.empty())
            {
                error = "Build Windows Game requires a complete active project.";
                return false;
            }

            const fs::path descriptor = fs::u8path(project.descriptorPath);
            const std::string rootDocument = descriptor.filename().generic_u8string();
            if (rootDocument.empty())
            {
                error = "Build Windows Game could not resolve the project descriptor root.";
                return false;
            }

            ProjectDependencyProvider projectProvider(MakeProjectDependencyReader());
            StoryFlowDependencyProvider flowProvider(
                MakeStoryFlowDependencyReader(project.projectId));
            RuntimeScreenDependencyProvider screenProvider(
                MakeRuntimeScreenDependencyReader(project.projectId));
            WisceneDependencyProvider sceneProvider(MakeWisceneDependencyReader());
            GltfDependencyProvider gltfProvider(MakeGltfDependencyReader());

            DependencyCollector collector(project.rootPath);
            if (!collector.RegisterProvider(projectProvider, error) ||
                !collector.RegisterProvider(flowProvider, error) ||
                !collector.RegisterProvider(screenProvider, error) ||
                !collector.RegisterProvider(sceneProvider, error) ||
                !collector.RegisterProvider(gltfProvider, error))
            {
                error = "Build Windows Game could not configure dependency discovery: " +
                    error;
                return false;
            }

            DependencyRoot root;
            root.declaredPath = rootDocument;
            root.dependencyClass = DependencyClass::ProjectDocument;
            root.requirement = DependencyRequirement::Required;
            root.provenance = "project:active-descriptor";
            if (!collector.AddRoot(root, error) ||
                !collector.DiscoverRootDependencies(error) ||
                !collector.DiscoverTransitiveDependencies(error))
            {
                error = "Build Windows Game dependency discovery failed: " + error;
                return false;
            }

            graph = collector.Graph();
            error.clear();
            return true;
        }

        bool RefreshCurrentRegistry(
            const ProjectMetadata& project,
            const DependencyGraph& graph,
            AssetRegistry& registry,
            std::string& error)
        {
            std::string registryPath;
            if (!ResolveAssetRegistryDocumentPath(
                    project.rootPath, registryPath, error))
            {
                error = "Build Windows Game could not resolve the LC01 registry: " +
                    error;
                return false;
            }

            AssetRegistry existing;
            const AssetRegistry* existingRegistry = nullptr;
            std::error_code ec;
            const fs::path path = fs::u8path(registryPath);
            if (fs::exists(path, ec))
            {
                if (ec || !fs::is_regular_file(path, ec) || ec)
                {
                    error = "Build Windows Game found an invalid LC01 registry path.";
                    return false;
                }
                if (!ReadAssetRegistry(
                        project.rootPath,
                        project.projectId,
                        existing,
                        error))
                {
                    error = "Build Windows Game rejected the existing LC01 registry: " +
                        error;
                    return false;
                }
                existingRegistry = &existing;
            }
            else if (ec)
            {
                error = "Build Windows Game could not inspect the LC01 registry path.";
                return false;
            }

            AssetRegistryRefresh refresh;
            if (!RefreshAssetRegistry(
                    project.projectId,
                    graph,
                    existingRegistry,
                    refresh,
                    error))
            {
                error = "Build Windows Game could not refresh stable asset identity: " +
                    error;
                return false;
            }

            const ProjectDocumentTransactionResult write =
                WriteAssetRegistry(project.rootPath, refresh.registry);
            if (!write.success)
            {
                error = "Build Windows Game could not persist the LC01 registry: " +
                    (write.message.empty() ? write.code : write.message);
                return false;
            }

            registry = std::move(refresh.registry);
            error.clear();
            return true;
        }

        bool ComputeTestAllTrace(
            const ProjectMetadata& project,
            std::vector<std::string>& trace,
            std::size_t& levelCompletionCount,
            std::string& error)
        {
            trace.clear();
            levelCompletionCount = 0;
            if (project.startupFlowId.empty() || project.startupFlow.empty())
            {
                error = "Build Windows Game requires a startup Story Flow.";
                return false;
            }
            if (project.startupScreenId.empty() || project.startupScreen.empty())
            {
                error = "Build Windows Game requires a startup Runtime Screen for the LP06 standalone smoke.";
                return false;
            }

            std::string flowPath;
            if (!ResolveStoryFlowDocumentPath(
                    project.rootPath,
                    project.projectId,
                    project.startupFlowId,
                    project.startupFlow,
                    flowPath,
                    error))
            {
                error = "Build Windows Game could not resolve the startup Story Flow: " +
                    error;
                return false;
            }

            FlowDocument document;
            if (!ReadFlowDocument(
                    flowPath, project.projectId, document, error))
            {
                error = "Build Windows Game could not read the startup Story Flow: " +
                    error;
                return false;
            }

            FlowInterpreter interpreter;
            if (!interpreter.Initialize(std::move(document), error))
            {
                error = "Build Windows Game rejected the startup Story Flow: " + error;
                return false;
            }

            FlowStepResult step = interpreter.Start();
            if (!step.succeeded)
            {
                error = "Build Windows Game could not start Test All flow: " +
                    step.message;
                return false;
            }
            trace.push_back(TraceLine(step));

            step = interpreter.EmitOutcome(GameStartOutcome);
            if (!step.succeeded)
            {
                error = "Build Windows Game could not enter the first Test All level: " +
                    step.message;
                return false;
            }
            trace.push_back(TraceLine(step));

            constexpr std::size_t MaximumCompletionSteps = 128;
            while (step.terminalAction == FlowTerminalAction::None)
            {
                if (step.currentNodeKind != FlowNodeKind::Level)
                {
                    error = "Build Windows Game Test All route reached a non-Level node before Complete Game.";
                    return false;
                }
                if (levelCompletionCount >= MaximumCompletionSteps)
                {
                    error = "Build Windows Game Test All route exceeded the bounded completion-step limit.";
                    return false;
                }

                step = interpreter.EmitOutcome("level.complete");
                if (!step.succeeded)
                {
                    error = "Build Windows Game currently requires deterministic level.complete progression for the LP06 smoke: " +
                        step.message;
                    return false;
                }
                ++levelCompletionCount;
                trace.push_back(TraceLine(step));
            }

            if (step.terminalAction != FlowTerminalAction::CompleteGame)
            {
                error = "Build Windows Game Test All route must terminate at Complete Game for the LP06 smoke.";
                return false;
            }
            if (trace.empty())
            {
                error = "Build Windows Game produced no Test All trace.";
                return false;
            }

            error.clear();
            return true;
        }
    }

    bool PrepareWindowsGameBuildProjectState(
        const ProjectMetadata& project,
        WindowsGameBuildProjectState& state,
        std::string& error)
    {
        state = {};
        error.clear();

        if (!CollectCurrentDependencies(project, state.dependencyGraph, error))
            return false;
        if (!RefreshCurrentRegistry(
                project, state.dependencyGraph, state.assetRegistry, error))
            return false;
        if (!ComputeTestAllTrace(
                project,
                state.expectedFlowTrace,
                state.levelCompletionCount,
                error))
            return false;

        error.clear();
        return true;
    }
}
