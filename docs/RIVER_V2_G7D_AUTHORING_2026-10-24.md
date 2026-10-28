# River V2 G7D authoring and transport foundation — 24 October 2026

Status: implemented and self-tested on DESIGN; production Studio/Runtime connected rendering is unfinished. No new owner-facing candidate or release gate is certified. Owner accepted G7C appearance/amount previously and authorized continuing creator integration. Mountain streams remain paused. No commit, push or merge.

Integration HEAD `11bbb90e4214776d5e51c8bc4cba2e7f1ccde296`; Wicked pin `3a800b7134aafe58461093c8abb2e274d4e64033`, including its preexisting private modifications, unchanged. All 1601 preexisting source files were hashed and backed up before implementation. Evidence and exact executed command scripts live in the original river worktree `BUILD/river-v2-G7D-20261024-authoring`.

## Implemented

- `RiverFlowSpanService` clips the validated G1 native bank mesh at native parameter intervals. It interpolates real native mesh bank edges, retains interior mesh rows, and never inserts or physically splits authored controls. `RiverFlowNetworkService` accepts disjoint logical spans from one native river, refusing overlapping spans.
- `RiverWaterwayService` saves `renegade.river.connection.v1` on the tributary: version, receiver UUID, receiving-control UUID. No raw ECS IDs or GPU data are serialized. Interior receiver attachment yields upstream main, incoming tributary and downstream main logical reaches. Direction/reversal, finite speed, flat height, source ownership, topology and snap bounds validate before acceptance.
- Guarded explicit connect/disconnect commands support exact Undo/Redo and atomic rollback. Explicit disconnect can recover malformed connection metadata and Undo restores its exact original text. Creator-safe factories wrap connection mutations with the existing linked-terrain transaction.
- `SetWatercourseTransformCommand` now wraps related native smooth edits in the connected-edit command. Moving the receiving control updates incoming downstream ends to the actual native anchor. Dragging a connected tributary endpoint promotes its delta to the receiver's smooth node edit. One command captures all affected native poses; existing outer terrain transaction recomposes linked beds once. Unconnected command behavior is retained.
- Stale Undo/Redo guards compare exact transform and spline settings, parent matrices, control identity/order, profiles and the complete connection-carrier set. Float width comparisons use binary values, avoiding decimal-rounding holes. Restoration resolves all target transforms before publishing any pose. Invalid/non-flat edits roll back.
- New private bridge `RiverWaterwayTransport.h` adopts the accepted G7C conservative projection/transport algorithm without a Tests dependency or literal west/east/outlet labels. Native inlet identity comes from validated topology. The existing waterline classifier becomes a template so both native single-river fields and the immutable junction adapter use the same code. Aggregate flow/source/100-step foam are exactly equal to the accepted proof, including UUID-addressed inputs.

Transport remains CPU domain preparation only. Renderer cache, GPU dispatch and transient union bindings are not yet connected to this service. No accepted shaders or packaged Studio/Runtime executables were replaced.

## Verification

All commands below are recorded verbatim as scripts in the evidence directory. Builds reuse integration `BUILD/g6`, Windows x64 VS18 Release, parallelism 2. They do not make another complete build tree.

| Executed script | Result |
| --- | --- |
| `build-attempt-10.ps1` | Build exit 0; flow field/network/junction surface/waterway CTests 4/4, 0.88 s |
| direct `Tests/Release/RenegadeRiverWaterwayTests.exe` | 32,204 checks pass; exact topology/pose Undo/Redo; 32,000 native-span footprint samples |
| `native-v7.ps1` | Actual DX12 native Scene Update create exit 0, fresh-process archive reopen exit 0; zero native shader errors |
| `regression.ps1` | Rebuilt affected spline/profile/underwater/foam/source tests; 6/6 pass, 7.25 s |
| `terrain-regression.ps1` | Rebuilt terrain suite; 1/1 pass, 0.24 s |
| `build-transport.ps1` | Release bridge and adopted domain build exit 0; transport/profile/oracle/adoption tests 4/4, 7.28 s |

There are 12 distinct affected CPU CTests across these focused runs; this is not a claim that the entire 253-test registry was rerun.

