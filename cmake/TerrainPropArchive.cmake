# See docs/TERRAIN_PROP_P1.md. Keep the pinned checkout and compose only
# these serializer hunks with other configured overlays.
find_package(Git REQUIRED)
set(_prop_patch "${CMAKE_SOURCE_DIR}/EngineBridge/patches/TerrainPropArchive.patch")
set(_prop_wicked "${CMAKE_SOURCE_DIR}/WickedEngine")
execute_process(COMMAND "${GIT_EXECUTABLE}" apply --reverse --check "${_prop_patch}"
    WORKING_DIRECTORY "${_prop_wicked}" RESULT_VARIABLE _prop_applied ERROR_QUIET)
if(NOT _prop_applied EQUAL 0)
    execute_process(COMMAND "${GIT_EXECUTABLE}" apply "${_prop_patch}"
        WORKING_DIRECTORY "${_prop_wicked}" COMMAND_ERROR_IS_FATAL ANY)
endif()
