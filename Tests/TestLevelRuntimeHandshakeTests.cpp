// LP04 Gate 3B2a - real Runtime handshake proof.
//
// Deliberately NOT wired into CTest/CI yet (see Tests/CMakeLists.txt: this
// target is EXCLUDE_FROM_ALL with no add_test registration). Everything
// tested here has only been proven with a synthetic fixture
// (RenegadeTestLevelProcessFixture) so far, or in isolation:
//   - TestLevelSnapshotRuntimeTests proves a real snapshot resolves and
//     loads through Runtime's own bootstrap logic, in-process, no window.
//   - TestLevelRuntimeProcessTests proves the launcher/watcher/cleanup
//     contract against a synthetic fixture that fakes readiness.
//   - main_Windows.cpp now signals a real ready event, but nothing has
//     launched the real windowed RenegadeRuntime.exe end to end yet.
//
// This test launches the real, windowed RenegadeRuntime.exe against a real
// snapshot and proves the full join: snapshot -> launch -> real DX12
// startup -> real ready signal -> Running -> Stop() -> process dies ->
// snapshot cleaned up. It will visibly open and close a real window.
//
// Run manually first (see the build/run instructions in the LP04 handoff
// notes) before this is promoted into CTest per Gate 3B2b.

#include "TestLevelRuntimeProcess.h"

#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/StudioSession.h"
#include "renegade/bridge/TestLevelSnapshotService.h"

#include <Windows.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

namespace
{
    namespace fs = std::filesystem;
    using renegade::bridge::ProjectMetadata;
    using renegade::bridge::ProjectService;
    using renegade::bridge::StudioSession;
    using renegade::bridge::TestLevelSnapshot;
    using renegade::bridge::TestLevelSnapshotService;
    using renegade::studio::TestLevelLaunchOptions;
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

    fs::path MakeRoot()
    {
        const fs::path root = fs::temp_directory_path() /
            ("RenegadeGate3B2-" +
             std::to_string(GetCurrentProcessId()) + "-" +
             std::to_string(GetTickCount64()));
        fs::create_directories(root);
        return root;
    }

    const char* StateName(const TestLevelProcessState state)
    {
        switch (state)
        {
            case TestLevelProcessState::Idle: return "Idle";
            case TestLevelProcessState::Starting: return "Starting";
            case TestLevelProcessState::Running: return "Running";
            case TestLevelProcessState::Completed: return "Completed";
            case TestLevelProcessState::LaunchFailed: return "LaunchFailed";
            case TestLevelProcessState::RuntimeReportedFailure:
                return "RuntimeReportedFailure";
            case TestLevelProcessState::AbnormalExit: return "AbnormalExit";
            case TestLevelProcessState::StartupTimedOut:
                return "StartupTimedOut";
            case TestLevelProcessState::Stopped: return "Stopped";
            case TestLevelProcessState::WatchFailed: return "WatchFailed";
        }
        return "Unknown";
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr <<
            "Runtime executable path argument is required "
            "(pass $<TARGET_FILE:RenegadeRuntime>).\n";
        return 2;
    }
    const std::string runtimePath = argv[1];
    if (!fs::is_regular_file(fs::u8path(runtimePath)))
    {
        std::cerr << "RenegadeRuntime.exe not found at: " << runtimePath
                   << '\n';
        return 2;
    }

    const fs::path root = MakeRoot();
    const fs::path projectRoot = root / "Project";
    const fs::path scenePath =
        projectRoot / "Content" / "Scenes" / "Authoritative.wiscene";
    fs::create_directories(scenePath.parent_path());

    ProjectMetadata project;
    project.formatVersion = ProjectService::CurrentFormatVersion;
    project.projectId = "77777777-7777-4777-8777-777777777777";
    project.name = "Gate 3B2 Handshake Fixture";
    project.descriptorPath =
        (projectRoot / "Handshake.renegade").generic_u8string();
    project.rootPath = projectRoot.generic_u8string();
    project.startupScene = "Content/Scenes/Authoritative.wiscene";

