#include "RuntimeBootstrap.h"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <utility>

namespace
{
    namespace fs = std::filesystem;

    bool IsWithinProjectRoot(
        const fs::path& projectRoot,
        const fs::path& startupScene,
        std::string& error)
    {
        std::error_code pathError;
        const fs::path canonicalRoot =
            fs::weakly_canonical(projectRoot, pathError);
        if (pathError)
        {
            error = "Could not canonicalize the project root: " +
                pathError.message();
            return false;
        }

        const fs::path canonicalScene =
            fs::weakly_canonical(startupScene, pathError);
        if (pathError)
        {
            error = "Could not canonicalize the project startup scene: " +
                pathError.message();
            return false;
        }

        const fs::path relative =
            fs::relative(canonicalScene, canonicalRoot, pathError);
        if (pathError || relative.empty() || relative.is_absolute())
        {
            error = "The project startup scene is outside the project root.";
            return false;
        }

        const bool escapes = std::any_of(
            relative.begin(),
            relative.end(),
            [](const fs::path& part)
            {
                return part == "..";
            });
        if (escapes)
        {
            error = "The project startup scene escapes the project root.";
            return false;
        }

        error.clear();
        return true;
    }

    renegade::runtime::RuntimeBootstrapResult Fail(
        renegade::runtime::RuntimeBootstrapResult result,
        const renegade::runtime::RuntimeBootstrapCode code,
        std::string message)
    {
        result.succeeded = false;
        result.code = code;
        result.message = std::move(message);
        result.entityCount = 0;
        return result;
    }
}

namespace renegade::runtime
{
    RuntimeBootstrapResult ParseRuntimeLaunchArguments(
        const std::vector<std::string>& arguments)
    {
        RuntimeBootstrapResult result;
        bool foundProject = false;

        for (std::size_t index = 0; index < arguments.size(); ++index)
        {
            const std::string& argument = arguments[index];
            std::string projectPath;

            if (argument == "--project")
            {
                if (index + 1 >= arguments.size() ||
                    arguments[index + 1].empty())
                {
                    return Fail(
                        std::move(result),
                        RuntimeBootstrapCode::InvalidArguments,
                        "The --project argument requires a .renegade file path.");
                }
                projectPath = arguments[++index];
            }
            else if (argument.rfind("--project=", 0) == 0)
            {
                projectPath = argument.substr(std::string("--project=").size());
                if (projectPath.empty())
                {
                    return Fail(
                        std::move(result),
                        RuntimeBootstrapCode::InvalidArguments,
                        "The --project argument requires a .renegade file path.");
                }
            }
            else
            {
                // Wicked still consumes graphics flags such as dx12/vulkan.
                // Every unrelated argument is deliberately ignored here.
                continue;
            }

            if (foundProject)
            {
                return Fail(
                    std::move(result),
                    RuntimeBootstrapCode::InvalidArguments,
                    "The Runtime received more than one --project argument.");
            }

            foundProject = true;
            result.projectDescriptorPath = std::move(projectPath);
        }

        if (!foundProject)
        {
            return Fail(
                std::move(result),
                RuntimeBootstrapCode::MissingProjectArgument,
                "Renegade Runtime requires --project <path-to-project.renegade>.");
        }

        result.succeeded = true;
        result.code = RuntimeBootstrapCode::Success;
        result.message = "Runtime launch arguments accepted.";
        return result;
    }

