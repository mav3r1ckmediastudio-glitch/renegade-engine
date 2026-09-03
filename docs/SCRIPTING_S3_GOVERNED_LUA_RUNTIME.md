# S3 — Governed Lua Runtime

## Status

S3 executes the persistent script attachments introduced by S2. It is based on
the merged S2 data model and deliberately remains separate from the later S4
metadata/Inspector work and S5 gameplay API/event work.

Merged S2 baseline:

`c86187e73216d301af06d55cb45993f468845861`

S3 branch:

`scripting/s3-governed-lua-runtime`

The branch was initially prepared while S2 acceptance CI ran, then re-anchored
onto the exact squash-merged S2 `main` commit so the S3 pull request contains
only S3 changes.

## Runtime ownership

Creator scripts do **not** execute on Wicked's broad global Lua VM.

`RuntimeScriptRuntime` owns one dedicated Lua 5.4.8 `lua_State` for the Renegade
Runtime process. The state is created with `lua_newstate` and a Renegade memory
allocator, then explicitly destroyed before Runtime/Wicked process shutdown.

The initial process memory budget is 64 MiB. This is a safety boundary, not a
public tuning API; profiling may justify a later configurable budget.

## Safe standard library surface

S3 opens only:

- Lua base functions selected into each instance environment;
- `table`;
- `string`;
- `math`; and
- `utf8`.

Creator environments do not receive `package`, `io`, `os`, `debug`, `coroutine`,
`dofile`, `loadfile`, `load` or `collectgarbage`.

Source is loaded in text-only mode (`luaL_loadbufferx(..., "t")`), so a creator
attachment cannot smuggle precompiled Lua bytecode through the ordinary source
path.

## Per-instance isolation

Every enabled S2 `ScriptInstanceId` receives its own:

- `_ENV` table;
- lifecycle-table registry reference;
- `self` context table;
- module cache; and
- runtime failure state.

The source is compiled/executed separately for every instance even when many
attachments intentionally share one `sourceId`. Module-level globals and mutable
state therefore do not leak between instances.

An attachment source returns one lifecycle table. Supported S3 callbacks are:

- `on_start(self)`
- `on_pause(self)`
- `on_resume(self)`
- `on_reset(self)`
- `on_stop(self)`
- optional `on_update(self, dt)`

Missing callbacks are normal. `on_update` is recorded at load time and is not
looked up or called each frame when absent.

Level `GLOBAL SCRIPT` instances receive `self.entity == nil`. Entity ACTION and
SCRIPT instances receive an opaque EntityRef.

## Context and S2 properties

`self.properties` is populated from the S2 per-instance property state.

Primitive, Colour and Vector values are projected directly into Lua. Entity refs
become opaque generation-aware EntityRef userdata. Asset, Animation and Audio
references become opaque ResourceRef userdata; their stable IDs are not exposed
as ordinary Lua strings.

An unresolved but structurally valid EntityRef remains present. It simply fails
`renegade.entity.is_valid(ref)` until its target exists in the active Scene.

## Opaque EntityRef lifetime

EntityRef userdata contains Renegade-internal stable identity plus the current
Runtime Scene generation. Lua cannot retrieve a Wicked ECS entity number or a
Jolt/native handle from the ref.

Every API use validates:

1. the ref belongs to this governed Runtime;
2. its generation matches the active Scene generation; and
3. its stable entity identity still resolves to the same live entity in the
   active Scene.

The stable-ID map is therefore a resolver rather than a permanent liveness
cache. If an entity is removed during the current Level generation,
`renegade.entity.is_valid(ref)` immediately becomes false without requiring a
Level reload. Stopping/resetting/replacing a Scene also advances the generation,
making retained refs stale rather than allowing them to alias a later entity.

S3 exposes only the minimal bootstrap entity surface:

- `renegade.api_version == 1`
- `renegade.entity.is_valid(ref)`
- `renegade.entity.equals(a, b)`

The larger entity/transform/input/player/audio/physics/events API remains S5.
There is no `native_id` escape hatch.

## Controlled require

S3 installs a Renegade-owned `require()` in every instance environment. There is
no Lua `package` search path.

A module can load only when it is already declared as an S2 `ScriptModule`
dependency for that source. The creator may request the exact governed path or
the module alias derived from `Content/Scripts/.../*.lua` (for example
`Content/Scripts/common.lua` -> `require("common")`).

