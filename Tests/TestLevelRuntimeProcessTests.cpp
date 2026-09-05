#include "TestLevelRuntimeProcess.h"

#include <Windows.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using renegade::bridge::TestLevelSnapshot;
    using renegade::studio::TestLevelLaunchOptions;
    using renegade::studio::TestLevelProcessFailureInjection;
    using renegade::studio::TestLevelProcessResult;
    using renegade::studio::TestLevelProcessState;
    using renegade::studio::TestLevelRuntimeProcess;

    int failures = 0;

    void Expect(const bool condition, const std::string& message)
    {
        if (!condition)
        {
            ++failures;
            std::cerr << "FAIL: " << message << '\n';
        }
    }

    std::uint64_t CurrentCreationTime()
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
            return 0;
        }
        ULARGE_INTEGER value = {};
        value.LowPart = created.dwLowDateTime;
        value.HighPart = created.dwHighDateTime;
        return value.QuadPart;
    }

    fs::path MakeRoot(const std::string& name)
    {
        const fs::path root = fs::temp_directory_path() /
            ("RenegadeGate3A-" + name + "-" +
             std::to_string(GetCurrentProcessId()) + "-" +
             std::to_string(GetTickCount64()));
        fs::create_directories(root);
        return root;
    }

    TestLevelSnapshot MakeSnapshot(
        const fs::path& root,
        const std::string& token)
    {
        TestLevelSnapshot snapshot;
        const fs::path session =
            root / "Intermediate" / "TestLevelSnapshots" / token;
        fs::create_directories(session / "Content" / "Scenes");
        std::ofstream(session / "TestLevel.renegade") << "fixture";
        std::ofstream(session / "Content" / "Scenes" / "TestLevel.wiscene")
            << "fixture";
        snapshot.projectRoot = root.generic_u8string();
        snapshot.sessionDirectory = session.generic_u8string();
        snapshot.scenePath =
            (session / "Content" / "Scenes" / "TestLevel.wiscene")
                .generic_u8string();
        snapshot.descriptorPath =
            (session / "TestLevel.renegade").generic_u8string();
        return snapshot;
    }

    TestLevelLaunchOptions Options(
        const std::string& fixturePath,
        const std::string& mode,
        const int timeoutMilliseconds = 1500)
    {
        TestLevelLaunchOptions options;
        options.executablePath = fixturePath;
        options.workingDirectory =
            fs::u8path(fixturePath).parent_path().generic_u8string();
        options.arguments = {"--fixture=" + mode};
        options.startupTimeout =
            std::chrono::milliseconds(timeoutMilliseconds);
        return options;
    }

    TestLevelProcessResult PollUntilFinished(
        TestLevelRuntimeProcess& process,
        const int maximumMilliseconds = 5000)
    {
        const auto deadline = std::chrono::steady_clock::now() +
            std::chrono::milliseconds(maximumMilliseconds);
        TestLevelProcessResult result = process.LastResult();
        while (std::chrono::steady_clock::now() < deadline)
        {
            result = process.Poll();
            if (result.finished)
            {
                return result;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return process.Stop();
    }

    void ExpectCleaned(
        const TestLevelSnapshot& snapshot,
        const TestLevelProcessResult& result,
        const std::string& label)
    {
        Expect(result.cleanupAttempted, label + " attempted cleanup");
        Expect(result.cleanupSucceeded, label + " cleanup succeeded");
        Expect(!fs::exists(fs::u8path(snapshot.sessionDirectory)),
            label + " removed snapshot session");
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Fixture executable path argument is required.\n";
        return 2;
    }
    const std::string fixturePath = argv[1];

    {
        const fs::path root = MakeRoot("missing-exe");
        const auto snapshot = MakeSnapshot(root, "session");
        TestLevelRuntimeProcess process;
        std::string error;
        auto options = Options(fixturePath + ".missing", "ready-exit0");
        const bool launched = process.Launch(options, snapshot, error);
        Expect(!launched, "missing executable launch fails");
        Expect(process.LastResult().state == TestLevelProcessState::LaunchFailed,
            "missing executable classified as LaunchFailed");
        ExpectCleaned(snapshot, process.LastResult(), "missing executable");
        fs::remove_all(root);
    }

    {
        const fs::path root = MakeRoot("bad-exe");
        const auto snapshot = MakeSnapshot(root, "session");
        const fs::path badExecutable = root / "NotAnExecutable.exe";
        std::ofstream(badExecutable, std::ios::binary) << "not a PE image";
        TestLevelRuntimeProcess process;
        std::string error;
        auto options = Options(
            badExecutable.generic_u8string(),
            "ready-exit0");
        const bool launched = process.Launch(options, snapshot, error);
        Expect(!launched, "CreateProcessW failure is reported");
        Expect(process.LastResult().state == TestLevelProcessState::LaunchFailed,
            "CreateProcessW failure classified as LaunchFailed");
        ExpectCleaned(snapshot, process.LastResult(), "CreateProcessW failure");
        fs::remove_all(root);
    }

    {
        const fs::path root = MakeRoot("clean");
        const auto snapshot = MakeSnapshot(root, "session");
        TestLevelRuntimeProcess process;
        std::string error;
        Expect(process.Launch(Options(fixturePath, "ready-exit0"), snapshot, error),
            "clean fixture launches");
        const auto result = PollUntilFinished(process);
        Expect(result.state == TestLevelProcessState::Completed,
            "clean exit classified as Completed");
        Expect(result.succeeded && result.ready && result.exitCode == 0,
            "clean exit preserves ready/success/exit code");
        ExpectCleaned(snapshot, result, "clean exit");
        fs::remove_all(root);
    }

    {
        // Story Flow Preview launches the governed project directly. It must
        // use the same supervised Runtime lifecycle without inventing or
        // deleting an LP04 Test Level snapshot.
        TestLevelRuntimeProcess process;
        TestLevelSnapshot noSnapshot;
        std::string error;
        auto options = Options(fixturePath, "ready-exit0");
        options.ownsSnapshot = false;
        Expect(process.Launch(options, noSnapshot, error),
            "governed project preview launches without snapshot metadata");
        const auto result = PollUntilFinished(process);
        Expect(result.state == TestLevelProcessState::Completed &&
                result.succeeded && result.ready,
            "governed project preview preserves Runtime supervision");
        Expect(!result.cleanupAttempted && result.cleanupSucceeded,
            "governed project preview performs no snapshot cleanup");
    }

    {
        const fs::path root = MakeRoot("runtime-failure");
        const auto snapshot = MakeSnapshot(root, "session");
        TestLevelRuntimeProcess process;
        std::string error;
        Expect(process.Launch(
                Options(fixturePath, "runtime-failure-before-ready"),
                snapshot,
                error),
            "pre-ready runtime failure fixture launches");
        const auto result = PollUntilFinished(process);
        Expect(result.state == TestLevelProcessState::RuntimeReportedFailure,
            "pre-ready exit 24 classified as RuntimeReportedFailure");
        Expect(result.exitCode == 24, "runtime failure exit code retained");
        ExpectCleaned(snapshot, result, "runtime failure");
        fs::remove_all(root);
    }

    {
        const fs::path root = MakeRoot("abnormal");
        const auto snapshot = MakeSnapshot(root, "session");
        TestLevelRuntimeProcess process;
        std::string error;
        Expect(process.Launch(
                Options(fixturePath, "ready-abnormal"),
                snapshot,
                error),
            "abnormal fixture launches");
        const auto result = PollUntilFinished(process);
        Expect(result.state == TestLevelProcessState::AbnormalExit,
            "exit 99 classified as AbnormalExit");
        Expect(result.exitCode == 99, "abnormal exit code retained");
        ExpectCleaned(snapshot, result, "abnormal exit");
        fs::remove_all(root);
    }

    {
        const fs::path root = MakeRoot("timeout");
        const auto snapshot = MakeSnapshot(root, "session");
        TestLevelRuntimeProcess process;
        std::string error;
        Expect(process.Launch(
                Options(fixturePath, "never-ready", 150),
                snapshot,
                error),
            "never-ready fixture launches");
        const auto result = PollUntilFinished(process, 3000);
        Expect(result.state == TestLevelProcessState::StartupTimedOut,
            "never-ready process hits real startup timeout");
        ExpectCleaned(snapshot, result, "startup timeout");
        fs::remove_all(root);
    }

    {
        const fs::path root = MakeRoot("stop");
        const auto snapshot = MakeSnapshot(root, "session");
        TestLevelRuntimeProcess process;
        std::string error;
        Expect(process.Launch(Options(fixturePath, "ready-hang"), snapshot, error),
            "ready-hang fixture launches");
        const auto readyDeadline = std::chrono::steady_clock::now() +
            std::chrono::seconds(2);
        while (std::chrono::steady_clock::now() < readyDeadline &&
            process.LastResult().state != TestLevelProcessState::Running)
        {
            process.Poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        Expect(process.LastResult().state == TestLevelProcessState::Running,
            "ready-hang fixture reaches Running with no play-session timeout");
        const auto result = process.Stop();
        Expect(result.state == TestLevelProcessState::Stopped,
            "Stop terminates running Test Level");
        ExpectCleaned(snapshot, result, "manual stop");
        fs::remove_all(root);
    }

    {
        const fs::path root = MakeRoot("destructor");
        const auto snapshot = MakeSnapshot(root, "session");
        {
            TestLevelRuntimeProcess process;
            std::string error;
            Expect(process.Launch(Options(fixturePath, "ready-hang"), snapshot, error),
                "destructor fixture launches");
            const auto readyDeadline = std::chrono::steady_clock::now() +
                std::chrono::seconds(2);
            while (std::chrono::steady_clock::now() < readyDeadline &&
                process.LastResult().state != TestLevelProcessState::Running)
            {
                process.Poll();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            Expect(process.LastResult().state == TestLevelProcessState::Running,
                "destructor fixture reaches Running");
        }
        Expect(!fs::exists(fs::u8path(snapshot.sessionDirectory)),
            "process owner destructor terminates child and cleans snapshot");
        fs::remove_all(root);
    }

    {
        const fs::path root = MakeRoot("watch-failure");
        const auto snapshot = MakeSnapshot(root, "session");
        TestLevelRuntimeProcess process;
        std::string error;
        auto options = Options(fixturePath, "ready-hang");
        options.failureInjection = TestLevelProcessFailureInjection::WatchFailure;
        Expect(process.Launch(options, snapshot, error),
            "watch failure fixture launches");
        const auto result = process.Poll();
        Expect(result.state == TestLevelProcessState::WatchFailed,
            "watcher failure classified as WatchFailed");
        ExpectCleaned(snapshot, result, "watcher failure");
        fs::remove_all(root);
    }

    {
        const fs::path root = MakeRoot("job-failure");
        const auto snapshot = MakeSnapshot(root, "session");
        TestLevelRuntimeProcess process;
        std::string error;
        auto options = Options(fixturePath, "ready-exit0");
        options.failureInjection =
            TestLevelProcessFailureInjection::JobAssignmentFailure;
        Expect(process.Launch(options, snapshot, error),
            "job assignment failure does not block launch");
        const auto result = PollUntilFinished(process);
        Expect(result.state == TestLevelProcessState::Completed,
            "job assignment failure still permits clean completion");
        Expect(!result.jobAssigned && !result.warning.empty(),
            "job assignment failure is surfaced as defense-in-depth warning");
        ExpectCleaned(snapshot, result, "job assignment failure");
        fs::remove_all(root);
    }

    {
        const fs::path root = MakeRoot("pid-reuse");
        const auto snapshot = MakeSnapshot(root, "session");
        const fs::path marker = fs::u8path(snapshot.sessionDirectory) /
            TestLevelRuntimeProcess::OwnershipMarkerFileName();
        std::ofstream stream(marker, std::ios::binary | std::ios::trunc);
        stream << "format=renegade-test-level-owner\n";
        stream << "version=1\n";
        stream << "owner_pid=" << GetCurrentProcessId() << '\n';
        stream << "owner_creation_time=" << (CurrentCreationTime() + 1) << '\n';
        stream.close();

        std::vector<std::string> removed;
        std::vector<std::string> warnings;
        Expect(TestLevelRuntimeProcess::SweepAbandonedSnapshots(
                root.generic_u8string(), removed, warnings),
            "PID-reuse sweep completes");
        Expect(removed.size() == 1,
            "same PID with different creation time is abandoned");
        Expect(!fs::exists(fs::u8path(snapshot.sessionDirectory)),
            "PID-reuse stale session is removed");
        fs::remove_all(root);
    }

    {
        const fs::path root = MakeRoot("live-owner");
        const auto snapshot = MakeSnapshot(root, "session");
        std::string markerError;
        Expect(TestLevelRuntimeProcess::WriteOwnershipMarker(
                snapshot.sessionDirectory, markerError),
            "live owner marker writes");
        std::vector<std::string> removed;
        std::vector<std::string> warnings;
        Expect(TestLevelRuntimeProcess::SweepAbandonedSnapshots(
                root.generic_u8string(), removed, warnings),
            "live-owner sweep completes");
        Expect(removed.empty(), "live owner session is retained");
        Expect(fs::exists(fs::u8path(snapshot.sessionDirectory)),
            "live owner snapshot remains on disk");
        std::string cleanupError;
        renegade::bridge::TestLevelSnapshotService::CleanupDirectory(
            snapshot.projectRoot,
            snapshot.sessionDirectory,
            cleanupError);
        fs::remove_all(root);
    }

    if (failures != 0)
    {
        std::cerr << failures << " Gate 3A assertion(s) failed.\n";
        return 1;
    }
    std::cout << "LP04 Gate 3A process lifecycle proof passed.\n";
    return 0;
}
