set(service_header "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/AnimationService.h")
set(service_source "${RENEGADE_SOURCE_DIR}/EngineBridge/src/AnimationService.cpp")
set(inspector_source "${RENEGADE_SOURCE_DIR}/Studio/src/Phase7AnimationInspector.cpp")
set(migration_source "${RENEGADE_SOURCE_DIR}/Studio/src/S1BInspectorSectionMigration.cpp")

foreach(path IN ITEMS
    "${service_header}"
    "${service_source}"
    "${inspector_source}"
    "${migration_source}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Phase 7A required source is missing: ${path}")
    endif()
endforeach()

file(READ "${service_header}" header_text)
file(READ "${service_source}" service_text)
file(READ "${inspector_source}" inspector_text)
file(READ "${migration_source}" migration_text)

function(require_text haystack needle description)
    string(FIND "${${haystack}}" "${needle}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Phase 7A contract missing ${description}: ${needle}")
    endif()
endfunction()

require_text(header_text "AnimationAuthoredState" "curated authored state")
require_text(header_text "SetAnimationAuthoredStateCommand" "undoable animation command")
require_text(service_text "ResolveSceneComponentAuthoringRoot" "selected-hierarchy ownership resolver")
require_text(service_text "scene.animations" "native Wicked animation component access")
require_text(service_text "animation->Play()" "native Wicked playback")
require_text(service_text "animation->Pause()" "native Wicked pause")
require_text(service_text "animation->SetPingPong()" "native Wicked playback mode")
require_text(service_text "animation->RootMotionOn()" "native Wicked root-motion flag")
require_text(inspector_text "ANIMATION" "creator-facing animation section")
require_text(inspector_text "PlayAnimationPreviewFromStart" "play-from-start transport")
require_text(inspector_text "ScrubAnimationPreview" "preview scrub transport")
require_text(inspector_text "SetAnimationAuthoredStateCommand" "command-backed authored controls")
require_text(inspector_text "LiveAnimationStatusLabel" "live playback readout")
require_text(migration_text "RegisterPhase7AnimationInspector" "Inspector registry integration")
require_text(migration_text "PreparePhase7AnimationInspector" "Inspector layout integration")

# Guard the key architecture rule: preview transport must not rewrite authored
# speed as a side effect. Speed changes belong only to the command-backed path.
string(REGEX MATCH "PlayAnimationPreview\\([^}]*speed[ ]*=" preview_speed_write "${service_text}")
if(preview_speed_write)
    message(FATAL_ERROR "Phase 7A preview transport writes authored speed")
endif()

message(STATUS "Phase 7A animation source contract passed")