    StudioSession session;
    const auto entity = wi::ecs::CreateEntity();
    session.Scenes().GetScene().names.Create(entity) = "Gate 3B2 Entity";
    auto& transform = session.Scenes().GetScene().transforms.Create(entity);
    transform.translation_local = XMFLOAT3(0.0f, 0.0f, 0.0f);
    transform.SetDirty();
    transform.UpdateTransform();

    if (!session.SaveScene(scenePath.generic_u8string()))
    {
        std::cerr << "Could not save the Gate 3B2 handshake fixture scene.\n";
        std::error_code ignored;
        fs::remove_all(root, ignored);
        return 1;
    }

    TestLevelSnapshotService snapshots(session.Scenes(), session.Commands());
    TestLevelSnapshot snapshot;
    std::string snapshotError;
    if (!snapshots.Create(project, snapshot, snapshotError))
    {
        std::cerr << "Snapshot creation failed: " << snapshotError << '\n';
        std::error_code ignored;
        fs::remove_all(root, ignored);
        return 1;
    }
    Expect(snapshot.IsRuntimeReady(),
        "snapshot is a Runtime-ready shadow project");

    TestLevelLaunchOptions options;
    options.executablePath = runtimePath;
    options.workingDirectory =
        fs::u8path(runtimePath).parent_path().generic_u8string();
    options.arguments = {"dx12", "--project", snapshot.descriptorPath};
    // Generous first-attempt allowance: this is the first time this
    // codebase has ever launched the real windowed Runtime under
    // automated control, and DX12 device/window creation timing on this
    // machine is not yet empirically known. Tighten once real timing is
    // observed (see the manual run notes this test's result should be
    // recorded against).
    options.startupTimeout = std::chrono::milliseconds(60000);

    TestLevelRuntimeProcess process;
    std::string launchError;
    const bool launched = process.Launch(options, snapshot, launchError);
    Expect(launched,
        "real RenegadeRuntime.exe launches: " + launchError);

    if (launched)
    {
        const auto deadline = std::chrono::steady_clock::now() +
            std::chrono::seconds(75);
        TestLevelProcessResult result = process.LastResult();
        while (std::chrono::steady_clock::now() < deadline)
        {
            result = process.Poll();
            if (result.state == TestLevelProcessState::Running ||
                result.finished)
            {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        std::cout << "Real Runtime state after wait: "
                   << StateName(result.state)
                   << " (exitCode=" << result.exitCode
                   << ", message=\"" << result.message << "\""
                   << (result.warning.empty()
                           ? std::string()
                           : ", warning=\"" + result.warning + "\"")
                   << ")\n";

        if (result.state != TestLevelProcessState::Running)
        {
            ++failures;
            std::cerr <<
                "FAIL: real RenegadeRuntime.exe did not reach Running "
                "within the wait window.\n";
            if (process.IsActive())
            {
                static_cast<void>(process.Stop());
            }
        }
        else
        {
            const auto stopResult = process.Stop();
            Expect(stopResult.state == TestLevelProcessState::Stopped,
                "Stop() terminates the real running Runtime");
            Expect(
                stopResult.cleanupAttempted && stopResult.cleanupSucceeded,
                "snapshot cleanup succeeded after Stop()");
            Expect(
                !fs::exists(fs::u8path(snapshot.sessionDirectory)),
                "snapshot session directory removed after Stop()");
        }
    }

    std::error_code ignored;
    fs::remove_all(root, ignored);

    if (failures != 0)
    {
        std::cerr << failures
                   << " Gate 3B2 real Runtime handshake assertion(s) failed.\n";
        return 1;
    }
    std::cout <<
        "LP04 Gate 3B2 real Runtime handshake proof passed.\n";
    return 0;
}
