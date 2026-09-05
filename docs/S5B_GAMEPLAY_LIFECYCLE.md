# S5B — Gameplay Lifecycle and Governed Gameplay Access

## Status

S5B is the next scripting gate after the accepted S5A core Entity and Transform API. It extends the existing dedicated Renegade Lua runtime; it does not create a second Lua state, input poller, player controller, audio system or physics world.

## First vertical slice

The first S5B slice exposes the already accepted Phase 6 gameplay lifecycle to governed creator scripts:

```lua
renegade.player.is_present()
renegade.player.get_position()
renegade.player.get_forward()
renegade.input.is_down("move_forward")
renegade.input.was_pressed("jump")
```

All returned entity references remain opaque `Renegade.EntityRef` userdata. Vector values are plain immutable-by-contract tables with finite numeric components.

## Authority

- `GameplayInputService` remains the only raw device polling boundary.
- `PlayerService` remains the only player movement/controller boundary.
- Runtime Lua receives a read-only gameplay view in the first slice.
- Lua cannot inject raw keyboard/mouse events or access Wicked ECS/Jolt IDs.
- Pause, reset and stop continue through the existing Runtime lifecycle.

## Acceptance

The S5B gate must prove:

- lifecycle callbacks see deterministic player/input state;
- input is sourced from the governed project action map;
- absent player state returns a recoverable Lua error;
- stale EntityRefs remain rejected through S5A;
- pause/reset/stop isolate and clean up script instances;
- save/reopen preserves authored script attachments;
- Test Level and packaged Runtime use the same bindings;
- full Studio Debug/Release and Windows baseline Debug/Release CI pass;
- owner test verifies a governed script reads jump/movement state and player position in Test Level.

## Explicitly deferred

Player mutation, custom input injection, audio control, physics mutation, cross-script messaging and diagnostics IPC remain later S5B/S5C slices and require their own contracts and acceptance evidence.
