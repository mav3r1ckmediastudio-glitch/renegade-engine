> OWNER VISUAL REJECTION (2026-10-25): the lower join folds/protrudes and forms a hard line across the river. Earlier automated engineering results below remain historical evidence, not visual acceptance. G10C replaces the three independent joins with a continuous rounded surface. Owner requests automatic rounding, with no cascade-style/angle/curvature controls.

# G10B native mini-cascade proof — 2026-10-25

Status: consolidated engineering checks PASS; owner visual acceptance pending. Isolated Windows x64/DX12 proof only.

## Owner requirement and preservation

The owner likes G10A graded rivers and wants BOTH gradual grades/rapids and local steep drops, including consecutive nodes directly above one another. This explicitly authorizes the bounded mini-cascade experiment. It does not restart the paused mountain-stream terrain system or authorize a broad waterfall/Studio deployment.

G10A source, shaders and executable remain hash-identical. G9 production appearance and all earlier accepted builds remain untouched. No owner project, main, alpha, Wicked source or submodule pin is changed. No commit/push/merge.

## Implemented boundary

New EngineBridge/src/RiverCascadeFlow.h builds bounded 3D surface-distance, velocity, travel-time and sheet-thickness records from actual native ribbon mesh rows. Gravity adds kinetic energy; discharge determines thickness. The proof uses actual native spline frames with an authored diagonal up direction that remains nonparallel to horizontal and vertical flow. The default flat river factory retains its XZ-distance validation; the experimental fixture creates a separate native falling reach rather than weakening that existing contract.

A single logical test watercourse is represented by upstream, falling and receiving native WATER reaches. Their authoritative endpoints are synchronized at explicit structural edit boundaries. These are proof-owned native spline components, not foam overlay geometry. The fixture is not a finished stable-ID waterway graph or general Studio node editor. Named prototype roles are not a production serialization schema.

The upstream travel field uses actual native 3D geometry and energy; its derived bed follows that field. The receiving reach retains the G10A bounded mild-grade model, measured triangle depths, obstacle classification and conservative projected flux. The falling reach uses the receiving inlet's measured discharge, rather than the slightly different target profile discharge. Vertical energy is treated as dissipated into a calibrated local impact-foam source; it is not incorrectly converted wholesale into horizontal inlet velocity. Foam then uses the existing accepted lower-river GPU transport, lifetime, detail and original artwork.

Falling surface motion and original-art aeration follow native travel-time coordinates. They do not use the ambiguous world-XZ mapping across the drop. Temporal/spatial filtering limits fast-detail sparkle. A small bounded 12 mm ripple uses a refined derivative of the actual native falling mesh, with lip/toe/bank fade and conservative culling bounds; native source geometry is validated before refinement. Ordinary river WATER optics and accepted foam remain separate.

Native WATER lighting, normal sampling, reflection/refraction and transparency infrastructure are retained. Owned staged shader extensions add local-plane optics and falling-surface motion/aeration. The existing single planar reflection remains approximate across elevations. A vertical sheet is still shaded with the native water half-space optics; this does not solve finite-volume free-jet optics.

## Controls

Distinct Cascade proof, preserving the Graded Drop viewer.

- G cycles receiving-level drops 0.5 / 1.1 / 1.6 m, preserving the lower reach's mild grade.
- J cycles direct / tight / forward horizontal offsets; the direct case has exactly stacked endpoints.
- U/Y Undo/Redo affect actual lower native node transforms and rebuild the connected falling reach.
- N toggles native node spheres; 1–5 select camera views.
- Foam amount retains the accepted 0.3 default. V toggles bounded surface motion.
- Structural changes reset derived simulation history. Appearance-only tuning preserves it.

## Tests and evidence

CPU tests cover native evaluator frames at zero / 0.04 / 0.5 m horizontal runs and 0.5 / 1.1 / 1.6 m drops, world translation, finite downward velocity, gravity speed, cross-section discharge, travel UVs and malformed mesh refusal. GPU fixture tests actual Wicked-generated mesh/evaluator parity before derived refinement, direct and forward native offsets, edit/Undo/Redo geometry hashes, bulk CPU/GPU transport parity, source-only foam, history, expiry, zero flow and native restoration.

The native save boundary temporarily strips all custom IDs and descriptor-bearing userdata for the added upstream/falling materials, then restores live bindings. Fresh reload asserts native bindings before rebuilding derived data. Native control transforms and ordinary asset data persist; runtime descriptors must not.

Evidence directory on DESIGN:
C:\Users\paulw\source\repos\renegade-spline-water-20261020\BUILD\river-v2-G10B-20261025-cascade

build.py records exact Release configure/build/CPU commands and exit codes. stage.py records native PS/main VS/shadow VS compiler commands and exits; accepted bulk/detail CS and original artwork are copied unchanged. batch.py records exact consolidated regression, GPU run, fresh reopen and preservation commands in BATCH_COMMANDS.json. Per-run executable argument arrays, exit codes and durations are in *-exit.json; logs and captures retain intermediate engineering failures.

## Known limits and next work

This is a guided, thin native surface cascade with a calibrated energy/entrainment/impact source. It is not a ballistic free-jet solver, volumetric fluid simulation, hydraulic-jump solver, spray/mist simulation or arbitrary obstacle-aware falling-water solver. Endpoints stay attached; natural lip/jet/toe shapes and appearance need owner review. Impact footprint follows the actual falling toe into the receiving ribbon and is masked to active water cells.

