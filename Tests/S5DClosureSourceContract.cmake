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
require_text("${endpoint_cpp}" "127.0.0.1" "loopback-only diagnostic transport")
require_text("${endpoint_cpp}" "GET /snapshot" "read-only snapshot route")

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
require_text("${process_h}" "TestLevelProcessState::" "placeholder")
