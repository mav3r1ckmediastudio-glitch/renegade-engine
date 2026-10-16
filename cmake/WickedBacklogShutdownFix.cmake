# Renegade-owned fix for a shutdown deadlock in the pinned Wicked backlog writer.
# The writer thread's static localQueue registers a CRT exit handler from a
# background thread. At process exit the main thread joins that worker, while
# it may wait on CRT exit-handler registration: a reproducible deadlock.
# Use stack-scoped queues instead. Keep the Wicked submodule pinned and clean.
if(NOT TARGET WickedEngine_common)
    message(FATAL_ERROR "WickedEngine_common target required for backlog fix")
endif()

set(_renegade_backlog_src "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/wiBacklog.cpp")
file(READ "${_renegade_backlog_src}" _renegade_backlog_text)
set(_renegade_writer_anchor "while (running.load())")
set(_renegade_static_queue "static std::deque<std::string> localQueue;")
set(_renegade_tls_queue "static thread_local std::deque<std::string> localQueue;")
foreach(_anchor IN ITEMS
    _renegade_writer_anchor _renegade_static_queue _renegade_tls_queue)
    string(FIND "${_renegade_backlog_text}" "${${_anchor}}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "Pinned Wicked backlog changed: missing ${_anchor}")
    endif()
endforeach()

# The worker queue is created before the loop, once per worker lifetime.
# The synchronous flush queue needs no thread-local storage.
string(REPLACE "${_renegade_writer_anchor}"
    "std::deque<std::string> localQueue;\n\t\t\twhile (running.load())"
    _renegade_backlog_text "${_renegade_backlog_text}")
string(REPLACE "${_renegade_static_queue}"
    "// Writer queue is stack-scoped; no CRT atexit registration."
    _renegade_backlog_text "${_renegade_backlog_text}")
string(REPLACE "${_renegade_tls_queue}"
    "std::deque<std::string> localQueue;"
    _renegade_backlog_text "${_renegade_backlog_text}")

set(_renegade_backlog_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/RenegadeWickedFix")
set(_renegade_backlog_generated_src "${_renegade_backlog_generated_dir}/wiBacklog.cpp")
file(MAKE_DIRECTORY "${_renegade_backlog_generated_dir}")
set(_write_backlog TRUE)
if(EXISTS "${_renegade_backlog_generated_src}")
    file(READ "${_renegade_backlog_generated_src}" _prev_backlog)
    if(_prev_backlog STREQUAL _renegade_backlog_text)
        set(_write_backlog FALSE)
    endif()
endif()
if(_write_backlog)
    file(WRITE "${_renegade_backlog_generated_src}" "${_renegade_backlog_text}")
endif()

set_source_files_properties("${_renegade_backlog_src}"
    TARGET_DIRECTORY WickedEngine_common PROPERTIES HEADER_FILE_ONLY TRUE)
target_sources(WickedEngine_common PRIVATE "${_renegade_backlog_generated_src}")
target_include_directories(WickedEngine_common PRIVATE
    "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine")
message(STATUS "Renegade Wicked backlog shutdown fix enabled (submodule unchanged)")
