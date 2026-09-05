#include "renegade/bridge/BuildService.h"
#include "renegade/bridge/FlowService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/WindowsGameBuildProjectService.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    int Fail(const fs::path& root, const std::string& message)
    {
        std::error_code ignored;
        fs::remove_all(root, ignored);
        std::cerr << "FAIL: " << message << '\n';
        return 1;
    }

    bool CopyTree(
        const fs::path& source,
        const fs::path& destination,
        std::string& error)
    {
        std::error_code ec;
        fs::create_directories(destination, ec);
        if (ec)
        {
            error = "could not create disposable project root: " + ec.message();
            return false;
        }

        fs::recursive_directory_iterator iterator(
            source,
            fs::directory_options::skip_permission_denied,
            ec);
        const fs::recursive_directory_iterator end;
        while (!ec && iterator != end)
        {
            const fs::path relative = fs::relative(iterator->path(), source, ec);
            if (ec)
                break;
            const fs::path target = destination / relative;
            if (iterator->is_directory(ec))
            {
                if (ec)
                    break;
                fs::create_directories(target, ec);
            }
            else if (iterator->is_regular_file(ec))
            {
                if (ec)
                    break;
                fs::create_directories(target.parent_path(), ec);
                if (ec)
                    break;
                fs::copy_file(
                    iterator->path(),
                    target,
                    fs::copy_options::overwrite_existing,
                    ec);
            }
            else if (!ec)
            {
                error = "fixture contains an unsupported filesystem entry";
                return false;
            }
            if (ec)
                break;
            iterator.increment(ec);
        }

        if (ec)
        {
            error = "could not copy disposable project fixture: " + ec.message();
            return false;
        }
        error.clear();
        return true;
    }

    const DependencyNode* FindNode(
        const DependencyGraph& graph,
        const std::string& path)
    {
        const auto found = std::find_if(
            graph.nodes.begin(),
            graph.nodes.end(),
            [&path](const DependencyNode& node)
            {
                return node.projectRelativePath == path;
            });
        return found == graph.nodes.end() ? nullptr : &*found;
    }

    const AssetRecord* FindRecord(
        const AssetRegistry& registry,
        const std::string& path)
    {
        const auto found = std::find_if(
            registry.records.begin(), registry.records.end(),
            [&path](const AssetRecord& record)
            {
                return record.projectRelativePath == path;
            });
        return found == registry.records.end() ? nullptr : &*found;
    }

    const WindowsGameBuildFile* FindPlanFile(
        const WindowsGameBuildPlan& plan,
        const std::string& destination)
    {
        const auto found = std::find_if(
            plan.files.begin(),
            plan.files.end(),
            [&destination](const WindowsGameBuildFile& file)
            {
                return file.destinationPath == destination;
            });
        return found == plan.files.end() ? nullptr : &*found;
    }

    bool HasEdge(
        const DependencyGraph& graph,
        const std::string& sourceId,
        const std::string& targetId,
        const std::string& provenance)
    {
        return std::any_of(
            graph.edges.begin(), graph.edges.end(),
            [&](const DependencyEdge& edge)
            {
                return edge.sourceId == sourceId &&
                    edge.targetId == targetId &&
                    edge.provenance == provenance;
            });
    }

    bool ReplaceDescriptorText(
        const fs::path& descriptor,
        const std::string& from,
        const std::string& to,
        std::string& error)
    {
        std::ifstream input(descriptor, std::ios::binary);
        if (!input)
        {
            error = "could not open project descriptor for Gate 10 migration proof";
            return false;
        }
        std::string text{
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
        const std::size_t found = text.find(from);
        if (found == std::string::npos)
        {
            error = "Gate 10 fixture descriptor did not contain expected legacy field";
            return false;
        }
        text.replace(found, from.size(), to);

        std::ofstream output(descriptor, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            error = "could not rewrite project descriptor for Gate 10 migration proof";
            return false;
        }
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!output)
        {
            error = "could not finish Gate 10 project descriptor rewrite";
            return false;
        }
        error.clear();
        return true;
    }

    FlowRoute Route(
        const StableId& source,
        std::string outcome,
        const StableId& destination,
        std::string destinationEntry = {})
    {
        FlowRoute route;
        route.id = GenerateStableId();
        route.sourceNodeId = source;
        route.outcome = std::move(outcome);
        route.destinationNodeId = destination;
        route.destinationEntry = std::move(destinationEntry);
        return route;
    }
}

