#include "renegade/bridge/FlowService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/StoryFlowProjectHomeService.h"
#include "renegade/bridge/StudioProjectService.h"

#include <wiArchive.h>
#include <wiScene.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    int failures = 0;

    void Check(const bool condition, const char* message)
    {
        if (!condition)
        {
            ++failures;
            std::cerr << "FAIL: " << message << '\n';
        }
    }

    struct TemporaryDirectory
    {
        fs::path path;
        ~TemporaryDirectory()
        {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
    };

    bool WriteBlankWiscene(const fs::path& path)
    {
        std::error_code error;
        fs::create_directories(path.parent_path(), error);
        if (error) return false;

        wi::scene::Scene scene;
        wi::Archive archive(path.generic_u8string(), false, false);
        if (!archive.IsOpen()) return false;
        archive.SetCompressionEnabled(true);
        scene.Serialize(archive);
        const bool saved = archive.SaveFile(path.generic_u8string());
        archive = wi::Archive();
        return saved && fs::is_regular_file(path);
    }

    void VerifyMigratedHome(
        const ProjectMetadata& project,
        const StoryFlowProjectHomeResult& migration)
    {
        Check(migration.succeeded,
            "project-home migration did not succeed");
        Check(IsValidStableId(migration.flowDocumentId),
            "project-home migration did not return a stable Flow ID");
        Check(migration.flowPathHint ==
                "Content/StoryFlow/Main.renegade-flow",
            "project-home migration used the wrong canonical Flow hint");

        FlowDocument flow;
        std::string error;
        Check(ReadFlowDocument(
                migration.flowPath,
                project.projectId,
                flow,
                error),
            "migrated Story Flow did not reopen");
        if (flow.nodes.empty()) return;

        const auto starts = std::count_if(
            flow.nodes.begin(), flow.nodes.end(),
            [](const FlowNode& node)
            {
                return node.kind == FlowNodeKind::GameStart;
            });
        Check(starts == 1,
            "migrated Story Flow did not retain exactly one Game Start");

        const auto level = std::find_if(
            flow.nodes.begin(), flow.nodes.end(),
            [&project](const FlowNode& node)
            {
                return node.kind == FlowNodeKind::Level &&
                    node.scenePathHint == project.startupScene;
            });
        Check(level != flow.nodes.end(),
            "migrated Story Flow did not adopt the startup Scene as a Level");
        if (level == flow.nodes.end()) return;

        const bool routed = std::any_of(
            flow.routes.begin(), flow.routes.end(),
            [&flow, &level](const FlowRoute& route)
            {
                return route.sourceNodeId == flow.startNodeId &&
                    route.outcome == GameStartOutcome &&
                    route.destinationNodeId == level->id &&
                    route.destinationEntry == "default";
            });
        Check(routed,
            "migrated Story Flow did not route Game Start to the startup Level");
    }

    void TestCreateAndRecentEntry(
        const fs::path& root,
        const fs::path& templateScene)
    {
        const fs::path projectsRoot = root / "create-projects";
        fs::create_directories(projectsRoot);
        const fs::path statePath = root / "create-editor-state.ini";

        StudioProjectService studio;
        studio.Initialize(statePath.generic_u8string());
        Check(studio.CreateProject(
                projectsRoot.generic_u8string(),
                "Create Entry",
                templateScene.generic_u8string()),
            "Create Project did not stage its candidate");
        Check(studio.HasPendingProject() && !studio.HasProject(),
            "Create Project bypassed the staged adoption boundary");
        Check(studio.CommitPendingProject(),
            "Create Project candidate did not become authoritative");
        if (!studio.HasProject()) return;

        const ProjectMetadata staleProject = studio.CurrentProject();
        Check(staleProject.startupFlowId.empty() &&
                staleProject.startupFlow.empty(),
            "new project fixture unexpectedly started with Story Flow metadata");

        StoryFlowProjectHomeService homes;
        const auto migrated = homes.Ensure(staleProject);
        Check(migrated.createdFlow && migrated.adoptedStartupScene &&
                migrated.updatedProjectDescriptor,
            "new project did not create, adopt and bind its Story Flow home");
        VerifyMigratedHome(staleProject, migrated);

        // A stale active snapshot must observe the already-committed descriptor
        // and never replay the migration writes while Studio refreshes metadata.
        const auto idempotent = homes.Ensure(staleProject);
        Check(idempotent.succeeded &&
                !idempotent.createdFlow &&
                !idempotent.adoptedStartupScene &&
                !idempotent.updatedProjectDescriptor &&
                idempotent.flowDocumentId == migrated.flowDocumentId,
            "stale active metadata replayed the project-home migration");

        Check(studio.RefreshCurrentProject(),
            "new project Story Flow metadata did not refresh authoritatively");
        Check(!studio.HasPendingProject() &&
                studio.CurrentProject().startupFlowId == migrated.flowDocumentId &&
                studio.CurrentProject().startupFlow == migrated.flowPathHint,
            "new project refresh left Story Flow staged or inactive");

        // Recent Project enters through the normal staged open boundary, then
        // must retain the same already-governed Story Flow home.
        StudioProjectService recent;
        recent.Initialize(statePath.generic_u8string());
        Check(!recent.RecentProjects().empty(),
            "new project did not persist a Recent Project entry");
        if (recent.RecentProjects().empty()) return;
        Check(recent.OpenProject(
                recent.RecentProjects().front().descriptorPath) &&
                recent.CommitPendingProject(),
            "Recent Project did not stage and commit");
        Check(recent.CurrentProject().startupFlowId == migrated.flowDocumentId &&
                recent.CurrentProject().startupFlow == migrated.flowPathHint,
            "Recent Project reopen lost its Story Flow home");
        const auto existing = homes.Ensure(recent.CurrentProject());
        Check(existing.succeeded &&
                !existing.createdFlow &&
                !existing.updatedProjectDescriptor,
            "Recent Project reopen rewrote an existing Story Flow home");
    }

    void TestOpenSceneFirstProject(
        const fs::path& root,
        const fs::path& templateScene)
    {
        const fs::path projectsRoot = root / "open-projects";
        fs::create_directories(projectsRoot);

        ProjectService factory;
        Check(factory.CreateProject(
                projectsRoot.generic_u8string(),
                "Open Entry",
                templateScene.generic_u8string()),
            "scene-first Open Project fixture could not be created");
        if (!factory.HasProject()) return;
        const std::string descriptor = factory.CurrentProject().descriptorPath;

        StudioProjectService studio;
        studio.Initialize((root / "open-editor-state.ini").generic_u8string());
        Check(studio.OpenProject(descriptor) &&
                studio.CommitPendingProject(),
            "scene-first project did not enter through staged Open Project");
        if (!studio.HasProject()) return;

        const ProjectMetadata staleProject = studio.CurrentProject();
        StoryFlowProjectHomeService homes;
        const auto migrated = homes.Ensure(staleProject);
        Check(migrated.succeeded && migrated.adoptedStartupScene,
            "scene-first Open Project did not migrate to Story Flow");
        VerifyMigratedHome(staleProject, migrated);

        Check(studio.RefreshCurrentProject(),
            "opened project Story Flow metadata did not refresh authoritatively");
        Check(studio.CurrentProject().startupFlowId == migrated.flowDocumentId &&
                studio.CurrentProject().startupFlow == migrated.flowPathHint,
            "opened project remained on stale scene-first metadata");
    }
}

int main()
{
    const auto unique = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    TemporaryDirectory temporary{
        fs::temp_directory_path() /
        fs::u8path(
            "renegade-story-flow-gate5-home-" +
            std::to_string(unique))
    };
    const fs::path templateScene = temporary.path / "Template.wiscene";
    Check(WriteBlankWiscene(templateScene),
        "could not create the project-home WISCENE fixture");
    if (fs::is_regular_file(templateScene))
    {
        TestCreateAndRecentEntry(temporary.path, templateScene);
        TestOpenSceneFirstProject(temporary.path, templateScene);
    }

    if (failures != 0)
    {
        std::cerr << "RenegadeStoryFlowGate5ProjectHomeRecoveryTests: "
                  << failures << " failure(s)\n";
        return 1;
    }

    std::cout
        << "PASS: Story Flow Gate 5 Project Home Create/Open/Recent recovery\n";
    return 0;
}
