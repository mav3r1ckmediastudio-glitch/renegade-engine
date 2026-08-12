#include "renegade/bridge/WindowsGameBuildProjectService.h"

#include "renegade/bridge/FlowService.h"
#include "renegade/bridge/ReusableAssetDependencyService.h"

#include <algorithm>
#include <filesystem>
#include <sstream>
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
        // SourceAssets FBX/GLTF. LC01 freshness still needs the source's current
        // hash, otherwise RefreshAssetRegistry would tombstone it and every
        // reachable .rasset would look source-unavailable to BuildService.
        //
        // Add only the source records for imported products that the already-
        // discovered Runtime graph actually reaches. Do it after transitive
        // discovery has finished: the editor-only source node is hashed and
        // retained by LC01, but no source-side glTF buffer/image references can
        // become Runtime package dependencies. BuildService excludes the node
        // itself because its requirement remains EditorOnly.
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
                // DependencyRoot v1 records provider version 1. Preserve the
                // authoritative Gate 3 provider token so LC01 retains/recover
                // identity rather than rewriting source provenance at build.
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

            DependencyCollector collector(project.rootPath);
            if (!collector.RegisterProvider(projectProvider, error) ||
                !collector.RegisterProvider(flowProvider, error) ||
                !collector.RegisterProvider(screenProvider, error) ||
                !collector.RegisterProvider(sceneProvider, error) ||
                !collector.RegisterProvider(gltfProvider, error) ||
                !collector.RegisterProvider(sceneIdentityProvider, error) ||
                !collector.RegisterProvider(reusableAssetProvider, error))
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