Supported cascade envelope is at most 2 m height drop and bounded widths/discharge; the mild-grade receiver keeps its existing separate bounds. Multiple cascading drops, graded tributaries, arbitrary existing terrain conveyance, stable graph serialization, general node picking/dragging and Studio/Runtime/package adoption are not implemented here.

Structural CPU domain reconstruction remains synchronous and slow compared with steady rendering. Efficient invalidation/spatial acceleration or background rebuild is required before production editing. Small proof wall timings must not be advertised as whole-game FPS or GPU timestamps.

No proprietary Unreal content was copied. The previously verified native Blueprint/material concepts inform the authoring separation; no claim is made that its inspected graphs implement this gravity model.

## Engineering corrections

The flat factory correctly refused the initial close-XZ falling stroke. The separate native proof reach preserves the flat validator. An initial receiver exceeded the finite fixture bed; its extent was corrected. First imagery revealed isotropic falling foam; travel-aligned detail, filtering and bounded native-surface ripple replace that appearance. Measured receiver discharge replaces approximate target Q at the falling boundary. Added reach materials require their own transient-binding archive mask, beyond the existing main-river save guard.

Final results will be appended after consolidated validation and visual inspection.

## Falling-air boundary and numerical validation

Aerated foam carried by the falling reach now enters through the receiving domain's actual negative open boundary faces, in addition to the local energy-scaled impact source. This changes foam input, not the water-volume flux. The sum of the fed faces' measured volume flux is asserted equal to domain inflow within 1e-5. The dimensionless concentration scale (8 times the height-dependent entrainment response) is an explicit appearance calibration, not a measured air-volume fraction.

The denser inlet exposed an inappropriate shared absolute tolerance across density, age and mass-weighted coordinate channels. A diagnostic CPU reference dump and per-channel GPU comparison measured maximum errors [1.4782e-5, 4.09782e-7, 1.48773e-4, 2.81334e-5]; corresponding normalized errors [7.28216e-6, 4.09782e-7, 1.43169e-5, 7.45476e-6]; global density L1 relative error 2.12477e-6. The failing raw maximum was a source-coordinate moment, whose magnitude scales with foam concentration and world position.

The cascade-only oracle now requires absolute density error <2e-5, absolute age-moment error <2e-6, normalized coordinate-moment error <2e-5 per channel, and global density L1 relative error <5e-6. The original single-step bound remains. Existing accepted CPU/GPU transport code and earlier tests are unchanged. Diagnostic mode is not an acceptance pass; the full regression must satisfy these component-aware checks.

Native reaches currently carry their own local travel coordinates; visual wave phase can decorrelate across the sharp lip. A production connected-graph implementation should govern cumulative phase explicitly. The guided sheet remains attached to its nodes and does not solve ballistic detachment, hydraulic jumps or spray.

## Consolidated engineering result

17/17 river CPU regressions PASS; nine native evaluator cascade cases PASS. Full DX12 GPU proof and fresh native archive reopen exit 0. Actual generated falling mesh/evaluator parity is 0 in tested cases, including direct vertical and forward offsets. Native node Undo/Redo hashes, CPU/GPU bulk parity, surface/appearance history, expiry, zero flow and native restoration PASS. Shader builds PS/main VS/shadow VS exit 0. Added upstream/falling native archive bindings are clean before transient rebuild.

Full GPU run 164.12 s; fresh reopen 36.89 s. Owned-window J/G/U/Y control test and normal close exit 0; observed drop sequence [1.1, 1.6, 1.1, 1.6, 0.5, 0.5]. Exact commands/results/hashes: FINAL_RESULT.json and BATCH_COMMANDS.json in the G10B BUILD directory. No owner visual acceptance inferred.

Native 1.1 m case: matched measured Q=2.58234 m3/s, impact speed about5.14755 m/s, gravity power about27856.5 W; these are model/fixture quantities, not measured real-water calibration. Frame timing: ['STRAND_BENCH enabled=0 frameMS=1.00174 fps=998.259', 'STRAND_BENCH enabled=1 frameMS=1.01073 fps=989.386', 'STRAND_BENCH enabled=0 frameMS=0.958822 fps=1042.95', 'STRAND_BENCH enabled=1 frameMS=1.01166 fps=988.478', 'RAPIDS_FPS=1116.34']. Small isolated wall measurements, not whole-game or GPU timing guarantees. Synchronous structural rebuild remains an unresolved production UX/performance issue.

All G10A and G9 recorded source/candidate hashes, original Tests CMake prefixes, HEAD and private Wicked patch/pin comparisons PASS. No commit/push/merge. Separate Cascade desktop launcher prepared; Graded Drop remains intact. Visual assessment of the new cascade remains owner-gated; guided sheet and reflection/optics/spray/graph limitations above remain open.

Final running native 3D and HUD captures inspected: both upper/lower native control spheres visible; local graded depth active; foam amount0.3; incoming-air foam present at the receiving reach. Cascade is open on DESIGN, initial1.1m drop and tight0.04m run. Final owner launch PID7600 is recorded in live-launch.json. Full per-channel parity records are included in FINAL_RESULT.json. Modified existing task files pass git diff --check. Owner visual acceptance remains pending.
