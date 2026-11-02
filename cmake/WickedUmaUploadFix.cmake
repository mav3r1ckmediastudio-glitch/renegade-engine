# DX12 upload heaps are GPU read-only. The pinned renderer requests a UAV
# on its coherent-UMA upload allocation, which CreatePlacedResource rejects.
# Preserve the upstream pin and retain the UAV on DEFAULT allocations.
# See docs/RECOVERY_UMA_UPLOAD_CORE_DECISION_2026-11-02.md.
if(NOT WIN32)
    return()
endif()
set(_renegade_uma_src "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/wiRenderer.cpp")
file(READ "${_renegade_uma_src}" _renegade_uma_text)
set(_renegade_uma_anchor "desc.bind_flags = BindFlag::SHADER_RESOURCE | BindFlag::VERTEX_BUFFER | BindFlag::INDEX_BUFFER | BindFlag::UNORDERED_ACCESS;")
string(REPLACE "${_renegade_uma_anchor}" "" _renegade_uma_stripped "${_renegade_uma_text}")
string(LENGTH "${_renegade_uma_text}" _renegade_uma_length)
string(LENGTH "${_renegade_uma_stripped}" _renegade_uma_stripped_length)
string(LENGTH "${_renegade_uma_anchor}" _renegade_uma_anchor_length)
math(EXPR _renegade_uma_removed "${_renegade_uma_length} - ${_renegade_uma_stripped_length}")
if(NOT _renegade_uma_removed EQUAL _renegade_uma_anchor_length)
    message(FATAL_ERROR "Pinned Wicked GPU allocator changed: expected one bind-flags anchor")
endif()
set(_renegade_uma_replacement [=[desc.bind_flags = BindFlag::SHADER_RESOURCE | BindFlag::VERTEX_BUFFER | BindFlag::INDEX_BUFFER;
    // UPLOAD heaps remain GENERIC_READ and cannot host unordered access.
    // Mesh aliases only read these buffers. Keep DEFAULT heap UAV support.
    if (desc.usage == Usage::DEFAULT)
    {
        desc.bind_flags |= BindFlag::UNORDERED_ACCESS;
    }]=])
string(REPLACE "${_renegade_uma_anchor}" "${_renegade_uma_replacement}"
    _renegade_uma_text "${_renegade_uma_text}")
set(_renegade_uma_generated "${CMAKE_CURRENT_BINARY_DIR}/RenegadeWickedFix/wiRenderer.cpp")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/RenegadeWickedFix")
set(_renegade_write_uma TRUE)
if(EXISTS "${_renegade_uma_generated}")
    file(READ "${_renegade_uma_generated}" _renegade_previous_uma)
    if(_renegade_previous_uma STREQUAL _renegade_uma_text)
        set(_renegade_write_uma FALSE)
    endif()
endif()
if(_renegade_write_uma)
    file(WRITE "${_renegade_uma_generated}" "${_renegade_uma_text}")
endif()
# wiRenderer is compiled twice: embedded shaders for the applications and
# external shaders for the offline compiler. Keep their existing include/PCH
# configuration, including the embedded target's private wiShaderDump.h path.
foreach(_renegade_uma_target WickedEngine_ext_shaders WickedEngine_emb_shaders)
    if(NOT TARGET ${_renegade_uma_target})
        message(FATAL_ERROR "${_renegade_uma_target} required for GPU allocator repair")
    endif()
    set_source_files_properties("${_renegade_uma_src}"
        TARGET_DIRECTORY ${_renegade_uma_target} PROPERTIES HEADER_FILE_ONLY TRUE)
    target_sources(${_renegade_uma_target} PRIVATE "${_renegade_uma_generated}")
    target_include_directories(${_renegade_uma_target} PRIVATE
        "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine")
endforeach()
message(STATUS "Renegade coherent-UMA upload-buffer recovery enabled (Wicked pin unchanged)")