int main(int argc, char** argv)
{
    using namespace renegade::bridge;

    if (argc != 3)
    {
        std::cerr <<
            "usage: RenegadeWindowsGameBuildProjectTests <LP03 fixture root> <cube.wiscene>\n";
        return 2;
    }

    const fs::path fixtureRoot = fs::absolute(fs::u8path(argv[1]));
    const fs::path cubeScene = fs::absolute(fs::u8path(argv[2]));
    const fs::path root = fs::temp_directory_path() /
        fs::u8path(
            "renegade-gate5-project-state-" +
            std::to_string(GetCurrentProcessId()));
    const fs::path projectRoot = root / "LP06-Gate5-Owner-Test";

    std::error_code ignored;
    fs::remove_all(root, ignored);

    if (!fs::is_directory(fixtureRoot) || !fs::is_regular_file(cubeScene))
        return Fail(root, "required owner-build regression fixtures are missing");

    std::string error;
    if (!CopyTree(fixtureRoot, projectRoot, error))
        return Fail(root, error);

    std::error_code ec;
    const fs::path levelOne =
        projectRoot / "Content" / "Scenes" / "LevelOne.wiscene";
    const fs::path levelTwo =
        projectRoot / "Content" / "Scenes" / "LevelTwo.wiscene";
    fs::copy_file(cubeScene, levelOne, fs::copy_options::overwrite_existing, ec);
    if (ec)
        return Fail(root, "could not materialize Level One WISCENE fixture: " + ec.message());
    ec.clear();
    fs::copy_file(cubeScene, levelTwo, fs::copy_options::overwrite_existing, ec);
    if (ec)
        return Fail(root, "could not materialize Level Two WISCENE fixture: " + ec.message());

    ProjectService projects;
    ProjectMetadata project;
    const fs::path descriptor = projectRoot / "ScreenProject.renegade";
    if (!projects.InspectProject(descriptor.generic_u8string(), project, error))
        return Fail(root, "could not inspect disposable owner project: " + error);

    // Preserve the accepted LP06 regression first. The old Level-only fixture
    // must continue producing exactly the established deterministic smoke.
    WindowsGameBuildProjectState state;
    if (!PrepareWindowsGameBuildProjectState(project, state, error))
        return Fail(root, "owner-build project preparation failed: " + error);

    const std::string levelOnePath = "Content/Scenes/LevelOne.wiscene";
    const std::string levelTwoPath = "Content/Scenes/LevelTwo.wiscene";
    const std::string levelOneMeta = levelOnePath + ".rmeta";
    const std::string levelTwoMeta = levelTwoPath + ".rmeta";

    const DependencyNode* levelOneNode = FindNode(state.dependencyGraph, levelOnePath);
    const DependencyNode* levelTwoNode = FindNode(state.dependencyGraph, levelTwoPath);
    const DependencyNode* levelOneMetaNode = FindNode(state.dependencyGraph, levelOneMeta);
    const DependencyNode* levelTwoMetaNode = FindNode(state.dependencyGraph, levelTwoMeta);
    if (levelOneNode == nullptr || levelTwoNode == nullptr ||
        levelOneMetaNode == nullptr || levelTwoMetaNode == nullptr)
    {
        return Fail(root,
            "owner-build dependency graph omitted reachable scene identity companions");
    }

    const auto validCompanion = [](const DependencyNode& node)
    {
        return node.dependencyClass == DependencyClass::GeneratedData &&
            node.requirement == DependencyRequirement::Required &&
            node.provider == "lp06-scene-identity-companion" &&
            node.providerVersion == 1 &&
            node.contentHash != "missing";
    };
    if (!validCompanion(*levelOneMetaNode) ||
        !validCompanion(*levelTwoMetaNode))
    {
        return Fail(root,
            "scene identity companions were not governed as required generated data");
    }

    if (!HasEdge(
            state.dependencyGraph,
            levelOneNode->id,
            levelOneMetaNode->id,
            "lp06.scene_identity_companion") ||
        !HasEdge(
            state.dependencyGraph,
            levelTwoNode->id,
            levelTwoMetaNode->id,
            "lp06.scene_identity_companion"))
    {
        return Fail(root,
            "scene identity companion graph edges lost LP06 provenance");
    }

    if (FindRecord(state.assetRegistry, levelOneMeta) == nullptr ||
        FindRecord(state.assetRegistry, levelTwoMeta) == nullptr)
    {
        return Fail(root,
            "LC01 refresh omitted reachable scene identity companion records");
    }

    if (state.levelCompletionCount != 2 ||
        state.expectedFlowTrace.size() != 4 ||
        state.smokeOutcomes !=
            std::vector<std::string>{"level.complete", "level.complete"})
    {
        return Fail(root,
            "owner-build project preparation changed the established LP06 route");
    }

    // Gate 10: convert the disposable project to the current Story Flow-native
    // architecture. The project descriptor no longer owns a startup Scene or
    // startup Screen; the Screen is an executable destination inside Flow and
    // must be discovered, packaged and traversed from there.
    const StableId screenDocumentId = project.startupScreenId;
    const std::string screenPath = project.startupScreen;
    const fs::path flowPath = projectRoot / "Content/Flow/Main.renegade-flow";
    FlowDocument flow;
    if (!ReadFlowDocument(
            flowPath.generic_u8string(), project.projectId, flow, error) ||
        flow.nodes.size() != 4 || flow.routes.size() != 3)
    {
        return Fail(root, "could not read established Flow before Gate 10 conversion: " + error);
    }

    const StableId gameStartId = flow.startNodeId;
    const StableId levelOneId = flow.nodes[1].id;
    const StableId levelTwoId = flow.nodes[2].id;
    const StableId completeId = flow.nodes[3].id;
    const FlowRoute levelOneComplete = flow.routes[1];
    const FlowRoute levelTwoComplete = flow.routes[2];

    FlowNode titleScreen;
    titleScreen.id = GenerateStableId();
    titleScreen.kind = FlowNodeKind::Screen;
    titleScreen.name = "Title Screen";
    titleScreen.screenDocumentId = screenDocumentId;
    titleScreen.screenPathHint = screenPath;

    FlowNode quit;
    quit.id = GenerateStableId();
    quit.kind = FlowNodeKind::Quit;
    quit.name = "Quit";

    flow.nodes.push_back(titleScreen);
    flow.nodes.push_back(quit);
    flow.routes = {
        Route(gameStartId, GameStartOutcome, titleScreen.id),
        Route(titleScreen.id, "play", levelOneId, "default"),
        Route(titleScreen.id, "quit", quit.id),
        levelOneComplete,
        levelTwoComplete,
    };

    StoryFlowRuntimeRoute liveRoute;
    if (!ResolveStoryFlowRuntimeRoute(flow, liveRoute, error) ||
        liveRoute.outcomes !=
            std::vector<std::string>{
                "play", "level.complete", "level.complete"})
    {
        return Fail(root,
            "shared live Story Flow Runtime readiness rejected the valid journey: " +
                error);
    }
    FlowDocument unrouted = flow;
    unrouted.routes.erase(
        std::remove_if(
            unrouted.routes.begin(),
            unrouted.routes.end(),
            [&](const FlowRoute& route)
            {
                return route.sourceNodeId == gameStartId;
            }),
        unrouted.routes.end());
    if (ResolveStoryFlowRuntimeRoute(unrouted, liveRoute, error) ||
        error.find("could not leave Game Start") == std::string::npos)
    {
        return Fail(root,
            "shared live Story Flow Runtime readiness accepted an unrouted Game Start");
    }

    if (!WriteFlowDocument(flowPath.generic_u8string(), flow, error))
        return Fail(root, "could not persist Gate 10 Screen/Level Story Flow: " + error);

    // Scene UI Gate 6 recovery: new Terrain binds Renegade's bundled default
    // grass through an absolute Studio-package path. It remains outside the
    // creator project, but the exact file is safe only when the controller
    // also stages it as hashed Runtime support at its governed destination.
    const fs::path bundledBaseColor = root / "StudioPackage" / "Content" /
        "terrain" / "default_grass" / "default_grass_basecolor.tga";
    fs::create_directories(bundledBaseColor.parent_path(), ec);
    if (ec)
        return Fail(root, "could not create bundled Terrain resource fixture");
    std::ofstream(bundledBaseColor, std::ios::binary) << "bundled-grass";

    wi::scene::Scene terrainScene;
    auto& terrainMaterial =
        terrainScene.materials.Create(wi::ecs::CreateEntity());
    terrainMaterial.textures[
        wi::scene::MaterialComponent::BASECOLORMAP].name =
            bundledBaseColor.generic_u8string();
    wi::Archive terrainArchive(levelOne.generic_u8string(), false, false);
    if (!terrainArchive.IsOpen())
        return Fail(root, "could not open Terrain build regression archive");
    terrainScene.Serialize(terrainArchive);
    if (!terrainArchive.SaveFile(levelOne.generic_u8string()))
        return Fail(root, "could not save Terrain build regression archive");
    terrainArchive = wi::Archive();

    if (!ReplaceDescriptorText(
            descriptor,
            "startup_scene = Content/Scenes/LevelOne.wiscene",
            "startup_scene = ",
            error) ||
        !ReplaceDescriptorText(
            descriptor,
            "startup_screen_id = " + screenDocumentId,
            "startup_screen_id = ",
            error) ||
        !ReplaceDescriptorText(
            descriptor,
            "startup_screen = " + screenPath,
            "startup_screen = ",
            error))
    {
        return Fail(root, error);
    }

    ProjectMetadata gate10Project;
    if (!projects.InspectProject(
            descriptor.generic_u8string(), gate10Project, error) ||
        !gate10Project.startupScene.empty() ||
        !gate10Project.startupScreen.empty() ||
        gate10Project.startupFlowId != project.startupFlowId)
    {
        return Fail(root,
            "Gate 10 Flow-native project descriptor did not reopen cleanly: " + error);
    }

    WindowsGameBuildProjectState unsafeTerrainState;
    if (!PrepareWindowsGameBuildProjectState(
            gate10Project, unsafeTerrainState, error))
    {
        return Fail(root,
            "unmapped Terrain dependency could not be inspected: " + error);
    }
    const auto hasOutsideProject = [](const DependencyGraph& graph)
    {
        return std::any_of(
            graph.diagnostics.begin(),
            graph.diagnostics.end(),
            [](const DependencyDiagnostic& diagnostic)
            {
                return diagnostic.code ==
                    DependencyDiagnosticCode::OutsideProject;
            });
    };
    if (!hasOutsideProject(unsafeTerrainState.dependencyGraph))
    {
        return Fail(root,
            "unmapped Studio-package Terrain resource was not rejected as external");
    }

    const fs::path unrelatedBundled = root / "StudioPackage" / "Content" /
        "terrain" / "unrelated.tga";
    std::ofstream(unrelatedBundled, std::ios::binary) << "unrelated";
    const std::vector<WindowsGameBundledResource> wrongBundledResources = {
        {
            "renegade-unrelated-resource",
            unrelatedBundled.generic_u8string(),
            "Content/terrain/unrelated.tga",
        },
    };
    WindowsGameBuildProjectState wrongTerrainState;
    if (!PrepareWindowsGameBuildProjectState(
            gate10Project, wrongBundledResources, wrongTerrainState, error) ||
        !hasOutsideProject(wrongTerrainState.dependencyGraph))
    {
        return Fail(root,
            "an unrelated bundled file admitted an arbitrary external Scene dependency");
    }

    const std::vector<WindowsGameBundledResource> bundledResources = {
        {
            "renegade-terrain-default-grass-basecolor",
            bundledBaseColor.generic_u8string(),
            "Content/terrain/default_grass/default_grass_basecolor.tga",
        },
    };
    WindowsGameBuildProjectState gate10State;
    if (!PrepareWindowsGameBuildProjectState(
            gate10Project, bundledResources, gate10State, error))
    {
        return Fail(root,
            "Gate 10 build preparation rejected a Story Flow Screen/Level journey: " + error);
    }
    if (hasOutsideProject(gate10State.dependencyGraph))
    {
        return Fail(root,
            "exact governed Terrain Runtime resource remained an external dependency");
    }
    if (gate10State.bundledResources.size() != 1 ||
        gate10State.bundledResources.front().logicalName !=
            bundledResources.front().logicalName ||
        gate10State.bundledResources.front().sourcePath !=
            bundledResources.front().sourcePath ||
        gate10State.bundledResources.front().destinationPath !=
            bundledResources.front().destinationPath)
    {
        return Fail(root,
            "build preparation did not preserve the validated Runtime resource declaration");
    }

    const std::vector<std::string> expectedOutcomes = {
        "play",
        "level.complete",
        "level.complete",
    };
    if (gate10State.smokeOutcomes != expectedOutcomes ||
        gate10State.levelCompletionCount != 2 ||
        gate10State.expectedFlowTrace.size() != 5)
    {
        return Fail(root,
            "Gate 10 did not derive the exact Screen/Level standalone smoke path");
    }

    if (FindNode(gate10State.dependencyGraph, "Content/Flow/Main.renegade-flow") == nullptr ||
        FindNode(gate10State.dependencyGraph, screenPath) == nullptr ||
        FindNode(gate10State.dependencyGraph, levelOnePath) == nullptr ||
        FindNode(gate10State.dependencyGraph, levelTwoPath) == nullptr)
    {
        return Fail(root,
            "Gate 10 Flow-native dependency closure omitted Screen or Level content");
    }

    WindowsGameBuildRequest request;
    request.gameName = gate10Project.name;
    request.executableBaseName = gate10Project.name;
    request.saveDataId = gate10Project.projectId;

    const std::vector<WindowsRuntimeSupportInput> runtimeSupport = {
        {
            "renegade-runtime",
            gate10Project.name + ".exe",
            1,
            std::string(64, 'a'),
            "repo:gate10-owner-regression",
        },
        {
            "directx-shader-compiler",
            "dxcompiler.dll",
            1,
            std::string(64, 'b'),
            "pinned:gate10-owner-regression",
        },
        {
            "renegade-terrain-default-grass-basecolor",
            "Content/terrain/default_grass/default_grass_basecolor.tga",
            13,
            std::string(64, 'c'),
            "repo:gate10-owner-regression",
        },
    };

    WindowsGameBuildPlan plan;
    if (!CreateWindowsGameBuildPlan(
            gate10Project,
            gate10State.dependencyGraph,
            gate10State.assetRegistry,
            request,
            runtimeSupport,
            plan,
            error))
    {
        return Fail(root, "Gate 10 build plan rejected corrected project state: " + error);
    }

    if (FindPlanFile(plan, "GameData/" + levelOneMeta) == nullptr ||
        FindPlanFile(plan, "GameData/" + levelTwoMeta) == nullptr ||
        FindPlanFile(plan, "GameData/Content/Flow/Main.renegade-flow") == nullptr ||
        FindPlanFile(plan, "GameData/" + screenPath) == nullptr)
    {
        return Fail(root,
            "Gate 10 build plan omitted governed Flow/Screen/Scene runtime content");
    }
    const WindowsGameBuildFile* bundledTerrain = FindPlanFile(
        plan, "Content/terrain/default_grass/default_grass_basecolor.tga");
    if (bundledTerrain == nullptr ||
        bundledTerrain->kind != WindowsGameBuildFileKind::RuntimeSupport)
    {
        return Fail(root,
            "Gate 6 build recovery omitted governed Terrain Runtime support");
    }

    fs::remove_all(root, ignored);
    std::cout
        << "PASS: Gate 10 Windows build follows Story Flow Screen/Level authority\n";
    return 0;
}
