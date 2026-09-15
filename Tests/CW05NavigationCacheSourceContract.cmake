if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

set(CACHE_HEADER
    "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/TestLevelNavigationCacheService.h")
set(CACHE_SOURCE
    "${RENEGADE_SOURCE_DIR}/EngineBridge/src/TestLevelNavigationCacheService.cpp")
set(NAVIGATION_HEADER
    "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/NavigationService.h")
set(PATROL_ROUTE_HEADER
    "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/PatrolRouteService.h")
set(SNAPSHOT_HEADER
    "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/TestLevelSnapshotService.h")
set(SNAPSHOT_WRAPPER
    "${RENEGADE_SOURCE_DIR}/EngineBridge/src/TestLevelSnapshotNavigationWrapper.cpp")
set(BRIDGE_CMAKE
    "${RENEGADE_SOURCE_DIR}/EngineBridge/CMakeLists.txt")

foreach(path IN ITEMS
        "${CACHE_HEADER}"
        "${CACHE_SOURCE}"
        "${NAVIGATION_HEADER}"
        "${PATROL_ROUTE_HEADER}"
        "${SNAPSHOT_HEADER}"
        "${SNAPSHOT_WRAPPER}"
        "${BRIDGE_CMAKE}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR
            "CW-05 navigation cache contract input is missing: ${path}")
    endif()
endforeach()

file(READ "${CACHE_HEADER}" cache_header)
file(READ "${CACHE_SOURCE}" cache_source)
file(READ "${NAVIGATION_HEADER}" navigation_header)
file(READ "${PATROL_ROUTE_HEADER}" patrol_route_header)
file(READ "${SNAPSHOT_HEADER}" snapshot_header)
file(READ "${SNAPSHOT_WRAPPER}" snapshot_wrapper)
file(READ "${BRIDGE_CMAKE}" bridge_cmake)

function(require_text haystack_var needle description)
    string(FIND "${${haystack_var}}" "${needle}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "CW-05 navigation cache contract missing ${description}: ${needle}")
    endif()
endfunction()

require_text(cache_header
    "PrepareTestLevelNavigationCache"
    "automatic navigation preparation seam")
require_text(cache_source
    "Intermediate\" /"
    "project-persistent Intermediate cache root")
require_text(cache_source
    "\"NavigationCache\""
    "navigation cache directory")
require_text(cache_source
    "TryLoadCache"
    "cache reuse path")
require_text(cache_source
    "SaveCache"
    "cache persistence path")
require_text(cache_source
    "PrepareRigidBodyNavigationGeometry"
    "Jolt rigid-body navigation admission")
require_text(cache_source
    "FILTER_NAVIGATION_MESH"
    "navigation-only geometry signature")
require_text(cache_source
    "FILTER_COLLIDER"
    "native collider navigation input")
require_text(cache_source
    "scene.VoxelizeScene"
    "native Wicked voxelization")
require_text(cache_source
    "grid.Serialize"
    "native Wicked VoxelGrid cache serialization")
require_text(navigation_header
    "NavigationDefaultGridMetadataKey"
    "Runtime-default navigation marker")
require_text(patrol_route_header
    "FindDefaultNavigationGrid"
    "Character AI default-grid resolver")
require_text(patrol_route_header
    "renegade.navigation.default_grid"
    "prepared TestGame preferred-grid marker support")
require_text(snapshot_wrapper
    "PrepareWickedSceneOpen(created.scenePath)"
    "detached TestGame scene preparation")
require_text(snapshot_wrapper
    "CaptureAuthoredNavigationIdentities"
    "authored-grid ownership capture before detached preparation")
require_text(snapshot_wrapper
    "VerifyAuthoredNavigationIdentities"
    "authored-grid stable identity verification after detached preparation")
require_text(snapshot_wrapper
    "StableAutomaticNavigationGridId"
    "deterministic identity for scenes without authored navigation")
require_text(snapshot_wrapper
    "if (authored.empty())"
    "automatic-grid-only identity branch")
require_text(snapshot_wrapper
    "AssignPersistentEntityId"
    "automatic-grid stable-ID assignment")
require_text(snapshot_wrapper
    "NavigationDefaultGridMetadataKey"
    "prepared Runtime-default marker")
require_text(snapshot_wrapper
    "FindDefaultNavigationGrid(scene) != runtimeGrid"
    "default-grid ownership verification")
require_text(snapshot_wrapper
    "PrepareTestLevelNavigationCache"
    "TestGame navigation cache integration")
require_text(snapshot_wrapper
    "commands_.UndoCount() != undoBefore"
    "Undo-history non-mutation guard")
require_text(snapshot_wrapper
    "commands_.IsDirty() != dirtyBefore"
    "dirty-state non-mutation guard")
require_text(snapshot_header
    "navigationCacheReused"
    "owner-visible cache reuse evidence")
require_text(snapshot_header
    "navigationCacheRebuilt"
    "owner-visible cache rebuild evidence")
require_text(bridge_cmake
    "TestLevelSnapshotService=LegacyTestLevelSnapshotService"
    "bounded LP04 legacy routing")
require_text(bridge_cmake
    "TestLevelSnapshotNavigationWrapper.cpp"
    "CW-05 TestGame wrapper registration")
