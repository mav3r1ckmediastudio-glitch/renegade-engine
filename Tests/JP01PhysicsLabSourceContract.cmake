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
set(ROOT_CMAKE "${RENEGADE_SOURCE_DIR}/CMakeLists.txt")
set(PHYSICS_LUA_SOURCE "${RENEGADE_SOURCE_DIR}/EngineBridge/src/PhysicsLuaService.cpp")
set(SCENE_SERVICE_HEADER "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/SceneService.h")
set(SCENE_DOCUMENT_SOURCE "${RENEGADE_SOURCE_DIR}/EngineBridge/src/SceneDocumentService.cpp")
set(COLLISION_HEADER "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/CollisionService.h")
set(COLLISION_SOURCE "${RENEGADE_SOURCE_DIR}/EngineBridge/src/CollisionService.cpp")
set(RUNTIME_SOURCE "${RENEGADE_SOURCE_DIR}/Runtime/src/RuntimeApplication.cpp")
set(PHYSICS_LUA_TEST "${RENEGADE_SOURCE_DIR}/Tests/PhysicsLuaTests.cpp")
set(PHYSICS_LUA_CMAKE "${RENEGADE_SOURCE_DIR}/Tests/JP01PhysicsLua.cmake")
set(REUSABLE_PHYSICS_TEST "${RENEGADE_SOURCE_DIR}/Tests/JP01ReusableAssetPhysicsTests.cpp")
set(REUSABLE_PHYSICS_CMAKE "${RENEGADE_SOURCE_DIR}/Tests/JP01ReusableAssetPhysics.cmake")
set(TERRAIN_SOURCE "${RENEGADE_SOURCE_DIR}/EngineBridge/src/TerrainService.cpp")

foreach(path IN ITEMS
    "${PHYSICS_WORKSPACE_HEADER}"
    "${PHYSICS_WORKSPACE_SOURCE}"
    "${PHYSICS_CHROME_HEADER}"
    "${PHYSICS_CHROME_SOURCE}"
    "${STUDIO_HEADER}"
    "${STUDIO_SOURCE}"
    "${CHROME_HEADER}"
    "${STUDIO_CMAKE}"
    "${ROOT_CMAKE}"
    "${PHYSICS_LUA_SOURCE}"
    "${SCENE_SERVICE_HEADER}"
    "${SCENE_DOCUMENT_SOURCE}"
    "${COLLISION_HEADER}"
    "${COLLISION_SOURCE}"
    "${RUNTIME_SOURCE}"
    "${PHYSICS_LUA_TEST}"
    "${PHYSICS_LUA_CMAKE}"
    "${REUSABLE_PHYSICS_TEST}"
    "${REUSABLE_PHYSICS_CMAKE}"
    "${TERRAIN_SOURCE}")
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
file(READ "${ROOT_CMAKE}" root_cmake)
file(READ "${PHYSICS_LUA_SOURCE}" physics_lua_source)
file(READ "${SCENE_SERVICE_HEADER}" scene_service_header)
file(READ "${SCENE_DOCUMENT_SOURCE}" scene_document_source)
file(READ "${COLLISION_HEADER}" collision_header)
file(READ "${COLLISION_SOURCE}" collision_source)
file(READ "${RUNTIME_SOURCE}" runtime_source)
file(READ "${PHYSICS_LUA_TEST}" physics_lua_test)
file(READ "${PHYSICS_LUA_CMAKE}" physics_lua_cmake)
file(READ "${REUSABLE_PHYSICS_TEST}" reusable_physics_test)
file(READ "${REUSABLE_PHYSICS_CMAKE}" reusable_physics_cmake)
file(READ "${TERRAIN_SOURCE}" terrain_source)

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
require_text(physics_chrome_header
    "bool IsPhysicsLabActive() const noexcept"
    "Physics Lab active-state query")
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

# Creator-facing product language belongs to Renegade. Upstream implementation
# attribution remains in source comments, audits and licence notices rather than
# occupying the authoring workspace itself.
require_text(physics_workspace_source
    "\"PHYSICS AUTHORING\""
    "Renegade Physics Lab subtitle")
require_text(physics_workspace_source
    "\"SECONDARY COLLIDER\""
    "creator-facing secondary collider label")
require_text(physics_workspace_source
    "Lightweight CPU/GPU collision for particles, hair and other secondary systems."
    "secondary collider creator explanation")
