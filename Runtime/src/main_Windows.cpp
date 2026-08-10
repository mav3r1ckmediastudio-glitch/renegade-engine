#include "RuntimeApplication.h"
#include "RuntimePackageBootstrap.h"

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

    void WriteBootstrapEvidence(
        const renegade::runtime::RuntimeBootstrapResult& result)
    {
        std::string logError;
        const bool logged = renegade::runtime::WriteRuntimeBootstrapLog(
            result,
            BootstrapLogPath,
            logError);
        (void)logged;
    }

    int ReportBootstrapFailure(
        const renegade::runtime::RuntimeBootstrapResult& result,
        const bool showDialog = true)
    {
        std::string logError;
        const bool logged = renegade::runtime::WriteRuntimeBootstrapLog(
            result,
            BootstrapLogPath,
            logError);

        std::string message = result.message;
        message += "\n\nCode: ";
        message += renegade::runtime::RuntimeBootstrapCodeName(result.code);
        if (logged)
        {
            message += "\nLog: ";
            message += BootstrapLogPath;
        }
        else
        {
            message += "\nLog error: ";
            message += logError;
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
            PostQuitMessage(0);
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

    auto bootstrap = renegade::runtime::ResolveRuntimeLaunch(
        processArguments,
        executablePath);
    bootstrap =
        renegade::runtime::ResolveRuntimeProject(std::move(bootstrap));
    if (!bootstrap.succeeded)
    {
        return ReportBootstrapFailure(bootstrap, !testLevelLaunch);
    }

    application.SetBootstrapResult(bootstrap);

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
                WriteBootstrapEvidence(application.StartupResult());
            }

            if (!startupHandled && application.StartupFinished())
            {
                startupHandled = true;
                const auto& result = application.StartupResult();
                if (!result.succeeded)
                {
                    const int exitCode = ReportBootstrapFailure(
                        result,
                        !testLevelLaunch);
                    PostQuitMessage(exitCode);
                    continue;
                }

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
                            ReportBootstrapFailure(failure, false);
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
                            ReportBootstrapFailure(failure, false);
                        PostQuitMessage(exitCode);
                        continue;
                    }
                }
            }

            if (!quitWindowRequested && application.QuitRequested())
            {
                quitWindowRequested = true;
                DestroyWindow(window);
            }
        }
    }

    wi::jobsystem::ShutDown();
    return static_cast<int>(message.wParam);
}
