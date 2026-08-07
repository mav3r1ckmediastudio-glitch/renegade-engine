#include "TestLevelRuntimeProcess.h"

// Windows.h's min/max macros silently break std::max and
// std::numeric_limits<T>::max() below; NOMINMAX must be defined before the
// include, not passed only as a compile definition, since translation-unit
// order still matters if this header is ever included after Windows.h
// elsewhere.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <sstream>
#include <system_error>
#include <utility>

namespace
{
    namespace fs = std::filesystem;

    constexpr wchar_t ReadyEventArgument[] = L"--renegade-ready-event=";
    constexpr char OwnershipMarker[] = ".renegade-test-level-owner";
    constexpr char OwnershipFormat[] = "renegade-test-level-owner";
    constexpr std::uint32_t OwnershipVersion = 1;
    constexpr DWORD ForcedStopExitCode = 0xEE01u;
    constexpr DWORD StartupTimeoutExitCode = 0xEE02u;
    constexpr DWORD WatchFailureExitCode = 0xEE03u;
    constexpr DWORD TerminationWaitMilliseconds = 5000u;
    std::atomic<std::uint64_t> readyEventSequence{0};

    std::wstring Utf8ToWide(const std::string& value)
    {
        if (value.empty())
        {
            return {};
        }
        const int count = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(value.size()),
            nullptr,
            0);
        if (count <= 0)
        {
            return std::wstring(value.begin(), value.end());
        }
        std::wstring result(static_cast<std::size_t>(count), L'\0');
        MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(value.size()),
            result.data(),
            count);
        return result;
    }

    std::string WindowsErrorMessage(const DWORD code)
    {
        wchar_t* buffer = nullptr;
        const DWORD count = FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
                FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            code,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<wchar_t*>(&buffer),
            0,
            nullptr);
        std::wstring message;
        if (count != 0 && buffer != nullptr)
        {
            message.assign(buffer, count);
            LocalFree(buffer);
        }
        while (!message.empty() &&
            (message.back() == L'\r' || message.back() == L'\n' ||
             message.back() == L' '))
        {
            message.pop_back();
        }
        std::string narrow(message.begin(), message.end());
        if (narrow.empty())
        {
            narrow = "Windows error " + std::to_string(code);
        }
        return narrow;
    }

    std::wstring QuoteArgument(const std::wstring& value)
    {
        if (value.empty())
        {
            return L"\"\"";
        }
        const bool needsQuotes = value.find_first_of(L" \t\"") !=
            std::wstring::npos;
        if (!needsQuotes)
        {
            return value;
        }

        std::wstring quoted = L"\"";
        std::size_t backslashes = 0;
        for (const wchar_t character : value)
        {
            if (character == L'\\')
            {
                ++backslashes;
                continue;
            }
            if (character == L'\"')
            {
                quoted.append(backslashes * 2 + 1, L'\\');
                quoted.push_back(L'\"');
                backslashes = 0;
                continue;
            }
            quoted.append(backslashes, L'\\');
            backslashes = 0;
            quoted.push_back(character);
        }
        quoted.append(backslashes * 2, L'\\');
        quoted.push_back(L'\"');
        return quoted;
    }

    std::uint64_t FileTimeValue(const FILETIME& value) noexcept
    {
        ULARGE_INTEGER combined = {};
        combined.LowPart = value.dwLowDateTime;
        combined.HighPart = value.dwHighDateTime;
        return combined.QuadPart;
    }

    bool CurrentProcessIdentity(
        DWORD& processId,
        std::uint64_t& creationTime,
        std::string& error)
    {
        FILETIME created = {};
        FILETIME exited = {};
        FILETIME kernel = {};
        FILETIME user = {};
        if (!GetProcessTimes(
                GetCurrentProcess(),
                &created,
                &exited,
                &kernel,
                &user))
        {
            const DWORD code = GetLastError();
            error = "Could not query Studio process creation time: " +
                WindowsErrorMessage(code);
            return false;
        }
        processId = GetCurrentProcessId();
        creationTime = FileTimeValue(created);
        error.clear();
        return true;
    }

    enum class OwnerState
    {
        Alive,
        Abandoned,
        Unknown,
    };

    OwnerState CheckOwner(
        const DWORD processId,
        const std::uint64_t expectedCreationTime,
        std::string& warning)
    {
        HANDLE process = OpenProcess(
            PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE,
            FALSE,
            processId);
        if (process == nullptr)
        {
            const DWORD code = GetLastError();
            if (code == ERROR_INVALID_PARAMETER)
            {
                return OwnerState::Abandoned;
            }
            warning = "Could not verify Test Level owner PID " +
                std::to_string(processId) + ": " + WindowsErrorMessage(code);
            return OwnerState::Unknown;
        }

        FILETIME created = {};
        FILETIME exited = {};
        FILETIME kernel = {};
        FILETIME user = {};
        if (!GetProcessTimes(process, &created, &exited, &kernel, &user))
        {
            const DWORD code = GetLastError();
            CloseHandle(process);
            warning = "Could not read creation time for Test Level owner PID " +
                std::to_string(processId) + ": " + WindowsErrorMessage(code);
            return OwnerState::Unknown;
        }

        if (FileTimeValue(created) != expectedCreationTime)
        {
            CloseHandle(process);
            return OwnerState::Abandoned;
        }

        const DWORD wait = WaitForSingleObject(process, 0);
        CloseHandle(process);
        if (wait == WAIT_TIMEOUT)
        {
            return OwnerState::Alive;
        }
        if (wait == WAIT_OBJECT_0)
        {
            return OwnerState::Abandoned;
        }

        warning = "Could not poll Test Level owner PID " +
            std::to_string(processId) + ".";
        return OwnerState::Unknown;
    }

    bool ReadOwnerMarker(
        const fs::path& markerPath,
        DWORD& processId,
        std::uint64_t& creationTime,
        std::string& warning)
    {
        std::ifstream stream(markerPath, std::ios::binary);
        if (!stream)
        {
            return false;
        }

        std::string format;
        std::uint32_t version = 0;
        std::uint64_t pid = 0;
        std::uint64_t created = 0;
        std::string line;
        while (std::getline(stream, line))
        {
            const auto separator = line.find('=');
            if (separator == std::string::npos)
            {
                continue;
            }
            const std::string key = line.substr(0, separator);
            const std::string value = line.substr(separator + 1);
            try
            {
                if (key == "format")
                {
                    format = value;
                }
                else if (key == "version")
                {
                    version = static_cast<std::uint32_t>(std::stoul(value));
                }
                else if (key == "owner_pid")
                {
                    pid = std::stoull(value);
                }
                else if (key == "owner_creation_time")
                {
                    created = std::stoull(value);
                }
            }
            catch (...)
            {
                warning = "Ignoring malformed Test Level owner marker: " +
                    markerPath.generic_u8string();
                return false;
            }
        }

        if (format != OwnershipFormat || version != OwnershipVersion ||
            pid == 0 || pid > std::numeric_limits<DWORD>::max() || created == 0)
        {
            warning = "Ignoring incomplete Test Level owner marker: " +
                markerPath.generic_u8string();
            return false;
        }
        processId = static_cast<DWORD>(pid);
        creationTime = created;
        return true;
    }

    std::wstring MakeReadyEventName()
    {
        DWORD processId = 0;
        std::uint64_t creationTime = 0;
        std::string ignored;
        CurrentProcessIdentity(processId, creationTime, ignored);
        std::wostringstream stream;
        stream << L"Local\\RenegadeTestLevelReady-"
               << processId << L'-'
               << creationTime << L'-'
               << readyEventSequence.fetch_add(1);
        return stream.str();
    }
}

