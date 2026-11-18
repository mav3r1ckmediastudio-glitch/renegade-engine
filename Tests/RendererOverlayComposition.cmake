foreach(required IN ITEMS RENEGADE_SOURCE_DIR TEST_OUTPUT_ROOT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
if(NOT DEFINED WIREFRAME_MODULE)
    set(WIREFRAME_MODULE "${RENEGADE_SOURCE_DIR}/cmake/WickedWireframeOverlayFix.cmake")
endif()
string(RANDOM LENGTH 10 ALPHABET 0123456789abcdef run_id)
set(evidence "${TEST_OUTPUT_ROOT}/${run_id}")
# A cold Visual Studio configure performs compiler identification and creates many
# small files. On removable/exFAT project volumes that can exceed the test budget
# before the renderer checks begin. Keep that disposable work on the system
# scratch volume while retaining the registered build-tree evidence location.
# TEST_WORK_ROOT permits explicit validation on another filesystem.
if(NOT DEFINED TEST_WORK_ROOT OR TEST_WORK_ROOT STREQUAL "")
    if(DEFINED ENV{TEMP} AND IS_DIRECTORY "$ENV{TEMP}")
        set(TEST_WORK_ROOT "$ENV{TEMP}/RenegadeRendererOverlayComposition")
    else()
        set(TEST_WORK_ROOT "${TEST_OUTPUT_ROOT}")
    endif()
endif()
set(root "${TEST_WORK_ROOT}/${run_id}")
file(MAKE_DIRECTORY "${evidence}")
file(WRITE "${evidence}/work-root.txt" "${root}\n")
set(source "${root}/source")
set(build "${root}/build")
file(MAKE_DIRECTORY "${source}/WickedEngine/WickedEngine")
configure_file("${RENEGADE_SOURCE_DIR}/Tests/RendererOverlayCompositionFixture.cmake"
    "${source}/CMakeLists.txt" COPYONLY)
file(COPY "${RENEGADE_SOURCE_DIR}/WickedEngine/WickedEngine/wiRenderer.cpp"
    DESTINATION "${source}/WickedEngine/WickedEngine")
file(SHA256 "${source}/WickedEngine/WickedEngine/wiRenderer.cpp" pinned_before)
set(configure "${CMAKE_COMMAND}" -S "${source}" -B "${build}"
    "-DUMA_MODULE=${RENEGADE_SOURCE_DIR}/cmake/WickedUmaUploadFix.cmake"
    "-DWIREFRAME_MODULE=${WIREFRAME_MODULE}")
if(DEFINED TEST_GENERATOR AND NOT TEST_GENERATOR STREQUAL "")
    list(APPEND configure -G "${TEST_GENERATOR}")
endif()
if(DEFINED TEST_PLATFORM AND NOT TEST_PLATFORM STREQUAL "")
    list(APPEND configure -A "${TEST_PLATFORM}")
endif()
execute_process(COMMAND ${configure} RESULT_VARIABLE first
    OUTPUT_VARIABLE first_out ERROR_VARIABLE first_err)
file(WRITE "${evidence}/configure-first.txt" "${first_out}\n${first_err}")
if(NOT first EQUAL 0)
    message(FATAL_ERROR "Cold renderer composition failed:\n${first_out}\n${first_err}")
endif()
set(final "${build}/RenegadeWickedFix/wiRendererWireframe.cpp")
set(uma "${build}/RenegadeWickedFix/wiRenderer.cpp")
file(SHA256 "${final}" first_hash)
file(TIMESTAMP "${final}" first_time "%s.%f" UTC)
file(TIMESTAMP "${uma}" first_uma_time "%s.%f" UTC)
# The owner's exFAT volume has coarse timestamps. A real second configure
# must not rewrite either stage merely because both patches are enabled.
execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 3)
execute_process(COMMAND ${configure} RESULT_VARIABLE second
    OUTPUT_VARIABLE second_out ERROR_VARIABLE second_err)
file(WRITE "${evidence}/configure-second.txt" "${second_out}\n${second_err}")
if(NOT second EQUAL 0)
    message(FATAL_ERROR "Repeat renderer composition failed:\n${second_out}\n${second_err}")
endif()
file(SHA256 "${final}" second_hash)
file(TIMESTAMP "${final}" second_time "%s.%f" UTC)
file(TIMESTAMP "${uma}" second_uma_time "%s.%f" UTC)
file(SHA256 "${source}/WickedEngine/WickedEngine/wiRenderer.cpp" pinned_after)
if(NOT first_hash STREQUAL second_hash OR
   NOT first_time STREQUAL second_time OR
   NOT first_uma_time STREQUAL second_uma_time OR
   NOT pinned_before STREQUAL pinned_after)
    message(FATAL_ERROR "Renderer composition changed stable output or the pinned source")
endif()
file(WRITE "${evidence}/result.txt" "hash=${second_hash}\nscratch=${root}\n")
message(STATUS "RENDERER_COMPOSITION_COLD_AND_RECONFIGURE_PASS hash=${second_hash} evidence=${evidence} scratch=${root}")