Modules execute in the same instance `_ENV` and have a per-instance registry
cache. A shared source therefore still gets isolated module state per attachment.
Undeclared modules and module cycles fail only the calling ScriptInstanceId.

S6 will replace flat authoring declarations with the complete adopted transitive
Library/package closure. S3 does not invent arbitrary filesystem search rules in
the meantime.

## Failure isolation and execution budgets

Lua syntax errors, top-level execution errors, malformed lifecycle fields,
callback failures, unavailable sources, undeclared module loads and unsupported
Advanced/Unsafe attachments disable only that ScriptInstanceId.

Other instances continue.

Every protected Lua execution is also covered by an instruction-count hook. The
initial callback budget is 500,000 Lua VM instructions. A runaway callback is
converted into the normal isolated error path instead of freezing Runtime.

The Lua protected-call/error boundary is explicitly longjmp-safe: hook cleanup is
performed after `lua_pcall` returns rather than through C++ RAII, and C callbacks
that can invoke `lua_error` do not retain non-trivial C++ locals across that
boundary.

S5 will add richer severity/category/sequence diagnostics and Studio IPC. S3
retains structured in-process diagnostics containing ScriptInstanceId, owner,
source, callback and message, and RuntimeApplication publishes those diagnostics
to the Runtime backlog without changing the execution model.

## Scene companion loading

For a Scene with an adjacent S2 companion:

```text
Content/Scenes/Castle.wiscene
Content/Scenes/Castle.wiscene.rmeta
Content/Scenes/Castle.wiscene.rscripts
```

Runtime reads the Scene document ID from `.wiscene.rmeta`, then accepts the
`.rscripts` document only when project ID and Scene ID match.

A Scene with no `.rscripts` companion remains a valid zero-script Scene. This
keeps projects created before S2/S3 compatible.

Malformed script documents are document-level failures. Individual Lua/source
failures after a valid document is accepted remain instance-level failures.

## RuntimeApplication integration

S3 is wired into the live Runtime lifecycle rather than remaining a standalone
library proof.

RuntimeApplication now:

- starts the governed script scene when Runtime owns a Level;
- revision-gates script startup alongside the existing scene-owned services;
- restarts scripts when Story Flow loads a different Level;
- stops creator scripts when Runtime enters a Screen;
- forwards pause and resume to the governed runtime;
- dispatches `on_reset` before rebuilding the play session;
- calls optional `on_update` only while the Level is running and unpaused;
- publishes new per-instance diagnostics to the Runtime backlog;
- resets the diagnostic publication cursor when a new Level generation starts;
  and
- explicitly shuts the governed state down from the process/window shutdown
  path before the Wicked jobsystem teardown.

The creator Lua state remains independent of the existing Wicked Lua state used
by engine-internal/legacy integrations. No S3 attachment is represented by a
Wicked `ScriptComponent` or carrier entity.

## Automated acceptance coverage

The S3 behavioural tests cover:

1. dedicated governed Lua-state creation and shutdown;
2. safe standard-library exposure and forbidden host surfaces;
3. independent environments for shared-source ScriptInstances;
4. Entity and Level-global context semantics;
5. S2 typed-property projection and opaque references;
6. declared-only module loading and per-instance module caching;
7. syntax/load failure isolation;
8. Advanced/Unsafe rejection;
9. optional `on_update` and callback failure isolation;
10. runaway-script instruction budgets;
11. pause/resume/reset lifecycle dispatch;
12. same-generation EntityRef invalidation after entity removal;
13. `.rscripts` + Scene identity binding; and
14. no-companion zero-script compatibility.

A source contract additionally guards the separate Lua-state architecture,
forbidden Wicked creator-VM/native-ID seams, live EntityRef validation,
RuntimeApplication lifecycle ownership, diagnostic cursor reset and explicit
process-exit shutdown.

## Explicit S3 non-goals

S3 does not add:

- final Lua metadata schema/evaluation;
- Inspector ACTION/SCRIPT/GLOBAL SCRIPT UI;
- event queue or custom messages;
- interaction, trigger, collision or damage producers;
- full gameplay namespaces;
- diagnostics IPC;
- installed/personal Library adoption;
- package dependency closure;
- stock Actions;
- hot reload; or
- raw Wicked/Jolt Advanced/Unsafe execution.

Those remain S4-S7/later according to the accepted scripting programme.
