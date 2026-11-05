# Handle a missing default playback endpoint without modifying the Wicked pin.
# CreateMasteringVoice documents ERROR_NOT_FOUND for this device condition:
# https://learn.microsoft.com/en-us/windows/win32/api/xaudio2/nf-xaudio2-ixaudio2-createmasteringvoice
# All other HRESULT failures retain the pinned assertion/error path.
if(NOT WIN32)
    return()
endif()
if(NOT TARGET WickedEngine_common)
    message(FATAL_ERROR "WickedEngine_common required for audio endpoint fix")
endif()
set(_renegade_audio_src "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/wiAudio.cpp")
file(READ "${_renegade_audio_src}" _renegade_audio_text)
set(_renegade_audio_anchor [=[hr = xaudio_check(audioEngine->CreateMasteringVoice(&masteringVoice));]=])
string(FIND "${_renegade_audio_text}" "${_renegade_audio_anchor}" _renegade_audio_pos)
if(_renegade_audio_pos EQUAL -1)
    message(FATAL_ERROR "Pinned Wicked audio changed: mastering-voice anchor missing")
endif()
set(_renegade_audio_replacement [=[hr = audioEngine->CreateMasteringVoice(&masteringVoice);
            // A machine without a playback endpoint has no native audio. This
            // expected device condition must not assert during graphics startup.
            // Every other failed HRESULT retains the pinned error/assert path.
            if (hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND))
            {
                wilog_warning("No default audio output device; native audio unavailable (HRESULT=0x%08X).", static_cast<unsigned>(hr));
                return;
            }
            xaudio_assert(SUCCEEDED(hr), "IXAudio2::CreateMasteringVoice");]=])
string(REPLACE "${_renegade_audio_anchor}" "${_renegade_audio_replacement}"
    _renegade_audio_text "${_renegade_audio_text}")
set(_renegade_audio_generated_src "${CMAKE_CURRENT_BINARY_DIR}/RenegadeWickedFix/wiAudio.cpp")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/RenegadeWickedFix")
set(_renegade_write_audio TRUE)
if(EXISTS "${_renegade_audio_generated_src}")
    file(READ "${_renegade_audio_generated_src}" _renegade_previous_audio)
    if(_renegade_previous_audio STREQUAL _renegade_audio_text)
        set(_renegade_write_audio FALSE)
    endif()
endif()
if(_renegade_write_audio)
    file(WRITE "${_renegade_audio_generated_src}" "${_renegade_audio_text}")
endif()
set_source_files_properties("${_renegade_audio_src}"
    TARGET_DIRECTORY WickedEngine_common PROPERTIES HEADER_FILE_ONLY TRUE)
target_sources(WickedEngine_common PRIVATE "${_renegade_audio_generated_src}")
target_include_directories(WickedEngine_common PRIVATE "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine")
message(STATUS "Renegade missing-audio-endpoint recovery enabled (Wicked pin unchanged)")
