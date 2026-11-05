# Compile-time fault injection replaces only the mastering-voice API call in
# the exact generated production source. No runtime switch ships in Studio.
file(READ "${_renegade_audio_generated_src}" _audio_fault_source)
set(_audio_fault_call "audioEngine->CreateMasteringVoice(&masteringVoice)")
string(FIND "${_audio_fault_source}" "${_audio_fault_call}" _audio_fault_pos)
if(_audio_fault_pos EQUAL -1)
    message(FATAL_ERROR "Audio endpoint fault-injection anchor missing")
endif()
string(REPLACE "${_audio_fault_call}"
    "RenegadeTestCreateMasteringVoice(audioEngine.Get(), &masteringVoice)"
    _audio_fault_source "${_audio_fault_source}")
# Keep assertion handling active even in Release so the negative control proves
# that unexpected failures have not been converted into an unavailable device.
set(_audio_fault_source "#ifdef NDEBUG\n#undef NDEBUG\n#endif\n#include \"AudioEndpointFaultInjection.h\"\n${_audio_fault_source}\nnamespace wi::audio { bool RenegadeTestAudioReady() { return audio_internal && audio_internal->IsValid(); } }\n")
set(_audio_fault_generated "${CMAKE_CURRENT_BINARY_DIR}/AudioEndpointProof/wiAudio-test.cpp")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/AudioEndpointProof")
set(_audio_fault_write TRUE)
if(EXISTS "${_audio_fault_generated}")
    file(READ "${_audio_fault_generated}" _audio_fault_previous)
    if(_audio_fault_previous STREQUAL _audio_fault_source)
        set(_audio_fault_write FALSE)
    endif()
endif()
if(_audio_fault_write)
    file(WRITE "${_audio_fault_generated}" "${_audio_fault_source}")
endif()
add_executable(RenegadeAudioEndpointRecoveryProof
    "${CMAKE_CURRENT_LIST_DIR}/AudioEndpointRecoveryProof.cpp" "${_audio_fault_generated}")
target_link_libraries(RenegadeAudioEndpointRecoveryProof PRIVATE Renegade::EngineBridge)
target_include_directories(RenegadeAudioEndpointRecoveryProof PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}" "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine")
target_compile_options(RenegadeAudioEndpointRecoveryProof PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
add_dependencies(RenegadeBridgeTests RenegadeAudioEndpointRecoveryProof)
foreach(_audio_mode IN ITEMS missing unexpected)
    if(_audio_mode STREQUAL "missing")
        set(_audio_test_name RenegadeAudioMissingEndpointRecovery)
    else()
        set(_audio_test_name RenegadeAudioUnexpectedEndpointFailure)
    endif()
    add_test(NAME ${_audio_test_name} COMMAND "${CMAKE_COMMAND}"
        "-DAUDIO_PROOF=$<TARGET_FILE:RenegadeAudioEndpointRecoveryProof>"
        "-DAUDIO_MODE=${_audio_mode}"
        -P "${CMAKE_CURRENT_LIST_DIR}/RunAudioEndpointRecovery.cmake")
    set_tests_properties(${_audio_test_name} PROPERTIES TIMEOUT 45)
endforeach()
