# Renegade owns a narrowly scoped fix to the pinned Wicked job-system shutdown.
# Do not edit the WickedEngine git submodule: the baseline must remain pristine.
#
# Windows Release diagnosis: the projectile-world test completed its assertions,
# but CRT process exit deadlocked joining a helper thread. The job system's
# static destructor created that helper even when Initialize() was never called.
# The existing wake_loop flag is also shared across threads without an atomic.
# Generate a corrected translation unit and compile it in place of the original.
if(NOT TARGET WickedEngine_common)
    message(FATAL_ERROR "WickedEngine_common is required for the Renegade shutdown fix")
endif()

set(_renegade_wicked_job_src
    "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/wiJobSystem.cpp")
file(READ "${_renegade_wicked_job_src}" _renegade_wicked_job_text)

set(_renegade_alive_line
    "alive.store(false); // indicate that new jobs cannot be started from this point")
set(_renegade_wake_line "bool wake_loop = true;")
string(FIND "${_renegade_wicked_job_text}" "${_renegade_alive_line}" _renegade_alive_pos)
string(FIND "${_renegade_wicked_job_text}" "${_renegade_wake_line}" _renegade_wake_pos)
if(_renegade_alive_pos EQUAL -1 OR _renegade_wake_pos EQUAL -1)
    message(FATAL_ERROR
        "Pinned Wicked shutdown layout changed; review the Renegade patch before building")
endif()

string(REPLACE "${_renegade_alive_line}"
    "${_renegade_alive_line}\n            if (numCores == 0) return; // No workers: no CRT-exit helper thread."
    _renegade_wicked_job_text "${_renegade_wicked_job_text}")
string(REPLACE "${_renegade_wake_line}"
    "std::atomic_bool wake_loop{true};"
    _renegade_wicked_job_text "${_renegade_wicked_job_text}")

set(_renegade_job_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/RenegadeWickedFix")
set(_renegade_job_generated_src "${_renegade_job_generated_dir}/wiJobSystem.cpp")
file(MAKE_DIRECTORY "${_renegade_job_generated_dir}")
set(_renegade_write_patch TRUE)
if(EXISTS "${_renegade_job_generated_src}")
    file(READ "${_renegade_job_generated_src}" _renegade_previous_job_text)
    if(_renegade_previous_job_text STREQUAL _renegade_wicked_job_text)
        set(_renegade_write_patch FALSE)
    endif()
endif()
if(_renegade_write_patch)
    file(WRITE "${_renegade_job_generated_src}" "${_renegade_wicked_job_text}")
endif()

# The original remains tracked at its pinned commit but must not be compiled
# alongside the generated corrected source (duplicate symbols).
set_source_files_properties("${_renegade_wicked_job_src}"
    TARGET_DIRECTORY WickedEngine_common PROPERTIES HEADER_FILE_ONLY TRUE)
target_sources(WickedEngine_common PRIVATE "${_renegade_job_generated_src}")
target_include_directories(WickedEngine_common PRIVATE
    "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine")
message(STATUS "Renegade pinned-Wicked shutdown fix applied without altering the submodule")
