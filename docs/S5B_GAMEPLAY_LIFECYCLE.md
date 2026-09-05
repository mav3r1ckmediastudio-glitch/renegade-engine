# S5B — Governed gameplay lifecycle

S5B is the first complete gameplay-facing Lua slice on top of the S5A
entity/transform runtime. It is implemented in the governed Runtime Lua state,
with no access to Wicked's global Lua VM, raw ECS integers, filesystem APIs, or
device polling.

The shipped API is:

- `renegade.player.is_present()`
- `renegade.player.get_position()`
- `renegade.player.get_forward()`
- `renegade.player.get_yaw()`
- `renegade.player.get_pitch()`
- `renegade.input.get_axis(action)`
- `renegade.input.is_down(action)`
- `renegade.input.was_pressed(action)`

Actions use the stable names from the project GameplayInput map:
`move_forward`, `move_backward`, `move_left`, `move_right`,
`look_yaw`, `look_pitch`, `jump`, `sprint`, `pause`, and `reset`.

Player and input state are projections of the live EngineBridge services. Scripts
never poll devices or mutate the PlayerService directly. Lifecycle callbacks
`on_start`, `on_update`, `on_pause`, `on_resume`, `on_reset`, and
`on_stop` remain isolated per script instance and are invoked by the Runtime
generation that owns the active Scene.

All API errors use the existing governed convention: query functions return
`value, error`; boolean/action functions return `false, error`. Invalid
action names are rejected. The acceptance target is
`RenegadeS5BGameplayLifecycleTests`, and it is registered in the normal full
CI test graph.
