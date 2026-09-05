# S4A — Metadata Contract & Evaluator

## Purpose

S4A freezes the creator-facing metadata contract that S4B/S4C will use to build SCRIPT/ACTION/GLOBAL SCRIPT Inspector authoring. It does not add scripting UI and does not expand the S3 gameplay API.

Metadata is authoring information, not gameplay state. Renegade therefore evaluates it in a dedicated restricted Lua state rather than borrowing the governed gameplay state introduced by S3 or Wicked's global Lua VM.

## Schema-v1 declaration

The runtime-safe declaration idiom is:

```lua
if renegade and renegade.metadata then
    renegade.metadata({
        schema_version = 1,
        name = "Open Door",
        description = "Opens an entity when activated.",
        category = "Interaction",
        role = "ACTION",
        properties = {
            {
                name = "speed",
                label = "Speed",
                description = "Opening speed in metres per second.",
                type = "float",
                default = 2.0,
                min = 0.0,
                max = 20.0,
                step = 0.25,
            },
        },
    })
end

return {
    on_start = function(self)
        -- gameplay code
    end,
}
```

The guard is intentional. S4A's metadata evaluator supplies `renegade.metadata`; the S3 gameplay Runtime does not. The same source therefore exposes metadata to Studio but skips the authoring block during gameplay execution without introducing a metadata function into the gameplay API.

The metadata evaluator terminates execution immediately after the first successful call to `renegade.metadata(...)`. Gameplay setup after that declaration is never executed during inspection.

## Root fields

Schema v1 uses:

- `schema_version` — required integer, currently exactly `1`.
- `name` — required creator-facing source name.
- `description` — optional creator-facing description.
- `category` — required source category used by later selection UI.
- `role` — required and exactly one of `ACTION`, `SCRIPT`, `GLOBAL SCRIPT`.
- `properties` — optional ordered array of property descriptors.

Property order is authored order. S4C must preserve this order when generating Inspector controls.

## Property descriptor

Every property has:

- `name` — required unique Lua-style ASCII identifier. This is the stable property key persisted by S2 and exposed as `self.properties.<name>` by S3.
- `label` — optional UI label; defaults to `name`.
- `description` — optional help text.
- `type` — required schema-v1 type token.
- `default` — required for non-reference types.

Numeric properties may additionally declare `min`, `max`, and `step`. `step` must be greater than zero and defaults must lie inside the declared range.

## Schema-v1 property types

The exact supported type tokens are:

| Metadata token | S2/S3 type | Default form |
| --- | --- | --- |
| `boolean` | Boolean | Lua boolean |
| `integer` | Integer | Lua integer |
| `float` | Float | finite Lua number |
| `string` | String | Lua string |
| `colour` | Colour | `{r=0..1,g=0..1,b=0..1,a=0..1}` |
| `vector2` | Vector2 | `{x=...,y=...}` |
| `vector3` | Vector3 | `{x=...,y=...,z=...}` |
| `entity` | Entity ref | no source-authored ID default |
| `asset` | Asset ref | no source-authored ID default |
| `animation` | Animation | no source-authored ID default |
| `audio` | Audio | no source-authored ID default |
| `enum` | Enum | string matching one option |

Reference-backed properties intentionally begin unresolved. Lua source must never embed raw Wicked/ECS/Jolt IDs or fake persistent IDs as metadata defaults. S4D will provide creator-facing reference pickers and store the opaque stable references in the S2 document.

## Enum options

Enums require an ordered `options` array. An option can be a string shorthand:

```lua
options = { "Idle", "Alert", "Combat" }
```

or a value/label table:

```lua
options = {
    { value = "idle", label = "Idle" },
    { value = "alert", label = "Alert" },
}
```

Option values must be unique. The enum default must match one declared option value.

## Metadata execution boundary

`ScriptMetadataService` creates a new Lua state for each evaluation. It is independent of:

- the S3 governed gameplay state,
- per-ScriptInstance gameplay environments,
- Wicked's global Lua VM.

The metadata state has its own memory and instruction budgets. It opens only the selected base/table/string/math/utf8 surface and removes filesystem, package, OS, debug, coroutine, dynamic loading, garbage-collection control, and `require()` entry points.

Lua source is compiled in text-only mode and must already pass S2's governed project-source containment checks.

This is intentionally not a general compile-time scripting environment. Metadata should be declarative and cheap.

## Diagnostics

Evaluation returns structured diagnostics with:

- severity,
- stable diagnostic code,
- source path,
- metadata field path,
- message,
- optional source line slot reserved for richer source navigation later.

Schema errors do not partially produce an accepted descriptor. `succeeded` is true only after a metadata declaration is captured with no error diagnostics.

## Applying defaults to S2 attachments

`ApplyScriptMetadataDefaults` bridges the S4A descriptor into the S2 persistent model:

- metadata role must match attachment presentation,
- `GLOBAL SCRIPT` requires Level scope,
- `ACTION` and `SCRIPT` require Entity scope,
- existing persisted property values remain authoritative,
- missing declared properties are seeded from metadata defaults,
- reference-backed properties are seeded unresolved rather than with raw IDs,
- persisted type mismatches fail closed.

The project `.wiscene.rscripts` companion remains the authoritative instance/property store. Metadata never replaces S2 persistence.

## Explicit non-goals

S4A does not add:

- ACTION/SCRIPT Inspector sections,
- script attachment/removal/reordering UI,
- property controls,
- reference pickers,
- GLOBAL SCRIPT Inspector UI,
- gameplay events or event queues,
- collision/interaction/damage APIs,
- diagnostics IPC,
- Library adoption/package closure,
- the built-in Lua editor.

Those remain later S4/S5/S6 gates.

## Acceptance

S4A is accepted when CI proves:

1. valid metadata captures deterministic creator information without executing the gameplay body;
2. all 12 frozen property types validate;
3. ACTION/SCRIPT/GLOBAL SCRIPT vocabulary is enforced exactly;
4. ordered properties and enum options are retained;
5. invalid defaults/ranges/types/duplicates fail with structured diagnostics;
6. reference defaults cannot smuggle raw IDs into creator data;
7. metadata evaluation has independent memory/instruction budgets and no unsafe standard libraries;
8. syntax errors, missing declarations, sandbox escapes, runaway code and source-path escapes fail closed;
9. S2 persisted values override metadata defaults rather than being silently overwritten;
10. no Studio UI is introduced by S4A.
