#include <Windows.h>

#include <wiInitializer.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _DEBUG
#include <crtdbg.h>
#endif

namespace
{
    namespace fs = std::filesystem;

    constexpr const wchar_t* DiagnosticRootEnvironment =
        L"RENEGADE_GATE4_DIAGNOSTIC_ROOT";

    bool IsGate4SmokeProcess()
    {
        const wchar_t* commandLine = GetCommandLineW();
        return commandLine != nullptr &&
            (wcsstr(commandLine, L"--renegade-smoke-autoplay") != nullptr ||
             wcsstr(commandLine, L"--renegade-smoke-exit") != nullptr);
    }

    fs::path DiagnosticRoot()
    {
        const DWORD required =
            GetEnvironmentVariableW(DiagnosticRootEnvironment, nullptr, 0);
        if (required <= 1)
            return {};

        std::wstring value(static_cast<std::size_t>(required), L'\0');
        const DWORD written = GetEnvironmentVariableW(
            DiagnosticRootEnvironment,
            value.data(),
            required);
        if (written == 0 || written >= required)
            return {};
        value.resize(written);
        return fs::path(value);
    }

    fs::path LocalAppDataRoot()
    {
        const DWORD required = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
        if (required <= 1)
            return {};

        std::wstring value(static_cast<std::size_t>(required), L'\0');
        const DWORD written =
            GetEnvironmentVariableW(L"LOCALAPPDATA", value.data(), required);
        if (written == 0 || written >= required)
            return {};
        value.resize(written);
        return fs::path(value);
    }

    std::string WideToUtf8(const std::wstring& value)
    {
        if (value.empty())
            return {};
        const int required = WideCharToMultiByte(
            CP_UTF8,
            0,
            value.data(),
            static_cast<int>(value.size()),
            nullptr,
            0,
            nullptr,
            nullptr);
        if (required <= 0)
            return {};
        std::string result(static_cast<std::size_t>(required), '\0');
        WideCharToMultiByte(
            CP_UTF8,
            0,
            value.data(),
            static_cast<int>(value.size()),
            result.data(),
            required,
            nullptr,
            nullptr);
        return result;
    }

    void AppendDiagnostic(const fs::path& root, const std::string& line)
    {
        if (root.empty())
            return;
        std::error_code ec;
        fs::create_directories(root, ec);
        if (ec)
            return;
        std::ofstream output(
            root / "RuntimeDiagnostic.log",
            std::ios::binary | std::ios::app);
        if (!output)
            return;
        output << line << '\n';
        output.flush();
    }

    struct WindowSnapshotContext
    {
        DWORD processId = 0;
        std::vector<std::string> windows;
    };

    BOOL CALLBACK CollectProcessWindow(HWND window, LPARAM parameter)
    {
        auto* context =
            reinterpret_cast<WindowSnapshotContext*>(parameter);
        if (context == nullptr)
            return TRUE;

        DWORD processId = 0;
        GetWindowThreadProcessId(window, &processId);
        if (processId != context->processId)
            return TRUE;

        wchar_t className[128] = {};
        wchar_t title[512] = {};
        GetClassNameW(window, className, static_cast<int>(std::size(className)));
        GetWindowTextW(window, title, static_cast<int>(std::size(title)));

        std::ostringstream text;
        text << "hwnd="
             << reinterpret_cast<std::uintptr_t>(window)
             << ",visible=" << (IsWindowVisible(window) ? "true" : "false")
             << ",class=" << WideToUtf8(className)
             << ",title=" << WideToUtf8(title);
        context->windows.push_back(text.str());
        return TRUE;
    }

    std::string InitializerState()
    {
        static constexpr std::array<const char*,
            wi::initializer::INITIALIZED_SYSTEM_COUNT> Names = {
            "font",
            "image",
            "input",
            "renderer",
            "texturehelper",
            "hair",
            "emitted_particles",
            "ocean",
            "gpusort",
            "gpubvh",
            "physics",
            "lua",
            "audio",
            "trail",
            "gaussian_splat",
        };

        std::ostringstream state;
        state << "initializer_all="
              << (wi::initializer::IsInitializeFinished() ? "true" : "false")
              << ",pending=";
        bool first = true;
        for (int index = 0;
             index < wi::initializer::INITIALIZED_SYSTEM_COUNT;
             ++index)
        {
            const auto system =
                static_cast<wi::initializer::INITIALIZED_SYSTEM>(index);
            if (wi::initializer::IsInitializeFinished(system))
                continue;
            if (!first)
                state << ',';
            first = false;
            state << Names[static_cast<std::size_t>(index)];
        }
        if (first)
            state << "none";
        return state.str();
    }

