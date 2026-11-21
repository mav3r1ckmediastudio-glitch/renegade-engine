# Feed collision interests into the existing terrain generation queue.
# Compose a build-tree source; preserve every existing Wicked overlay byte.
set(_p3_terrain_input "${PROJECT_SOURCE_DIR}/WickedEngine/WickedEngine/wiTerrain.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_p3_terrain_input}")
file(READ "${_p3_terrain_input}" _p3_terrain)
string(REPLACE "\r\n" "\n" _p3_terrain "${_p3_terrain}")
function(p3_terrain_replace_once old replacement)
    string(FIND "${_p3_terrain}" "${old}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Pinned terrain collision-interest anchor changed: ${old}")
    endif()
    string(REPLACE "${old}" "${replacement}" patched "${_p3_terrain}")
    set(_p3_terrain "${patched}" PARENT_SCOPE)
endfunction()
p3_terrain_replace_once(
    "\t\t// Ensure that enough grass chunks are generated"
    "\t\tconst auto p3_collision_requests = wi::physics::terrain_props::GetTerrainRequests(*scene, terrainEntity);\n\n\t\t// Ensure that enough grass chunks are generated")
p3_terrain_replace_once(
    "\t\t\tchunk_data.visible = camera.frustum.CheckSphere(chunk_data.sphere.center, chunk_data.sphere.radius);"
    "\t\t\tchunk_data.visible = camera.frustum.CheckSphere(chunk_data.sphere.center, chunk_data.sphere.radius);\n\t\t\tconst bool p3_collision_wanted = p3_collision_requests && std::binary_search(p3_collision_requests->begin(), p3_collision_requests->end(), wi::physics::terrain_props::TerrainChunkKey{chunk.x, chunk.z});")
p3_terrain_replace_once(
    "if (dist > removal_threshold)"
    "if (dist > removal_threshold && !p3_collision_wanted)")
p3_terrain_replace_once(
    "if (dist < physics_generation)"
    "if (dist < physics_generation || p3_collision_wanted)")
p3_terrain_replace_once(
    "\t\t\tit++;\n\t\t}\n\n\t\tif (props_regenerated)"
    "\t\t\tif(p3_collision_wanted) wi::physics::terrain_props::PublishTerrainChunk(*scene, terrainEntity, {chunk.x,chunk.z}, chunk_data.entity, !chunk_data.invalidated && chunk_mesh != nullptr);\n\t\t\tit++;\n\t\t}\n\n\t\tif (props_regenerated)")
p3_terrain_replace_once(
    [=[generator->build_batch.push_back(chunk);]=]
    [=[// Collision, invalidation and camera requests can address the same chunk
					// before setup_chunk() reserves it. Each worker must own one chunk.
					const auto& pending = generator->build_batch;
					if (std::find(pending.begin(), pending.end(), chunk) != pending.end())
					{
					    return true;
					}
					generator->build_batch.push_back(chunk);]=])
p3_terrain_replace_once(
    "if (!request_chunk(chunk.x, chunk.z))"
    "if (!request_chunk(chunk.x - center_chunk.x, chunk.z - center_chunk.z))")
p3_terrain_replace_once(
    "\t\t\tauto request_all_chunks = [&]()\n\t\t\t{"
    "\t\t\tauto request_all_chunks = [&]()\n\t\t\t{\n\t\t\t\t// Collision interests share this queue and its existing time budget.\n\t\t\t\tif(const auto requests = wi::physics::terrain_props::GetTerrainRequests(*scene, terrainEntity))\n\t\t\t\t\tfor(const auto& key:*requests)\n\t\t\t\t\t\tif(!request_chunk(key.x-center_chunk.x,key.z-center_chunk.z))return;")
string(PREPEND _p3_terrain
    "#include \"${PROJECT_SOURCE_DIR}/EngineBridge/include/renegade/bridge/TerrainPropTerrainPhysics.h\"\n#include <algorithm>\n")
set(_p3_terrain_output "${CMAKE_CURRENT_BINARY_DIR}/renegade_terrain_queries/wiTerrain.cpp")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/renegade_terrain_queries")
set(_p3_existing "")
if(EXISTS "${_p3_terrain_output}")
    file(READ "${_p3_terrain_output}" _p3_existing)
endif()
if(NOT _p3_existing STREQUAL _p3_terrain)
    file(WRITE "${_p3_terrain_output}" "${_p3_terrain}")
endif()
get_target_property(_p3_sources WickedEngine_common SOURCES)
list(FIND _p3_sources "${_p3_terrain_input}" _p3_source_index)
if(_p3_source_index EQUAL -1)
    message(FATAL_ERROR "Pinned terrain source is absent from WickedEngine_common")
endif()
list(REMOVE_ITEM _p3_sources "${_p3_terrain_input}")
list(APPEND _p3_sources "${_p3_terrain_output}")
set_property(TARGET WickedEngine_common PROPERTY SOURCES "${_p3_sources}")
set_source_files_properties("${_p3_terrain_output}" TARGET_DIRECTORY WickedEngine_common
    PROPERTIES SKIP_UNITY_BUILD_INCLUSION TRUE)