require_text(physics_chrome_source
    "SetStatusText(\"PHYSICS LAB\")"
    "plain Physics Lab chrome status")
forbid_text(physics_workspace_source
    "WICKED EDITOR PARITY"
    "developer provenance in creator-facing Physics Lab")
forbid_text(physics_workspace_source
    "JOLT-POWERED"
    "implementation branding in creator-facing Physics Lab")
forbid_text(physics_workspace_source
    "WICKED COLLIDER // NOT JOLT"
    "implementation distinction in creator-facing heading")
forbid_text(physics_workspace_source
    "All seven Wicked shapes"
    "upstream parity wording in creator-facing rigid-body hint")
forbid_text(physics_workspace_source
    "matching Wicked"
    "upstream parity wording in creator-facing world hint")
forbid_text(physics_chrome_source
    "PHYSICS LAB // WICKED EDITOR PARITY"
    "developer provenance in creator-facing chrome status")

# Owner-validation interaction regression: layout may change widget visibility,
# but it must never be driven every frame from Update(). Wicked resets hidden
# widget state to IDLE, which otherwise makes the Lab look live while all
# clicks and slider drags are cancelled before completion.
require_text(physics_chrome_source
    "Bounds only change when Studio layout changes."
    "layout-only Physics Lab bounds refresh")
require_text(physics_chrome_source
    "Do not call SetBounds here."
    "per-frame relayout prohibition")
require_text(physics_chrome_source
    "physicsLab_.Update(canvas, dt);"
    "stateful Physics Lab widget update")

# Owner-validation overlay regression: Physics Lab keeps authoritative Scene
# selection, but Scene-only post passes must not render above its opaque GUI.
# The derived render path bypasses only the selection-mask/gizmo post passes;
# normal RenderPath3D scene rendering and the shared GUI continue unchanged.
require_text(studio_header
    "class PhysicsLabStudioRenderPath final : public StudioRenderPath"
    "Physics-aware Studio render path")
require_text(studio_header
    "return studioChrome_.IsPhysicsLabActive();"
    "Physics Lab render-path state forwarding")
require_text(studio_header
    "wi::RenderPath3D::Render();"
    "Physics Lab selection-mask suppression")
require_text(studio_header
    "wi::RenderPath3D::Compose(cmd);"
    "Physics Lab gizmo/outline composition suppression")
require_text(studio_header
    "PhysicsLabStudioRenderPath renderer_;"
    "Physics-aware renderer ownership")

# Owner validation exposed a severe imported-asset failure: attaching a dynamic
# body to an internal GLTF/FBX node lets Wicked's parented rigid-body feedback
# repeatedly decompose the imported transform chain, producing runaway scale,
# clipping and flicker. Reusable assets must resolve rigid-body ownership to the
# stable creator wrapper and old unambiguous scenes must be repaired before the
# first physics update.
require_text(collision_header
    "bool startDeactivated = true;"
    "Wicked-editor start-deactivated rigid-body default")
require_text(collision_header
    "ResolveCollisionAuthoringTarget("
    "creator-safe rigid-body target resolver")
require_text(collision_header
    "RepairReusableAssetCollisionTargets"
    "serialized nested-body recovery API")
require_text(collision_source
    "ReusableAssetInstanceIdMetadataKey"
    "stable reusable wrapper identification")
require_text(collision_source
    "entity_(ResolveCollisionAuthoringTarget(scene, targetEntity))"
    "CreateCollisionCommand stable-wrapper targeting")
require_text(collision_source
    "scene.rigidbodies.Remove(nestedBody);"
    "nested reusable rigid-body removal during migration")
require_text(collision_source
    "auto& rootBody = scene.rigidbodies.Create(wrapper);"
    "stable-wrapper rigid-body migration")
require_text(collision_source
    "rootBody.physicsobject.reset();"
    "no live Jolt handle migration between entities")
require_text(scene_document_source
    "(void)RepairReusableAssetCollisionTargets(*prepared.scene_);"
    "pre-physics WISCENE reusable-body repair")
require_text(scene_document_source
    "RepairReusableAssetCollisionTargets(scenes_.scene_);"
    "pre-save active-scene reusable-body canonicalization")
require_text(physics_chrome_source
    "bridge::ResolveCollisionAuthoringTarget(scene, selected);"
    "Physics Lab stable-root selection follow")
