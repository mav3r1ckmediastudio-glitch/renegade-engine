#include "RuntimeBootstrap.h"

#include "renegade/bridge/ScreenService.h"

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
        const fs::path& candidatePath,
        const char* subject,
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

        const fs::path canonicalCandidate =
            fs::weakly_canonical(candidatePath, pathError);
        if (pathError)
        {
            error = std::string("Could not canonicalize the project ") +
                subject + ": " + pathError.message();
            return false;
        }

        const fs::path relative =
            fs::relative(canonicalCandidate, canonicalRoot, pathError);
        if (pathError || relative.empty() || relative.is_absolute())
        {
            error = std::string("The project ") + subject +
                " is outside the project root.";
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
            error = std::string("The project ") + subject +
                " escapes the project root.";
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

            if (argument == "--flow-outcome")
            {
                if (index + 1 >= arguments.size() ||
                    arguments[index + 1].empty())
                {
                    return Fail(
                        std::move(result),
                        RuntimeBootstrapCode::InvalidArguments,
                        "The --flow-outcome argument requires a named outcome.");
                }
                result.flowOutcomes.push_back(arguments[++index]);
                continue;
            }
            if (argument.rfind("--flow-outcome=", 0) == 0)
            {
                const std::string outcome = argument.substr(
                    std::string("--flow-outcome=").size());
                if (outcome.empty())
                {
                    return Fail(
                        std::move(result),
                        RuntimeBootstrapCode::InvalidArguments,
                        "The --flow-outcome argument requires a named outcome.");
                }
                result.flowOutcomes.push_back(outcome);
                continue;
            }

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
                // Gate 4 smoke/capability switches are consumed by platform
                // code and deliberately remain unrelated to project parsing.
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
        std::string startupScenePath;
        if (!metadata.startupScene.empty())
        {
            const fs::path startupScene =
                (projectRoot / fs::u8path(metadata.startupScene))
                    .lexically_normal();
            std::string containmentError;
            if (!IsWithinProjectRoot(
                    projectRoot,
                    startupScene,
                    "startup scene",
                    containmentError))
            {
                return Fail(
                    std::move(result),
                    RuntimeBootstrapCode::StartupSceneOutsideProject,
                    containmentError);
            }
            startupScenePath = startupScene.generic_u8string();
        }

        std::string startupFlowPath;
        if (!metadata.startupFlow.empty())
        {
            std::string flowError;
            if (!bridge::ResolveStoryFlowDocumentPath(
                    metadata.rootPath,
                    metadata.projectId,
                    metadata.startupFlowId,
                    metadata.startupFlow,
                    startupFlowPath,
                    flowError))
            {
                return Fail(
                    std::move(result),
                    RuntimeBootstrapCode::StartupFlowRejected,
                    "Could not resolve the project startup Story Flow: " +
                        flowError);
            }
        }
        else if (!result.flowOutcomes.empty())
        {
            return Fail(
                std::move(result),
                RuntimeBootstrapCode::FlowRejected,
                "The project does not declare startup_flow, so diagnostic "
                "flow outcomes cannot be executed.");
        }

        std::string startupScreenPath;
        if (!metadata.startupScreen.empty())
        {
            std::string screenError;
            if (!bridge::ResolveRuntimeScreenDocumentPath(
                    metadata.rootPath,
                    metadata.projectId,
                    metadata.startupScreenId,
                    metadata.startupScreen,
                    startupScreenPath,
                    screenError))
            {
                return Fail(
                    std::move(result),
                    RuntimeBootstrapCode::StartupScreenRejected,
                    "Could not resolve the project startup Runtime screen: " +
                        screenError);
            }
        }

        result.project = std::move(metadata);
        result.projectDescriptorPath = result.project.descriptorPath;
        result.startupScenePath = std::move(startupScenePath);
        result.startupFlowPath = std::move(startupFlowPath);
        result.startupScreenPath = std::move(startupScreenPath);
        result.succeeded = true;
        result.code = RuntimeBootstrapCode::Success;
        result.message =
            "Project manifest accepted: " + result.project.name;
        return result;
    }

    RuntimeBootstrapResult LoadRuntimeProjectScene(
        bridge::SceneService& scenes,
        RuntimeBootstrapResult result,
        bridge::MaterialTextureResourceLoader authoringTextureLoader)
    {
        if (!result.succeeded)
        {
            return result;
        }

        if (result.startupScenePath.empty())
        {
            return Fail(
                std::move(result),
                RuntimeBootstrapCode::SceneLoadFailed,
                "The project does not declare a startup Scene; Runtime must enter through its Story Flow.");
        }

        if (!scenes.LoadScene(result.startupScenePath))
        {
            return Fail(
                std::move(result),
                RuntimeBootstrapCode::SceneLoadFailed,
                "Could not load the project startup scene: " +
                    scenes.LastError());
        }

        std::string refreshError;
        if (!RefreshRuntimeReusableAssets(
                scenes, result, refreshError, std::move(authoringTextureLoader)))
        {
            return Fail(
                std::move(result),
                RuntimeBootstrapCode::SceneLoadFailed,
                "Could not refresh packaged reusable assets in startup scene: " +
                    refreshError);
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
        case RuntimeBootstrapCode::StartupFlowRejected:
            return "STARTUP_FLOW_REJECTED";
        case RuntimeBootstrapCode::FlowRejected:
            return "FLOW_REJECTED";
        case RuntimeBootstrapCode::FlowExecutionFailed:
            return "FLOW_EXECUTION_FAILED";
        case RuntimeBootstrapCode::StartupScreenRejected:
            return "STARTUP_SCREEN_REJECTED";
        case RuntimeBootstrapCode::ScreenLoadFailed:
            return "SCREEN_LOAD_FAILED";
        case RuntimeBootstrapCode::PackageIntegrityFailed:
            return "PACKAGE_INTEGRITY_FAILED";
        case RuntimeBootstrapCode::GraphicsPrerequisiteMissing:
            return "GRAPHICS_PREREQUISITE_MISSING";
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
                << "schema=renegade-runtime-bootstrap-v2\n"
                << "status=" << (result.succeeded ? "PASS" : "FAIL") << '\n'
                << "code=" << RuntimeBootstrapCodeName(result.code) << '\n'
                << "exit_code=" << static_cast<int>(result.code) << '\n'
                << "message=" << result.message << '\n'
                << "package_relative_launch="
                << (result.packageRelativeLaunch ? "true" : "false") << '\n'
                << "package_root=" << result.packageRootPath << '\n'
                << "package_integrity=" << result.packageIntegrityStatus << '\n'
                << "package_integrity_code=" << result.packageIntegrityCode << '\n'
                << "package_manifest_sha256=" << result.packageManifestSha256 << '\n'
                << "graphics_backend_requested="
                << result.graphicsBackendRequested << '\n'
                << "graphics_backend=" << result.graphicsBackend << '\n'
                << "graphics_capability=" << result.graphicsCapability << '\n'
                << "windows_prerequisite_policy="
                << result.windowsPrerequisitePolicy << '\n'
                << "smoke_status=" << result.smokeStatus << '\n'
                << "smoke_quit_reason=" << result.smokeQuitReason << '\n'
                << "project_descriptor=" << result.projectDescriptorPath << '\n'
                << "project_id=" << result.project.projectId << '\n'
                << "project_name=" << result.project.name << '\n'
                << "project_root=" << result.project.rootPath << '\n'
                << "startup_scene=" << result.startupScenePath << '\n'
                << "startup_flow_id=" << result.project.startupFlowId << '\n'
                << "startup_flow=" << result.startupFlowPath << '\n'
                << "startup_screen_id=" << result.project.startupScreenId << '\n'
                << "startup_screen=" << result.startupScreenPath << '\n'
                << "flow_document_id=" << result.flowDocumentId << '\n'
                << "flow_node_id=" << result.flowNodeId << '\n'
                << "flow_node_name=" << result.flowNodeName << '\n'
                << "flow_entry=" << result.flowEntry << '\n'
                << "flow_terminal="
                << bridge::FlowTerminalActionName(result.flowTerminalAction)
                << '\n'
                << "flow_trace_count=" << result.flowTrace.size() << '\n'
                << "screen_document_id=" << result.screenDocumentId << '\n'
                << "screen_loaded=" << (result.screenLoaded ? "true" : "false")
                << '\n'
                << "screen_was_loaded="
                << (result.screenWasLoaded ? "true" : "false") << '\n'
                << "screen_focused_widget=" << result.screenFocusedWidgetId << '\n'
                << "last_action_id=" << result.lastActionId << '\n'
                << "last_action_widget=" << result.lastActionWidgetId << '\n'
                << "last_action_input=" << result.lastActionInput << '\n'
                << "last_action_code=" << result.lastActionCode << '\n'
                << "last_action_message=" << result.lastActionMessage << '\n'
                << "last_action_sequence=" << result.lastActionSequence << '\n'
                << "entity_count=" << result.entityCount << '\n'
                << "reusable_asset_instances_discovered="
                << result.reusableAssetInstancesDiscovered << '\n'
                << "reusable_asset_instances_refreshed="
                << result.reusableAssetInstancesRefreshed << '\n'
                << "reusable_asset_refresh_trace_count="
                << result.reusableAssetRefreshTrace.size() << '\n';

            for (std::size_t index = 0; index < result.flowTrace.size(); ++index)
            {
                stream << "flow_trace_" << index << '='
                       << result.flowTrace[index] << '\n';
            }
            for (std::size_t index = 0;
                index < result.reusableAssetRefreshTrace.size(); ++index)
            {
                stream << "reusable_asset_refresh_" << index << '='
                       << result.reusableAssetRefreshTrace[index] << '\n';
            }

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
