#include "WindowsGameBuildController.h"

#include "renegade/bridge/PackageIntegrityService.h"
#include "renegade/bridge/StudioSession.h"
#include "renegade/bridge/WindowsGameBuildProjectService.h"
#include "renegade/bridge/WindowsGameBuildWorkflow.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#ifndef RENEGADE_SOURCE_REVISION
#error RENEGADE_SOURCE_REVISION must be supplied by the Renegade Studio build.
#endif

#ifndef RENEGADE_WICKED_REVISION
#error RENEGADE_WICKED_REVISION must be supplied by the Renegade Studio build.
#endif

namespace renegade::studio
{
    namespace
    {
        namespace fs = std::filesystem;

        constexpr const char* DeveloperPublisher = "Maverick Media Studio";
        constexpr DWORD SmokeTimeoutMilliseconds = 120000;

        fs::path StudioDirectory()
        {
            std::wstring path(32768, L'\0');
            const DWORD count = GetModuleFileNameW(
                nullptr, path.data(), static_cast<DWORD>(path.size()));
            if (count == 0 || count >= path.size())
                return {};
            path.resize(count);
            return fs::path(path).parent_path();
        }

        bool IsRegularFile(const fs::path& path)
        {
            std::error_code ec;
            return fs::is_regular_file(path, ec) && !ec &&
                !fs::is_symlink(path, ec) && !ec;
        }

        bool FindFirstRegularFile(
            const std::vector<fs::path>& candidates,
            fs::path& result)
        {
            for (const fs::path& candidate : candidates)
            {
                if (IsRegularFile(candidate))
                {
                    result = candidate;
                    return true;
                }
            }
            result.clear();
            return false;
        }

        std::string UtcTimestamp()
        {
            const std::time_t now = std::time(nullptr);
            std::tm utc{};
            if (gmtime_s(&utc, &now) != 0)
                return {};
            std::ostringstream stream;
            stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
            return stream.str();
        }

        std::string UniqueStagingId()
        {
            const auto ticks = static_cast<std::uint64_t>(
                std::chrono::high_resolution_clock::now()
                    .time_since_epoch().count());
            std::ostringstream stream;
            stream << "studio-" << std::hex << std::nouppercase << ticks
                << "-" << GetCurrentProcessId();
            return stream.str();
        }

