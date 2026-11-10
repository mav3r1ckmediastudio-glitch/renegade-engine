# Bounded particle overlay; upstream gitlink unchanged.
# See docs/PARTICLE_SHAPES_TIMING_CORE_DECISION.md.
find_package(Git REQUIRED)
set(_particle_wicked "${CMAKE_SOURCE_DIR}/WickedEngine")
set(_particle_patch "${CMAKE_SOURCE_DIR}/EngineBridge/patches/particle-authoring.patch")
execute_process(COMMAND "${GIT_EXECUTABLE}" -c "safe.directory=${_particle_wicked}" apply --reverse --check "${_particle_patch}"
 WORKING_DIRECTORY "${_particle_wicked}" RESULT_VARIABLE _particle_already OUTPUT_QUIET ERROR_QUIET)
if(NOT _particle_already EQUAL 0)
 execute_process(COMMAND "${GIT_EXECUTABLE}" -c "safe.directory=${_particle_wicked}" apply --check "${_particle_patch}"
  WORKING_DIRECTORY "${_particle_wicked}" RESULT_VARIABLE _particle_check ERROR_VARIABLE _particle_error)
 if(NOT _particle_check EQUAL 0)
  message(FATAL_ERROR "Pinned particle overlay needs review: ${_particle_error}")
 endif()
 execute_process(COMMAND "${GIT_EXECUTABLE}" -c "safe.directory=${_particle_wicked}" apply "${_particle_patch}"
  WORKING_DIRECTORY "${_particle_wicked}" RESULT_VARIABLE _particle_apply)
 if(NOT _particle_apply EQUAL 0)
  message(FATAL_ERROR "Particle overlay application failed")
 endif()
endif()
message(STATUS "Renegade particle overlay ready; Wicked pin unchanged")
