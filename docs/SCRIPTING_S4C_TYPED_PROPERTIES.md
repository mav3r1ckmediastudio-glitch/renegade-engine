# S4C — Typed Script Properties

## Purpose

S4C turns the S4A metadata property contract into creator-facing controls inside the S4B ACTION and SCRIPT attachment Inspector. It does not introduce another property store: the S2 `.rscripts` companion remains authoritative, and every accepted edit enters Studio's shared `CommandService` Undo/Redo history.

## Authoring boundary

S4C adds a small EngineBridge authoring seam rather than placing property policy in Studio UI code:

- `NormalizeScriptPropertyForAuthoring` enforces the S4A descriptor for a proposed value.
- `CommitScriptPropertyAuthoringEdit` commits the normalized value through S2's `MakeSetScriptPropertyCommand` and the shared `CommandService`.
- no Wicked `ScriptComponent`, Wicked global Lua state or raw native entity ID becomes creator authority.

Numeric metadata remains authoritative. Declared `min`, `max` and `step` constraints are enforced before the value is committed. Unconstrained 64-bit integers retain exact integer precision rather than round-tripping through floating-point storage.

## Generated controls

For each attached ACTION or SCRIPT, S4C renders metadata properties directly below the attachment row in authored metadata order.

Schema-v1 mapping:

| S4A property type | S4C creator control |
| --- | --- |
| `boolean` | checkbox |
| `integer` | validated numeric input |
| `float` | validated numeric input |
| `string` | text input |
| `colour` | R / G / B / A component inputs, clamped to 0..1 |
| `vector2` | X / Y component inputs |
| `vector3` | X / Y / Z component inputs |
| `enum` | dropdown using declared option labels/values |
| `entity` | unresolved/assigned placeholder; picker deferred to S4D |
| `asset` | unresolved/assigned placeholder; picker deferred to S4D |
| `animation` | unresolved/assigned placeholder; picker deferred to S4D |
| `audio` | unresolved/assigned placeholder; picker deferred to S4D |

Reference-backed placeholders intentionally never print the opaque stable reference ID. S4D owns the creator picker UX for those types.

## Persistence and Undo/Redo

S4C edits mutate the live S2 `ScriptDocument` through commands. Therefore:

- editing a property marks the normal Studio authoring history dirty;
- Undo/Redo restores property values alongside other Scene/script authoring commands;
- normal Scene save persists values into the adjacent `.wiscene.rscripts` companion through the S4B save boundary;
- save/reopen keeps the same ScriptInstanceId and its per-instance property values.

Metadata defaults remain source defaults only. Once a value exists in the S2 attachment, the persisted instance value remains authoritative.

## Owner test

The S4B owner-test Lua files can be reused immediately:

- `S4B_TestAction.lua` should expose `Speed` and `Message` below its ACTION attachment.
- `S4B_TestScript.lua` should expose `Enabled Flag`, `Interval` and `Mode` below its SCRIPT attachment.

Acceptance owner testing should verify edits, numeric clamping/step behavior, enum selection, Undo/Redo, save/reopen persistence, and independence between two instances of the same source.

## Explicit non-goals

S4C does not add:

- Entity/asset/animation/audio reference pickers;
- GLOBAL SCRIPT Level authoring;
- gameplay entity/audio/animation APIs;
- event queues or gameplay event bindings;
- diagnostics IPC;
- Library adoption or package closure;
- built-in Lua source editing or hot reload.

Reference pickers, GLOBAL SCRIPT authoring and S4 authoring hardening remain S4D. Gameplay-facing APIs/events/diagnostics remain S5.
