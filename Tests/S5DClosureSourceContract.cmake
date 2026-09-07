if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

function(read_required path output)
    set(full_path "${RENEGADE_SOURCE_DIR}/${path}")
    if(NOT EXISTS "${full_path}")
        message(FATAL_ERROR "S5D contract missing source file: ${path}")
    endif()
    file(READ "${full_path}" content)
    set(${output} "${content}" PARENT_SCOPE)
endfunction()

function(require_text haystack needle label)
    string(FIND "${haystack}" "${needle}" index)
    if(index EQUAL -1)
        message(FATAL_ERROR "S5D contract missing ${label}: ${needle}")
    endif()
endfunction()

function(require_order haystack first second label)
    string(FIND "${haystack}" "${first}" first_index)
    string(FIND "${haystack}" "${second}" second_index)
    if(first_index EQUAL -1 OR second_index EQUAL -1 OR
       NOT first_index LESS second_index)
        message(FATAL_ERROR
            "S5D contract lost ${label}: expected '${first}' before '${second}'")
    endif()
endfunction()

read_required("EngineBridge/include/renegade/bridge/DiagnosticService.h" diagnostic_h)
read_required("EngineBridge/src/DiagnosticService.cpp" diagnostic_cpp)
read_required("EngineBridge/src/DiagnosticEndpoint.cpp" endpoint_cpp)
read_required("Studio/src/StudioLiveDiagnostics.cpp" studio_diag)
read_required("Runtime/src/RuntimeLiveDiagnostics.cpp" runtime_diag)
read_required("Runtime/src/RuntimeApplication.cpp" runtime_app)
read_required("Runtime/src/main_Windows.cpp" runtime_main)
read_required("Studio/src/TestLevelRuntimeProcess.h" process_h)
read_required("Studio/src/TestLevelRuntimeProcess.cpp" process_cpp)
read_required("Tests/TestLevelRuntimeProcessTests.cpp" process_tests)
read_required("Tests/CMakeLists.txt" tests_cmake)
read_required("Tools/Read-RenegadeDiagnostics.py" reader_py)

# Structured copied-state diagnostics remain read-only and process-local.
foreach(required
    "void Identify(std::string processType)"
    "bool StartLocalEndpoint(std::uint16_t port = 38741)"
    "void SetState(std::string group, DiagnosticState state)"
    "void Observe(std::string group, DiagnosticState state, std::string source)"
    "void Heartbeat()"
    "ReadPeerSnapshot() const"
    "ReadPeerSummary() const")
    require_text("${diagnostic_h}" "${required}" "DiagnosticService API")
endforeach()
require_text("${diagnostic_cpp}" "renegade.diagnostics.v2" "diagnostic snapshot schema")
require_text("${endpoint_cpp}" "127.0.0.1" "loopback-only diagnostic transport")
require_text("${endpoint_cpp}" "GET /snapshot HTTP/1.1" "read-only snapshot route")
require_text("${endpoint_cpp}" "GET /summary HTTP/1.1" "read-only summary route")

# Studio and Runtime publish distinct endpoints and bounded machine-readable
# evidence. The Test Level group must carry the child association state.
require_text("${studio_diag}" "StartLocalEndpoint(38741)" "Studio endpoint")
require_text("${runtime_app}" "StartLocalEndpoint(38742)" "Runtime endpoint")
require_text("${studio_diag}" "Observe(\"test_level\"" "Studio Test Level group")
foreach(field
    "{\"active\", testLevelRuntime_.IsActive()}"
    "{\"ready\", play.ready}"
    "{\"state_code\""
    "{\"message\", play.message}"
    "{\"warning\", play.warning}"
    "{\"child_pid\"")
    require_text("${studio_diag}" "${field}" "Studio Test Level diagnostic field")
endforeach()
foreach(field
    "{\"project\", startupResult_.projectDescriptorPath}"
    "{\"scene\", scenes_.CurrentPath()}"
    "{\"startup_finished\", startupFinished_}"
    "{\"startup_succeeded\", startupResult_.succeeded}"
    "{\"player_spawned\", player_.IsSpawned()}"
    "{\"scripts_running\", creatorScripts_.IsRunning()}"
    "{\"scripts_active\""
    "{\"scripts_disabled\""
    "{\"audio_scene_synced\""
    "{\"audio_source_count\"")
    require_text("${runtime_diag}" "${field}" "Runtime diagnostic field")
endforeach()
require_text("${studio_diag}" "ReadPeerSummary()" "Studio Runtime peer evidence")
require_text("${reader_py}" "child_pid" "reader child PID association")
require_text("${reader_py}" "process" "reader process identity")

