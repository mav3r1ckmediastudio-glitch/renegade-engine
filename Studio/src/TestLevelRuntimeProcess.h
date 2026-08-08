#pragma once

#include "renegade/bridge/TestLevelSnapshotService.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace renegade::studio
{
    enum class TestLevelProcessState
    {
        Idle,
        Starting,
        Running,
        Completed,
        LaunchFailed,
        RuntimeReportedFailure,
        AbnormalExit,
        StartupTimedOut,
        Stopped,
        WatchFailed,
    };

    enum class TestLevelProcessFailureInjection
    {
        None,
        JobAssignmentFailure,
        WatchFailure,
    };

    struct TestLevelProcessResult
    {
        TestLevelProcessState state = TestLevelProcessState::Idle;
        bool finished = false;
        bool succeeded = false;
        bool ready = false;
        bool cleanupAttempted = false;
        bool cleanupSucceeded = false;
        bool jobAssigned = false;
        std::uint32_t exitCode = 0;
        std::string message;
        std::string warning;
    };

    struct TestLevelLaunchOptions
    {
        std::string executablePath;
        std::string workingDirectory;
        std::vector<std::string> arguments;
        std::chrono::milliseconds startupTimeout{15000};
        TestLevelProcessFailureInjection failureInjection =
            TestLevelProcessFailureInjection::None;
    };

    // Windows-owned LP04 Gate 3A primitive. It launches a separate process in
    // CREATE_SUSPENDED state, tries to attach it to a kill-on-close Job Object,
    // resumes it, waits non-blockingly for an explicit startup-ready event,
    // classifies exit codes, and owns snapshot cleanup on every terminal path.
    //
    // Job assignment is defense in depth: failure is recorded as a warning and
    // launch continues. Snapshot cleanup and explicit child termination remain
    // the primary lifecycle guarantees.
    class TestLevelRuntimeProcess final
    {
    public:
        TestLevelRuntimeProcess();
        ~TestLevelRuntimeProcess();

        TestLevelRuntimeProcess(const TestLevelRuntimeProcess&) = delete;
        TestLevelRuntimeProcess& operator=(const TestLevelRuntimeProcess&) = delete;

        [[nodiscard]] bool Launch(
            TestLevelLaunchOptions options,
            bridge::TestLevelSnapshot snapshot,
            std::string& error);

        [[nodiscard]] TestLevelProcessResult Poll();
        [[nodiscard]] TestLevelProcessResult Stop();

        [[nodiscard]] bool IsActive() const noexcept;
        [[nodiscard]] const TestLevelProcessResult& LastResult() const noexcept;

        // Writes the Studio process identity into the snapshot session. The
        // creation timestamp is stored with the PID so abandoned-session
        // recovery cannot be fooled by Windows reusing a PID later.
        [[nodiscard]] static bool WriteOwnershipMarker(
            const std::string& sessionDirectory,
            std::string& error);

        // Removes only marked abandoned direct-child sessions. A session is
        // considered live only when both PID and process creation time match.
        // Unmarked or unverifiable sessions are kept conservatively.
        [[nodiscard]] static bool SweepAbandonedSnapshots(
            const std::string& projectRoot,
            std::vector<std::string>& removedSessions,
            std::vector<std::string>& warnings);

        [[nodiscard]] static const char* ReadyEventArgumentName() noexcept;
        [[nodiscard]] static const char* OwnershipMarkerFileName() noexcept;

    private:
        struct Implementation;
        Implementation* implementation_ = nullptr;
    };
}
