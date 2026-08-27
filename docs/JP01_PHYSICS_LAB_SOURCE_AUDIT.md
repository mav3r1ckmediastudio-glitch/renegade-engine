# JP01 Physics Lab — Pre-CI Source Audit

Status: **READY FOR WINDOWS CI / OWNER VALIDATION**

Audit boundary:
- Backend checkpoint: `044671852ba53aae66de05786def7d01c95b3aa7`
- Physics Lab staging branch: `staging/jp01-physics-lab-ui`
- Physics Lab staging head at audit start: `b90f4dd35647f9da025737d8da849a0b24b6a1cd`
- The staging history is a strict continuation of the backend checkpoint: no divergence, no merge commit and no replacement physics branch.

## Architectural checks

- Wicked Scene components remain the serialized physics authority.
- Wicked's existing Jolt world remains the only live physics world.
- Physics Lab does not own or instantiate `JPH::PhysicsSystem`.
- Physics Lab does not add a parallel physics serializer.
- Physics creator state is edited through the existing JP01 EngineBridge services.
- Undoable physics authoring uses the existing `StudioSession` command history.
- Physics implementation is not added to `StudioApplication.cpp`.
- `StudioRenderPath` only changes its chrome host type to `RenegadePhysicsLabStudioChrome`.
- The physics-aware chrome derives from the already accepted creator chrome and retains hierarchy, inspector, Asset Browser and existing Studio shell behavior.

## Physics Lab surface

The dedicated workspace contains eight curated pages:

1. World
2. Rigid Body
3. Constraint
4. Character
5. Vehicle
6. Ragdoll
7. Soft Body
8. Wicked Collider

This is intentionally Wicked Editor parity, not an attempt to expose every internal Jolt type as a creator-facing control.

### Rigid Body

The source exposes all seven Wicked Editor collision shapes:
- Box
- Sphere
- Capsule
- Cylinder
- Convex Hull
- Triangle Mesh
- Height Field

It also exposes the creator-facing Wicked properties already bridged by JP01, including mass, friction, restitution, damping, buoyancy, mesh LOD, kinematic mode, 2D lock, deactivation controls and local offset. Complex shapes validate the selected target rather than silently authoring invalid state.

### Constraints

The source exposes all eight Wicked Editor constraint families:
- Fixed
- Point
- Distance
- Hinge
- Cone
- Six DOF
- Swing Twist
- Slider

Body A/B selection is populated from the authoritative Scene. The UI uses Wicked's public ComboBox query APIs rather than reaching through protected widget storage.

### Character, vehicle, ragdoll and soft body

- Character physics exposes enable, slope and gravity-factor authoring plus live ground-state readout when available.
- Vehicle physics exposes Car/Motorcycle setup, collision mode, chassis/wheel geometry, drivetrain, steering, suspension, motorcycle controls, visual wheel mapping and bounded live drive-test controls.
- Humanoid/Ragdoll exposes the Wicked creator controls already represented by the JP01 bridge and live-state acknowledgement.
- Soft Body resolves Object selections to their Mesh and exposes the Wicked editor authoring surface plus reset/wake operations.

### Wicked Collider

Wicked Collider is explicitly labelled **NOT JOLT** in Physics Lab. It is presented as the lightweight CPU/GPU collider used by Wicked secondary systems such as particles, hair and springs, avoiding the incorrect implication that it is a Jolt trigger/sensor component.

## Studio/chrome checks

- `CreatorAssetStudioChrome` is extensible rather than sealed; its existing behavior is retained.
- `RenegadePhysicsLabStudioChrome` owns the Physics Lab overlay.
- The Physics workspace occupies the unused workspace-tab slot after Terrain; the established shell geometry is not widened.
- Opening Physics reconciles the underlying Level Editor to Scene mode, then presents Physics Lab over the Scene viewport.
- Clicking Scene, Environment or Terrain exits Physics Lab through the existing workspace action route.
- Importer/workspace reconciliation preserves Physics Lab when Physics is the active workspace.
- Physics Lab input consumption is combined with the existing creator chrome consumption so Physics controls cannot fall through into viewport selection, gizmos or fly-camera input.
- Combo boxes render last in reverse order so open dropdowns are not masked by later controls.
- UI refresh requested by a control callback is deferred until the active-widget iteration finishes, avoiding iterator invalidation/rebuild hazards.
- Controls that require a missing rigid body, constraint, humanoid, soft body or collider are disabled rather than showing apparently editable stale state.

## Build/source-contract checks

`Studio/CMakeLists.txt` explicitly includes:
- `RenegadePhysicsLabWorkspace.cpp/.h`
- `RenegadePhysicsLabStudioChrome.cpp/.h`

The CTest `RenegadeJP01PhysicsLabSourceContract` runs `Tests/JP01PhysicsLabSourceContract.cmake` and guards the key architectural/parity requirements, including:
- dedicated chrome ownership,
- no Physics Lab implementation in `StudioApplication.cpp`,
- eight Physics Lab pages,
- seven rigid shapes,
- eight Wicked constraints,
- public ComboBox APIs only,
- no raw/second `JPH::PhysicsSystem` ownership,
- no Physics Lab `wi::Archive` serialization,
- shared Studio command history,
- honest Wicked Collider semantics,
- explicit RenegadeStudio source inclusion.

## Pre-CI fixes already made

The staging pass corrected several issues before consuming another Windows CI run:
- removed protected `ComboBox::items` access,
- replaced mixed-derived-pointer initializer lists that are not valid portable C++,
- converted the prototype header-only implementation into normal `.h/.cpp` ownership,
- added the missing standard `<utility>` dependency for `std::move`,
- deferred refresh/rebuild after widget callbacks,
- corrected debug draw distance to the Wicked editor's 0–2000 creator range,
- separated global/session physics settings from Scene-serialized gravity,
- disabled controls whose required component is absent,
- preserved the distinction between Jolt physics and Wicked secondary-system colliders.

## What Windows CI must prove

The source audit cannot replace a real Windows compile/runtime pass. The next PR build is intentionally the first expensive validation after this staging pass and should prove:
- MSVC Debug Studio build,
- MSVC Release Studio build,
- the complete existing CTest suite plus the JP01 Physics Lab source contract,
- no link/ODR regressions from the new workspace/chrome translation units,
- packaged owner-validation artifact generation if the workflow reaches that stage.

If CI is green, the next owner validation should focus on the actual Physics Lab UX and live creator behavior rather than repeating backend unit checks already covered by the suite.
