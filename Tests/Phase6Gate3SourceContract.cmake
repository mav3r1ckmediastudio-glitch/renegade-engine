if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

set(service_header "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/AudioService.h")
set(service_source "${RENEGADE_SOURCE_DIR}/EngineBridge/src/AudioService.cpp")
set(studio_audio_chrome "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadePhysicsLabStudioChrome.cpp")
set(studio_audio_workspace "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeAudioWorkspace.cpp")
set(studio_application_header "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.h")

foreach(path IN ITEMS
    "${service_header}"
    "${service_source}"
    "${studio_audio_chrome}"
    "${studio_audio_workspace}"
    "${studio_application_header}")
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

# Studio layout contract. ViewportBounds is left/top/right/bottom, while the
# Audio workspace expects x/y/width/height. Gate 3 must therefore anchor the
# Inspector at viewport.z and derive its height from bottom - top. AUDIO also
# lives in the viewport-tool row; it must never reuse RENDER's +224..286 slot.
file(READ "${studio_audio_chrome}" studio_chrome_text)
foreach(token IN ITEMS
    "AudioViewportToolBounds"
    "AudioViewportToolHit"
    "RenderAudioViewportTool"
    "viewport.z"
    "viewport.w - viewport.y")
    string(FIND "${studio_chrome_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 6 Gate 3 Studio layout contract missing ${token}")
    endif()
endforeach()

string(FIND "${studio_chrome_text}" "sceneMetaX + 224.0f" render_overlap)
if(NOT render_overlap EQUAL -1)
    message(FATAL_ERROR "Phase 6 Gate 3 AUDIO must not overlap the RENDER workspace slot")
endif()

file(READ "${studio_audio_workspace}" studio_workspace_text)
foreach(token IN ITEMS
    "ADD SOUND SOURCE..."
    "PREVIEW PLAY"
    "AUDIO ASSET..."
    "Content/Audio")
    string(FIND "${studio_workspace_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 6 Gate 3 Audio workspace contract missing ${token}")
    endif()
endforeach()

# The Scene Inspector is a top-level Wicked GUI Window rendered after the
# Renegade chrome. Audio lives inside that chrome, so the ordinary Inspector
# must be hidden while Audio owns the right-hand panel or it will paint over all
# Gate 3 controls. The derived render path performs this reconciliation after
# the normal Studio update and restores the Inspector outside Audio/Render/Hub.
file(READ "${studio_application_header}" studio_application_header_text)
foreach(token IN ITEMS
    "SyncAudioInspectorPresentation"
    "studioChrome_.IsAudioWorkspaceActive()"
    "inspectorPanel_.SetVisible(false)"
    "renderWorkspacePanel_.SetVisible(false)"
    "!projectHubVisible_ && !renderWorkspaceActive_"
    "StudioRenderPath::Update(dt);"
    "SyncAudioInspectorPresentation();")
    string(FIND "${studio_application_header_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 6 Gate 3 Audio Inspector ownership contract missing ${token}")
    endif()
endforeach()
