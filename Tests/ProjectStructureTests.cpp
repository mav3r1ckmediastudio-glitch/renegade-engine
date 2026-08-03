#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <wiConfig.h>
#include "renegade/bridge/ProjectService.h"

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

    std::error_code ignored;
    fs::remove_all(root, ignored);
    std::cout << "PASS: existing project structure backfilled on open\n";
    return 0;
}
