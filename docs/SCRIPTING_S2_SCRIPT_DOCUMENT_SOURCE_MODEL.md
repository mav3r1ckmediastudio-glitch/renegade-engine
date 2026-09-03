# S2 — Script Document & Source Model

## Status

Implementation gate for the Renegade scripting redesign. S2 defines persistent
creator scripting state and source governance only. It does **not** execute Lua,
create a Lua VM, expose `renegade.*`, dispatch gameplay events, or add the final
Inspector UI. Those begin in later gates.

## Baseline

S2 starts from merged `main` after PR #130:

`e767b6f8dd177ea5256ffcafb68a993d0ed22e94`

The discarded Phase 6 Gate 4 prototype PR #126 is not a persistence authority.
S2 deliberately does not use Wicked `ScriptComponent`, carrier entities, raw ECS
IDs, or Wicked's broad Lua VM as the creator scripting model.

## Scene scripting companion

Each authored Scene may have one adjacent Renegade scripting companion:

```text
Content/Scenes/Castle.wiscene
Content/Scenes/Castle.wiscene.rmeta
Content/Scenes/Castle.wiscene.rscripts
```

`.wiscene.rmeta` keeps its existing ADR 0004 identity/provenance responsibility.
Creator gameplay semantics are **not** added to `.rmeta`.

`.wiscene.rscripts` is a Renegade-owned transactional document. It has its own
normal `DocumentEnvelope` and is explicitly bound to the owning Scene's stable
document ID. Its document type is `scene-scripts`; its script schema identifier
is `renegade-script-document` version 1.

Entity attachments and Level globals live in this same companion. There is no
parallel entity carrier or second Scene serialization model.

## Attachment identity and scopes

Every attachment owns a stable `ScriptInstanceId`. Save/reopen preserves that
ID. Duplicating an attachment or duplicating an entity creates new
`ScriptInstanceId` values while retaining the governed `sourceId` so many
instances can intentionally share one source.

Persisted scopes are:

- `Entity`
- `Level`
- `Game`

`Game` is reserved in schema v1 but rejected by validation because its semantics
are intentionally deferred. This reserves the format token without silently
inventing Game-wide lifetime rules in S2.

The creator-facing presentation roles remain exactly:

- **ACTION** — entity scoped
- **SCRIPT** — entity scoped
- **GLOBAL SCRIPT** — Level scoped

A GLOBAL SCRIPT has no owner entity. ACTION and SCRIPT attachments require a
valid Renegade persistent entity ID, never a Wicked/ECS numeric ID.

## Source authority

Each attachment persists:

- `sourceId`
- project-relative `sourcePath`
- presentation role
- `apiVersion`
- Advanced/Unsafe opt-in bit
- provenance
- governed dependencies
- declared capability tokens

A single `sourceId` must map to one source path and one source authority within
the document. Conversely, one source path may not silently acquire multiple
source IDs. Multiple attachments may share that source authority while retaining
independent enabled state, order, property state and `ScriptInstanceId`.

S2 source validation requires creator Lua to remain under
`Content/Scripts/*.lua`, rejects traversal and symlink paths, requires a regular
file inside the canonical project root, and limits an authoring source to 1 MiB
by default. S2 does not compile or execute the file. Lua 5.4 syntax validation
moves into S3 with the governed Lua Runtime.

## Provenance and dependencies

Provenance distinguishes:

- project-authored source;
- installed Renegade Library adoption; and
- reserved personal-library adoption.

Library-derived provenance persists library ID, library version and content
hash. This is the S2 persistence basis for the later S6 rule that first use
adopts a complete governed transitive closure into the project without silently
overwriting a modified project copy.

Dependencies are typed as either script modules or governed assets. They persist
stable ID, path hint and optional/required state. Arbitrary dynamic dependency
resolution is not introduced by S2.

## Typed instance properties

S2 persists the complete agreed property type set so S4 can build the Inspector
without changing the document schema:

- Boolean
- Integer
- Float
- String
- Colour
- Vector2
- Vector3
- Entity ref
- Asset ref
- Animation
- Audio
- Enum

Entity/asset-backed references persist stable IDs plus optional path hints. A
valid EntityRef is retained even when the target is currently unresolved in the
Scene; S2 does not delete or rewrite it. Runtime liveness/generation validation
belongs to S3/S5.

Arbitrary user text and path-bearing known fields are hex encoded inside the
Wicked config-backed document so `#`, `;`, leading/trailing spaces and similar
config delimiters round-trip exactly.

## Validation and forward compatibility

Reads are transactional: a candidate document is completely parsed and
validated before replacing the caller's previously valid document.

Validation rejects:

- malformed or duplicate `ScriptInstanceId` values;
- malformed source IDs;
- source ID/path authority conflicts;
- invalid scope/presentation combinations;
- schema-v1 Game scope;
- sparse or duplicate attachment order within one owner scope;
- invalid or duplicate property names;
- invalid typed property payloads;
- duplicate dependencies/capabilities; and
- companion/Scene identity mismatches.

Unknown root fields, envelope-section fields, script-document fields, attachment
fields and unknown sections are retained where they do not collide with v1
authority. This gives later schemas a safe preservation seam while keeping the
current writer authoritative for fields it understands.

## Transaction and Undo/Redo boundaries

Persistent writes use the existing `WriteTransactionalDocument` / LF02 project
document transaction boundary, including staged validation, backup and rollback.

S2 also exposes UI-independent edit primitives for:

- attach;
- detach;
- enable/disable;
- reorder;
- source replacement;
- property set/remove;
- attachment duplication; and
- entity attachment duplication.

Command factories wrap the creator edits in the existing `CommandService`, so
S4 Inspector work can obtain Undo/Redo without adding scripting state directly
to `StudioApplication.cpp` or inventing a second history stack.

## Explicit non-goals

S2 does not add:

- a Lua state;
- script execution;
- Wicked `ScriptComponent` creator authority;
- `native_id` or raw ECS/Jolt handles;
- lifecycle callbacks;
- the bounded event queue;
- Runtime diagnostics IPC;
- metadata execution/parsing;
- Inspector script sections;
- Library adoption/copying;
- package dependency closure; or
- the stock Action pack.

Those remain S3 through S7 according to the accepted scripting architecture.

## Acceptance

S2 is accepted only when the exact PR head passes both Windows baseline jobs and
both Renegade Studio jobs. Its automated tests must prove:

1. ACTION/SCRIPT/GLOBAL SCRIPT scope rules;
2. stable save/reopen `ScriptInstanceId`;
3. fresh IDs on attachment/entity duplication;
4. shared-source identity;
5. all twelve property types;
6. unresolved EntityRef retention;
7. source containment/traversal/size governance;
8. source ID/path conflict rejection;
9. Game-scope reservation;
10. CommandService Undo/Redo;
11. transactional load behaviour; and
12. forward unknown-field/section preservation.