namespace renegade::studio
{
    struct TestLevelRuntimeProcess::Implementation
    {
        HANDLE process = nullptr;
        HANDLE readyEvent = nullptr;
        HANDLE job = nullptr;
        std::chrono::steady_clock::time_point launchedAt = {};
        std::chrono::milliseconds startupTimeout{15000};
        bridge::TestLevelSnapshot snapshot;
        TestLevelProcessResult result;
        TestLevelProcessFailureInjection failureInjection =
            TestLevelProcessFailureInjection::None;
        bool cleanupDone = false;
        bool watcherFailureInjected = false;

        void CloseHandles() noexcept
        {
            if (process != nullptr)
            {
                CloseHandle(process);
                process = nullptr;
            }
            if (readyEvent != nullptr)
            {
                CloseHandle(readyEvent);
                readyEvent = nullptr;
            }
            if (job != nullptr)
            {
                CloseHandle(job);
                job = nullptr;
            }
        }

        void CleanupSnapshot()
        {
            if (cleanupDone)
            {
                return;
            }
            cleanupDone = true;
            result.cleanupAttempted = true;
            std::string cleanupError;
            result.cleanupSucceeded =
                bridge::TestLevelSnapshotService::CleanupDirectory(
                    snapshot.projectRoot,
                    snapshot.sessionDirectory,
                    cleanupError);
            if (!result.cleanupSucceeded)
            {
                if (!result.message.empty())
                {
                    result.message += " ";
                }
                result.message += "Snapshot cleanup failed: " + cleanupError;
            }
        }

