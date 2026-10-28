# River V2 G7A — flat waterway topology foundation
Date: 2026-10-24 (Europe/London).
Status: CPU foundation implemented; rendering and owner visual gate NOT implemented.
G6 repaired editable Studio/Runtime candidate was owner accepted: "it works beautifully".
No commits, pushes, merges, Wicked edits, owner-scene edits or candidate replacement.

## Bounded contract

The implementation authority labels G7 "NOT NOW" and lists multiple independently governed features. Owner subsequently requested continuation after accepting G6. This task takes only the first bounded part of connected flat waterways: explicit reach graph, endpoint flow semantics and native water-level join validation. Mountain streams, waterfalls, lake/ocean transitions, volumetric physics, rapids and renderer adoption remain deferred.

RiverFlowNetworkService is separate from Story Flow. BuildRiverFlowNetwork consumes a stable const scene, explicit caller-supplied stable reach identities, native spline entities and G1 flow settings, plus named junctions. Each junction joins 1..8 downstream reach ends into exactly one upstream reach end. There is no proximity inference, snapping or geometry mutation. Reversed authoring selects the correct physical end cap as well as the reversed velocity direction.

An immutable graph contains shared immutable CPU fields, actual native cap centres/banks/widths, flow-direction ports, canonical junctions and deterministic upstream-first ordering. Disconnected reaches are allowed explicitly. IDs and entities must be unique; port reuse/splits, self joins, cycles, missing references, degenerate/nonfinite/unsupported native fields, stopped flow at a join, opposed/perpendicular outlet flow and coincident parallel incoming directions fail closed. Previous output remains untouched on failure.

Limits checked before input copies: 1..16 reaches; <=15 junctions; <=8 incoming each; IDs <=128 ASCII letters/digits or -_.:. Existing G1 limits apply to each reach. All native bank rows must lie within 5 mm of the reach's first native cap centre height. Every pair of joining native cap centres must lie within 20 mm horizontal / 5 mm vertical. Incoming/outgoing tangent dot must exceed .05; incoming directions with dot >=.9999 are refused as ambiguous. These conservative numeric policies are G7A validation limits, not physical constants. They never alter authored geometry or velocities.

The join anchor is the outlet's actual upstream cap centre, not an averaged/silently snapped point. Hashes use canonical sorted IDs and G1 deterministic field hashes, excluding ECS entity numbers, pointers, padding and input order. Hash contract is a transient Windows x64 rebuild identity, not a persisted file format.

## Exact source changes

New:
- EngineBridge/include/renegade/bridge/RiverFlowNetworkService.h
- EngineBridge/src/RiverFlowNetworkService.cpp
- Tests/RiverFlowNetworkTests.cpp
- Tests/RiverFlowNetworkNativeProof.cpp

Target registrations only in EngineBridge/CMakeLists.txt and Tests/CMakeLists.txt.
Documentation updates: HANDOFF.md, docs/ARCHITECTURE.md, docs/FEATURE_MATRIX.csv, this report, G6 acceptance notices in the G6 reports; navigation-only prefix in the original river HANDOFF.md.
Existing G1 field, G6 renderer, Studio/Runtime call sites, HLSL and native sources are unchanged.

## Commands and results on DESIGN

Integration worktree: renegade-river-v2-integration-20261023.
HEAD 11bbb90e4214776d5e51c8bc4cba2e7f1ccde296, branch feature/river-v2-g6-20261023, plus preserved uncommitted source.
Wicked pin 3a800b7134aafe58461093c8abb2e274d4e64033.
New evidence root: BUILD/river-v2-G7A-20261024-0018 in original river worktree.
Exact absolute commands, executable paths and working directories are retained in the evidence scripts. Existing BUILD/g6 CMake cache reused to avoid duplicating native build artefacts on a nearly full drive. Accepted desktop candidate binaries are not rebuilt or overwritten.

1. `powershell -NoProfile -ExecutionPolicy Bypass -File <evidence>/build-g7a.ps1`
   Runs VS18 CMake configure, then `cmake --build BUILD/g6 --config Release --target RenegadeRiverFlowNetworkTests RenegadeRiverFlowNetworkNativeProof --parallel 2`.
   EXIT0, build.log/configure.log. Initial, strengthened and final bounded-input builds all passed.
2. `powershell -NoProfile -ExecutionPolicy Bypass -File <evidence>/test-g7a.ps1`
   Actual new Release executable; EXIT0, 135 checks. Tests confluence, canonical reorder/repeat hash, disconnected reaches, malformed IDs, duplicate identities/entities/ports, budgets, missing references, cycle, gap/height/opposed/zero/parallel refusal, native field nonfinite refusal, reversed end semantics, common transformed parent, multi-junction chain, full16-reach/15-join chain, eight-way fan-in and sub-tolerance joins without snapping.
3. `powershell -NoProfile -ExecutionPolicy Bypass -File <evidence>/native-g7a-v2.ps1`
   Actual hidden DX12 Application/RenderPath3D generates curved native WATER ribbons for two tributaries and one outlet. One tributary is authored backwards with reversed flow; all share a translated, yaw-rotated, nonuniformly scaled parent. Verify actual Scene::Update mesh vertex end caps/width/tangents against graph ports, not an independent curve. Maximum cap/bank error 3.8147e-06 metres; minimum tangent dot .999997. A real control edit regenerates native geometry and graph correctly refuses the gap while preserving prior output.
   Native create EXIT0; separate fresh process reopens generated WISCENE and repeats parity/hash verification EXIT0. Hash12911869218294088112. Logs native-create-v2.log and native-reopen-v2.log. Native shader errors zero. Original straight native proof also passed both processes and remains in v1 logs.
4. `powershell -NoProfile -ExecutionPolicy Bypass -File <evidence>/ctest-g7a.ps1`
   Existing river/bridge/importer/snapshot/runtime/source-contract selection plus the new graph CTest: final85/85 PASS, EXIT0 (ctest.log). Selection regex is retained verbatim in script.
5. Baseline and final preservation: baseline.py/baseline.json and final-audit.py/final-audit.json record source hashes, HEAD, dirty native patch, exact changed files and unchanged accepted candidate hashes. Final source whitespace check recorded separately.

## Honest acceptance boundary and next step

A topology-valid graph is NOT a watertight confluence. Native ribbons currently overlap near a shared endpoint; this service does not merge/trim them, resolve transparent overdraw, route a unified velocity atlas or carry foam density between reaches. It does not conserve hydraulic discharge: depth/cross-section is not an input and velocities remain authored. Endpoint-only attachments exclude joining to the interior of another river; the later authoring operation must explicitly split a reach first. No UI, metadata schema, migration, Undo command, persisted graph or production per-frame call site exists yet. Archive proof rebuilds the graph using supplied IDs after native scene load; it does not serialize graph settings.

Next independently bounded G7B: a disposable flat confluence geometry/velocity diagnostic, proving single WATER surface coverage without overlapping transparent caps, coherent directions across the join, and retained native optics. Foam transfer follows only after geometry/velocity validation; no overlay foam mesh or proprietary Unreal content. Production Studio/Runtime integration and owner visual acceptance are later gates. Accepted G6 remains the owner-facing candidate.
