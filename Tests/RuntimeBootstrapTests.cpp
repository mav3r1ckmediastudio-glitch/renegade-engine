#include "RuntimeBootstrap.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include <wiArchive.h>
#include <wiConfig.h>
#include <wiScene.h>

namespace
{
    namespace fs = std::filesystem;

    struct TemporaryDirectory
    {
        fs::path path;

        ~TemporaryDirectory()
        {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
    };

    int Fail(const fs::path& root, const char* message)
    {
        std::error_code ignored;
        fs::remove_all(root, ignored);
        std::cerr << "RenegadeRuntimeBootstrapTests: " << message << '\n';
        return 1;
    }

    bool WriteDescriptor(
        const fs::path& descriptor,
        const std::string& name,
        const int version,
        const std::string& startupScene)
    {
        wi::config::File file;
        file.Open(descriptor.generic_u8string());
        file.Set("format", "renegade-project");
        file.Set("version", version);
        auto& project = file.GetSection("project");
        project.Set("name", name);
        project.Set("startup_scene", startupScene);
        file.Commit();
        return fs::is_regular_file(descriptor);
    }

    bool WriteMinimalScene(const fs::path& scenePath)
    {
        try
        {
            wi::scene::Scene scene;
            scene.Entity_CreateTransform("LP01 Bootstrap Entity");

            {
                wi::Archive archive(
                    scenePath.generic_u8string(),
                    false,
                    false);
                if (!archive.IsOpen())
                {
                    return false;
                }

                scene.Serialize(archive);
            }

            std::error_code sizeError;
            return fs::is_regular_file(scenePath, sizeError) &&
                !sizeError &&
                fs::file_size(scenePath, sizeError) >
                    sizeof(wi::Archive::Header) &&
                !sizeError;
        }
        catch (...)
        {
            return false;
        }
    }
}

int main()
{
    const auto unique = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    TemporaryDirectory temporary{
        fs::temp_directory_path() /
        fs::u8path(
            "renegade lp01 bootstrap tests " + std::to_string(unique))
    };
    fs::create_directories(temporary.path);

    const fs::path validRoot = temporary.path / "Valid Project With Spaces";
    const fs::path validScene =
        validRoot / "Content/Scenes/BootstrapTest.wiscene";
    const fs::path validDescriptor =
        validRoot / "Bootstrap Project.renegade";
    fs::create_directories(validScene.parent_path());

    if (!WriteMinimalScene(validScene))
    {
        return Fail(
            temporary.path,
            "could not create the minimal valid WISCENE fixture");
    }

    if (!WriteDescriptor(
            validDescriptor,
            "LP01 Bootstrap Project",
            1,
            "Content/Scenes/BootstrapTest.wiscene"))
    {
        return Fail(temporary.path, "could not create valid descriptor");
    }


    const auto parsed = renegade::runtime::ParseRuntimeLaunchArguments(
        std::vector<std::string>{
            "dx12",
            "--project",
            validDescriptor.generic_u8string()
        });
    if (!parsed.succeeded ||
        parsed.projectDescriptorPath != validDescriptor.generic_u8string())
    {
        return Fail(
            temporary.path,
            "quoted/path-with-spaces launch argument was not preserved");
    }

    const auto duplicate = renegade::runtime::ParseRuntimeLaunchArguments(
        std::vector<std::string>{
            "--project",
            validDescriptor.generic_u8string(),
            "--project=" + validDescriptor.generic_u8string()
        });
    if (duplicate.succeeded ||
        duplicate.code !=
            renegade::runtime::RuntimeBootstrapCode::InvalidArguments)
    {
        return Fail(
            temporary.path,
            "duplicate --project arguments were not rejected");
    }

    const auto missingArgument =
        renegade::runtime::ParseRuntimeLaunchArguments({"vulkan"});
    if (missingArgument.succeeded ||
        missingArgument.code !=
            renegade::runtime::RuntimeBootstrapCode::MissingProjectArgument)
    {
        return Fail(
            temporary.path,
            "missing --project argument was not rejected");
    }


    auto resolved =
        renegade::runtime::ResolveRuntimeProject(parsed);
    if (!resolved.succeeded ||
        resolved.project.name != "LP01 Bootstrap Project" ||
        fs::u8path(resolved.startupScenePath) !=
            validScene.lexically_normal())
    {
        return Fail(
            temporary.path,
            "valid Runtime project did not resolve its startup scene");
    }


    renegade::bridge::SceneService scenes;
    auto loaded = renegade::runtime::LoadRuntimeProjectScene(
        scenes,
        resolved);
    if (!loaded.succeeded ||
        loaded.entityCount == 0 ||
        fs::u8path(scenes.CurrentPath()) != validScene.lexically_normal())
    {
        return Fail(
            temporary.path,
            "valid project-selected WISCENE did not load through SceneService");
    }


    const fs::path invalidVersionRoot =
        temporary.path / "Invalid Version";
    const fs::path invalidVersionDescriptor =
        invalidVersionRoot / "InvalidVersion.renegade";
    fs::create_directories(invalidVersionRoot);
    if (!WriteDescriptor(
            invalidVersionDescriptor,
            "Invalid Version",
            99,
            "Content/Scenes/BootstrapTest.wiscene"))
    {
        return Fail(
            temporary.path,
            "could not create invalid-version descriptor");
    }
    auto invalidVersion = renegade::runtime::ResolveRuntimeProject(
        renegade::runtime::ParseRuntimeLaunchArguments(
            {"--project", invalidVersionDescriptor.generic_u8string()}));
    if (invalidVersion.succeeded ||
        invalidVersion.code !=
            renegade::runtime::RuntimeBootstrapCode::ProjectRejected ||
        invalidVersion.message.find("Unsupported") == std::string::npos)
    {
        return Fail(
            temporary.path,
            "unsupported project version did not fail closed");
    }

    const fs::path escapeRoot = temporary.path / "Escaping Path";
    const fs::path escapeDescriptor =
        escapeRoot / "EscapingPath.renegade";
    fs::create_directories(escapeRoot);
    if (!WriteDescriptor(
            escapeDescriptor,
            "Escaping Path",
            1,
            "../Outside.wiscene"))
    {
        return Fail(
            temporary.path,
            "could not create path-escape descriptor");
    }
    auto escaping = renegade::runtime::ResolveRuntimeProject(
        renegade::runtime::ParseRuntimeLaunchArguments(
            {"--project", escapeDescriptor.generic_u8string()}));
    if (escaping.succeeded ||
        escaping.code !=
            renegade::runtime::RuntimeBootstrapCode::ProjectRejected)
    {
        return Fail(
            temporary.path,
            "escaping startup path did not fail closed");
    }

    const fs::path missingSceneRoot =
        temporary.path / "Missing Scene";
    const fs::path missingSceneDescriptor =
        missingSceneRoot / "MissingScene.renegade";
    fs::create_directories(missingSceneRoot);
    if (!WriteDescriptor(
            missingSceneDescriptor,
            "Missing Scene",
            1,
            "Content/Scenes/Missing.wiscene"))
    {
        return Fail(
            temporary.path,
            "could not create missing-scene descriptor");
    }
    auto missingScene = renegade::runtime::ResolveRuntimeProject(
        renegade::runtime::ParseRuntimeLaunchArguments(
            {"--project", missingSceneDescriptor.generic_u8string()}));
    if (missingScene.succeeded ||
        missingScene.code !=
            renegade::runtime::RuntimeBootstrapCode::ProjectRejected ||
        missingScene.message.find("missing") == std::string::npos)
    {
        return Fail(
            temporary.path,
            "missing startup scene did not fail closed");
    }

    const fs::path corruptRoot = temporary.path / "Corrupt Scene";
    const fs::path corruptScene =
        corruptRoot / "Content/Scenes/Corrupt.wiscene";
    const fs::path corruptDescriptor =
        corruptRoot / "CorruptScene.renegade";
    fs::create_directories(corruptScene.parent_path());
    std::ofstream(corruptScene, std::ios::binary)
        << "not a Wicked scene archive";
    if (!WriteDescriptor(
            corruptDescriptor,
            "Corrupt Scene",
            1,
            "Content/Scenes/Corrupt.wiscene"))
    {
        return Fail(
            temporary.path,
            "could not create corrupt-scene descriptor");
    }

    auto corrupt = renegade::runtime::ResolveRuntimeProject(
        renegade::runtime::ParseRuntimeLaunchArguments(
            {"--project", corruptDescriptor.generic_u8string()}));
    if (!corrupt.succeeded)
    {
        return Fail(
            temporary.path,
            "corrupt scene fixture was rejected before the scene-load boundary");
    }
    corrupt = renegade::runtime::LoadRuntimeProjectScene(
        scenes,
        std::move(corrupt));
    if (corrupt.succeeded ||
        corrupt.code !=
            renegade::runtime::RuntimeBootstrapCode::SceneLoadFailed)
    {
        return Fail(
            temporary.path,
            "corrupt WISCENE did not produce structured scene-load failure");
    }


    const fs::path logPath =
        temporary.path / "Logs/RuntimeBootstrap.log";
    std::string logError;
    if (!renegade::runtime::WriteRuntimeBootstrapLog(
            loaded,
            logPath.generic_u8string(),
            logError) ||
        !fs::is_regular_file(logPath))
    {
        return Fail(
            temporary.path,
            "Runtime bootstrap evidence log was not written");
    }


    std::ifstream log(logPath, std::ios::binary);
    const std::string logText(
        (std::istreambuf_iterator<char>(log)),
        std::istreambuf_iterator<char>());
    if (logText.find("status=PASS") == std::string::npos ||
        logText.find("code=SUCCESS") == std::string::npos ||
        logText.find("LP01 Bootstrap Project") == std::string::npos)
    {
        return Fail(
            temporary.path,
            "Runtime bootstrap log did not contain structured evidence");
    }

    std::cout
        << "PASS: LP01 project-aware Runtime bootstrap contract\n";
    return 0;
}
