if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

set(input_header "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/GameplayInputService.h")
set(input_source "${RENEGADE_SOURCE_DIR}/EngineBridge/src/GameplayInputService.cpp")
set(studio_project "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/StudioProjectService.h")
set(runtime_header "${RENEGADE_SOURCE_DIR}/Runtime/src/RuntimeApplication.h")
set(runtime_source "${RENEGADE_SOURCE_DIR}/Runtime/src/RuntimeApplication.cpp")

foreach(path IN ITEMS
    "${input_header}"
    "${input_source}"
    "${studio_project}"
    "${runtime_header}"
    "${runtime_source}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Phase 6 Gate 2 missing required source: ${path}")
    endif()
endforeach()

file(READ "${input_header}" input_header_text)
file(READ "${input_source}" input_source_text)
file(READ "${studio_project}" studio_project_text)
file(READ "${runtime_header}" runtime_header_text)
file(READ "${runtime_source}" runtime_text)

foreach(token IN ITEMS
    "GameplayInputDocumentRelativePath"
    "GameplayAction::Pause"
    "GameplayAction::Reset"
    "GameplayInputFrame"
    "CaptureGameplayInput")
    string(FIND "${input_header_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 6 Gate 2 input contract missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "renegade-input-map"
    "LEFT_Y_POS"
    "MOUSE_X"
    "BUTTON_2"
    "ESCAPE"
    "ProjectDocumentTransaction"
    "GameplayInput.renegade-input")
    string(FIND "${input_source_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 6 Gate 2 input implementation missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "PrepareGameplayInput"
    "EnsureGameplayInputMap"
    "data:Content/Data/GameplayInput.renegade-input"
    "SetAlwaysInclude")
    string(FIND "${studio_project_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 6 Gate 2 project governance missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "LoadGameplayInput"
    "ResetPlaySession"
    "SetPaused"
    "GameplayInputMap")
    string(FIND "${runtime_header_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 6 Gate 2 Runtime declaration missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "CaptureGameplayInput(inputMap_, dt)"
    "wi::Application::Update(paused_ ? 0.0f : dt)"
    "wi::physics::SetSimulationEnabled(false)"
    "LoadRuntimeProjectScene"
    "LoadRuntimeProjectFlow"
    "ESC RESUME"
    "play session reset to its authored startup state")
    string(FIND "${runtime_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 6 Gate 2 Runtime lifecycle missing ${token}")
    endif()
endforeach()

string(FIND "${runtime_text}" "CapturePlayerInput" stale_raw_input)
if(NOT stale_raw_input EQUAL -1)
    message(FATAL_ERROR "Phase 6 Gate 2 left Gate 1 hardcoded Runtime input polling in place")
endif()
