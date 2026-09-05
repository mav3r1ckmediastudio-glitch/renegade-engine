# LP02 — Minimal Flow Interpreter

## Proof boundary

LP02 adds the first serialized Story Flow model and executes it through the real
project-aware Runtime. The minimum proof is:

`Game Start -> Level One -> Level Two -> Complete Game`

The transition contract uses named outcomes. Level Exit Zones, gameplay
triggers, screens, HUDs, loading presentation, save state, dependency cooking,
visual graph authoring and project-wide document transactions remain outside
LP02.

## Serialized contract

A Story Flow is a `renegade-document` v1 envelope whose document type is
`story-flow`. Runtime semantics are stored independently from any future canvas
layout:

- one permanent Game Start node;
- Level nodes with stable Scene document IDs and project-relative path hints;
- terminal Complete Game, Return to Main Menu and Quit nodes;
- routes with stable IDs, named outcomes, destination node IDs, destination
  Player Entry names, numeric priority and optional state conditions;
- conditions supporting equals, not-equals, exists and missing.

Routes never contain scene paths or hard-coded destination enums. The
interpreter selects the lowest numeric matching priority. More than one
matching route at that priority is an actionable ambiguity rather than an
arbitrary choice.

## Stable reference resolution

The `.renegade` manifest stores both `startup_flow_id` and `startup_flow`. The
ID is authoritative and the path is a hint. LP02 tries the hint and then falls
back to a deterministic project-local scan for the same document ID, proving
that moving or renaming the flow does not change its identity.

Level Scene references use the same rule. LP02 verifies each Scene ID against
an adjacent `.rmeta` `renegade-document` envelope. A stale flow path hint is
repaired by scanning project metadata for the stable ID and then using the
metadata envelope's current path hint. This linear scan is a bounded proof
seam; the later Asset Registry may replace it with an index without changing
serialized Story Flow references.

## Required diagnostics

LP02 fails explicitly for:

- malformed or duplicate node and route IDs;
- missing or multiple Game Start nodes;
- routes whose source or destination does not resolve;
- path/canvas data used as a route destination;
- invalid named outcomes or destination entries;
- duplicate equal-priority unconditional routes;
- missing matching routes at execution;
- multiple matching routes at the selected priority;
- cross-project, wrong-type, missing or duplicate document identities;
- Scene references that cannot be resolved inside the project.

## Evidence targets

`RenegadeFlowTests` covers document validation, serialization, stable-ID
rename/move repair, deterministic traversal, route priority and conditions,
missing and ambiguous route diagnostics, hard-coded destination rejection and
project ownership.

`RenegadeRuntimeFlowTests` creates real minimal WISCENEs, resolves a moved flow
and moved Scene by stable ID, executes the four-node proof through Runtime code,
and verifies structured missing-route, ambiguous-route and wrong-owner
failures plus the Runtime evidence log.

The packaged fixture is under `Runtime/fixtures/LP02`, with
`Runtime/package/Run-LP02-Flow-Proof.cmd` as the manual DX12 proof launcher.

## Verification status

Implementation prepared on `poc/lp02-minimal-flow-interpreter`. Windows Release
build, full CTest regression and packaged Runtime execution must pass before
LP02 is treated as complete.
