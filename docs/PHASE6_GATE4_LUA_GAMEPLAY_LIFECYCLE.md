# Phase 6 Gate 4 — Lua Gameplay Lifecycle

## Outcome

Gate 4 adds one governed Lua gameplay path shared by Test Level and packaged
Runtime. Studio imports a `.lua` source into `Content/Scripts`, creates a native
WISCENE `ScriptComponent` carrier and selects it. The Inspector shows the
project-relative source and an undoable Enabled control.

Wicked continues to own the single Lua VM. Renegade owns deterministic gameplay
lifecycle dispatch and the stable gameplay API. Governed components always have
Wicked's native `PLAYING` and `PLAY_ONCE` flags cleared before Scene update, so
the same file cannot also execute through Wicked's whole-file-per-frame path.

## Creator workflow

1. Choose `ADD > GAMEPLAY SCRIPT...`.
2. Select a `.lua` file. Studio validates that it is a regular, non-symlink file
   containing 1 byte to 1 MiB of source and compiles it for Lua syntax without
   executing it.
3. An external source is copied under `Content/Scripts` with a collision-free
   filename. A source already in that folder is retained.
4. Studio creates and selects a transform-free script carrier named from the
   file. Save/Open persists its native Script component and Renegade metadata.
5. Test Level or the packaged game loads the file relative to the active project
   root and invokes its lifecycle table.

The imported file is a project asset, not command history. Undo removes the
WISCENE attachment but deliberately does not delete the source file.

## Script contract

A gameplay script returns one table. Every callback is optional. `self` is that
same table on every callback, so ordinary table fields hold per-instance state.
`context.entity_id` is the carrier's persistent UUID and is the stable identity
authority.

```lua
return {
    on_start = function(self, context)
        self.elapsed = 0
    end,

    on_update = function(self, context, dt)
        self.elapsed = self.elapsed + dt
    end,

    on_pause = function(self, context) end,
    on_resume = function(self, context) end,
    on_reset = function(self, context) end,
    on_stop = function(self, context) end,
}
```

Instances are sorted by persistent entity ID when the Level starts and retain
that order for every lifecycle phase.
`on_start` runs once after a Level becomes active. `on_update` runs once per
unpaused Runtime frame with delta clamped to 0–0.1 seconds. Pause and Resume run
once on their matching transitions. Reset invokes `on_reset`, then `on_stop`,
reloads the authored startup state and starts new instances. A Scene/Flow Level
change invokes `on_stop` before starting the replacement Level.

If a file cannot load, does not return a table, exposes a non-function callback,
or throws from a callback, only that instance is disabled. Runtime logs one
structured diagnostic containing its stable entity ID, source path, callback
phase and Lua message. Other scripts continue in deterministic order.

## Gameplay API version 1

All surfaces live below the global `renegade` table and expose
`contract_version = 1`.

### `renegade.entity`

- `exists(stable_id)` — whether a persistent entity ID resolves in this Level.
- `find(name)` — the stable ID for one uniquely named entity; returns `nil` for
  no match or an ambiguous name.
- `position(stable_id)` — world `{x, y, z}` or `nil` without a Transform.
- `native_id(stable_id)` — the current process-local numeric entity handle or
  `nil`. This is the explicit adapter for the accepted `renegade.physics`
  functions; scripts must persist the stable ID, never the returned number.

### `renegade.input`

- `value(action)` — analog value for movement/look actions, otherwise zero.
- `pressed(action)` — one-frame state for jump, pause and reset.
- `down(action)` — held/active state for sprint and non-zero analog actions.

Supported action names are `move_forward`, `move_backward`, `move_left`,
`move_right`, `look_yaw`, `look_pitch`, `jump`, `sprint`, `pause` and `reset`.
Gate 5 owns the first objective-specific interaction action.

### `renegade.player`

- `is_spawned()` — whether Runtime possesses a Player Start.
- `position()` — player feet position as `{x, y, z}`, or `nil` in spectator
  fallback.

### `renegade.audio`

- `play(stable_id)` — play a resolved Renegade sound source.
- `stop(stable_id)` — stop a resolved Renegade sound source.

### `renegade.physics`

The accepted JP01 namespace remains authoritative. Gate 4 does not duplicate
its physics world or functions. Resolve a persistent entity through
`renegade.entity.native_id()` only at the point of use:

```lua
local id = renegade.entity.find("Movable Crate")
local entity = id and renegade.entity.native_id(id)
if entity then
    renegade.physics.apply_impulse(entity, 0, 3, 0)
end
```

No gameplay API returns an engine pointer, component pointer or owned native
object.

## Persistence and packaging

The WISCENE owns the native Script component filename and versioned Renegade
metadata. Renegade metadata stores the safe `Content/Scripts/*.lua`
project-relative authority. The live native component receives the validated
absolute resource path, allowing Wicked's serializer to persist the correct
scene-relative `../Scripts/...` reference and reconstruct it on load. Existing
WISCENE dependency extraction therefore emits a typed Script edge, and the
accepted LP05/LC01/LP06 build chain carries the file into the same relative
location under packaged `GameData`.

Test Level uses an isolated shadow project. Snapshot creation stages every
governed script referenced by the live WISCENE into that shadow project's
`Content/Scripts` path and validates the copy before launch. This keeps unsaved
Test Level playback on the same relative-path contract as a packaged build.

Lua source is never executed by dependency discovery or packaging.

## Automated evidence

`RenegadePhase6Gate4GameplayScriptTests` covers:

- contained import and path-traversal rejection;
- command Execute/Undo/Redo and native Wicked flag exclusion;
- immediate stable carrier identity and Enabled-state Undo/Redo;
- valid and invalid syntax preflight;
- Test Level shadow-project script staging;
- stable entity, input, player, audio and accepted physics namespace access;
- deterministic callback ordering;
- pause suppression, Resume and Reset/Stop order; and
- per-script runtime error isolation.

`RenegadePhase6Gate4SourceContract` pins the Studio action, Runtime lifecycle,
API namespaces and single lifecycle owner.

## Owner acceptance

After exact-head Windows CI passes:

1. Add a valid sample Lua file and confirm its named carrier is selected and the
   Inspector displays its `Content/Scripts/...` path.
2. Save, reopen and confirm the attachment and Enabled state remain.
3. Run Test Level and verify Start/Update, Pause/Resume and Reset behavior from
   Runtime diagnostics or an obvious scripted effect.
4. Introduce a throwing second script and confirm the first keeps running while
   one precise error is logged.
5. Build Windows Game and repeat the lifecycle proof from the packaged
   executable.
6. Recheck Player Start movement, global/3D sound and the corrected Scene Mix
   label spacing carried from Gate 3.

## Explicit deferrals

- Objective state, interaction prompts and Screen/Story Flow outcome dispatch
  belong to Gate 5.
- Shared trigger volumes remain deferred to the common ZoneService slice.
- Hot reload, an integrated code editor/debugger and arbitrary C modules are not
  part of this gate.
