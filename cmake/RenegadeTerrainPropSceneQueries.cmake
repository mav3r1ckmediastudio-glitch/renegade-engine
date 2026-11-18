# Compose the native CharacterComponent query seam after the existing overlays.
# The pinned Wicked source and header stay untouched.
set(_p3_scene_input "${PROJECT_SOURCE_DIR}/WickedEngine/WickedEngine/wiScene.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_p3_scene_input}")
file(READ "${_p3_scene_input}" _p3_scene)
string(REPLACE "\r\n" "\n" _p3_scene "${_p3_scene}")
set(_p3_capsule_anchor "Scene::CapsuleIntersectionResult Scene::Intersects(const Capsule& capsule, uint32_t filterMask, uint32_t layerMask, uint32_t lod) const\n\t{\n\t\tCapsuleIntersectionResult result;")
set(_p3_ray_anchor "Scene::RayIntersectionResult Scene::Intersects(const Ray& ray, uint32_t filterMask, uint32_t layerMask, uint32_t lod) const\n\t{\n\t\tRayIntersectionResult result;")
foreach(_p3_kind capsule ray)
    string(FIND "${_p3_scene}" "${_p3_${_p3_kind}_anchor}" _p3_found)
    if(_p3_found EQUAL -1)
        message(FATAL_ERROR "Pinned native terrain-prop ${_p3_kind} query anchor changed")
    endif()
endforeach()
string(REPLACE "${_p3_capsule_anchor}"
    "Scene::CapsuleIntersectionResult Scene::Intersects(const Capsule& capsule, uint32_t filterMask, uint32_t layerMask, uint32_t lod) const\n\t{\n\t\tCapsuleIntersectionResult result = (filterMask & (FILTER_NAVIGATION_MESH | FILTER_COLLIDER)) ? wi::physics::terrain_props::IntersectCapsule(*this, capsule, layerMask) : CapsuleIntersectionResult{};"
    _p3_scene "${_p3_scene}")
string(REPLACE "${_p3_ray_anchor}"
    "Scene::RayIntersectionResult Scene::Intersects(const Ray& ray, uint32_t filterMask, uint32_t layerMask, uint32_t lod) const\n\t{\n\t\tRayIntersectionResult result = (filterMask & (FILTER_NAVIGATION_MESH | FILTER_COLLIDER)) ? wi::physics::terrain_props::IntersectRay(*this, ray, layerMask) : RayIntersectionResult{};"
    _p3_scene "${_p3_scene}")
set(_p3_terrain_gate_anchor "\t\t// count colliders in background thread before procedural anim system")
string(FIND "${_p3_scene}" "${_p3_terrain_gate_anchor}" _p3_gate_found)
if(_p3_gate_found EQUAL -1)
    message(FATAL_ERROR "Pinned native scene terrain-readiness gate changed")
endif()
string(REPLACE "${_p3_terrain_gate_anchor}"
    "\t\tif (dt > 0 && !wi::physics::terrain_props::AreTerrainRequestsReady(*this)) { time -= dt; dt = 0; this->dt = 0; }\n\n${_p3_terrain_gate_anchor}"
    _p3_scene "${_p3_scene}")
set(_p3_terrain_update_anchor "\t\t// Terrains updates kick off:\n\t\tif (dt > 0)")
string(FIND "${_p3_scene}" "${_p3_terrain_update_anchor}" _p3_update_found)
if(_p3_update_found EQUAL -1)
    message(FATAL_ERROR "Pinned native terrain generation time gate changed")
endif()
string(REPLACE "${_p3_terrain_update_anchor}"
    "\t\t// Terrain collision prefetch uses this same pump while simulation is held.\n\t\tif (dt > 0 || wi::physics::terrain_props::HasTerrainRequests(*this))"
    _p3_scene "${_p3_scene}")
string(PREPEND _p3_scene
    "#include \"${PROJECT_SOURCE_DIR}/EngineBridge/include/renegade/bridge/TerrainPropTerrainPhysics.h\"\n")
string(PREPEND _p3_scene
    "#include \"${PROJECT_SOURCE_DIR}/EngineBridge/include/renegade/bridge/TerrainPropPhysicsAdapter.h\"\n")
set(_p3_scene_output "${CMAKE_CURRENT_BINARY_DIR}/renegade_scene_queries/wiScene.cpp")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/renegade_scene_queries")
set(_p3_existing "")
if(EXISTS "${_p3_scene_output}")
    file(READ "${_p3_scene_output}" _p3_existing)
endif()
if(NOT _p3_existing STREQUAL _p3_scene)
    file(WRITE "${_p3_scene_output}" "${_p3_scene}")
endif()
get_target_property(_p3_sources WickedEngine_common SOURCES)
list(FIND _p3_sources "${_p3_scene_input}" _p3_source_index)
if(_p3_source_index EQUAL -1)
    message(FATAL_ERROR "Pinned native scene is absent from WickedEngine_common")
endif()
list(REMOVE_ITEM _p3_sources "${_p3_scene_input}")
list(APPEND _p3_sources "${_p3_scene_output}")
set_property(TARGET WickedEngine_common PROPERTY SOURCES "${_p3_sources}")
set_source_files_properties("${_p3_scene_output}" TARGET_DIRECTORY WickedEngine_common
    PROPERTIES SKIP_UNITY_BUILD_INCLUSION TRUE)
