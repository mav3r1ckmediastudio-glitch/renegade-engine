#include "renegade/bridge/WindowsGameBuildProjectService.h"

#include "renegade/bridge/FlowService.h"
#include "renegade/bridge/ResourceAssetDependencyService.h"
#include "renegade/bridge/ReusableAssetDependencyService.h"

#include <algorithm>
#include <deque>
#include <filesystem>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace renegade::bridge
{
    namespace
    {
        namespace fs = std::filesystem;

        // LP06 owner builds package only the LP05-reachable project closure,
        // but Runtime resolves Story Flow Level scenes by their stable scene
        // document IDs. Those IDs live in the Renegade-owned <scene>.rmeta
        // sidecars rather than inside the Wicked WISCENE bytes. Keep this
        // packaging prerequisite local to LP06 so the accepted LP05 extractor
        // and its canonical evidence remain unchanged.
        class SceneIdentityCompanionProvider final : public IDependencyProvider
        {
        public:
            [[nodiscard]] const char* Name() const noexcept override
            {
                return "lp06-scene-identity-companion";
            }

            [[nodiscard]] std::uint32_t Version() const noexcept override
            {
                return 1;
            }

            [[nodiscard]] bool Supports(
                const DependencyClass dependencyClass) const noexcept override
            {
                return dependencyClass == DependencyClass::Scene;
            }

            [[nodiscard]] bool Discover(
                const DependencyProviderContext& context,
                const DependencyCandidateSink& emit,
                const DependencyDiagnosticSink&,
                std::string& error) const override
            {
                if (context.source == nullptr ||
                    context.source->projectRelativePath.empty())
                {
                    error =
                        "LP06 scene identity companion provider requires a scene source node.";
                    return false;
                }

                DependencyCandidate companion;
                companion.declaredPath =
                    context.source->projectRelativePath + ".rmeta";
                companion.dependencyClass = DependencyClass::GeneratedData;
                companion.requirement = DependencyRequirement::Required;
                companion.provenance = "lp06.scene_identity_companion";
                emit(companion);
                error.clear();
                return true;
            }
        };

        const AssetRecord* FindAssetRecord(
            const AssetRegistry& registry,
            const StableId& assetId)
        {
            const auto found = std::find_if(
                registry.records.begin(), registry.records.end(),
                [&assetId](const AssetRecord& record)
                {
                    return record.assetId == assetId;
                });
            return found == registry.records.end() ? nullptr : &*found;
        }

        const MissingAssetRecord* FindMissingAssetRecord(
            const AssetRegistry& registry,
            const StableId& assetId)
        {
            const auto found = std::find_if(
                registry.missingAssets.begin(), registry.missingAssets.end(),
                [&assetId](const MissingAssetRecord& record)
                {
                    return record.assetId == assetId;
                });
            return found == registry.missingAssets.end() ? nullptr : &*found;
        }

        bool GraphContainsPath(
            const DependencyGraph& graph,
            const std::string& projectRelativePath)
        {
            return std::any_of(
                graph.nodes.begin(), graph.nodes.end(),
                [&projectRelativePath](const DependencyNode& node)
                {
                    return node.projectRelativePath == projectRelativePath;
                });
        }

        // An imported source is provenance authority, not Runtime content. A
        // normal LP05 closure therefore (correctly) does not reach the retained
        // SourceAssets input. LC01 freshness still needs the source's current
        // hash, otherwise RefreshAssetRegistry would tombstone it and every
        // reachable .rasset would look source-unavailable to BuildService.
        //
        // Add only the source records for imported products that the already-
        // discovered Runtime graph actually reaches. Do it after transitive
        // discovery has finished: the editor-only source node is hashed and
        // retained by LC01, but cannot become a Runtime package dependency.
        bool AddImportedSourceFreshnessRoots(
            const ProjectMetadata& project,
            DependencyCollector& collector,
            std::string& error)
        {
            std::string registryPath;
            if (!ResolveAssetRegistryDocumentPath(
                    project.rootPath, registryPath, error))
            {
                error =
                    "Build Windows Game could not resolve LC01 while preparing imported-source freshness: " +
                    error;
                return false;
            }

            std::error_code ec;
            const fs::path path = fs::u8path(registryPath);
            if (!fs::exists(path, ec))
            {
                if (ec)
                {
                    error =
                        "Build Windows Game could not inspect LC01 while preparing imported-source freshness.";
                    return false;
                }
                error.clear();
                return true;
            }
            if (!fs::is_regular_file(path, ec) || ec)
            {
                error =
                    "Build Windows Game found an invalid LC01 path while preparing imported-source freshness.";
                return false;
            }

            AssetRegistry registry;
            if (!ReadAssetRegistry(
                    project.rootPath, project.projectId, registry, error))
            {
                error =
                    "Build Windows Game rejected LC01 while preparing imported-source freshness: " +
                    error;
                return false;
            }

            const DependencyGraph runtimeGraph = collector.Graph();
            for (const ImportedProductRecord& imported : registry.importedProducts)
            {
                const AssetRecord* product =
                    FindAssetRecord(registry, imported.productAssetId);
                if (product == nullptr ||
                    !GraphContainsPath(runtimeGraph, product->projectRelativePath))
                {
                    continue;
                }

                std::string sourcePath;
                DependencyClass sourceClass = DependencyClass::ImportedContent;
                DependencyRequirement sourceRequirement =
                    DependencyRequirement::EditorOnly;
                std::string sourceProvider;
                std::uint32_t sourceProviderVersion = 0;

                if (const AssetRecord* source =
                        FindAssetRecord(registry, imported.sourceAssetId))
                {
                    sourcePath = source->projectRelativePath;
                    sourceClass = source->dependencyClass;
                    sourceRequirement = source->requirement;
                    sourceProvider = source->provider;
                    sourceProviderVersion = source->providerVersion;
                }
                else if (const MissingAssetRecord* source =
                            FindMissingAssetRecord(registry, imported.sourceAssetId))
                {
                    sourcePath = source->lastKnownPath;
                    sourceClass = source->dependencyClass;
                    sourceRequirement = source->requirement;
                    sourceProvider = source->provider;
                    sourceProviderVersion = source->providerVersion;
                }
                else
                {
                    error =
                        "Build Windows Game could not resolve authoritative import source stable ID: " +
                        imported.sourceAssetId;
                    return false;
                }

                if (sourcePath.empty() ||
                    sourceClass != DependencyClass::ImportedContent ||
                    sourceRequirement != DependencyRequirement::EditorOnly ||
                    sourceProvider.empty() || sourceProviderVersion != 1)
                {
                    error =
                        "Build Windows Game found unsupported imported-source tracking state for: " +
                        imported.sourceAssetId;
                    return false;
                }
                if (GraphContainsPath(collector.Graph(), sourcePath))
                    continue;

                DependencyRoot sourceRoot;
                sourceRoot.declaredPath = sourcePath;
                sourceRoot.dependencyClass = sourceClass;
                sourceRoot.requirement = DependencyRequirement::EditorOnly;
                sourceRoot.provenance = sourceProvider;
                if (!collector.AddRoot(sourceRoot, error))
                {
                    error =
                        "Build Windows Game could not add imported-source freshness root '" +
                        sourcePath + "': " + error;
                    return false;
                }
            }

            error.clear();
            return true;
        }

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
            const std::vector<WindowsGameBundledResource>& bundledResources,
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
            SceneIdentityCompanionProvider sceneIdentityProvider;
            ReusableAssetDependencyProvider reusableAssetProvider(project.projectId);
            ResourceAssetDependencyProvider resourceAssetProvider(project.projectId);

            DependencyCollector collector(project.rootPath);
            if (!collector.RegisterProvider(projectProvider, error) ||
                !collector.RegisterProvider(flowProvider, error) ||
                !collector.RegisterProvider(screenProvider, error) ||
                !collector.RegisterProvider(sceneProvider, error) ||
                !collector.RegisterProvider(gltfProvider, error) ||
                !collector.RegisterProvider(sceneIdentityProvider, error) ||
                !collector.RegisterProvider(reusableAssetProvider, error) ||
                !collector.RegisterProvider(resourceAssetProvider, error))
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
            if (!AddImportedSourceFreshnessRoots(project, collector, error))
                return false;

            graph = collector.Graph();

            std::vector<fs::path> governedSources;
            governedSources.reserve(bundledResources.size());
            for (const WindowsGameBundledResource& resource : bundledResources)
            {
                if (resource.logicalName.empty() || resource.sourcePath.empty() ||
                    resource.destinationPath.empty())
                {
                    error = "Build Windows Game received an incomplete bundled Runtime resource declaration.";
                    return false;
                }

                std::error_code ec;
                const fs::path source = fs::weakly_canonical(
                    fs::u8path(resource.sourcePath), ec);
                if (ec || !fs::is_regular_file(source, ec) || ec)
                {
                    error = "Build Windows Game could not validate bundled Runtime resource: " +
                        resource.sourcePath;
                    return false;
                }
                governedSources.push_back(source);
            }

            graph.diagnostics.erase(
                std::remove_if(
                    graph.diagnostics.begin(),
                    graph.diagnostics.end(),
                    [&](const DependencyDiagnostic& diagnostic)
                    {
                        if (diagnostic.code !=
                                DependencyDiagnosticCode::OutsideProject ||
                            diagnostic.sourceId.empty())
                        {
                            return false;
                        }

                        const auto sourceNode = std::find_if(
                            graph.nodes.begin(),
                            graph.nodes.end(),
                            [&](const DependencyNode& node)
                            {
                                return node.id == diagnostic.sourceId;
                            });
                        if (sourceNode == graph.nodes.end() ||
                            sourceNode->dependencyClass != DependencyClass::Scene)
                        {
                            return false;
                        }

                        fs::path declared = fs::u8path(diagnostic.path);
                        if (!declared.is_absolute())
                            declared = fs::u8path(project.rootPath) / declared;
                        std::error_code ec;
                        declared = fs::weakly_canonical(declared, ec);
                        if (ec || !fs::is_regular_file(declared, ec) || ec)
                            return false;

                        return std::any_of(
                            governedSources.begin(),
                            governedSources.end(),
                            [&](const fs::path& governed)
                            {
                                std::error_code equivalentError;
                                return fs::equivalent(
                                    declared, governed, equivalentError) &&
                                    !equivalentError;
                            });
                    }),
                graph.diagnostics.end());
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

        bool ReplaySmokePath(
            const FlowDocument& document,
            const std::vector<std::string>& outcomes,
            std::vector<std::string>& trace,
            FlowStepResult& finalStep,
            std::string& error)
        {
            trace.clear();
            finalStep = {};

            FlowInterpreter interpreter;
            if (!interpreter.Initialize(document, error))
                return false;

            FlowStepResult step = interpreter.Start();
            if (!step.succeeded)
            {
                error = "could not start Story Flow: " + step.message;
                return false;
            }
            trace.push_back(TraceLine(step));

            step = interpreter.EmitOutcome(GameStartOutcome);
            if (!step.succeeded)
            {
                error = "could not leave Game Start: " + step.message;
                return false;
            }
            trace.push_back(TraceLine(step));

            for (const std::string& outcome : outcomes)
            {
                if (step.terminalAction != FlowTerminalAction::None)
                {
                    error = "smoke outcome sequence continued after a terminal node";
                    return false;
                }

                step = interpreter.EmitOutcome(outcome);
                if (!step.succeeded)
                {
                    error = "outcome '" + outcome + "' was not executable: " +
                        step.message;
                    return false;
                }
                trace.push_back(TraceLine(step));
            }

            finalStep = std::move(step);
            error.clear();
            return true;
        }

        std::vector<std::string> OutgoingOutcomes(
            const FlowDocument& document,
            const StableId& nodeId)
        {
            std::vector<std::string> outcomes;
            for (const FlowRoute& route : document.routes)
            {
                if (route.sourceNodeId == nodeId && !route.outcome.empty())
                    outcomes.push_back(route.outcome);
            }
            std::sort(outcomes.begin(), outcomes.end());
            outcomes.erase(
                std::unique(outcomes.begin(), outcomes.end()), outcomes.end());
            return outcomes;
        }

        bool ComputeSmokeTrace(
            const ProjectMetadata& project,
            std::vector<std::string>& trace,
            std::vector<std::string>& smokeOutcomes,
            std::size_t& levelCompletionCount,
            std::string& error)
        {
            trace.clear();
            smokeOutcomes.clear();
            levelCompletionCount = 0;
            if (project.startupFlowId.empty() || project.startupFlow.empty())
            {
                error = "Build Windows Game requires a startup Story Flow.";
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

            StoryFlowRuntimeRoute resolved;
            if (!ResolveStoryFlowRuntimeRoute(document, resolved, error))
            {
                error = "Build Windows Game could not establish a deterministic Story Flow route: " +
                    error;
                return false;
            }

            trace = std::move(resolved.trace);
            smokeOutcomes = std::move(resolved.outcomes);
            levelCompletionCount = resolved.levelCompletionCount;
            error.clear();
            return true;
        }
    }

    bool ResolveStoryFlowRuntimeRoute(
        const FlowDocument& document,
        StoryFlowRuntimeRoute& route,
        std::string& error)
    {
        route = {};

        std::vector<std::string> initialTrace;
        FlowStepResult initialStep;
        if (!ReplaySmokePath(
                document, {}, initialTrace, initialStep, error))
        {
            return false;
        }

        if (initialStep.terminalAction == FlowTerminalAction::CompleteGame)
        {
            route.trace = std::move(initialTrace);
            error.clear();
            return true;
        }
        if (initialStep.terminalAction != FlowTerminalAction::None)
        {
            error =
                "Story Flow reaches a non-Complete terminal directly from Game Start.";
            return false;
        }

        struct Candidate
        {
            StableId nodeId;
            std::vector<std::string> outcomes;
        };

        std::deque<Candidate> pending;
        pending.push_back({initialStep.currentNodeId, {}});
        std::unordered_set<StableId> visited;
        visited.insert(initialStep.currentNodeId);

        constexpr std::size_t MaximumSmokeSteps = 128;
        constexpr std::size_t MaximumSmokeCandidates = 4096;
        std::size_t expanded = 0;

        while (!pending.empty() && expanded < MaximumSmokeCandidates)
        {
            Candidate candidate = std::move(pending.front());
            pending.pop_front();
            ++expanded;

            for (const std::string& outcome :
                    OutgoingOutcomes(document, candidate.nodeId))
            {
                std::vector<std::string> nextOutcomes = candidate.outcomes;
                nextOutcomes.push_back(outcome);
                if (nextOutcomes.size() > MaximumSmokeSteps)
                    continue;

                std::vector<std::string> candidateTrace;
                FlowStepResult step;
                std::string replayError;
                if (!ReplaySmokePath(
                        document,
                        nextOutcomes,
                        candidateTrace,
                        step,
                        replayError))
                {
                    // A structurally declared outcome can still be unavailable
                    // or ambiguous under the default Runtime state.
                    continue;
                }

                if (step.terminalAction == FlowTerminalAction::CompleteGame)
                {
                    route.trace = std::move(candidateTrace);
                    route.outcomes = std::move(nextOutcomes);
                    route.levelCompletionCount = static_cast<std::size_t>(
                        std::count(
                            route.outcomes.begin(),
                            route.outcomes.end(),
                            std::string("level.complete")));
                    error.clear();
                    return true;
                }
                if (step.terminalAction != FlowTerminalAction::None ||
                    step.currentNodeId.empty())
                {
                    continue;
                }

                if (visited.insert(step.currentNodeId).second)
                {
                    pending.push_back({
                        step.currentNodeId,
                        std::move(nextOutcomes),
                    });
                }
            }
        }

        error =
            "Story Flow has no bounded deterministic route from Game Start to Complete Game under the default Runtime state.";
        return false;
    }

    bool PrepareWindowsGameBuildProjectState(
        const ProjectMetadata& project,
        WindowsGameBuildProjectState& state,
        std::string& error)
    {
        return PrepareWindowsGameBuildProjectState(
            project, {}, state, error);
    }

    bool PrepareWindowsGameBuildProjectState(
        const ProjectMetadata& project,
        const std::vector<WindowsGameBundledResource>& bundledResources,
        WindowsGameBuildProjectState& state,
        std::string& error)
    {
        state = {};
        error.clear();

        if (!CollectCurrentDependencies(
                project,
                bundledResources,
                state.dependencyGraph,
                error))
            return false;
        state.bundledResources = bundledResources;
        if (!RefreshCurrentRegistry(
                project, state.dependencyGraph, state.assetRegistry, error))
            return false;
        if (!ComputeSmokeTrace(
                project,
                state.expectedFlowTrace,
                state.smokeOutcomes,
                state.levelCompletionCount,
                error))
            return false;

        error.clear();
        return true;
    }
}
