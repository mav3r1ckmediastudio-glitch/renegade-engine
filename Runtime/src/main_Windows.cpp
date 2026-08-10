#include "RuntimeApplication.h"
#include "RuntimePackageBootstrap.h"

#include "renegade/bridge/BuildVerificationService.h"
#include "wiBacklog.h"

#include <Windows.h>
#include <shellapi.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace
{
    namespace fs = std::filesystem;

    constexpr const char* BootstrapLogPath = "Logs/RuntimeBootstrap.log";
    constexpr const char* ReadyEventArgument = "--renegade-ready-event=";
    constexpr const char* SmokeAutoPlayArgument = "--renegade-smoke-autoplay";
    constexpr const char* SmokeExitArgument = "--renegade-smoke-exit";
    constexpr const char* CapabilityProbeArgument =
        "--renegade-capability-probe";
    renegade::runtime::RuntimeApplication application;

    std::string WideToUtf8(const wchar_t* value)
    {
        if (value == nullptr || *value == L'\0')
        {
            return {};
        }

        const int byteCount = WideCharToMultiByte(
            CP_UTF8,
            0,
            value,
            -1,
            nullptr,
            0,
            nullptr,
            nullptr);
        if (byteCount <= 1)
        {
            return {};
        }

        std::string result(static_cast<std::size_t>(byteCount), '\0');
        WideCharToMultiByte(
            CP_UTF8,
            0,
            value,
            -1,
            result.data(),
            byteCount,
            nullptr,
            nullptr);
        result.resize(static_cast<std::size_t>(byteCount - 1));
        return result;
    }

    std::wstring Utf8ToWide(const std::string& value)
    {
        if (value.empty())
        {
            return {};
        }

        const int characterCount = MultiByteToWideChar(
            CP_UTF8,
            0,
            value.c_str(),
            static_cast<int>(value.size()),
            nullptr,
            0);
        if (characterCount <= 0)
        {
            return std::wstring(value.begin(), value.end());
        }

        std::wstring result(
            static_cast<std::size_t>(characterCount),
            L'\0');
        MultiByteToWideChar(
            CP_UTF8,
            0,
            value.c_str(),
            static_cast<int>(value.size()),
            result.data(),
            characterCount);
        return result;
    }

    std::wstring ReadyEventName(
        const std::vector<std::string>& arguments)
    {
        for (const auto& argument : arguments)
        {
            if (argument.rfind(ReadyEventArgument, 0) == 0)
            {
                return Utf8ToWide(
                    argument.substr(std::strlen(ReadyEventArgument)));
            }
        }
        return {};
    }

    bool HasArgument(
        const std::vector<std::string>& arguments,
        const char* expected)
    {
        for (const std::string& argument : arguments)
        {
            if (argument == expected)
                return true;
        }
        return false;
    }

    std::string RequestedGraphicsBackend(
        const std::vector<std::string>& arguments)
    {
        return HasArgument(arguments, "vulkan") ? "Vulkan" : "DX12";
    }

    std::vector<std::string> CollectProcessArguments()
    {
        int argumentCount = 0;
        wchar_t** arguments =
            CommandLineToArgvW(GetCommandLineW(), &argumentCount);
        if (arguments == nullptr)
        {
            return {};
        }

        std::vector<std::string> result;
        result.reserve(
            argumentCount > 1 ?
                static_cast<std::size_t>(argumentCount - 1) :
                0u);
        for (int index = 1; index < argumentCount; ++index)
        {
            result.push_back(WideToUtf8(arguments[index]));
        }

        LocalFree(arguments);
        return result;
    }

    std::string ExecutablePathUtf8()
    {
        std::wstring executablePath(32768, L'\0');
        const DWORD length = GetModuleFileNameW(
            nullptr,
            executablePath.data(),
            static_cast<DWORD>(executablePath.size()));
        if (length == 0 || length >= executablePath.size())
        {
            return {};
        }
        executablePath.resize(length);
        return WideToUtf8(executablePath.c_str());
    }

    void SetExecutableWorkingDirectory()
    {
        std::wstring executablePath(32768, L'\0');
        const DWORD length = GetModuleFileNameW(
            nullptr,
            executablePath.data(),
            static_cast<DWORD>(executablePath.size()));
        if (length == 0 || length >= executablePath.size())
        {
            return;
        }

        executablePath.resize(length);
        const fs::path directory =
            fs::path(executablePath).parent_path();
        if (!directory.empty())
        {
            const std::wstring directoryText = directory.wstring();
            SetCurrentDirectoryW(directoryText.c_str());
        }
    }

    std::wstring GraphicsBackendTitle(const std::string& projectName)
    {
        std::wstring title = L"Renegade Runtime";
        if (!projectName.empty())
        {
            title += L" - ";
            title += Utf8ToWide(projectName);
        }

        const auto* device = wi::graphics::GetDevice();
        if (device != nullptr &&
            std::strcmp(device->GetTag(), "[Vulkan]") == 0)
        {
            title += L" [Vulkan]";
        }
        else
        {
            title += L" [DX12]";
        }
        return title;
    }

    std::string ActualGraphicsBackend()
    {
        const auto* device = wi::graphics::GetDevice();
        if (device != nullptr &&
            std::strcmp(device->GetTag(), "[Vulkan]") == 0)
        {
            return "Vulkan";
        }
        return device == nullptr ? std::string{} : "DX12";
    }

    std::string LocalAppDataUtf8()
    {
        const DWORD required = GetEnvironmentVariableW(
            L"LOCALAPPDATA",
            nullptr,
            0);
        if (required <= 1)
            return {};
        std::wstring value(static_cast<std::size_t>(required), L'\0');
        const DWORD written = GetEnvironmentVariableW(
            L"LOCALAPPDATA",
            value.data(),
            required);
        if (written == 0 || written >= required)
            return {};
        value.resize(written);
        return WideToUtf8(value.c_str());
    }

    std::string RuntimeEvidencePath(
        const renegade::runtime::RuntimeBootstrapResult& result)
    {
        if (!result.packageRelativeLaunch)
            return BootstrapLogPath;

        const std::string localAppData = LocalAppDataUtf8();
        if (localAppData.empty())
            return {};

        std::string identity = result.saveDataId;
        if (identity.empty())
            identity = result.project.projectId;
        if (identity.empty())
            identity = "unknown-package";

        return (fs::u8path(localAppData) /
            "RenegadeEngine" /
            fs::u8path(identity) /
            "Logs" /
            "RuntimeBootstrap.log").generic_u8string();
    }

    bool ConfigurePackageRuntimeLogging(
        const renegade::runtime::RuntimeBootstrapResult& result,
        const std::string& evidencePath,
        std::string& error)
    {
        if (!result.packageRelativeLaunch)
        {
            error.clear();
            return true;
        }
        if (evidencePath.empty())
        {
            error = "LOCALAPPDATA is unavailable for packaged Runtime logging.";
            return false;
        }

        const fs::path logDirectory =
            fs::u8path(evidencePath).parent_path();
        std::error_code ec;
        fs::create_directories(logDirectory, ec);
        if (ec)
        {
            error = "Could not create packaged Runtime log directory: " +
                ec.message();
            return false;
        }

        wi::backlog::SetLogFile(
            (logDirectory / "WickedRuntime.log").generic_u8string());
        error.clear();
        return true;
    }

    void WriteBootstrapEvidence(
        const renegade::runtime::RuntimeBootstrapResult& result,
        const std::string& logPath)
    {
        if (logPath.empty())
            return;
        std::string logError;
        const bool logged = renegade::runtime::WriteRuntimeBootstrapLog(
            result,
            logPath,
            logError);
        (void)logged;
    }

    int ReportBootstrapFailure(
        const renegade::runtime::RuntimeBootstrapResult& result,
        const std::string& logPath,
        const bool showDialog = true)
    {
        std::string logError;
        const bool logged = !logPath.empty() &&
            renegade::runtime::WriteRuntimeBootstrapLog(
                result,
                logPath,
                logError);

        std::string message = result.message;
        message += "\n\nCode: ";
        message += renegade::runtime::RuntimeBootstrapCodeName(result.code);
        if (logged)
        {
            message += "\nLog: ";
            message += logPath;
        }
        else
        {
            message += "\nLog error: ";
            message += logPath.empty()
                ? "LOCALAPPDATA is unavailable."
                : logError;
        }

        if (showDialog)
        {
            const std::wstring wideMessage = Utf8ToWide(message);
            MessageBoxW(
                nullptr,
                wideMessage.c_str(),
                L"Renegade Runtime startup failed",
                MB_OK | MB_ICONERROR);
        }

        return static_cast<int>(result.code);
    }

    bool VulkanLoaderAvailable()
    {
        const HMODULE loader = LoadLibraryExW(
            L"vulkan-1.dll",
            nullptr,
            LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (loader == nullptr)
            return false;
        const bool available =
            GetProcAddress(loader, "vkGetInstanceProcAddr") != nullptr;
        FreeLibrary(loader);
        return available;
    }

    int RunCapabilityProbe(
        renegade::runtime::RuntimeBootstrapResult result)
    {
        const std::string logPath = RuntimeEvidencePath(result);
        if (result.graphicsBackendRequested == "Vulkan")
        {
            if (VulkanLoaderAvailable())
            {
                result.succeeded = true;
                result.code = renegade::runtime::RuntimeBootstrapCode::Success;
                result.graphicsCapability = "VULKAN_LOADER_AVAILABLE";
                result.message =
                    "Vulkan capability probe found the system Vulkan loader and vkGetInstanceProcAddr.";
                WriteBootstrapEvidence(result, logPath);
                return 0;
            }

            result.succeeded = false;
            result.code =
                renegade::runtime::RuntimeBootstrapCode::GraphicsPrerequisiteMissing;
            result.graphicsCapability = "VULKAN_LOADER_MISSING";
            result.message =
                "Vulkan capability probe could not load system vulkan-1.dll with vkGetInstanceProcAddr.";
            WriteBootstrapEvidence(result, logPath);
            return static_cast<int>(result.code);
        }

        result.succeeded = true;
        result.code = renegade::runtime::RuntimeBootstrapCode::Success;
        result.graphicsCapability = "DX12_SYSTEM_COMPONENT_DECLARED";
        result.message =
            "DX12 capability probe uses the Windows system Direct3D 12 prerequisite; full DX12 startup is proven by the Gate 4 smoke launch.";
        WriteBootstrapEvidence(result, logPath);
        return 0;
    }

    LRESULT CALLBACK RenegadeRuntimeWindowProc(
        const HWND window,
        const UINT message,
        const WPARAM wParam,
        const LPARAM lParam)
    {
        switch (message)
        {
        case WM_SIZE:
            if (application.is_window_active)
            {
                application.SetWindow(window);
            }
            return 0;

        case WM_DPICHANGED:
        {
            const auto* suggested = reinterpret_cast<const RECT*>(lParam);
            SetWindowPos(
                window,
                nullptr,
                suggested->left,
                suggested->top,
                suggested->right - suggested->left,
                suggested->bottom - suggested->top,
                SWP_NOACTIVATE | SWP_NOZORDER);
            if (application.is_window_active)
            {
                application.SetWindow(window);
            }
            return 0;
        }

        case WM_INPUT:
            wi::input::rawinput::ParseMessage(reinterpret_cast<void*>(lParam));
            return 0;

        case WM_KILLFOCUS:
            application.is_window_active = false;
            return 0;

        case WM_SETFOCUS:
            application.is_window_active = true;
            return 0;

        case WM_DESTROY:
            PostQuitMessage(application.ExitCode());
            return 0;

        default:
            return DefWindowProcW(window, message, wParam, lParam);
        }
    }
}

int APIENTRY wWinMain(
    _In_ HINSTANCE instance,
    _In_opt_ HINSTANCE,
    _In_ LPWSTR commandLine,
    _In_ int showCommand)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    wi::arguments::Parse(commandLine);
    const std::string executablePath = ExecutablePathUtf8();
    SetExecutableWorkingDirectory();

    const auto processArguments = CollectProcessArguments();
    const std::wstring readyEventName = ReadyEventName(processArguments);
    const bool testLevelLaunch = !readyEventName.empty();
    const bool smokeAutoPlay =
        HasArgument(processArguments, SmokeAutoPlayArgument);
    const bool smokeExit =
        HasArgument(processArguments, SmokeExitArgument);
    const bool capabilityProbe =
        HasArgument(processArguments, CapabilityProbeArgument);

    auto bootstrap = renegade::runtime::ResolveRuntimeLaunch(
        processArguments,
        executablePath);
    bootstrap.graphicsBackendRequested =
        RequestedGraphicsBackend(processArguments);
    if (bootstrap.packageRelativeLaunch)
    {
        bootstrap.windowsPrerequisitePolicy =
            renegade::bridge::WindowsVcRuntimePrerequisitePolicy;
    }
    bootstrap =
        renegade::runtime::ResolveRuntimeProject(std::move(bootstrap));
    const std::string evidencePath = RuntimeEvidencePath(bootstrap);
    if (!bootstrap.succeeded)
    {
        return ReportBootstrapFailure(
            bootstrap,
            evidencePath,
            !testLevelLaunch && !capabilityProbe);
    }

    std::string runtimeLoggingError;
    if (!ConfigurePackageRuntimeLogging(
            bootstrap,
            evidencePath,
            runtimeLoggingError))
    {
        bootstrap.succeeded = false;
        bootstrap.code =
            renegade::runtime::RuntimeBootstrapCode::ProjectRejected;
        bootstrap.message = runtimeLoggingError;
        return ReportBootstrapFailure(
            bootstrap,
            evidencePath,
            !testLevelLaunch && !capabilityProbe);
    }

    if (capabilityProbe)
        return RunCapabilityProbe(std::move(bootstrap));

    application.SetBootstrapResult(bootstrap);
    application.SetSmokeOptions(smokeAutoPlay, smokeExit);

    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = RenegadeRuntimeWindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = L"RenegadeRuntimeWindow";

    if (RegisterClassExW(&windowClass) == 0)
    {
        return 1;
    }

    const std::wstring startingTitle =
        L"Renegade Runtime - Starting " +
        Utf8ToWide(bootstrap.project.name);
    const HWND window = CreateWindowW(
        windowClass.lpszClassName,
        startingTitle.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        0,
        1280,
        720,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (window == nullptr)
    {
        return 2;
    }

    ShowWindow(window, showCommand);
    application.SetWindow(window);

    bool startupHandled = false;
    bool quitWindowRequested = false;
    std::uint64_t writtenEvidenceRevision = 0;
    MSG message = {};
    while (message.message != WM_QUIT)
    {
        if (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        else
        {
            application.Run();

            if (application.EvidenceRevision() != writtenEvidenceRevision)
            {
                writtenEvidenceRevision = application.EvidenceRevision();
                WriteBootstrapEvidence(
                    application.StartupResult(),
                    evidencePath);
            }

            if (!startupHandled && application.StartupFinished())
            {
                startupHandled = true;
                const auto& result = application.StartupResult();
                if (!result.succeeded)
                {
                    const int exitCode = ReportBootstrapFailure(
                        result,
                        evidencePath,
                        !testLevelLaunch);
                    PostQuitMessage(exitCode);
                    continue;
                }

                const std::string actualBackend = ActualGraphicsBackend();
                application.SetGraphicsRuntimeEvidence(
                    actualBackend,
                    actualBackend.empty() ? "DEVICE_MISSING" : "STARTED");

                const std::wstring title =
                    GraphicsBackendTitle(result.project.name);
                SetWindowTextW(window, title.c_str());

                if (testLevelLaunch)
                {
                    const HANDLE readyEvent = OpenEventW(
                        EVENT_MODIFY_STATE,
                        FALSE,
                        readyEventName.c_str());
                    if (readyEvent == nullptr)
                    {
                        auto failure = result;
                        failure.succeeded = false;
                        failure.code =
                            renegade::runtime::RuntimeBootstrapCode::InvalidArguments;
                        failure.message =
                            "Test Level Runtime could not open the Studio "
                            "readiness event.";
                        const int exitCode =
                            ReportBootstrapFailure(
                                failure,
                                evidencePath,
                                false);
                        PostQuitMessage(exitCode);
                        continue;
                    }

                    const BOOL readySignaled = SetEvent(readyEvent);
                    CloseHandle(readyEvent);
                    if (!readySignaled)
                    {
                        auto failure = result;
                        failure.succeeded = false;
                        failure.code =
                            renegade::runtime::RuntimeBootstrapCode::InvalidArguments;
                        failure.message =
                            "Test Level Runtime could not signal Studio that "
                            "startup completed.";
                        const int exitCode =
                            ReportBootstrapFailure(
                                failure,
                                evidencePath,
                                false);
                        PostQuitMessage(exitCode);
                        continue;
                    }
                }
            }

            if (!quitWindowRequested && application.QuitRequested())
            {
                // Smoke can reach completion in the same Run() that first
                // exposes startup. Persist the post-device/action evidence
                // immediately before the window is destroyed.
                WriteBootstrapEvidence(
                    application.StartupResult(),
                    evidencePath);
                quitWindowRequested = true;
                DestroyWindow(window);
            }
        }
    }

    wi::jobsystem::ShutDown();
    return static_cast<int>(message.wParam);
}