require_text(physics_chrome_source
    "session->Selection().Select(target);"
    "Physics Lab selection transfer to body owner")
forbid_text(collision_source
    "JPH::PhysicsSystem"
    "new/raw Jolt world ownership in collision recovery")

# A headless regression reproduces the exact wrapper -> payload -> imported-node
# ownership pattern and proves Add/Undo/Redo plus pre-physics migration.
require_text(root_cmake
    "include(Tests/JP01ReusableAssetPhysics.cmake)"
    "reusable-asset physics regression registration")
require_text(reusable_physics_cmake
    "RenegadeJP01ReusableAssetPhysicsTests"
    "reusable-asset physics test target")
require_text(reusable_physics_test
    "ResolveCollisionAuthoringTarget(scene, fixture.nested) != fixture.wrapper"
    "nested reusable target assertion")
require_text(reusable_physics_test
    "RepairReusableAssetCollisionTargets(recoveryScene)"
    "serialized nested-body repair assertion")
require_text(reusable_physics_test
    "!commands.Redo()"
    "wrapper rigid-body Redo assertion")

# Lua lifecycle ownership: SceneService construction must be inert, and the
# Renegade binding layer must never bootstrap Wicked's global VM itself.
require_text(scene_service_header
    "SceneService() = default;"
    "Lua-inert SceneService construction")
forbid_text(scene_service_header
    "BindPhysicsLua(scene_);"
    "Lua binding during SceneService construction")
forbid_text(physics_lua_source
    "wi::lua::Initialize()"
    "Renegade-owned Wicked Lua initialization")
require_text(physics_lua_source
    "return BindPhysicsLua(scene, wi::lua::GetLuaState());"
    "non-owning production Lua-state lookup")
require_text(runtime_source
    "bridge::BindPhysicsLua(scenes_.GetScene())"
    "Runtime post-Wicked Lua binding")
require_text(physics_chrome_source
    "bridge::BindPhysicsLua(session->Scenes().GetScene())"
    "Studio post-Wicked Lua binding")
require_text(physics_lua_test
    "luaL_newstate()"
    "isolated Lua contract state")
forbid_text(physics_lua_test
    "wi::lua::RunText"
    "full Wicked global Lua execution in isolated contract test")
require_text(physics_lua_cmake
    "set_tests_properties(RenegadePhysicsLuaTests PROPERTIES TIMEOUT 30)"
    "fail-fast Lua contract timeout")

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

# Backend coverage: all seven rigid shapes and all eight exposed constraints.
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

# Physics Lab keeps the advanced Renegade runtime escape hatch visible without
# exposing implementation provenance in the creator-facing labels.
require_text(physics_workspace_source
    "ADVANCED RUNTIME // renegade.physics"
    "advanced Lua runtime signpost")

# Existing-project terrain reload must anchor bundled default-grass resources to
# the executable/install location, not a working directory that native Windows
# file dialogs can mutate. Rebinding a clean deserialized material must also
# preserve its clean state so load cannot trigger an unwanted terrain restart.
require_text(terrain_source
    "std::string BundledDefaultGrassRoot()"
    "stable bundled terrain resource root")
require_text(terrain_source
    "wi::helper::GetExecutablePath()"
    "executable-relative bundled terrain root")
require_text(terrain_source
    "const std::string root = BundledDefaultGrassRoot();"
    "default grass material stable-root use")
forbid_text(terrain_source
    "const std::string root = wi::helper::GetCurrentPath()"
    "CWD-owned default grass texture root")
require_text(terrain_source
    "const bool wasDirty = material->IsDirty();"
    "terrain material dirty-state capture on rebind")
require_text(terrain_source
    "material->SetDirty(false);"
    "terrain material clean-state restoration on rebind")

# Explicit Studio build ownership. No accidental header-only inclusion.
foreach(source IN ITEMS
    "src/RenegadePhysicsLabWorkspace.cpp"
    "src/RenegadePhysicsLabWorkspace.h"
    "src/RenegadePhysicsLabStudioChrome.cpp"
    "src/RenegadePhysicsLabStudioChrome.h")
    require_text(studio_cmake "${source}" "RenegadeStudio source ${source}")
endforeach()

message(STATUS "JP01 Physics Lab source contract passed")