        bool ForceTerminate(const DWORD exitCode, std::string& warning)
        {
            if (process == nullptr)
            {
                return true;
            }
            DWORD wait = WaitForSingleObject(process, 0);
            if (wait == WAIT_OBJECT_0)
            {
                return true;
            }

            bool requested = TerminateProcess(process, exitCode) != FALSE;
            if (!requested && job != nullptr && result.jobAssigned)
            {
                requested = TerminateJobObject(job, exitCode) != FALSE;
            }
            if (!requested)
            {
                const DWORD code = GetLastError();
                warning = "Could not terminate the Test Level Runtime: " +
                    WindowsErrorMessage(code);
                return false;
            }

            wait = WaitForSingleObject(process, TerminationWaitMilliseconds);
            if (wait == WAIT_OBJECT_0)
            {
                return true;
            }

            // Closing an assigned KILL_ON_JOB_CLOSE job is the final Windows
            // backstop. It is intentionally used only after explicit
            // termination failed to produce a signaled process promptly.
            if (job != nullptr && result.jobAssigned)
            {
                CloseHandle(job);
                job = nullptr;
                wait = WaitForSingleObject(process, TerminationWaitMilliseconds);
                if (wait == WAIT_OBJECT_0)
                {
                    return true;
                }
            }

            warning = "The Test Level Runtime did not confirm termination.";
            return false;
        }

        TestLevelProcessResult Finalize(
            const TestLevelProcessState state,
            const bool succeeded,
            const DWORD exitCode,
            std::string message)
        {
            result.state = state;
            result.finished = true;
            result.succeeded = succeeded;
            result.exitCode = exitCode;
            result.message = std::move(message);
            CleanupSnapshot();
            CloseHandles();
            snapshot = {};
            return result;
        }

        TestLevelProcessResult ForceTerminateAndFinalize(
            const TestLevelProcessState state,
            const DWORD terminationCode,
            std::string message)
        {
            std::string terminationWarning;
            const bool terminated =
                ForceTerminate(terminationCode, terminationWarning);
            if (!terminationWarning.empty())
            {
                if (!result.warning.empty())
                {
                    result.warning += " ";
                }
                result.warning += terminationWarning;
            }
            if (!terminated)
            {
                message += " Child termination could not be confirmed.";
            }
            return Finalize(state, false, terminationCode, std::move(message));
        }
    };

    TestLevelRuntimeProcess::TestLevelRuntimeProcess()
        : implementation_(new Implementation())
    {
    }

    TestLevelRuntimeProcess::~TestLevelRuntimeProcess()
    {
        if (implementation_ != nullptr)
        {
            if (IsActive())
            {
                implementation_->ForceTerminateAndFinalize(
                    TestLevelProcessState::Stopped,
                    ForcedStopExitCode,
                    "Studio closed while Test Level was running.");
            }
            delete implementation_;
            implementation_ = nullptr;
        }
    }

