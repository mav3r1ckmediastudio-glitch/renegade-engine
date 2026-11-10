# See docs/SHARED_ALLOCATOR_CORE_DECISION_2026-11-08.md.
# This header is included with quotes from upstream headers, so an include-path
# adapter cannot override it. Apply the bounded overlay before compiling targets.
set(_allocator_header "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/wiAllocator.h")
file(READ "${_allocator_header}" _allocator_original)
set(_allocator_old "inline static SharedBlockAllocator<T, block_size>* shared_block_allocator")
set(_allocator_new "inline SharedBlockAllocator<T, block_size>* shared_block_allocator")
string(FIND "${_allocator_original}" "${_allocator_old}" _allocator_old_pos)
string(FIND "${_allocator_original}" "${_allocator_new}" _allocator_new_pos)
if(NOT _allocator_old_pos EQUAL -1 AND _allocator_new_pos EQUAL -1)
    string(REPLACE "${_allocator_old}" "${_allocator_new}"
        _allocator_patched "${_allocator_original}")
    file(WRITE "${_allocator_header}" "${_allocator_patched}")
elseif(_allocator_new_pos EQUAL -1 OR NOT _allocator_old_pos EQUAL -1)
    message(FATAL_ERROR "Pinned Wicked shared allocator declaration changed; review core overlay")
endif()
message(STATUS "Renegade shared allocator has one registry entry per type across translation units")
