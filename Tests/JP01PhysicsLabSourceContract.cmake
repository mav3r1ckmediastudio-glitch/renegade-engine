if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

set(PHYSICS_WORKSPACE_HEADER "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadePhysicsLabWorkspace.h")
set(PHYSICS_WORKSPACE_SOURCE "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadePhysicsLabWorkspace.cpp")
set(PHYSICS_CHROME_HEADER "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadePhysicsLabStudioChrome.h")
set(PHYSICS_CHROME_SOURCE "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadePhysicsLabStudioChrome.cpp")
set(STUDIO_HEADER "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.h")
set(STUDIO_SOURCE "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.cpp")
set(CHROME_HEADER "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStudioChrome.h")
set(STUDIO_CMAKE "${RENEGADE_SOURCE_DIR}/Studio/CMakeLists.txt")

foreach(path IN ITEMS
    "${PHYSICS_WORKSPACE_HEADER}"
    "${PHYSICS_WORKSPACE_SOURCE}"
    "${PHYSICS_CHROME_HEADER}"
    "${PHYSICS_CHROME_SOURCE}"
    "${STUDIO_HEADER}"
    "${STUDIO_SOURCE}"
    "${CHROME_HEADER}"
    "${STUDIO_CMAKE}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "JP01 Physics Lab source contract input is missing: ${path}")
    endif()
endforeach()

file(READ "${PHYSICS_WORKSPACE_HEADER}" physics_workspace_header)
file(READ "${PHYSICS_WORKSPACE_SOURCE}" physics_workspace_source)
file(READ "${PHYSICS_CHROME_HEADER}" physics_chrome_header)
file(READ "${PHYSICS_CHROME_SOURCE}" physics_chrome_source)
file(READ "${STUDIO_HEADER}" studio_header)
file(READ "${STUDIO_SOURCE}" studio_source)
file(READ "${CHROME_HEADER}" chrome_header)
file(READ "${STUDIO_CMAKE}" studio_cmake)

function(require_text haystack_var needle description)
    string(FIND "${${haystack_var}}" "${needle}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "JP01 Physics Lab contract missing ${description}: ${needle}")
    endif()
endfunction()

function(forbid_text haystack_var needle description)
    string(FIND "${${haystack_var}}" "${needle}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "JP01 Physics Lab contract forbids ${description}: ${needle}")
    endif()
endfunction()

# Dedicated workspace/chrome ownership. Physics remains out of StudioApplication.cpp.
require_text(studio_header
    "#include \"RenegadePhysicsLabStudioChrome.h\""
    "physics-aware chrome include")
require_text(studio_header
    "RenegadePhysicsLabStudioChrome studioChrome_;"
    "physics-aware Studio chrome host")
forbid_text(studio_source
    "RenegadePhysicsLabWorkspace"
    "Physics Lab implementation in StudioApplication.cpp")
forbid_text(studio_source
    "PhysicsService.h"
    "physics bridge implementation include in StudioApplication.cpp")
forbid_text(studio_source
    "CollisionService.h"
    "collision bridge implementation include in StudioApplication.cpp")

# Creator chrome remains the accepted base and the Physics shell derives from it.
require_text(chrome_header
    "class CreatorAssetStudioChrome : public RenegadeStudioChrome"
    "extensible creator chrome base")
forbid_text(chrome_header
    "class CreatorAssetStudioChrome final : public RenegadeStudioChrome"
    "sealed creator chrome")
require_text(physics_chrome_header
    "class RenegadePhysicsLabStudioChrome final : public CreatorAssetStudioChrome"
    "dedicated Physics Lab chrome")
require_text(physics_chrome_header
    "RenegadePhysicsLabWorkspace physicsLab_;"
    "Physics Lab workspace ownership")
require_text(physics_chrome_source
    "SetPhysicsLabActive(true);"
    "Physics workspace activation")
require_text(physics_chrome_source
    "studioAction_(Action::SceneWorkspace);"
    "Scene-mode reconciliation under Physics Lab")
require_text(physics_chrome_source
    "physicsLab_.ConsumedPointerThisFrame()"
    "Physics Lab input consumption")
require_text(physics_chrome_source
    "\"PHYSICS\""
    "Physics workspace tab")

# Eight curated creator-facing pages, matching the JP01 bounded scope.
foreach(page IN ITEMS
    "World"
    "RigidBody"
    "Constraint"
    "Character"
    "Vehicle"
    "Ragdoll"
    "SoftBody"
    "WickedCollider")
    require_text(physics_workspace_header "${page}," "Physics Lab page ${page}")
endforeach()

# Wicked editor parity: all seven rigid shapes and all eight exposed constraints.
foreach(shape IN ITEMS
    "Shape::BOX"
    "Shape::SPHERE"
    "Shape::CAPSULE"
    "Shape::CYLINDER"
    "Shape::CONVEX_HULL"
    "Shape::TRIANGLE_MESH"
    "Shape::HEIGHTFIELD")
    require_text(physics_workspace_source "${shape}" "rigid-body shape ${shape}")
endforeach()

foreach(constraint IN ITEMS
    "Type::Fixed"
    "Type::Point"
    "Type::Distance"
    "Type::Hinge"
    "Type::Cone"
    "Type::SixDOF"
    "Type::SwingTwist"
    "Type::Slider")
    require_text(physics_workspace_source "${constraint}" "constraint type ${constraint}")
endforeach()

# Public Wicked GUI APIs only; do not reach through ComboBox protected storage.
require_text(physics_workspace_source
    "combo.GetItemCount()"
    "public ComboBox item-count API")
require_text(physics_workspace_source
    "combo.GetItemUserData("
    "public ComboBox userdata API")
forbid_text(physics_workspace_source
    "combo.items"
    "protected ComboBox item storage access")

# The creator UI must keep Wicked Scene/Jolt authoritative and avoid a second world.
forbid_text(physics_workspace_source
    "JPH::PhysicsSystem"
    "second or raw Jolt PhysicsSystem ownership in Studio")
forbid_text(physics_chrome_source
    "JPH::PhysicsSystem"
    "raw Jolt PhysicsSystem ownership in chrome")
forbid_text(physics_workspace_source
    "wi::Archive"
    "duplicate Physics Lab serialization")
require_text(physics_workspace_source
    "bridge::StudioSession::Current()"
    "shared Studio session")
require_text(physics_workspace_source
    "session->Commands().Execute("
    "shared Studio Undo/Redo command stack")
require_text(physics_workspace_source
    "bridge::SetPhysicsGravity"
    "scene-authoritative gravity bridge")

# Important semantic split: Wicked Collider is secondary-system collision, not Jolt.
require_text(physics_workspace_source
    "WICKED COLLIDER // NOT JOLT"
    "honest Wicked Collider labelling")
require_text(physics_workspace_source
    "particles, hair and springs"
    "Wicked Collider secondary-system explanation")

# Physics Lab must expose the advanced runtime escape hatch without implementing
# scripting in the Studio shell itself.
require_text(physics_workspace_source
    "ADVANCED RUNTIME // renegade.physics"
    "advanced Lua runtime signpost")

# Explicit Studio build ownership. No accidental header-only inclusion.
foreach(source IN ITEMS
    "src/RenegadePhysicsLabWorkspace.cpp"
    "src/RenegadePhysicsLabWorkspace.h"
    "src/RenegadePhysicsLabStudioChrome.cpp"
    "src/RenegadePhysicsLabStudioChrome.h")
    require_text(studio_cmake "${source}" "RenegadeStudio source ${source}")
endforeach()

message(STATUS "JP01 Physics Lab source contract passed")
