if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

set(service_header "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/AudioService.h")
set(service_source "${RENEGADE_SOURCE_DIR}/EngineBridge/src/AudioService.cpp")
set(studio_audio_chrome "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadePhysicsLabStudioChrome.cpp")
set(studio_audio_workspace "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeAudioWorkspace.cpp")
set(studio_application_header "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.h")
set(runtime_application_source "${RENEGADE_SOURCE_DIR}/Runtime/src/RuntimeApplication.cpp")

foreach(path IN ITEMS
    "${service_header}"
    "${service_source}"
    "${studio_audio_chrome}"
    "${studio_audio_workspace}"
    "${studio_application_header}"
    "${runtime_application_source}")
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
    "SceneAudioPauseState"
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
    "DISABLE_3D"
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

file(READ "${runtime_application_source}" runtime_application_text)
foreach(token IN ITEMS
    "SyncAudioForScene"
    "ActivateSceneAudio"
    "SetSceneAudioPaused"
    "audioSceneRevision_ = 0")
    string(FIND "${runtime_application_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 6 Gate 3 Runtime lifecycle missing ${token}")
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
    "ADD GLOBAL SOUND"
    "ADD 3D SOUND"
    "CreateSpatialSound"
    "PREVIEW PLAY"
    "AUDIO ASSET..."
    "Content/Audio"
    "USE // SFX"
    "USE // MUSIC"
    "USE // AMBIENCE")
    string(FIND "${studio_workspace_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 6 Gate 3 Audio workspace contract missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "HandleAudioSceneIcons"
    "AudioBus::Music"
    "AudioBus::Ambience")
    file(STRINGS "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.cpp" token_matches REGEX "${token}")
    if(NOT token_matches)
        message(FATAL_ERROR "Phase 6 Gate 3 editor sound-source marker missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "ADD SOUND ZONE"
    "zoneEnabled"
    "zoneRadius"
    "durationSeconds"
    "repeatable"
    "UpdateSceneAudioZones")
    string(FIND "${studio_workspace_text}" "${token}" studio_zone_reference)
    file(STRINGS "${service_header}" header_zone_reference REGEX "${token}")
    file(STRINGS "${service_source}" source_zone_reference REGEX "${token}")
    if(NOT studio_zone_reference EQUAL -1 OR
        header_zone_reference OR source_zone_reference)
        message(FATAL_ERROR "Phase 6 Gate 3 must not expose deferred zone behavior: ${token}")
    endif()
endforeach()

file(STRINGS "${service_header}" pause_matches REGEX "wi::audio::Pause")
if(NOT pause_matches)
    message(FATAL_ERROR "Phase 6 Gate 3 pause must preserve the authored playback cursor")
endif()

# Audio is an independent top-level Widget, but Wicked can reorder active
# widgets during Update(). Reconcile ownership afterwards by toggling only the
# Inspector Window's base Widget visibility. Calling Window::SetVisible() here
# would rewrite every Inspector child's visibility and corrupt section state.
file(READ "${studio_application_header}" studio_application_header_text)
foreach(token IN ITEMS
    "SyncAudioInspectorPresentation"
    "studioChrome_.IsAudioWorkspaceActive()"
    "inspectorPanel_.wi::gui::Widget::SetVisible(false)"
    "renderWorkspacePanel_.wi::gui::Widget::SetVisible(false)"
    "inspectorPanel_.wi::gui::Widget::SetVisible(true)"
    "StudioRenderPath::Update(dt);"
    "SyncAudioInspectorPresentation();")
    string(FIND "${studio_application_header_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 6 Gate 3 Audio Inspector ownership missing ${token}")
    endif()
endforeach()

string(FIND "${studio_application_header_text}" "inspectorPanel_.SetVisible(false)" unsafe_window_hide)
if(NOT unsafe_window_hide EQUAL -1)
    message(FATAL_ERROR "Phase 6 Gate 3 must preserve Inspector child visibility")
endif()

file(READ "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.cpp" studio_application_source_text)
foreach(token IN ITEMS
    "AddWidget(&studioChrome_.AudioWorkspace())"
    "RemoveWidget(&inspectorPanel_)"
    "AddWidget(&inspectorPanel_)")
    string(FIND "${studio_application_source_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 6 Gate 3 dedicated Audio ownership missing ${token}")
    endif()
endforeach()
foreach(token IN ITEMS
    "previewInstance"
    "2D AUDITION"
    "StopPreview"
    "CreateSoundInstance")
    string(FIND "${studio_workspace_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 6 Gate 3 transient Preview contract missing ${token}")
    endif()
endforeach()
string(FIND "${studio_workspace_text}" "Play(&sound->soundinstance)" authored_preview)
if(NOT authored_preview EQUAL -1)
    message(FATAL_ERROR "Studio Preview must not play the authored spatial instance")
endif()

file(READ "${service_source}" service_source_text)
foreach(token IN ITEMS
    "ValidateAudioAssetForWicked"
    "Extended WAV headers"
    "wi::Resource nextResource"
    "wi::audio::SoundInstance nextInstance"
    "wi::resourcemanager::Load(safe.filename)"
    "wi::audio::CreateSoundInstance")
    string(FIND "${service_source_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 6 Gate 3 audio lifetime hardening missing ${token}")
    endif()
endforeach()

string(REGEX MATCHALL "wi::audio::CreateSoundInstance" instance_creations
    "${service_source_text}")
list(LENGTH instance_creations instance_creation_count)
if(NOT instance_creation_count EQUAL 1)
    message(FATAL_ERROR
        "Phase 6 Gate 3 must create exactly one authored Wicked audio instance path")
endif()
