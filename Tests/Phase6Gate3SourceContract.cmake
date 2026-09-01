if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

set(service_header "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/AudioService.h")
set(service_source "${RENEGADE_SOURCE_DIR}/EngineBridge/src/AudioService.cpp")

foreach(path IN ITEMS "${service_header}" "${service_source}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Phase 6 Gate 3 missing required source: ${path}")
    endif()
endforeach()

# Check declaration/serialization authority where it actually lives. The
# Renegade metadata keys are inline header constants by design so Studio,
# Runtime and tests share one exact WISCENE contract.
foreach(token IN ITEMS
    "SoundSourceState"
    "SceneAudioMixState"
    "AudioBus"
    "CreateSoundSourceCommand"
    "SetSoundSourceCommand"
    "SetSceneAudioMixCommand"
    "ActivateSceneAudio"
    "SetSceneAudioPaused"
    "renegade.audio.source"
    "renegade.audio.mix")
    file(STRINGS "${service_header}" token_matches REGEX "${token}")
    if(NOT token_matches)
        message(FATAL_ERROR "Phase 6 Gate 3 bridge contract missing ${token}")
    endif()
endforeach()

# Check the implementation against the native Wicked surface. Use line-wise
# matching instead of loading the full C++ file into one CMake string; this is
# resilient to source size and keeps failures tied to the actual mapping line.
foreach(token IN ITEMS
    "Entity_CreateSound"
    "scene.sounds"
    "SetLooped"
    "SetDisable3D"
    "SetEnableReverb"
    "CreateSoundInstance"
    "SUBMIX_TYPE_SOUNDEFFECT"
    "SUBMIX_TYPE_MUSIC"
    "SUBMIX_TYPE_USER0"
    "SUBMIX_TYPE_USER1"
    "SetSubmixVolume"
    "SetReverb")
    file(STRINGS "${service_source}" token_matches REGEX "${token}")
    if(NOT token_matches)
        message(FATAL_ERROR "Phase 6 Gate 3 native Wicked mapping missing ${token}")
    endif()
endforeach()
