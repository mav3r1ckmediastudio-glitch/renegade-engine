# JP01 — Reusable Asset Rigid Body Owner Recovery

Status: **STAGED FOR WINDOWS CI / OWNER RETEST**

## Owner reproduction

The owner validation build exposed a severe failure when a reusable imported crate was placed in the scene, a dynamic rigid body was added from a deeply nested imported hierarchy node, the scene was saved, and the creator returned to the normal editor view.

Observed symptoms included:
- the rendered crate growing/stretching dramatically,
- stretched-looking surface textures,
- large/unstable selection geometry,
- viewport flicker,
- camera clipping through what appeared to be an expanding object,
- the object disappearing when zooming out because its effective bounds had become pathological.

This is not valid rigid-body behaviour and is not an acceptable creator workflow.

## Root cause

Renegade's original `CreateCollisionCommand` accepted any entity with a `TransformComponent` for primitive collision. Physics Lab therefore allowed a body to be created on an arbitrary internal GLTF/FBX transform inside a reusable asset payload instead of on the reusable asset's stable creator-owned wrapper.

The pinned Wicked Jolt integration handles dynamic parented bodies by temporarily applying the recursive parent matrix before simulation and then applying the inverse parent matrix when feeding the result back to the transform. `TransformComponent::MatrixTransform()` decomposes the resulting matrix back into scale, rotation and translation.

That is normally reversible for simple transform chains. Deep imported hierarchies can contain conversion scales and rotated/non-uniform scale combinations that imply shear. Repeated matrix decomposition/recomposition on a dynamic child is therefore not a safe creator-facing ownership boundary and can amplify transform error into runaway scale/shear.

Renegade already had the correct stable boundary: reusable placement creates a wrapper specifically to own durable creator-authored transform/state while the imported payload below it remains replaceable.

## Final hardening policy

### Creator-facing asset root

Reusable wrappers are no longer intended to present as repeated `Reusable Asset Instance` labels. New placements derive a meaningful model name from the imported hierarchy and make it unique among reusable roots, for example:

- `crate002`
- `crate002 (2)`
- `crate002 (3)`

Old anonymous/generated wrapper names are repaired on the normal load/save canonicalization path. Explicit creator-renamed wrapper names are preserved.

### Rigid-body ownership

`ResolveCollisionAuthoringTarget()` walks from the selected entity upward through the actual Scene hierarchy. If the selection is inside a reusable asset, the nearest entity carrying `renegade.reusable_asset_id` is returned as the rigid-body owner.

`CreateCollisionCommand`, `SetCollisionCommand` and `RemoveCollisionCommand` all resolve through that owner. The creator can therefore select `crate002`, `Object_4` or another imported descendant and use normal rigid-body controls without creating a dynamic body on that imported node.

Physics Lab promotes descendant selection to the stable asset root on the three pages that share `RigidBodyPhysicsComponent`: **Rigid Body**, **Character** and **Vehicle**. Soft Body, Ragdoll and Secondary Collider keep legitimate descendant selection and are not globally redirected.

New bodies default `startDeactivated` to `true` so adding physics does not immediately throw the selected asset into simulation.

### Primitive auto-fit

Box, Sphere, Capsule and Cylinder bodies auto-fit to descendant render geometry by default. Measurement is performed in the resolved asset root's local space:

- imported descendant transforms are included,
- the reusable root's own authored scale is excluded from stored dimensions,
- selecting the asset root directly and selecting a descendant use the same fit path.

This avoids double scaling because Wicked/Jolt already applies the root `Transform` scale when the native collision shape is created.

Advanced/native callers can explicitly opt out of auto-fit when exact primitive dimensions are required.

### Scale transforms

Scale rebuild is not polled from Physics Lab and is not tied to whichever asset was selected most recently.

`SetTransformCommand::Apply()` is the central committed transform route used by Execute, Undo and Redo. When the transformed entity itself owns a rigid body and its authored scale changes, Renegade:

1. leaves the stored root-local primitive dimensions unchanged,
2. clears the implementation-owned native physics object,
3. requests normal physics refresh,
4. lets Wicked/Jolt recreate the native shape from the new root scale.

A child-only transform edit is not falsely reported as a whole-asset collider rebuild.

### Already-saved affected scenes

`RepairReusableAssetCollisionTargets()` handles the bounded owner-failure signature:
- a reusable asset root has no rigid body,
- exactly one rigid body exists on a descendant owned by that root.

For that unambiguous case it:
1. copies the complete Wicked `RigidBodyPhysicsComponent`,
2. clears the implementation-owned live `physicsobject`,
3. removes the component from the imported child,
4. creates/restores it on the stable reusable root,
5. requests normal physics refresh.

The repair runs on a deserialized WISCENE inside `PrepareWickedSceneOpen()` before the prepared scene can reach its first live physics update. It also runs on the active Scene immediately before save so the corrected ownership is persisted on the next save.

Ambiguous multi-body cases are not destructively guessed at; they are counted as conflicts and remain explicit advanced/compound review cases.

## Visual collision verification

Physics Lab retains the real **PHYSICS VISUALIZER** control. It enables the backend collision debug draw used to inspect the actual fitted physics shape; Renegade does not draw a fake replacement box. With the visualizer enabled, scaling a root-owned crate should visibly scale the green collision shape after the committed transform rebuild.

## Creator-facing terminology

Physics Lab uses Renegade-facing language such as **PHYSICS AUTHORING** and **SECONDARY COLLIDER**. Wicked/Jolt attribution remains in source, developer documentation and licences rather than occupying the creator-facing controls.

## Regression coverage

`RenegadeJP01ReusableAssetPhysicsTests` now covers the actual owner workflow, including a three-instance same-crate fixture. It verifies:

- descendant selection resolves to the stable root,
- Add/Set/Remove remain root-owned,
- primitive auto-fit excludes root scale,
- direct root selection also auto-fits,
- new bodies start deactivated,
- Undo/Redo keeps root ownership,
- Character and Vehicle routes cannot fabricate an unsafe nested body,
- three copies are named `crate002`, `crate002 (2)` and `crate002 (3)`,
- all three keep independent rigid bodies,
- different root scales do not alter stored root-local fit dimensions,
- scaling one root refreshes only that body's native shape,
- scale Undo and Redo both rebuild correctly,
- child-only scale does not masquerade as a whole-root rebuild,
- legacy anonymous names are repaired without overwriting a custom name,
- an unambiguous incorrectly saved nested body migrates before physics,
- complete migrated body authoring state survives,
- ambiguous multi-body hierarchies remain untouched,
- the backend physics visualizer can still be enabled.

`RenegadeJP01HardeningSourceContract` locks the corresponding architectural seams and explicitly forbids restoring the old per-frame selected-body scale tracker.

## Owner retest after green CI

Use three instances of the same crate:

1. confirm their hierarchy roots are meaningful and unique rather than three anonymous reusable-wrapper labels,
2. select a visible imported crate descendant and open **Physics -> Rigid Body**,
3. press **Add Rigid Body** and confirm authoring resolves to that asset's stable root,
4. enable **Physics Visualizer** and confirm the green primitive surrounds the visible crate,
5. repeat on the other two crate instances,
6. scale one crate root in Scene and confirm only that crate and its green collision shape scale together,
7. Undo and Redo the scale and confirm the collision shape follows both operations,
8. activate the bodies when ready and verify normal motion, water interaction and buoyancy,
9. save/reopen and verify the ownership, names and proportions remain stable.

Then reopen a scene saved by the faulty build. The single nested-body case should be repaired in memory before physics runs. If the old file itself captured an already-corrupted payload transform before it was saved, body ownership will still be repaired but the damaged authored transform cannot be reconstructed safely from the rigid-body component alone; use the automatic pre-save WISCENE backup or re-place that asset rather than guessing at transform data.