    bool TestLevelRuntimeProcess::Launch(
        TestLevelLaunchOptions options,
        bridge::TestLevelSnapshot snapshot,
        std::string& error)
    {
        error.clear();
        if (implementation_ == nullptr || IsActive())
        {
            error = "A Test Level Runtime is already active.";
            return false;
        }

        implementation_->CloseHandles();
        implementation_->snapshot = std::move(snapshot);
        implementation_->cleanupDone = false;
        implementation_->watcherFailureInjected = false;
        implementation_->failureInjection = options.failureInjection;
        implementation_->startupTimeout = std::max(
            std::chrono::milliseconds(1),
            options.startupTimeout);
        implementation_->result = {};
        implementation_->result.state = TestLevelProcessState::Starting;

        const auto failLaunch = [this, &error](std::string message)
        {
            implementation_->result = implementation_->Finalize(
                TestLevelProcessState::LaunchFailed,
                false,
                0,
                std::move(message));
            error = implementation_->result.message;
            return false;
        };

        if (options.executablePath.empty())
        {
            return failLaunch("The Test Level Runtime executable path is empty.");
        }
        if (implementation_->snapshot.projectRoot.empty() ||
            implementation_->snapshot.sessionDirectory.empty())
        {
            return failLaunch("The Test Level snapshot is missing cleanup ownership metadata.");
        }

        if (!WriteOwnershipMarker(
                implementation_->snapshot.sessionDirectory,
                error))
        {
            return failLaunch(error);
        }

        const std::wstring executable = Utf8ToWide(options.executablePath);
        if (GetFileAttributesW(executable.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            return failLaunch(
                "RenegadeRuntime.exe was not found at: " +
                options.executablePath);
        }

        const std::wstring readyEventName = MakeReadyEventName();
        implementation_->readyEvent = CreateEventW(
            nullptr,
            TRUE,
            FALSE,
            readyEventName.c_str());
        if (implementation_->readyEvent == nullptr)
        {
            const DWORD code = GetLastError();
            return failLaunch(
                "Could not create the Test Level startup-ready event: " +
                WindowsErrorMessage(code));
        }

        std::vector<std::wstring> arguments;
        arguments.reserve(options.arguments.size() + 2);
        arguments.push_back(executable);
        for (const auto& argument : options.arguments)
        {
            arguments.push_back(Utf8ToWide(argument));
        }
        arguments.push_back(
            std::wstring(ReadyEventArgument) + readyEventName);

        std::wstring commandLine;
        for (std::size_t index = 0; index < arguments.size(); ++index)
        {
            if (index > 0)
            {
                commandLine.push_back(L' ');
            }
            commandLine += QuoteArgument(arguments[index]);
        }
        std::vector<wchar_t> mutableCommandLine(
            commandLine.begin(), commandLine.end());
        mutableCommandLine.push_back(L'\0');

        STARTUPINFOW startup = {};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION processInfo = {};
        const std::wstring workingDirectory =
            Utf8ToWide(options.workingDirectory);
        const wchar_t* workingDirectoryPointer =
            workingDirectory.empty() ? nullptr : workingDirectory.c_str();

        // A malformed or unrecognized executable image (for example a file
        // that is not a valid PE image at all) makes CreateProcessW trigger
        // the OS's own hard-error popup (observed as the "Unsupported 16-Bit
        // Application" dialog) independently of the failure it reports back
        // to this process. That dialog is invisible to an automated caller
        // but blocks any unattended/CI launch indefinitely. Suppress it for
        // the duration of this call only, then restore the previous mode so
        // legitimate OS dialogs elsewhere in Studio are unaffected.
        const UINT previousErrorMode = SetErrorMode(
            SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
        const BOOL created = CreateProcessW(
                executable.c_str(),
                mutableCommandLine.data(),
                nullptr,
                nullptr,
                FALSE,
                CREATE_SUSPENDED,
                nullptr,
                workingDirectoryPointer,
                &startup,
                &processInfo);
        SetErrorMode(previousErrorMode);
        if (!created)
        {
            const DWORD code = GetLastError();
            return failLaunch(
                "CreateProcessW failed for Test Level Runtime: " +
                WindowsErrorMessage(code));
        }

        implementation_->process = processInfo.hProcess;
        HANDLE thread = processInfo.hThread;

        if (options.failureInjection !=
            TestLevelProcessFailureInjection::JobAssignmentFailure)
        {
            implementation_->job = CreateJobObjectW(nullptr, nullptr);
            if (implementation_->job != nullptr)
            {
                JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
                limits.BasicLimitInformation.LimitFlags =
                    JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
                if (SetInformationJobObject(
                        implementation_->job,
                        JobObjectExtendedLimitInformation,
                        &limits,
                        sizeof(limits)) &&
                    AssignProcessToJobObject(
                        implementation_->job,
                        implementation_->process))
                {
                    implementation_->result.jobAssigned = true;
                }
                else
                {
                    const DWORD code = GetLastError();
                    implementation_->result.warning =
                        "Test Level Runtime could not join the Studio Job Object; "
                        "continuing with explicit process cleanup: " +
                        WindowsErrorMessage(code);
                    CloseHandle(implementation_->job);
                    implementation_->job = nullptr;
                }
            }
            else
            {
                const DWORD code = GetLastError();
                implementation_->result.warning =
                    "Studio could not create the Test Level Job Object; "
                    "continuing with explicit process cleanup: " +
                    WindowsErrorMessage(code);
            }
        }
        else
        {
            implementation_->result.warning =
                "Injected Job Object assignment failure; continuing with "
                "explicit process cleanup.";
        }

        if (ResumeThread(thread) == static_cast<DWORD>(-1))
        {
            const DWORD code = GetLastError();
            CloseHandle(thread);
            implementation_->ForceTerminate(ForcedStopExitCode, error);
            return failLaunch(
                "Could not resume the Test Level Runtime process: " +
                WindowsErrorMessage(code));
        }
        CloseHandle(thread);

        implementation_->launchedAt = std::chrono::steady_clock::now();
        implementation_->result.state = TestLevelProcessState::Starting;
        implementation_->result.message =
            "Test Level Runtime launched; waiting for startup readiness.";
        return true;
    }

    TestLevelProcessResult TestLevelRuntimeProcess::Poll()
    {
        if (implementation_ == nullptr || !IsActive())
        {
            return LastResult();
        }

        if (implementation_->failureInjection ==
                TestLevelProcessFailureInjection::WatchFailure &&
            !implementation_->watcherFailureInjected)
        {
            implementation_->watcherFailureInjected = true;
            return implementation_->ForceTerminateAndFinalize(
                TestLevelProcessState::WatchFailed,
                WatchFailureExitCode,
                "Injected Test Level watcher failure.");
        }

        if (implementation_->result.state == TestLevelProcessState::Starting)
        {
            const DWORD ready = WaitForSingleObject(
                implementation_->readyEvent,
                0);
            if (ready == WAIT_OBJECT_0)
            {
                implementation_->result.state = TestLevelProcessState::Running;
                implementation_->result.ready = true;
                implementation_->result.message =
                    "Test Level Runtime startup completed.";
            }
            else if (ready == WAIT_FAILED)
            {
                const DWORD code = GetLastError();
                return implementation_->ForceTerminateAndFinalize(
                    TestLevelProcessState::WatchFailed,
                    WatchFailureExitCode,
                    "Could not poll the Test Level startup-ready event: " +
                        WindowsErrorMessage(code));
            }
        }

        const DWORD wait = WaitForSingleObject(implementation_->process, 0);
        if (wait == WAIT_OBJECT_0)
        {
            DWORD exitCode = 0;
            if (!GetExitCodeProcess(implementation_->process, &exitCode))
            {
                const DWORD code = GetLastError();
                return implementation_->Finalize(
                    TestLevelProcessState::WatchFailed,
                    false,
                    0,
                    "Could not read the Test Level Runtime exit code: " +
                        WindowsErrorMessage(code));
            }

            // Renegade bootstrap failures intentionally exit before the ready
            // event can be signaled, so preserve their structured 20-29 code
            // range even when startup never reached Running.
            if (exitCode >= 20 && exitCode <= 29)
            {
                return implementation_->Finalize(
                    TestLevelProcessState::RuntimeReportedFailure,
                    false,
                    exitCode,
                    "Renegade Runtime reported startup/runtime failure code " +
                        std::to_string(exitCode) + ".");
            }
            if (!implementation_->result.ready)
            {
                return implementation_->Finalize(
                    TestLevelProcessState::AbnormalExit,
                    false,
                    exitCode,
                    "Test Level Runtime exited before reporting startup readiness.");
            }
            if (exitCode == 0)
            {
                return implementation_->Finalize(
                    TestLevelProcessState::Completed,
                    true,
                    exitCode,
                    "Test Level Runtime exited cleanly.");
            }
            return implementation_->Finalize(
                TestLevelProcessState::AbnormalExit,
                false,
                exitCode,
                "Test Level Runtime exited abnormally with code " +
                    std::to_string(exitCode) + ".");
        }
        if (wait == WAIT_FAILED)
        {
            const DWORD code = GetLastError();
            return implementation_->ForceTerminateAndFinalize(
                TestLevelProcessState::WatchFailed,
                WatchFailureExitCode,
                "WaitForSingleObject failed while watching Test Level: " +
                    WindowsErrorMessage(code));
        }

        if (implementation_->result.state == TestLevelProcessState::Starting &&
            std::chrono::steady_clock::now() - implementation_->launchedAt >=
                implementation_->startupTimeout)
        {
            return implementation_->ForceTerminateAndFinalize(
                TestLevelProcessState::StartupTimedOut,
                StartupTimeoutExitCode,
                "Test Level Runtime did not report successful startup before "
                "the startup timeout expired.");
        }

        return implementation_->result;
    }

    TestLevelProcessResult TestLevelRuntimeProcess::Stop()
    {
        if (implementation_ == nullptr || !IsActive())
        {
            return LastResult();
        }
        return implementation_->ForceTerminateAndFinalize(
            TestLevelProcessState::Stopped,
            ForcedStopExitCode,
            "Test Level Runtime was stopped by Studio.");
    }

    bool TestLevelRuntimeProcess::IsActive() const noexcept
    {
        return implementation_ != nullptr &&
            implementation_->process != nullptr &&
            !implementation_->result.finished;
    }

    const TestLevelProcessResult& TestLevelRuntimeProcess::LastResult() const noexcept
    {
        static const TestLevelProcessResult empty;
        return implementation_ != nullptr ? implementation_->result : empty;
    }

    bool TestLevelRuntimeProcess::WriteOwnershipMarker(
        const std::string& sessionDirectory,
        std::string& error)
    {
        error.clear();
        if (sessionDirectory.empty())
        {
            error = "Test Level ownership marker is missing a session directory.";
            return false;
        }

        DWORD processId = 0;
        std::uint64_t creationTime = 0;
        if (!CurrentProcessIdentity(processId, creationTime, error))
        {
            return false;
        }

        const fs::path markerPath =
            fs::u8path(sessionDirectory) / OwnershipMarker;
        std::ofstream stream(markerPath, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            error = "Could not create Test Level ownership marker: " +
                markerPath.generic_u8string();
            return false;
        }
        stream << "format=" << OwnershipFormat << '\n';
        stream << "version=" << OwnershipVersion << '\n';
        stream << "owner_pid=" << processId << '\n';
        stream << "owner_creation_time=" << creationTime << '\n';
        stream.flush();
        if (!stream)
        {
            error = "Could not finish Test Level ownership marker: " +
                markerPath.generic_u8string();
            return false;
        }
        return true;
    }

    bool TestLevelRuntimeProcess::SweepAbandonedSnapshots(
        const std::string& projectRoot,
        std::vector<std::string>& removedSessions,
        std::vector<std::string>& warnings)
    {
        removedSessions.clear();
        warnings.clear();
        if (projectRoot.empty())
        {
            warnings.push_back("Cannot sweep Test Level snapshots without a project root.");
            return false;
        }

        const fs::path root = fs::u8path(projectRoot);
        const fs::path snapshotsRoot =
            root / "Intermediate" / "TestLevelSnapshots";
        std::error_code error;
        if (!fs::is_directory(snapshotsRoot, error))
        {
            return true;
        }

        std::vector<std::string> abandoned;
        fs::directory_iterator iterator(snapshotsRoot, error);
        if (error)
        {
            warnings.push_back(
                "Could not enumerate Test Level snapshots: " +
                error.message());
            return false;
        }
        const fs::directory_iterator end;
        while (iterator != end)
        {
            const fs::directory_entry entry = *iterator;
            iterator.increment(error);
            if (error)
            {
                warnings.push_back(
                    "Could not continue enumerating Test Level snapshots: " +
                    error.message());
                return false;
            }
            if (!entry.is_directory())
            {
                continue;
            }

            const fs::path markerPath = entry.path() / OwnershipMarker;
            DWORD ownerPid = 0;
            std::uint64_t ownerCreationTime = 0;
            std::string markerWarning;
            if (!ReadOwnerMarker(
                    markerPath,
                    ownerPid,
                    ownerCreationTime,
                    markerWarning))
            {
                if (!markerWarning.empty())
                {
                    warnings.push_back(std::move(markerWarning));
                }
                continue;
            }

            std::string ownerWarning;
            const OwnerState owner = CheckOwner(
                ownerPid,
                ownerCreationTime,
                ownerWarning);
            if (owner == OwnerState::Unknown)
            {
                if (!ownerWarning.empty())
                {
                    warnings.push_back(std::move(ownerWarning));
                }
                continue;
            }
            if (owner == OwnerState::Alive)
            {
                continue;
            }
            abandoned.push_back(entry.path().generic_u8string());
        }

        for (const auto& session : abandoned)
        {
            std::string cleanupError;
            if (!bridge::TestLevelSnapshotService::CleanupDirectory(
                    projectRoot,
                    session,
                    cleanupError))
            {
                warnings.push_back(
                    "Could not clean abandoned Test Level snapshot " +
                    session + ": " + cleanupError);
                continue;
            }
            removedSessions.push_back(session);
        }
        return true;
    }

    const char* TestLevelRuntimeProcess::ReadyEventArgumentName() noexcept
    {
        return "--renegade-ready-event";
    }

    const char* TestLevelRuntimeProcess::OwnershipMarkerFileName() noexcept
    {
        return OwnershipMarker;
    }
}
