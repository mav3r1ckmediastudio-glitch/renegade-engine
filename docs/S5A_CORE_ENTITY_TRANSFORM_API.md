# S5A — Core Entity & Transform API

## Status

S5A is the first creator-scripting gate that exposes deliberate gameplay mutation through the governed Renegade Lua runtime.

It builds directly on the accepted S3 runtime and S4 authoring model. It does not create a second Lua state, a second entity identity system, a Wicked `ScriptComponent` path, or a raw ECS/native-ID escape hatch.

## Creator API

S5A extends the versioned `renegade.*` surface with:

```lua
renegade.entity.get_name(ref)

renegade.transform.get_local_position(ref)
renegade.transform.set_local_position(ref, { x = 0, y = 0, z = 0 })
renegade.transform.translate_local(ref, { x = 0, y = 0, z = 0 })
```

The S3 entity helpers remain available:

```lua
renegade.entity.is_valid(ref)
renegade.entity.equals(a, b)
```

Entity and transform namespaces expose `contract_version = 1` while the governed runtime remains on Renegade API version 1.

## Reference and mutation authority

Creator Lua continues to receive only opaque `Renegade.EntityRef` userdata.

For every S5A entity/transform operation, Runtime:

1. projects the userdata to its Renegade persistent stable ID and Level generation internally;
2. rejects malformed or stale-generation references;
3. resolves through S3's current stable-ID map;
4. revalidates the live Scene entity with `PersistentEntityId` on every operation;
5. rejects removed/replaced entities even when the Level generation itself has not changed;
6. performs the requested operation against the live Runtime Scene only.

No Wicked ECS entity ID is returned to Lua.

## Transform semantics

S5A deliberately exposes **local position only**.

Rotation, scale and world-space transforms are not silently inferred from local transform behaviour. Those surfaces require their own explicit semantics and tests before exposure.

`set_local_position` and `translate_local` reject NaN/Infinity input. Translation also rejects a result that overflows the finite runtime range.

Vector inputs are plain tables with numeric `x`, `y` and `z` fields. Runtime reads those fields without invoking creator table metamethods.

## Error behaviour

Getters return the requested value on success, or `nil, error` on a recoverable API failure.

Mutators return `true` on success, or `false, error` on a recoverable API failure.

A script can choose to handle those failures. If it raises an error (for example with `assert`), the existing S3 isolation rule applies: only that ScriptInstanceId is disabled and diagnosed.

## Acceptance

Automated coverage includes:

- live persistent EntityRef resolution;
- creator-safe entity-name lookup;
- local-position read/write/translation;
- finite-value enforcement;
- stale-generation rejection;
- removed same-generation entity rejection;
- governed Lua registration and invocation through `renegade.*`;
- malformed Vector3 input returning a recoverable error;
- a full governed Lua fixture that moves a test barrel from `(0,0,0)` to `(1.5,1,5)` during `on_start`.

Creator owner acceptance after four authoritative CI greens is intentionally simple: attach the S5A movement fixture to the existing barrel, launch Test Level, and verify the barrel visibly moves by the Inspector-authored local offset.

## Programme boundary

S5A does not expose player/input, audio, animation or physics gameplay namespaces. Those are S5B.

It does not introduce queued cross-script messaging/event producers. Those are S5C.

It does not add Studio diagnostics IPC. That is S5D.
