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
    "GameplayInput.renegade-input"
    "renegade-input-map"
    "Pause,"
    "Reset,"
    "GameplayInputFrame"
    "CaptureGameplayInput")
    string(FIND "${input_header_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 6 Gate 2 input contract missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "LEFT_Y_POS"
    "MOUSE_X"
    "BUTTON_2"
    "ESCAPE"
    "ProjectDocumentTransaction")
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
    "wi::Application::Update((paused_||!collisionReady) ? 0.0f : dt)"
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

# Gameplay and the asynchronous loading screen are mutually exclusive paths.
# Each must have one native frame boundary; loading advances presentation with
# zero world time. Counting the whole file alone incorrectly rejects this split.
string(FIND "${runtime_text}" "void RuntimeApplication::Update(const float dt)" gameplay_begin)
string(FIND "${runtime_text}" "void RuntimeApplication::SyncLevelHud()" gameplay_end)
string(FIND "${runtime_text}" "void RuntimeApplication::UpdateLoadingScreen(float dt)" loading_begin)
string(FIND "${runtime_text}" "void RuntimeApplication::ProcessPendingActions()" loading_end)
if(gameplay_begin EQUAL -1 OR gameplay_end LESS gameplay_begin OR
   loading_begin EQUAL -1 OR loading_end LESS loading_begin)
    message(FATAL_ERROR "Runtime gameplay/loading frame owners are missing")
endif()
math(EXPR gameplay_length "${gameplay_end}-${gameplay_begin}")
math(EXPR loading_length "${loading_end}-${loading_begin}")
string(SUBSTRING "${runtime_text}" ${gameplay_begin} ${gameplay_length} gameplay_text)
string(SUBSTRING "${runtime_text}" ${loading_begin} ${loading_length} loading_text)
foreach(owner IN ITEMS gameplay loading)
    string(REGEX MATCHALL "wi::Application::Update\\(" native_updates "${${owner}_text}")
    list(LENGTH native_updates native_update_count)
    if(NOT native_update_count EQUAL 1)
        message(FATAL_ERROR "Runtime ${owner} path must retain exactly one native Application update")
    endif()
endforeach()
string(REGEX MATCHALL "wi::Application::Update\\(" all_native_updates "${runtime_text}")
list(LENGTH all_native_updates all_native_update_count)
if(NOT all_native_update_count EQUAL 2)
    message(FATAL_ERROR "Runtime native updates must belong only to gameplay and loading")
endif()
string(FIND "${gameplay_text}" "if (pendingLevelLoad_) { UpdateLoadingScreen(dt); return; }" loading_return)
string(FIND "${loading_text}" "wi::Application::Update(0);" loading_frozen)
if(loading_return EQUAL -1 OR loading_frozen EQUAL -1)
    message(FATAL_ERROR "Loading must return before gameplay and keep native simulation time at zero")
endif()
string(FIND "${gameplay_text}" "collisionReady=!activeCollision||activeCollision->UpdateSceneInterests(" admission)
string(FIND "${gameplay_text}" "wi::Application::Update(" native_update)
if(admission EQUAL -1 OR admission GREATER native_update)
    message(FATAL_ERROR "Collision admission must precede the single native gameplay step")
endif()
string(FIND "${runtime_text}" "if (!paused_&&collisionReady)" scripts_gate)
if(scripts_gate EQUAL -1)
    message(FATAL_ERROR "Runtime scripts must respect pause and collision readiness")
endif()

string(FIND "${runtime_text}" "CapturePlayerInput" stale_raw_input)
if(NOT stale_raw_input EQUAL -1)
    message(FATAL_ERROR "Phase 6 Gate 2 left Gate 1 hardcoded Runtime input polling in place")
endif()