    RuntimeBootstrapResult ResolveRuntimeProject(RuntimeBootstrapResult result)
    {
        if (!result.succeeded)
        {
            return result;
        }

        bridge::ProjectService projects;
        bridge::ProjectMetadata metadata;
        std::string projectError;
        if (!projects.InspectProject(
                result.projectDescriptorPath,
                metadata,
                projectError))
        {
            return Fail(
                std::move(result),
                RuntimeBootstrapCode::ProjectRejected,
                projectError);
        }

        const fs::path projectRoot = fs::u8path(metadata.rootPath);
        const fs::path startupScene =
            (projectRoot / fs::u8path(metadata.startupScene))
                .lexically_normal();

        std::string containmentError;
        if (!IsWithinProjectRoot(
                projectRoot,
                startupScene,
                containmentError))
        {
            return Fail(
                std::move(result),
                RuntimeBootstrapCode::StartupSceneOutsideProject,
                containmentError);
        }

        result.project = std::move(metadata);
        result.projectDescriptorPath = result.project.descriptorPath;
        result.startupScenePath = startupScene.generic_u8string();
        result.succeeded = true;
        result.code = RuntimeBootstrapCode::Success;
        result.message =
            "Project manifest accepted: " + result.project.name;
        return result;
    }

    RuntimeBootstrapResult LoadRuntimeProjectScene(
        bridge::SceneService& scenes,
        RuntimeBootstrapResult result)
    {
        if (!result.succeeded)
        {
            return result;
        }

        if (!scenes.LoadScene(result.startupScenePath))
        {
            return Fail(
                std::move(result),
                RuntimeBootstrapCode::SceneLoadFailed,
                "Could not load the project startup scene: " +
                    scenes.LastError());
        }

        result.entityCount = scenes.EntityCount();
        result.succeeded = true;
        result.code = RuntimeBootstrapCode::Success;
        result.message =
            "Loaded project startup scene: " + result.startupScenePath;
        return result;
    }

    const char* RuntimeBootstrapCodeName(
        const RuntimeBootstrapCode code) noexcept
    {
        switch (code)
        {
        case RuntimeBootstrapCode::Success:
            return "SUCCESS";
        case RuntimeBootstrapCode::MissingProjectArgument:
            return "MISSING_PROJECT_ARGUMENT";
        case RuntimeBootstrapCode::InvalidArguments:
            return "INVALID_ARGUMENTS";
        case RuntimeBootstrapCode::ProjectRejected:
            return "PROJECT_REJECTED";
        case RuntimeBootstrapCode::StartupSceneOutsideProject:
            return "STARTUP_SCENE_OUTSIDE_PROJECT";
        case RuntimeBootstrapCode::SceneLoadFailed:
            return "SCENE_LOAD_FAILED";
        default:
            return "UNKNOWN";
        }
    }

    bool WriteRuntimeBootstrapLog(
        const RuntimeBootstrapResult& result,
        const std::string& logPath,
        std::string& error)
    {
        try
        {
            const fs::path path = fs::u8path(logPath);
            if (!path.parent_path().empty())
            {
                fs::create_directories(path.parent_path());
            }

            std::ofstream stream(path, std::ios::binary | std::ios::trunc);
            if (!stream)
            {
                error = "Could not open Runtime bootstrap log: " +
                    path.generic_u8string();
                return false;
            }

            stream
                << "schema=renegade-runtime-bootstrap-v1\n"
                << "status=" << (result.succeeded ? "PASS" : "FAIL") << '\n'
                << "code=" << RuntimeBootstrapCodeName(result.code) << '\n'
                << "exit_code=" << static_cast<int>(result.code) << '\n'
                << "message=" << result.message << '\n'
                << "project_descriptor=" << result.projectDescriptorPath << '\n'
                << "project_id=" << result.project.projectId << '\n'
                << "project_name=" << result.project.name << '\n'
                << "project_root=" << result.project.rootPath << '\n'
                << "startup_scene=" << result.startupScenePath << '\n'
                << "entity_count=" << result.entityCount << '\n';

            if (!stream)
            {
                error = "Could not complete Runtime bootstrap log: " +
                    path.generic_u8string();
                return false;
            }

            error.clear();
            return true;
        }
        catch (const std::exception& exception)
        {
            error =
                std::string("Could not write Runtime bootstrap log: ") +
                exception.what();
            return false;
        }
    }
}