# Studio owns the supervised child and a unique per-launch named ready event.
foreach(required
    "Idle,"
    "Starting,"
    "Running,"
    "RuntimeReportedFailure,"
    "AbnormalExit,"
    "StartupTimedOut,"
    "Stopped,"
    "WatchFailed,")
    require_text("${process_h}" "${required}" "Test Level process state")
endforeach()
require_text("${process_h}" "std::uint32_t ProcessId() const noexcept" "child PID accessor")
require_text("${process_cpp}" "Local\\\\RenegadeTestLevelReady-" "unique ready-event namespace")
require_text("${process_cpp}" "readyEventSequence.fetch_add(1)" "per-launch ready-event sequence")
require_text("${process_cpp}" "CurrentProcessIdentity(processId, creationTime" "Studio process identity in ready event")
require_text("${process_cpp}" "CREATE_SUSPENDED" "suspended child launch")
require_text("${process_cpp}" "AssignProcessToJobObject" "kill-on-close Job Object supervision")
require_text("${process_cpp}" "ResumeThread(thread)" "child resume after supervision setup")
require_text("${process_cpp}" "implementation_->readyEvent" "ready-event supervision state")
require_text("${process_cpp}" "TestLevelProcessState::Running" "ready promotion to Running")
require_text("${process_cpp}" "implementation_->result.ready = true" "ready flag publication")
require_text("${process_cpp}" "TestLevelProcessState::StartupTimedOut" "startup timeout classification")
require_text("${process_cpp}" "TestLevelProcessState::RuntimeReportedFailure" "Runtime failure classification")
require_text("${process_cpp}" "TestLevelProcessState::WatchFailed" "watch failure classification")

# Runtime receives only the launch-specific event name and cannot signal it
# until its application startup has completed successfully.
require_text("${runtime_main}" "--renegade-ready-event=" "Runtime ready-event argument")
require_text("${runtime_main}" "application.StartupFinished()" "Runtime startup completion barrier")
require_text("${runtime_main}" "if (!result.succeeded)" "Runtime startup success gate")
require_text("${runtime_main}" "OpenEventW(" "Runtime opens Studio event")
require_text("${runtime_main}" "SetEvent(readyEvent)" "Runtime signals Studio readiness")
require_order("${runtime_main}"
    "application.StartupFinished()"
    "SetEvent(readyEvent)"
    "Runtime readiness occurs after StartupFinished")
require_order("${runtime_main}"
    "if (!result.succeeded)"
    "SetEvent(readyEvent)"
    "Runtime readiness occurs after startup success check")

# Temporary Test Level snapshots are session-owned. PID reuse must not turn an
# abandoned session into a live one: creation time is part of the identity.
require_text("${process_cpp}" "owner_creation_time=" "ownership creation timestamp")
require_text("${process_cpp}" "GetProcessTimes(" "owner process creation-time query")
require_text("${process_cpp}" "FileTimeValue(created) != expectedCreationTime" "PID reuse rejection")
require_text("${process_cpp}" "OwnerState::Unknown" "conservative unverifiable owner state")
require_text("${process_cpp}" "CleanupDirectory(" "owned snapshot cleanup")

# The production-path process tests, not a synthetic S5D-only substitute,
# exercise readiness, failure, timeout, stop and stale/live session recovery.
foreach(required
    "ready-exit0"
    "runtime-failure-before-ready"
    "ready-abnormal"
    "never-ready"
    "ready-hang"
    "WatchFailure"
    "JobAssignmentFailure"
    "pid-reuse"
    "live-owner")
    require_text("${process_tests}" "${required}" "Test Level process acceptance fixture")
endforeach()
require_text("${process_tests}" "TestLevelProcessState::Completed" "successful handshake acceptance")
require_text("${process_tests}" "TestLevelProcessState::StartupTimedOut" "timeout acceptance")
require_text("${process_tests}" "TestLevelProcessState::RuntimeReportedFailure" "Runtime failure acceptance")
require_text("${process_tests}" "TestLevelProcessState::Stopped" "owner stop acceptance")

# Both production tests must remain part of the normal CTest graph. S5D labels
# are added by Tests/S5CoreGameplayApi.cmake without duplicating test execution.
require_text("${tests_cmake}" "NAME RenegadeDiagnosticServiceTests" "diagnostic production test registration")
require_text("${tests_cmake}" "RenegadeTestLevelRuntimeProcessTests" "Test Level production test target")
require_text("${tests_cmake}" "NAME RenegadeTestLevelRuntimeProcessTests" "Test Level production test registration")

message(STATUS "S5D structured diagnostics and Studio/Test Level IPC source contract passed")
