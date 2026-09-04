# S5 — Core API, Events & Diagnostics

## Staging status

This document is staged on `scripting/s5-core-api-prep`, stacked directly on the
exact S4D PR #137 head while S4D acceptance CI runs. It must not be opened as an
S5 PR or treated as merge-ready until S4D is accepted, merged, and this branch is
re-anchored onto the resulting `main` commit.

S5 extends the governed S3 runtime and S4 authoring model. It does not create a
second Lua state, second entity identity scheme, or Wicked `ScriptComponent`
path.

## Programme invariants

- creator Lua remains vanilla Lua 5.4 on the dedicated governed Runtime state;
- public API remains versioned under `renegade.*`;
- `EntityRef` remains opaque, generation-aware and backed by Renegade persistent
  identity, never a Wicked/Jolt/native ID;
- Level `GLOBAL SCRIPT` instances keep `self.entity == nil`;
- one script failure disables only that ScriptInstanceId;
- runtime gameplay mutation is not Studio authoring and therefore does not route
  through Studio Undo/Redo;
- event/message dispatch is queued at Runtime safe points; there is no synchronous
  cross-script re-entry;
- installed Library adoption/package closure stays S6;
- stock beginner Actions stay S7;
- raw Wicked/Jolt access remains future explicit Advanced/Unsafe only.

## Proposed S5 gate split

### S5A — Core entity and transform API

First executable creator-facing gameplay slice.

Target API surface:

- `renegade.entity.is_valid(ref)` — retained from S3;
- `renegade.entity.equals(a, b)` — retained from S3;
- `renegade.entity.get_name(ref)`;
- `renegade.transform.get_local_position(ref)`;
- `renegade.transform.set_local_position(ref, value)`;
- `renegade.transform.translate_local(ref, delta)`;
- subsequent rotation/scale/world-transform access only with explicit, tested
  semantics rather than silently conflating local and world space.

The staged `RuntimeScriptEntityApi` is the C++ safety seam for this gate. It:

- accepts only the opaque ref's stable ID + generation projection;
- rejects stale generations;
- resolves only through S3's stable-ID map;
- revalidates `PersistentEntityId(scene, entity)` on every call;
- rejects removed/replaced same-generation entities;
- rejects NaN/Infinity transform writes;
- mutates only live Runtime Scene state;
- never exposes a raw ECS ID to Lua.

Initial owner-test target after Lua registration: attach a tiny SCRIPT to the
existing barrel and visibly translate it in Test Level.

### S5B — Gameplay namespaces

Build on the same governed API registration mechanism rather than bespoke Lua
bindings per subsystem.

Planned namespaces include:

- player/input;
- audio playback/control using governed AudioRef values;
- animation playback/control using governed AnimationRef values;
- physics queries/forces through Renegade's accepted Wicked/Jolt physics world;
- creator-safe entity lookup/tags once the entity tag authority is explicitly
  fixed (including future `renegade.entity.find_by_tag("Enemy")`).

No namespace may return raw Wicked/Jolt/ECS handles.

### S5C — Event queue, messaging and producers

Add a Runtime-owned bounded FIFO event queue.

Required semantics:

- dispatch only at defined Runtime safe points;
- deterministic attachment order;
- no synchronous cross-script re-entry;
- queued `emit` / targeted `send` semantics;
- payload validation and size/depth limits;
- per-frame/event-storm budgets;
- recursion/cycle protection through queue limits rather than call-stack re-entry;
- a failing recipient disables only that ScriptInstanceId;
- gameplay producers (interaction, trigger, collision, damage, etc.) enqueue into
  the same authority rather than invoking Lua directly.

### S5D — Diagnostics and Studio test bridge

This is the natural place for the previously unfrozen “S5D” label.

Upgrade S3's in-process diagnostic records to include:

- severity;
- category;
- monotonic sequence;
- ScriptInstanceId;
- owner scope/entity;
- source path;
- callback/event;
- concise message;
- disabled-instance state.

Test Level remains a separate Runtime process. Preferred Studio transport is a
Renegade-owned named pipe carrying structured diagnostics; Studio must not share
or borrow the Runtime Lua state.

The bridge must survive Runtime restart/disconnect cleanly and must never make
normal packaged games depend on Studio IPC.

## Staged S5A files

- `Runtime/src/RuntimeScriptEntityApi.h`
- `Runtime/src/RuntimeScriptEntityApi.cpp`
- `Runtime/S5CoreGameplayApi.cmake`
- `Tests/S5CoreGameplayApiTests.cpp`
- `Tests/S5CoreGameplayApi.cmake`
- `Tests/S5CoreGameplayApiSourceContract.cmake`

These files intentionally establish the safe C++ entity/transform layer first.
Lua registration into `RuntimeScriptRuntime` is the next S5A step after S4D is
accepted/merged and this stack is re-anchored.

## Re-anchor procedure after S4D

1. verify PR #137 merged and capture the exact new `main` SHA;
2. re-anchor/rebase `scripting/s5-core-api-prep` so its diff contains only S5;
3. run source contract and diff hygiene before expensive Windows CI;
4. complete Lua registration and S5A behavioural tests;
5. open S5A draft PR only when the branch is compile-audited;
6. authoritative acceptance remains Studio Debug/Release + baseline Debug/Release
   on the exact PR head, followed by the barrel movement owner test.
