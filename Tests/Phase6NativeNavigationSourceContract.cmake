function(require_text source_text needle description)
    string(FIND "${source_text}" "${needle}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 6 native navigation source contract missing ${description}: ${needle}")
    endif()
endfunction()

file(READ
    "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/NavigationService.h"
    navigation_header)
file(READ
    "${RENEGADE_SOURCE_DIR}/EngineBridge/src/NavigationService.cpp"
    navigation_source)
file(READ
    "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeNavigationWorkspace.cpp"
    navigation_workspace)
file(READ
    "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadePhysicsLabStudioChrome.cpp"
    studio_chrome)
file(READ
    "${RENEGADE_SOURCE_DIR}/Runtime/src/RuntimeLiveDiagnostics.cpp"
    runtime_source)
file(READ
    "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/TerrainService.h"
    terrain_header)
file(READ
    "${RENEGADE_SOURCE_DIR}/EngineBridge/src/TerrainService.cpp"
    terrain_source)
file(READ
    "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.cpp"
    studio_application)

require_text("${navigation_header}"
    "NavigationAgentMetadataKey"
    "persistent navigation-agent identity")
require_text("${navigation_header}"
    "CreateNavigationAgentPairCommand"
    "command-backed creator agent/target creation")
require_text("${navigation_header}"
    "NavigationRuntimeState"
    "runtime navigation state")
require_text("${navigation_source}"
    "PrepareRigidBodyNavigationGeometry(scene)"
    "Jolt rigid-body navigation participation before voxelization")
require_text("${navigation_source}"
    "object.filterMask |= wi::enums::FILTER_NAVIGATION_MESH"
    "rigid-body render geometry admitted to Wicked navigation")
require_text("${navigation_source}"
    "mesh->BuildBVH()"
    "Wicked character collision BVH preparation")
require_text("${navigation_source}"
    "character->SetPathGoal(goal, grid)"
    "native Wicked deferred path-goal handoff")
require_text("${navigation_source}"
    "character->pathquery.get_next_waypoint()"
    "native Wicked next-waypoint consumption")
require_text("${navigation_source}"
    "character->Turn(direction)"
    "native Wicked character turning")
require_text("${navigation_source}"
    "character->Move(XMFLOAT3("
    "native Wicked character movement")
require_text("${navigation_workspace}"
    "CREATE TEST AGENT + TARGET"
    "creator navigation proof controls")
require_text("${navigation_workspace}"
    "REFRESH PATH"
    "creator path refresh control")
require_text("${navigation_workspace}"
    "bridge::RebuildNavigationGridCommand"
    "manual path refresh rebakes current obstacle geometry")
require_text("${navigation_workspace}"
    "wi::renderer::DrawLine(line)"
    "visible queried-path debug drawing")
require_text("${studio_chrome}"
    "IsRenegadeNavigationEntity(scene, selected)"
    "grid/agent/destination specialist Inspector routing")
require_text("${runtime_source}"
    "InitializeRuntimeNavigation"
    "Runtime navigation initialization")
require_text("${runtime_source}"
    "UpdateRuntimeNavigation"
    "Runtime navigation frame integration")
require_text("${runtime_source}"
    "navigation_agents"
    "Runtime diagnostic evidence")

require_text("${terrain_header}"
    "physicsChunkRadius = DefaultTerrainChunkRadius + 1"
    "full fixed-terrain physics default")
require_text("${terrain_source}"
    "std::max(requestedPhysicsRadius, terrain.generation + 1)"
    "full fixed-terrain Jolt heightfield coverage")
require_text("${terrain_source}"
    "rootTransform->translation_local.y = -terrain.bottomLevel"
    "fresh terrain world-zero baseline mapping")
require_text("${studio_application}"
    "constexpr float gridPlaneHeight = 0.02f;"
    "shader grid restored to world-zero reference")

message(STATUS "Phase 6 native navigation + terrain foundation source contract passed")
