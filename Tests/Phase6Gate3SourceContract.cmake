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

file(READ "${service_header}" header_text)
file(READ "${service_source}" source_text)

foreach(token IN ITEMS
    "SoundSourceState"
    "SceneAudioMixState"
    "AudioBus"
    "CreateSoundSourceCommand"
    "SetSoundSourceCommand"
    "SetSceneAudioMixCommand"
    "ActivateSceneAudio")
    string(FIND "${header_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 6 Gate 3 bridge contract missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "Entity_CreateSound"
    "scene.sounds"
    "sound->SetLooped"
    "sound->SetDisable3D"
    "sound->soundinstance.SetEnableReverb"
    "SUBMIX_TYPE_SOUNDEFFECT"
    "SUBMIX_TYPE_MUSIC"
    "SUBMIX_TYPE_USER0"
    "SUBMIX_TYPE_USER1"
    "SetSubmixVolume"
    "SetReverb"
    "renegade.audio.source"
    "renegade.audio.mix")
    string(FIND "${service_source}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 6 Gate 3 native Wicked mapping missing ${token}")
    endif()
endforeach()