        void WriteU16(std::ofstream& output, const std::uint16_t value)
        {
            const std::array<unsigned char, 2> bytes = {
                static_cast<unsigned char>(value & 0xffu),
                static_cast<unsigned char>((value >> 8u) & 0xffu),
            };
            output.write(
                reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
        }

        void WriteU32(std::ofstream& output, const std::uint32_t value)
        {
            const std::array<unsigned char, 4> bytes = {
                static_cast<unsigned char>(value & 0xffu),
                static_cast<unsigned char>((value >> 8u) & 0xffu),
                static_cast<unsigned char>((value >> 16u) & 0xffu),
                static_cast<unsigned char>((value >> 24u) & 0xffu),
            };
            output.write(
                reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
        }

        bool WriteDefaultGameIcon(
            const fs::path& projectRoot,
            fs::path& iconPath,
            std::string& error)
        {
            iconPath = projectRoot / "Intermediate" / "Build" /
                "RenegadeDefaultGame.ico";
            std::error_code ec;
            fs::create_directories(iconPath.parent_path(), ec);
            if (ec)
            {
                error = "Build Windows Game could not create the governed icon directory.";
                return false;
            }

            std::ofstream output(iconPath, std::ios::binary | std::ios::trunc);
            if (!output)
            {
                error = "Build Windows Game could not create the governed default icon.";
                return false;
            }

            // Deterministic one-image ICO containing a 32-bit 1x1 DIB. The
            // default is intentionally simple; project-selectable branding is
            // a later authoring concern, not a Gate 5 packaging semantic.
            WriteU16(output, 0);
            WriteU16(output, 1);
            WriteU16(output, 1);
            output.put(1);
            output.put(1);
            output.put(0);
            output.put(0);
            WriteU16(output, 1);
            WriteU16(output, 32);
            WriteU32(output, 48);
            WriteU32(output, 22);

            WriteU32(output, 40);
            WriteU32(output, 1);
            WriteU32(output, 2);
            WriteU16(output, 1);
            WriteU16(output, 32);
            WriteU32(output, 0);
            WriteU32(output, 4);
            WriteU32(output, 0);
            WriteU32(output, 0);
            WriteU32(output, 0);
            WriteU32(output, 0);

            const std::array<unsigned char, 8> image = {
                29, 91, 210, 255,
                0, 0, 0, 0,
            };
            output.write(
                reinterpret_cast<const char*>(image.data()),
                static_cast<std::streamsize>(image.size()));
            output.close();
            if (!output)
            {
                error = "Build Windows Game could not finish the governed default icon.";
                return false;
            }
            error.clear();
            return true;
        }

        bool AddRuntimeSupport(
            const std::string& logicalName,
            const std::string& destination,
            const fs::path& source,
            const std::string& provenance,
            std::vector<bridge::WindowsRuntimeSupportInput>& planInputs,
            std::vector<bridge::WindowsRuntimeSupportSource>& stageInputs,
            std::string& error)
        {
            bridge::WindowsGamePackageFileDigest digest;
            if (!bridge::DigestWindowsGamePackageFile(
                    source.generic_u8string(), digest, error))
            {
                error = "Build Windows Game could not hash Runtime support " +
                    destination + ": " + error;
                return false;
            }

            bridge::WindowsRuntimeSupportInput plan;
            plan.logicalName = logicalName;
            plan.destinationPath = destination;
            plan.byteCount = digest.byteCount;
            plan.sha256 = digest.sha256;
            plan.provenance = provenance;
            planInputs.push_back(std::move(plan));

            bridge::WindowsRuntimeSupportSource stage;
            stage.destinationPath = destination;
            stage.sourcePath = source.generic_u8string();
            stageInputs.push_back(std::move(stage));
            return true;
        }

        bool AddPackageDocument(
            const fs::path& source,
            std::string destination,
            std::string component,
            std::string provenance,
            std::vector<bridge::WindowsPackageDocumentInput>& documents,
            std::string& error)
        {
            if (!IsRegularFile(source))
            {
                error = "Build Windows Game is missing governed package input: " +
                    source.generic_u8string();
                return false;
            }
            bridge::WindowsPackageDocumentInput document;
            document.destinationPath = std::move(destination);
            document.sourcePath = source.generic_u8string();
            document.component = std::move(component);
            document.provenance = std::move(provenance);
            documents.push_back(std::move(document));
            return true;
        }

        std::wstring QuoteArgument(const std::wstring& value)
        {
            if (value.find_first_of(L" \t\"") == std::wstring::npos)
                return value;
            std::wstring quoted = L"\"";
            std::size_t slashes = 0;
            for (const wchar_t character : value)
            {
                if (character == L'\\')
                {
                    ++slashes;
                    continue;
                }
                if (character == L'\"')
                {
                    quoted.append(slashes * 2 + 1, L'\\');
                    quoted.push_back(L'\"');
                    slashes = 0;
                    continue;
                }
                quoted.append(slashes, L'\\');
                slashes = 0;
                quoted.push_back(character);
            }
            quoted.append(slashes * 2, L'\\');
            quoted.push_back(L'\"');
            return quoted;
        }

        std::wstring Utf8ToWide(const std::string& value)
        {
            if (value.empty())
                return {};

            const int characterCount = MultiByteToWideChar(
                CP_UTF8,
                MB_ERR_INVALID_CHARS,
                value.data(),
                static_cast<int>(value.size()),
                nullptr,
                0);
            if (characterCount <= 0)
                return std::wstring(value.begin(), value.end());

            std::wstring result(
                static_cast<std::size_t>(characterCount), L'\0');
            MultiByteToWideChar(
                CP_UTF8,
                MB_ERR_INVALID_CHARS,
                value.data(),
                static_cast<int>(value.size()),
                result.data(),
                characterCount);
            return result;
        }

        class WindowsBuildProgressWindow final
        {
        public:
            ~WindowsBuildProgressWindow()
            {
                Close();
            }

            void Begin(const std::string& projectName)
            {
                Close();
                {
                    std::scoped_lock lock(mutex_);
                    projectName_ = projectName;
                    stage_ = "STARTING WINDOWS BUILD";
                    detail_ = "Preparing governed build workflow...";
                    percent_ = 2;
                    completed_ = false;
                    succeeded_ = false;
                    startedAt_ = std::chrono::steady_clock::now();
                }
                thread_ = std::thread([this]() { ThreadMain(); });
                for (int attempt = 0; attempt < 200 && hwnd_.load() == nullptr;
                    ++attempt)
                {
                    Sleep(1);
                }
            }

            void Stage(
                const int percent,
                std::string stage,
                std::string detail = {})
            {
                {
                    std::scoped_lock lock(mutex_);
                    percent_ = std::clamp(percent, 0, 100);
                    stage_ = std::move(stage);
                    detail_ = std::move(detail);
                }
                if (const HWND window = hwnd_.load())
                    PostMessageW(window, ProgressChangedMessage, 0, 0);
            }

            void Complete(const bool succeeded, std::string detail)
            {
                {
                    std::scoped_lock lock(mutex_);
                    percent_ = 100;
                    completed_ = true;
                    succeeded_ = succeeded;
                    stage_ = succeeded ? "BUILD COMPLETE" : "BUILD FAILED";
                    detail_ = std::move(detail);
                }
                if (const HWND window = hwnd_.load())
                    PostMessageW(window, ProgressChangedMessage, 0, 0);
            }

        private:
            static constexpr UINT ProgressChangedMessage = WM_APP + 73;
            static constexpr wchar_t WindowClassName[] =
                L"RenegadeWindowsBuildProgressWindow";

            static LRESULT CALLBACK WindowProc(
                HWND window,
                UINT message,
                WPARAM wParam,
                LPARAM lParam)
            {
                WindowsBuildProgressWindow* self =
                    reinterpret_cast<WindowsBuildProgressWindow*>(
                        GetWindowLongPtrW(window, GWLP_USERDATA));
                if (message == WM_NCCREATE)
                {
                    const auto* create =
                        reinterpret_cast<const CREATESTRUCTW*>(lParam);
                    self = reinterpret_cast<WindowsBuildProgressWindow*>(
                        create->lpCreateParams);
                    SetWindowLongPtrW(
                        window, GWLP_USERDATA,
                        reinterpret_cast<LONG_PTR>(self));
                }

                switch (message)
                {
                case ProgressChangedMessage:
                case WM_TIMER:
                    InvalidateRect(window, nullptr, FALSE);
                    return 0;
                case WM_ERASEBKGND:
                    return 1;
                case WM_PAINT:
                    if (self)
                        self->Paint(window);
                    return 0;
                case WM_CLOSE:
                    DestroyWindow(window);
                    return 0;
                case WM_DESTROY:
                    if (self)
                        self->hwnd_.store(nullptr);
                    PostQuitMessage(0);
                    return 0;
                default:
                    return DefWindowProcW(window, message, wParam, lParam);
                }
            }

            void ThreadMain()
            {
                const HINSTANCE instance = GetModuleHandleW(nullptr);
                WNDCLASSEXW windowClass{};
                windowClass.cbSize = sizeof(windowClass);
                windowClass.lpfnWndProc = WindowProc;
                windowClass.hInstance = instance;
                windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
                windowClass.lpszClassName = WindowClassName;
                if (RegisterClassExW(&windowClass) == 0 &&
                    GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
                {
                    return;
                }

                const HWND window = CreateWindowExW(
                    WS_EX_APPWINDOW,
                    WindowClassName,
                    L"Renegade Studio - Build Windows Game",
                    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                    CW_USEDEFAULT,
                    CW_USEDEFAULT,
                    620,
                    300,
                    nullptr,
                    nullptr,
                    instance,
                    this);
                if (!window)
                    return;

                hwnd_.store(window);
                SetTimer(window, 1, 250, nullptr);
                ShowWindow(window, SW_SHOWNORMAL);
                UpdateWindow(window);

                MSG message{};
                while (GetMessageW(&message, nullptr, 0, 0) > 0)
                {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
            }

            void Paint(const HWND window)
            {
                PAINTSTRUCT paint{};
                HDC dc = BeginPaint(window, &paint);
                if (!dc)
                    return;

                RECT client{};
                GetClientRect(window, &client);
                HBRUSH background = CreateSolidBrush(RGB(8, 13, 18));
                FillRect(dc, &client, background);
                DeleteObject(background);

                std::string projectName;
                std::string stage;
                std::string detail;
                int percent = 0;
                bool completed = false;
                bool succeeded = false;
                std::chrono::steady_clock::time_point started;
                {
                    std::scoped_lock lock(mutex_);
                    projectName = projectName_;
                    stage = stage_;
                    detail = detail_;
                    percent = percent_;
                    completed = completed_;
                    succeeded = succeeded_;
                    started = startedAt_;
                }

                SetBkMode(dc, TRANSPARENT);
                SetTextColor(dc, RGB(235, 241, 245));
                HFONT titleFont = CreateFontW(
                    24, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                    L"Segoe UI");
                HFONT bodyFont = CreateFontW(
                    17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                    L"Segoe UI");
                HFONT oldFont = static_cast<HFONT>(SelectObject(dc, titleFont));

                RECT titleRect{26, 20, client.right - 26, 55};
                const std::wstring title = Utf8ToWide(
                    projectName.empty()
                        ? "BUILD WINDOWS GAME"
                        : "BUILD WINDOWS GAME - " + projectName);
                DrawTextW(dc, title.c_str(), -1, &titleRect,
                    DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

                SelectObject(dc, bodyFont);
                SetTextColor(dc,
                    completed
                        ? (succeeded ? RGB(113, 205, 111) : RGB(229, 92, 92))
                        : RGB(203, 214, 222));
                RECT stageRect{26, 69, client.right - 26, 96};
                const std::wstring stageText = Utf8ToWide(stage);
                DrawTextW(dc, stageText.c_str(), -1, &stageRect,
                    DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

                const int barLeft = 26;
                const int barTop = 111;
                const int barRight = client.right - 26;
                const int barBottom = 139;
                HBRUSH track = CreateSolidBrush(RGB(27, 38, 47));
                RECT trackRect{barLeft, barTop, barRight, barBottom};
                FillRect(dc, &trackRect, track);
                DeleteObject(track);

                const int fillRight = barLeft +
                    (barRight - barLeft) * std::clamp(percent, 0, 100) / 100;
                HBRUSH fill = CreateSolidBrush(
                    completed && !succeeded
                        ? RGB(165, 54, 54)
                        : RGB(65, 158, 230));
                RECT fillRect{barLeft, barTop, fillRight, barBottom};
                FillRect(dc, &fillRect, fill);
                DeleteObject(fill);

                SetTextColor(dc, RGB(235, 241, 245));
                const std::wstring percentText =
                    std::to_wstring(std::clamp(percent, 0, 100)) + L"%";
                RECT percentRect{barLeft, barTop + 3, barRight - 8, barBottom};
                DrawTextW(dc, percentText.c_str(), -1, &percentRect,
                    DT_RIGHT | DT_SINGLELINE);

                SetTextColor(dc, RGB(164, 178, 188));
                RECT detailRect{26, 156, client.right - 26, 205};
                const std::wstring detailText = Utf8ToWide(detail);
                DrawTextW(dc, detailText.c_str(), -1, &detailRect,
                    DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);

                const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - started).count();
                std::ostringstream elapsedStream;
                elapsedStream << "Elapsed: " << elapsed << "s";
                const std::wstring elapsedText = Utf8ToWide(elapsedStream.str());
                RECT elapsedRect{26, client.bottom - 38, client.right - 26,
                    client.bottom - 14};
                DrawTextW(dc, elapsedText.c_str(), -1, &elapsedRect,
                    DT_LEFT | DT_SINGLELINE);

                SelectObject(dc, oldFont);
                DeleteObject(titleFont);
                DeleteObject(bodyFont);
                EndPaint(window, &paint);
            }

            void Close()
            {
                if (const HWND window = hwnd_.load())
                    PostMessageW(window, WM_CLOSE, 0, 0);
                if (thread_.joinable())
                    thread_.join();
                hwnd_.store(nullptr);
            }

            std::atomic<HWND> hwnd_{nullptr};
            std::thread thread_;
            std::mutex mutex_;
            std::string projectName_;
            std::string stage_;
            std::string detail_;
            int percent_ = 0;
            bool completed_ = false;
            bool succeeded_ = false;
            std::chrono::steady_clock::time_point startedAt_ =
                std::chrono::steady_clock::now();
        };

        fs::path RuntimeEvidencePath(const std::string& identity)
        {
            const DWORD required = GetEnvironmentVariableW(
                L"LOCALAPPDATA", nullptr, 0);
            if (required <= 1)
                return {};
            std::wstring localAppData(static_cast<std::size_t>(required), L'\0');
            const DWORD written = GetEnvironmentVariableW(
                L"LOCALAPPDATA", localAppData.data(), required);
            if (written == 0 || written >= required)
                return {};
            localAppData.resize(written);
            return fs::path(localAppData) /
                "RenegadeEngine" /
                fs::u8path(identity) /
                "Logs" /
                "RuntimeBootstrap.log";
        }

        bool RunStandaloneSmoke(
            const bridge::WindowsGameBuildPlan& plan,
            const bridge::WindowsGameBuildStageResult& stage,
            const std::vector<std::string>& smokeOutcomes,
            const std::string& stagingId,
            std::string& runtimeEvidencePath,
            std::string& error)
        {
            runtimeEvidencePath.clear();
            const fs::path executable = fs::u8path(stage.stagingPath) /
                fs::u8path(plan.executableFileName);
            if (!IsRegularFile(executable))
            {
                error = "Build Windows Game smoke cannot find the staged named executable.";
                return false;
            }

            const fs::path evidence = RuntimeEvidencePath(plan.saveDataId);
            if (evidence.empty())
            {
                error = "Build Windows Game smoke cannot resolve LOCALAPPDATA evidence storage.";
                return false;
            }
            std::error_code ec;
            const bool evidenceExists = fs::exists(evidence, ec);
            if (ec)
            {
                error = "Build Windows Game smoke could not inspect prior Runtime evidence.";
                return false;
            }
            if (evidenceExists)
            {
                const bool removed = fs::remove(evidence, ec);
                if (ec || !removed)
                {
                    error = "Build Windows Game smoke could not remove stale Runtime evidence.";
                    return false;
                }
            }

            ec.clear();
            const fs::path workingDirectory = fs::temp_directory_path(ec) /
                "RenegadeBuildSmoke" / fs::u8path(stagingId);
            if (ec)
            {
                error = "Build Windows Game smoke cannot resolve a detached working directory.";
                return false;
            }
            fs::create_directories(workingDirectory, ec);
            if (ec)
            {
                error = "Build Windows Game smoke cannot create its detached working directory.";
                return false;
            }

            std::wstring command = QuoteArgument(executable.wstring());
            command += L" dx12";
            for (const std::string& outcome : smokeOutcomes)
            {
                command += L" ";
                command += QuoteArgument(
                    Utf8ToWide("--flow-outcome=" + outcome));
            }
            command += L" --renegade-smoke-autoplay --renegade-smoke-exit";
            std::vector<wchar_t> commandBuffer(command.begin(), command.end());
            commandBuffer.push_back(L'\0');

            HANDLE job = CreateJobObjectW(nullptr, nullptr);
            if (job == nullptr)
            {
                error = "Build Windows Game smoke could not create a Runtime job object.";
                return false;
            }
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
            limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            if (!SetInformationJobObject(
                    job,
                    JobObjectExtendedLimitInformation,
                    &limits,
                    sizeof(limits)))
            {
                CloseHandle(job);
                error = "Build Windows Game smoke could not configure its Runtime job object.";
                return false;
            }

            STARTUPINFOW startup{};
            startup.cb = sizeof(startup);
            PROCESS_INFORMATION process{};
            const std::wstring cwd = workingDirectory.wstring();
            if (!CreateProcessW(
                    executable.wstring().c_str(),
                    commandBuffer.data(),
                    nullptr,
                    nullptr,
                    FALSE,
                    CREATE_UNICODE_ENVIRONMENT,
                    nullptr,
                    cwd.c_str(),
                    &startup,
                    &process))
            {
                const DWORD launchError = GetLastError();
                CloseHandle(job);
                error = "Build Windows Game smoke could not launch the staged named executable (Win32 " +
                    std::to_string(launchError) + ").";
                return false;
            }

            const bool assigned =
                AssignProcessToJobObject(job, process.hProcess) != FALSE;
            if (!assigned)
            {
                TerminateProcess(process.hProcess, 1);
                WaitForSingleObject(process.hProcess, 5000);
                CloseHandle(process.hThread);
                CloseHandle(process.hProcess);
                CloseHandle(job);
                error = "Build Windows Game smoke could not contain the Runtime process.";
                return false;
            }

            const DWORD wait = WaitForSingleObject(
                process.hProcess, SmokeTimeoutMilliseconds);
            if (wait != WAIT_OBJECT_0)
            {
                TerminateJobObject(job, 1);
                WaitForSingleObject(process.hProcess, 5000);
                CloseHandle(process.hThread);
                CloseHandle(process.hProcess);
                CloseHandle(job);
                error = wait == WAIT_TIMEOUT
                    ? "Build Windows Game smoke timed out before deterministic completion."
                    : "Build Windows Game smoke failed while waiting for Runtime completion.";
                return false;
            }

            DWORD exitCode = 1;
            const bool readExit =
                GetExitCodeProcess(process.hProcess, &exitCode) != FALSE;
            const DWORD exitReadError = readExit ? ERROR_SUCCESS : GetLastError();
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            CloseHandle(job);
            if (!readExit || exitCode != 0)
            {
                error = readExit
                    ? "Build Windows Game smoke Runtime exited with code " +
                        std::to_string(exitCode) + "."
                    : "Build Windows Game smoke could not read the Runtime exit code (Win32 " +
                        std::to_string(exitReadError) + ").";
                return false;
            }
            if (!IsRegularFile(evidence))
            {
                error = "Build Windows Game smoke completed without fresh Runtime evidence.";
                return false;
            }

            runtimeEvidencePath = evidence.generic_u8string();
            error.clear();
            return true;
        }
    }

    WindowsGameBuildUiResult BuildActiveWindowsGame()
    {
        WindowsGameBuildUiResult ui;

#if !defined(NDEBUG)
        ui.message =
            "Build Windows Game requires a Release Renegade Studio build; Debug remains regression-only.";
        return ui;
#else
        bridge::StudioSession* session = bridge::StudioSession::Current();
        if (session == nullptr || !session->Projects().HasProject())
        {
            ui.message = "Build Windows Game requires an active Renegade project.";
            return ui;
        }

        const bridge::ProjectMetadata project = session->Projects().CurrentProject();
        static WindowsBuildProgressWindow progress;
        progress.Begin(project.name);
        const auto fail = [&](std::string message)
        {
            ui.message = std::move(message);
            progress.Complete(false, ui.message);
            return ui;
        };

        progress.Stage(10, "VALIDATING STORYFLOW",
            "Checking the authoritative project and StoryFlow build state.");
        bridge::WindowsGameBuildProjectState projectState;
        std::string error;
        if (!bridge::PrepareWindowsGameBuildProjectState(
                project, projectState, error))
        {
            return fail(std::move(error));
        }

        progress.Stage(25, "RESOLVING DEPENDENCIES",
            "StoryFlow route and governed dependency closure resolved.");
        const fs::path studioDirectory = StudioDirectory();
        if (studioDirectory.empty())
        {
            return fail(
                "Build Windows Game could not resolve the Studio installation directory.");
        }

        fs::path runtimeExecutable;
        if (!FindFirstRegularFile(
                {
                    studioDirectory / "Runtime" / "RenegadeRuntime.exe",
                    studioDirectory.parent_path().parent_path() /
                        "Runtime" / "Release" / "RenegadeRuntime.exe",
                },
                runtimeExecutable))
        {
            return fail(
                "Build Windows Game requires the Release RenegadeRuntime.exe beside the Studio package or build tree.");
        }

        fs::path dxCompiler;
        if (!FindFirstRegularFile(
                {
                    studioDirectory / "dxcompiler.dll",
                    runtimeExecutable.parent_path() / "dxcompiler.dll",
                },
                dxCompiler))
        {
            return fail(
                "Build Windows Game could not find the governed dxcompiler.dll Runtime support.");
        }

        progress.Stage(38, "PREPARING RUNTIME SUPPORT",
            "Hashing the governed Runtime and DirectX shader compiler inputs.");
        const std::string renegadeRevision = RENEGADE_SOURCE_REVISION;
        const std::string wickedRevision = RENEGADE_WICKED_REVISION;
        const std::string stagingId = UniqueStagingId();
        const std::string timestamp = UtcTimestamp();
        if (timestamp.empty())
        {
            return fail(
                "Build Windows Game could not produce a UTC build timestamp.");
        }

        bridge::WindowsGameBuildWorkflowRequest request;
        request.project = project;
        request.dependencyGraph = std::move(projectState.dependencyGraph);
        request.assetRegistry = std::move(projectState.assetRegistry);
        request.build.gameName = project.name;
        request.build.executableBaseName = project.name;
        request.build.publicVersion = "0.1.0";
        request.build.saveDataId = project.projectId;
        request.build.platform = "windows-x64";
        request.build.configuration = "Release";

        const std::string namedExecutable = project.name + ".exe";
        if (!AddRuntimeSupport(
                "renegade-runtime",
                namedExecutable,
                runtimeExecutable,
                "repo:" + renegadeRevision,
                request.runtimeSupport,
                request.staging.runtimeSupportSources,
                error) ||
            !AddRuntimeSupport(
                "directx-shader-compiler",
                "dxcompiler.dll",
                dxCompiler,
                "pinned:" + wickedRevision,
                request.runtimeSupport,
                request.staging.runtimeSupportSources,
                error))
        {
            return fail(std::move(error));
        }

        progress.Stage(50, "STAGING PACKAGE INPUTS",
            "Collecting Runtime support, licences and governed package documents.");
        request.staging.projectRootPath = project.rootPath;
        request.staging.outputParentPath =
            (fs::u8path(project.rootPath) / "Builds" / "Windows")
                .generic_u8string();
        request.staging.stagingId = stagingId;
        request.staging.renegadeRevision = renegadeRevision;
        request.staging.wickedRevision = wickedRevision;

        const fs::path buildInputs = studioDirectory / "BuildInputs";
        if (!AddPackageDocument(
                buildInputs / "ReadMe.txt",
                "ReadMe.txt",
                "renegade-build-readme",
                "repo:" + renegadeRevision,
                request.staging.packageDocuments,
                error) ||
            !AddPackageDocument(
                buildInputs / "Renegade-Licence-or-Notice.txt",
                "Licences/Renegade-Licence-or-Notice.txt",
                "renegade",
                "repo:" + renegadeRevision,
                request.staging.packageDocuments,
                error) ||
            !AddPackageDocument(
                buildInputs / "WickedEngine-LICENSE.txt",
                "Licences/WickedEngine-LICENSE.txt",
                "wicked-engine",
                "pinned:" + wickedRevision,
                request.staging.packageDocuments,
                error) ||
            !AddPackageDocument(
                buildInputs / "WickedEngine-third_party_software.txt",
                "Licences/WickedEngine-third_party_software.txt",
                "wicked-engine-third-party",
                "pinned:" + wickedRevision,
                request.staging.packageDocuments,
                error) ||
            !AddPackageDocument(
                buildInputs / "DirectXShaderCompiler-LICENSE.txt",
                "Licences/DirectXShaderCompiler-LICENSE.txt",
                "directx-shader-compiler",
                "pinned:" + wickedRevision,
                request.staging.packageDocuments,
                error) ||
            !AddPackageDocument(
                buildInputs / "DirectXShaderCompiler-ThirdPartyNotices.txt",
                "Licences/DirectXShaderCompiler-ThirdPartyNotices.txt",
                "directx-shader-compiler-third-party",
                "pinned:" + wickedRevision,
                request.staging.packageDocuments,
                error))
        {
            return fail(std::move(error));
        }

        fs::path iconPath;
        if (!WriteDefaultGameIcon(
                fs::u8path(project.rootPath), iconPath, error))
        {
            return fail(std::move(error));
        }

        progress.Stage(62, "PREPARING EXECUTABLE",
            "Applying deterministic Windows executable identity and build metadata.");
        request.identity.developerPublisher = DeveloperPublisher;
        request.identity.description = project.name + " standalone game";
        request.identity.copyrightNotice =
            "Copyright 2026 Maverick Media Studio";
        request.identity.internalBuildId =
            "gate5-" + renegadeRevision.substr(0, 12) + "-" + stagingId;
        request.identity.buildTimestampUtc = timestamp;
        request.identity.iconSourcePath = iconPath.generic_u8string();

        request.verification.expectedGraphicsBackend = "DX12";
        request.verification.windowsPrerequisitePolicy =
            bridge::WindowsVcRuntimePrerequisitePolicy;
        request.verification.expectedFlowTrace = projectState.expectedFlowTrace;

        const std::vector<std::string> smokeOutcomes = projectState.smokeOutcomes;
        bridge::WindowsGameBuildWorkflowResult result;
        const bridge::WindowsGameBuildSmokeRunner smoke =
            [smokeOutcomes, stagingId, &progress](
                const bridge::WindowsGameBuildPlan& plan,
                const bridge::WindowsGameBuildStageResult& stage,
                std::string& evidencePath,
                std::string& smokeError)
            {
                progress.Stage(82, "VALIDATING PACKAGED RUNTIME",
                    "Running the staged standalone against the exact authored StoryFlow path.");
                return RunStandaloneSmoke(
                    plan,
                    stage,
                    smokeOutcomes,
                    stagingId,
                    evidencePath,
                    smokeError);
            };

        progress.Stage(70, "BUILDING WINDOWS PACKAGE",
            "Staging content, writing manifests and preparing verification evidence.");
        if (!bridge::BuildWindowsGame(
                request, smoke, result, error))
        {
            return fail(std::move(error));
        }

        ui.succeeded = true;
        ui.finalOutputPath = result.finalOutputPath;
        ui.message = "Windows game build promoted successfully: " +
            result.finalOutputPath;
        progress.Complete(true, result.finalOutputPath);
        return ui;
#endif
    }
}
