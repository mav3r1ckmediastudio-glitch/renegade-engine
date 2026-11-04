# Build a Renegade-owned physics wrapper without modifying the pinned Wicked submodule.
# Fail visibly if upstream changes the anchors; never silently ship the slow allocator.
if(NOT TARGET WickedEngine_common)
    message(FATAL_ERROR "WickedEngine_common must exist before installing the physics allocator overlay")
endif()
set(_renegade_wicked_physics "${PROJECT_SOURCE_DIR}/WickedEngine/WickedEngine/wiPhysics_Jolt.cpp")
file(READ "${_renegade_wicked_physics}" _renegade_physics_original)
string(REPLACE "\r\n" "\n" _renegade_physics_original "${_renegade_physics_original}")
set(_renegade_old_allocator "static TempAllocatorMalloc temp_allocator; // 10-100 MB was not enough for large simulation, I don't want to reserve more memory up front")
string(FIND "${_renegade_physics_original}" "ReusablePhysicsTempAllocator" _renegade_already_patched)
string(FIND "${_renegade_physics_original}" "${_renegade_old_allocator}" _renegade_original_found)
if(NOT _renegade_already_patched EQUAL -1 OR _renegade_original_found EQUAL -1)
    message(FATAL_ERROR "Expected unmodified pinned Wicked wiPhysics_Jolt.cpp; check its revision and local changes")
endif()
set(_renegade_physics_patched "${_renegade_physics_original}")
string(REPLACE "#include <thread>\n" "#include <thread>\n#include <array>\n#include <mutex>\n"
    _renegade_physics_patched "${_renegade_physics_patched}")
file(READ "${CMAKE_CURRENT_LIST_DIR}/RenegadeReusablePhysicsTempAllocator.inc" _renegade_allocator_class)
string(REPLACE "using namespace wi::scene;\n\n"
    "using namespace wi::scene;\n\n${_renegade_allocator_class}"
    _renegade_physics_patched "${_renegade_physics_patched}")
string(REPLACE "${_renegade_old_allocator}"
    "static ReusablePhysicsTempAllocator temp_allocator; // Reuse large buffers without reserving RAM up front or reducing capacity."
    _renegade_physics_patched "${_renegade_physics_patched}")
if(_renegade_physics_patched STREQUAL _renegade_physics_original)
    message(FATAL_ERROR "Physics allocator overlay did not change the original wrapper")
endif()
string(FIND "${_renegade_physics_patched}" "static ReusablePhysicsTempAllocator temp_allocator;" _renegade_installed)
if(_renegade_installed EQUAL -1)
    message(FATAL_ERROR "Physics allocator overlay replacement failed")
endif()
# Keep rendered terrain and physics triangles identical, including inside cells.
# This edits only the generated copy. See the explicit core compatibility decision.
function(renegade_physics_replace_once anchor replacement)
    string(REPLACE "${anchor}" "" _stripped "${_renegade_physics_patched}")
    string(LENGTH "${_renegade_physics_patched}" _length)
    string(LENGTH "${_stripped}" _stripped_length)
    string(LENGTH "${anchor}" _anchor_length)
    math(EXPR _removed "${_length} - ${_stripped_length}")
    if(NOT _removed EQUAL _anchor_length)
        message(FATAL_ERROR "Pinned terrain collider anchor changed or is ambiguous")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _replacement "${_renegade_physics_patched}")
    set(_renegade_physics_patched "${_replacement}" PARENT_SCOPE)
endfunction()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/RenegadeTerrainCollisionShape.inc")
file(READ "${CMAKE_CURRENT_LIST_DIR}/RenegadeTerrainCollisionShape.inc" _renegade_terrain_collision)
string(STRIP "${_renegade_terrain_collision}" _renegade_terrain_collision)
renegade_physics_replace_once(
    "Vec3 bottom_offset = Vec3::sZero();\n\n\t\tswitch (physicscomponent.shape)"
    "${_renegade_terrain_collision}")
renegade_physics_replace_once(
    "mesh->GetLODSubsetRange(physicscomponent.mesh_lod, first_subset, last_subset);"
    "mesh->GetLODSubsetRange(renegade_exact_heightfield_mesh ? 0 : physicscomponent.mesh_lod, first_subset, last_subset);")

set(_renegade_physics_output "${CMAKE_CURRENT_BINARY_DIR}/renegade_physics_override/wiPhysics_Jolt.cpp")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/renegade_physics_override")
set(_renegade_physics_existing "")
if(EXISTS "${_renegade_physics_output}")
    file(READ "${_renegade_physics_output}" _renegade_physics_existing)
    string(REPLACE "\r\n" "\n" _renegade_physics_existing "${_renegade_physics_existing}")
endif()
if(NOT _renegade_physics_existing STREQUAL _renegade_physics_patched)
    file(WRITE "${_renegade_physics_output}" "${_renegade_physics_patched}")
endif()
get_target_property(_renegade_wicked_sources WickedEngine_common SOURCES)
list(FIND _renegade_wicked_sources "${_renegade_wicked_physics}" _renegade_wicked_index)
if(_renegade_wicked_index EQUAL -1)
    message(FATAL_ERROR "Pinned Wicked physics wrapper is not in WickedEngine_common sources")
endif()
list(REMOVE_ITEM _renegade_wicked_sources "${_renegade_wicked_physics}")
list(APPEND _renegade_wicked_sources "${_renegade_physics_output}")
set_property(TARGET WickedEngine_common PROPERTY SOURCES "${_renegade_wicked_sources}")
set_source_files_properties("${_renegade_physics_output}"
    TARGET_DIRECTORY WickedEngine_common PROPERTIES SKIP_UNITY_BUILD_INCLUSION TRUE)
message(STATUS "Renegade physics: reusable-buffer wrapper generated without editing WickedEngine")
