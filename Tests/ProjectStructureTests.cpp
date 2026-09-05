#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <wiConfig.h>
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/StudioProjectService.h"

namespace
{
    namespace fs = std::filesystem;

    int Fail(const fs::path& root, const char* message)
    {
        std::error_code ignored;
        fs::remove_all(root, ignored);
        std::cerr << "FAIL: " << message << '\n';
        return 1;
    }
}

int main()
{
    const auto unique = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
        fs::u8path("renegade-project-backfill-" + std::to_string(unique));
    const fs::path descriptor = root / "LegacyProject.renegade";
    const fs::path scene = root / "Content/Scenes/Main.wiscene";

    fs::create_directories(scene.parent_path());
    std::ofstream(scene, std::ios::binary).put('\0');

    wi::config::File file;
    file.Open(descriptor.generic_u8string());
    file.Set("format", "renegade-project");
    file.Set("version", 1);
    auto& project = file.GetSection("project");
    project.Set("name", "LegacyProject");
    project.Set("startup_scene", "Content/Scenes/Main.wiscene");
    file.Commit();

    renegade::bridge::ProjectService projects;
    projects.Initialize((root / "editor-state.ini").generic_u8string());
    if (!projects.OpenProject(descriptor.generic_u8string()))
    {
        return Fail(root, "legacy project did not open");
    }

    constexpr const char* required[] = {
        "Content/Models",
        "Content/Materials",
        "Content/Textures",
        "Content/Audio/SFX",
        "Content/Weapons",
        "Content/Projectiles",
        "Content/Prefabs",
        "SourceAssets/Models",
        "Saved/ImportLogs",
        "Saved/Thumbnails",
        "Intermediate",
        "Builds/Windows",
    };
    for (const char* directory : required)
    {
        if (!fs::is_directory(root / fs::u8path(directory)))
        {
            return Fail(root, "standard project folder was not backfilled");
        }
    }

    // Gate 5: Studio must not make a candidate project authoritative until
    // its startup scene has reached the validated scene-adoption boundary.
    const fs::path switchRoot = root / "switch-tests";
    fs::create_directories(switchRoot);
    const fs::path templateScene = root / "Template.wiscene";
    std::ofstream(templateScene, std::ios::binary).put('\0');

    renegade::bridge::ProjectService factory;
    if (!factory.CreateProject(
            switchRoot.generic_u8string(),
            "ProjectA",
            templateScene.generic_u8string()))
    {
        return Fail(root, "Project A fixture creation failed");
    }
    const std::string projectADescriptor =
        factory.CurrentProject().descriptorPath;

    if (!factory.CreateProject(
            switchRoot.generic_u8string(),
            "ProjectB",
            templateScene.generic_u8string()))
    {
        return Fail(root, "Project B fixture creation failed");
    }
    const std::string projectBDescriptor =
        factory.CurrentProject().descriptorPath;
    const fs::path projectBScene =
        fs::u8path(factory.StartupScenePath());

    renegade::bridge::StudioProjectService studio;
    studio.Initialize(
        (root / "gate5-editor-state.ini").generic_u8string());

    if (!studio.OpenProject(projectADescriptor))
    {
        return Fail(root, "Project A did not stage");
    }
    if (studio.HasProject() || !studio.HasPendingProject() ||
        !studio.RecentProjects().empty())
    {
        return Fail(root, "staged Project A mutated active identity or recents");
    }
    if (!studio.CommitPendingProject())
    {
        return Fail(root, "Project A did not commit");
    }
    if (!studio.HasProject() || studio.CurrentProject().name != "ProjectA" ||
        studio.RecentProjects().size() != 1u)
    {
        return Fail(root, "Project A commit did not become authoritative");
    }

    if (!studio.OpenProject(projectBDescriptor))
    {
        return Fail(root, "Project B did not stage");
    }
    if (!studio.HasPendingProject() ||
        studio.CurrentProject().name != "ProjectA" ||
        studio.RecentProjects().size() != 1u ||
        studio.RecentProjects().front().name != "ProjectA")
    {
        return Fail(root, "staged Project B leaked into active identity or recents");
    }
    studio.DiscardPendingProject();
    if (studio.CurrentProject().name != "ProjectA" ||
        studio.RecentProjects().size() != 1u)
    {
        return Fail(root, "discarding Project B changed Project A context");
    }

    // A missing startup scene is rejected while Project A stays authoritative.
    std::error_code ioError;
    fs::remove(projectBScene, ioError);
    if (ioError)
    {
        return Fail(root, "could not remove Project B startup scene fixture");
    }
    if (studio.OpenProject(projectBDescriptor))
    {
        return Fail(root, "Project B staged despite missing startup scene");
    }
    if (studio.HasPendingProject() ||
        studio.CurrentProject().name != "ProjectA" ||
        studio.RecentProjects().size() != 1u)
    {
        return Fail(root, "failed Project B open changed Project A context");
    }

    fs::copy_file(
        templateScene,
        projectBScene,
        fs::copy_options::overwrite_existing,
        ioError);
    if (ioError)
    {
        return Fail(root, "could not restore Project B startup scene fixture");
    }
    if (!studio.OpenProject(projectBDescriptor) ||
        !studio.CommitPendingProject())
    {
        return Fail(root, "Project B did not stage and commit after repair");
    }
    if (studio.CurrentProject().name != "ProjectB" ||
        studio.RecentProjects().size() != 2u ||
        studio.RecentProjects()[0].name != "ProjectB" ||
        studio.RecentProjects()[1].name != "ProjectA")
    {
        return Fail(root, "Project A to B switch did not update authority atomically");
    }

    // Create Project uses the same staged adoption rule.
    const auto recentsBeforeCreate = studio.RecentProjects().size();
    if (!studio.CreateProject(
            switchRoot.generic_u8string(),
            "ProjectC",
            templateScene.generic_u8string()))
    {
        return Fail(root, "Project C did not stage from Create Project");
    }
    if (!studio.HasPendingProject() ||
        studio.CurrentProject().name != "ProjectB" ||
        studio.RecentProjects().size() != recentsBeforeCreate)
    {
        return Fail(root, "staged Project C changed active Project B context");
    }
    studio.DiscardPendingProject();

    std::error_code ignored;
    fs::remove_all(root, ignored);
    std::cout
        << "PASS: project structure backfill and Gate 5 staged lifecycle isolation\n";
    return 0;
}