    std::uint64_t FileTimeValue(const FILETIME& value)
    {
        ULARGE_INTEGER converted{};
        converted.LowPart = value.dwLowDateTime;
        converted.HighPart = value.dwHighDateTime;
        return converted.QuadPart;
    }

    void SnapshotRuntimeLogs(const fs::path& diagnosticRoot)
    {
        const fs::path localAppData = LocalAppDataRoot();
        if (localAppData.empty())
            return;

        const fs::path renegadeRoot = localAppData / "RenegadeEngine";
        std::error_code ec;
        if (!fs::is_directory(renegadeRoot, ec) || ec)
            return;

        for (fs::recursive_directory_iterator iterator(renegadeRoot, ec), end;
             !ec && iterator != end;
             iterator.increment(ec))
        {
            if (!iterator->is_regular_file(ec) || ec)
                continue;

            const std::wstring filename = iterator->path().filename().wstring();
            if (filename != L"WickedRuntime.log" &&
                filename != L"RuntimeBootstrap.log")
            {
                continue;
            }

            fs::path identity = iterator->path().parent_path();
            if (identity.filename() == L"Logs")
                identity = identity.parent_path().filename();
            else
                identity = L"unknown";

            const fs::path destination =
                diagnosticRoot /
                (identity.wstring() + L"-" + filename);
            std::error_code copyError;
            fs::copy_file(
                iterator->path(),
                destination,
                fs::copy_options::overwrite_existing,
                copyError);
        }
    }

    DWORD WINAPI DiagnosticThread(void* parameter)
    {
        const auto* rootPointer = reinterpret_cast<fs::path*>(parameter);
        if (rootPointer == nullptr)
            return 0;
        const fs::path root = *rootPointer;
        delete rootPointer;

        // Let C/C++ dynamic initialization and wWinMain get underway before
        // querying Wicked's atomic initializer state.
        Sleep(2000);
        const ULONGLONG started = GetTickCount64();

        for (;;)
        {
            FILETIME created{};
            FILETIME exited{};
            FILETIME kernel{};
            FILETIME user{};
            GetProcessTimes(
                GetCurrentProcess(),
                &created,
                &exited,
                &kernel,
                &user);

            WindowSnapshotContext windows;
            windows.processId = GetCurrentProcessId();
            EnumWindows(CollectProcessWindow,
                reinterpret_cast<LPARAM>(&windows));

            std::ostringstream heartbeat;
            heartbeat << "heartbeat_ms=" << (GetTickCount64() - started)
                      << ',' << InitializerState()
                      << ",kernel_100ns=" << FileTimeValue(kernel)
                      << ",user_100ns=" << FileTimeValue(user)
                      << ",windows=" << windows.windows.size();
            AppendDiagnostic(root, heartbeat.str());
            for (const std::string& window : windows.windows)
                AppendDiagnostic(root, "window:" + window);

            SnapshotRuntimeLogs(root);
            Sleep(10000);
        }
    }

    struct SmokeDiagnosticsRegistration
    {
        SmokeDiagnosticsRegistration()
        {
            if (!IsGate4SmokeProcess())
                return;

            const fs::path root = DiagnosticRoot();
            if (root.empty())
                return;

            std::error_code ec;
            fs::create_directories(root, ec);
            fs::remove(root / "RuntimeDiagnostic.log", ec);

            // Hosted CI must never wait behind an OS/CRT modal error dialog.
            SetErrorMode(
                GetErrorMode() |
                SEM_FAILCRITICALERRORS |
                SEM_NOGPFAULTERRORBOX |
                SEM_NOOPENFILEERRORBOX);
            SetThreadErrorMode(
                SEM_FAILCRITICALERRORS |
                SEM_NOGPFAULTERRORBOX |
                SEM_NOOPENFILEERRORBOX,
                nullptr);
#ifdef _MSC_VER
            _set_error_mode(_OUT_TO_STDERR);
            _set_abort_behavior(0, _WRITE_ABORT_MSG);
#endif
#ifdef _DEBUG
            _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
            _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
            _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);
#endif

            AppendDiagnostic(
                root,
                "phase=process_static_initialization_complete");

            auto* threadRoot = new fs::path(root);
            const HANDLE thread = CreateThread(
                nullptr,
                0,
                DiagnosticThread,
                threadRoot,
                0,
                nullptr);
            if (thread == nullptr)
            {
                delete threadRoot;
                AppendDiagnostic(
                    root,
                    "phase=diagnostic_thread_create_failed,error=" +
                        std::to_string(GetLastError()));
                return;
            }
            CloseHandle(thread);
            AppendDiagnostic(root, "phase=diagnostic_thread_started");
        }
    };

    SmokeDiagnosticsRegistration smokeDiagnosticsRegistration;
}