Native oracle compares derived clipped bank rows with Wicked's actual generated vertex buffers, including a reversed authored curve under translated/yaw-rotated/nonuniformly scaled parent. Connection leaves the receiver mesh positions, indices and control list byte-identical. Connected edit, native Scene Update, Undo and Redo pass. Maximum bank error is 1.90735e-6 m; all 44,800 sampled actual native-triangle footprint comparisons pass (1,533 wet). Native save and fresh-process reopen preserve deterministic network hash 1937257622045226968 despite changed ECS entity indices. Hashes differ across independently created fixtures because their persistent UUIDs differ; equality is required within each saved fixture.

Transport adoption: 11,423 wet cells; exact classifier, projected faces, velocity and aggregate source parity; exact 100-step density/age/material-coordinate state parity; exact parity after replacing proof labels with UUIDs and reversing input order. Inflow/outflow both 8.24617 in the unit-depth model. Existing oracle divergence 9.38307e-8, projection residual 9.55068e-9. Unsupported topology is refused before junction indexing. This preserves the accepted kinematic approximation; it is not a general hydraulic solver.

## Failures found and corrected

The initial build failed with disk-full LNK1180. An unnecessary CPU-test json.hpp include then failed compilation and was removed. The first native test genuinely failed bank parity: endpoint local transforms had changed without invalidating native spline mesh caches. Corrected by routing publication through the existing native transform command path; the oracle tolerance was not relaxed. Exact/no-op restoration also needed the publisher's exact restoration path rather than Execute's no-op threshold. Connected edit Undo initially failed on unchanged poses and was corrected. Earlier failure logs are retained.

## Owner-authorized disk cleanup

Read-only audit identified about 38.9 GiB of currently retained repository files created within its 25-hour window, not a proven 100 GB historical disk delta. Most river growth was repeated captures, native scene snapshots, compiler products and packaged dependencies. Audit timestamps cannot reconstruct total historical free space.

Reversible NTFS compression of 458 older owned generated files previously recovered 4,504,498,176 bytes. On the owner's subsequent cleanup authorization, `cleanup-exact.py` removed 83 byte-identical generated core-asset/compiler copies from obsolete intermediate G5/G7C proof packages. Each deletion was first compared by SHA256 against retained accepted G7C files. Cleanup recovered another 2,176,712,704 bytes with zero errors. Combined measured recovery: 6,681,210,880 bytes (6.22 GiB); this is recovery at the operation times, not current free space, which concurrent work can change.

`cleanup-manifest.json` preserves every removed path, retained restore source, byte size and SHA256. `cleanup-result.json` records actual free-space delta. Retired package markers identify the obsolete packages. Their original EXEs, unique assets/shaders, logs and all captures remain; accepted/final packages, projects, source, Git, other chats and the current build cache are preserved. Do not launch retired intermediate packages expecting their removed dependencies. Use the accepted G7C Desktop launcher.

## Open work and limits

- The surface/transport integration is bounded to one flat two-inlet/one-outlet confluence. Metadata graph capacity is broader than the current renderer; do not expose unsupported topology through Studio.
- Implement scoped transient union mesh/material/visibility bindings and a cached shared GPU domain; restore original native scene state outside native update/render scopes so authoring/save see originals. Preserve reflections, refraction, lights, normal maps and underwater behavior.
- Wire explicit creator connect/disconnect controls through bridge commands. Preview, width/profile/reversal and topology edits still need complete connected-network handling and validation before UI exposure. The committed move path is implemented; joined live drag preview is not certified.
- Preserve the owner-accepted lit foam and add a persistent amount control without reviving rejected overlays, collars or noise substitutes. The accepted G7C amount remains session-only.
- Build a new isolated combined Studio/Runtime candidate; verify actual visual editing, Undo, save/reopen, TestGame, exported/relocated Runtime, shader closure and performance. Current native oracle verifies source geometry and persistence, not final integrated appearance.
- Wider connected components, multiple junctions, height transitions, dynamic/deforming obstacles and general hydraulics remain outside this bounded proof. G7C's controlled gradient-sky fixture does not resolve its earlier atmospheric LUT startup issue.

Preservation audit JSON records unchanged branch/HEAD, exact dirty Wicked diff, expected source-only changes and accepted G6/G7C binary hashes. No release gate may be marked complete without independent/owner verification of the exact candidate.
