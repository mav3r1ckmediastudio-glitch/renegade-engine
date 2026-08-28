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

The pinned Wicked Jolt integration (`3a800b7134aafe58461093c8abb2e274d4e64033`) handles dynamic parented bodies by temporarily applying the recursive parent matrix before simulation and then applying the inverse parent matrix when feeding the result back to the transform. `TransformComponent::MatrixTransform()` decomposes the resulting matrix back into scale, rotation and translation.

That is normally reversible for simple transform chains. Deep imported hierarchies can contain conversion scales and rotated/non-uniform scale combinations that imply shear. Repeated matrix decomposition/recomposition on a dynamic child is therefore not a safe creator-facing ownership boundary and can amplify transform error into runaway scale/shear.

Renegade already had the correct stable boundary: `PlaceReusableModelCommand` creates a `Reusable Asset Instance` wrapper specifically to own durable creator-authored transform/state while the imported payload below it remains replaceable.

## Repair policy

### New authoring

`ResolveCollisionAuthoringTarget()` walks from the selected entity upward through the actual Scene hierarchy. If the selection is inside a reusable asset, the nearest entity carrying `renegade.reusable_asset_id` is returned as the rigid-body owner.

`CreateCollisionCommand` now resolves its target through that function before creating the Wicked `RigidBodyPhysicsComponent`.

The body therefore remains a normal Wicked/Jolt component in the authoritative Scene. No second physics world, Renegade serializer or Wicked fork is introduced.

Physics Lab follows the selection to that stable wrapper once the body exists so subsequent rigid-body controls edit the component that was actually created.

New bodies also restore Wicked Editor parity by defaulting `startDeactivated` to `true`.

### Already-saved affected scenes

`RepairReusableAssetCollisionTargets()` handles the bounded owner-failure signature:
- a reusable asset wrapper has no rigid body,
- exactly one rigid body exists on a descendant owned by that wrapper.

For that unambiguous case it:
1. copies the complete Wicked `RigidBodyPhysicsComponent`,
2. clears the implementation-owned live `physicsobject`,
3. removes the component from the imported child,
4. creates/restores it on the stable reusable wrapper,
5. requests normal Wicked parameter refresh.

The repair runs on a deserialized WISCENE inside `PrepareWickedSceneOpen()` before the prepared scene can reach its first live physics update. It also runs on the active Scene immediately before save so the corrected ownership is persisted on the next save.

Ambiguous multi-body cases are not destructively guessed at; they are counted as conflicts and remain explicit review cases.

## Regression coverage

`RenegadeJP01ReusableAssetPhysicsTests` builds a headless hierarchy matching the creator ownership pattern:

`Reusable Asset Instance -> imported payload root -> crate002`

It verifies:
- nested selections resolve to the stable wrapper,
- ordinary standalone entities remain unchanged,
- Add creates the body on the wrapper rather than the imported node,
- Wicked Editor start-deactivated parity,
- Undo/Redo keeps wrapper ownership,
- the unambiguous saved-scene repair moves a nested body to the wrapper,
- mass, friction, restitution, buoyancy, shape dimensions and flags survive migration,
- the live Jolt implementation pointer is never migrated,
- ambiguous root+nested multi-body state is left untouched rather than guessed.

`RenegadeJP01PhysicsLabSourceContract` additionally locks the authoring resolver, pre-physics repair call, pre-save canonicalization, Physics Lab selection follow, test registration, and the invariant that this recovery does not instantiate or own `JPH::PhysicsSystem`.

## Owner retest after green CI

Use a fresh reusable crate first:
1. place the crate normally,
2. select the same kind of nested crate node that was used in the failed test,
3. open Physics -> Rigid Body,
4. press Add Rigid Body,
5. confirm Physics Lab changes selection to `Reusable Asset Instance`,
6. save,
7. return to Scene/Environment,
8. confirm the crate keeps its original rendered size/proportions and textures,
9. confirm there is no runaway selection geometry, flicker or camera clipping,
10. activate the body when ready and verify normal motion/buoyancy.

Then reopen the previously affected scene. The single nested-body case should be repaired in memory before physics runs. If the old file itself captured an already-corrupted payload transform before it was saved, body ownership will still be repaired but the damaged authored transform cannot be reconstructed safely from the rigid-body component alone; use the automatic pre-save WISCENE backup or re-place that asset rather than guessing at transform data.
