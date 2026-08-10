#include "renegade/bridge/BuildService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/WindowsGameBuildProjectService.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
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
            registry.records.begin(),
            registry.records.end(),
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
            graph.edges.begin(),
            graph.edges.end(),
            [&](const DependencyEdge& edge)
            {
                return edge.sourceId == sourceId &&
                    edge.targetId == targetId &&
                    edge.provenance == provenance;
            });
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
        state.expectedFlowTrace.size() != 4)
    {
        return Fail(root,
            "owner-build project preparation changed the established Test All route");
    }

    WindowsGameBuildRequest request;
    request.gameName = project.name;
    request.executableBaseName = project.name;
    request.saveDataId = project.projectId;

    const std::vector<WindowsRuntimeSupportInput> runtimeSupport = {
        {
            "renegade-runtime",
            project.name + ".exe",
            1,
            std::string(64, 'a'),
            "repo:gate5-owner-regression",
        },
        {
            "directx-shader-compiler",
            "dxcompiler.dll",
            1,
            std::string(64, 'b'),
            "pinned:gate5-owner-regression",
        },
    };

    WindowsGameBuildPlan plan;
    if (!CreateWindowsGameBuildPlan(
            project,
            state.dependencyGraph,
            state.assetRegistry,
            request,
            runtimeSupport,
            plan,
            error))
    {
        return Fail(root, "owner-build plan rejected corrected project state: " + error);
    }

    if (FindPlanFile(plan, "GameData/" + levelOneMeta) == nullptr ||
        FindPlanFile(plan, "GameData/" + levelTwoMeta) == nullptr)
    {
        return Fail(root,
            "owner-build plan omitted scene identity companions required by Runtime");
    }

    fs::remove_all(root, ignored);
    std::cout
        << "PASS: LP06 Gate 5 owner build retains reachable scene identity companions\n";
    return 0;
}
