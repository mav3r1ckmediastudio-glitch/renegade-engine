# Goblin Cave editor performance and overlay correction — 6 November 2026

Compiled source: afa5a4c665a255b6db98db4cf1bb8c19e44c004d.
Base owner preview: 63d2dd4641378f5ef6e649a10f2bbf327427e6ce.
Wicked pin: 82557d439a41d104f559b0d3134d160b7bf6154e, unchanged.

## Problem and change

SceneService::ContainsEntity allocated a set and enumerated the entire scene for
each point query. Marker rendering, marker picking and other editor paths repeated
that operation for lights, particles and cameras. It now queries the registered
native component managers directly and retains no cache. Component-only entities,
direct native mutations, deletion, merging and clearing therefore remain current.

MarkerIconOverlay and the Player Start capsule GUI pass projected through the
render camera's temporally jittered view-projection after temporal resolve.
EditorOverlayViewProjection cancels that clip-space offset for icon drawing,
icon hit testing and capsule edges. Scene TAA and the pre-TAA geometry passes are
unchanged. This does not alter authored entity transforms.

## Validation

- Isolated Release Studio and Runtime builds passed.
- Focused RenegadeBridgeTests passed, 18.71 seconds. Added sparse membership,
  direct mutation, merge, clear, replacement and temporal sample-pattern checks.
- Actual installed Studio endpoint confirms compiled source afa5a4c.
- Same saved Goblin Cave and authored camera, same window/workspace: original
  editor HUD 21 FPS, patched HUD 81 FPS; a repeat capture read 77 FPS.
  A later unchanged reference-view capture read 82 FPS. These are native HUD
  observations, not a long benchmark or a guarantee across all views.
- Clicked a native point-light icon and verified its selected entity through the
  diagnostic endpoint. Stationary overlay sample masks were stable; owner motion
  acceptance of icons and capsule remains pending.
- Owner deleted lights were saved before testing. UI shows 40 remaining lights.
  Scene SHA256 still matches that saved owner checkpoint after the binary swap.
  No geometry, lighting, particles, material or render-setting quality changes during the membership-only comparison.
- Studio Play launched the original proven Runtime with casting arms visible.
  Runtime floor/torch view read 75 FPS and exited cleanly. This is NOT a matched
  proof of improvement over the owner's reported 65 FPS main-structure view.
- The newly compiled Runtime is NOT installed: its startup/FPS capture was
  inconclusive, so the original 63d2dd4 Runtime executables were restored.
  Installed Studio and Runtime revisions are recorded separately in the private
  package manifest. Do not replace Runtime from these artifacts without testing.

## Build evidence and limits

Artifacts/goblin-visibility contains build commands, logs, focused CTest log and
prebuilt-dependency receipt. The local Ninja build reused the existing Release
libraries and embedded shader header from the same accepted Wicked pin. Generated
Ninja overrides avoided rebuilding its shader dump and the bridge test's unrelated
umbrella dependencies. These are local build artifacts, not product build changes.
Full Debug/Release CI and independent exact-tree acceptance were not run.

Original preview binaries are preserved in the package's Before_Goblin_Visibility
checkpoint. Owner level backup is in the project's OWNER_LIGHT_DELETIONS checkpoint.
The original preview source/cloud experiment worktree was not edited.

## Native rock LOD follow-up

The original GLTF totals approximately 53,389,629 triangles across all source
instances. The owner's current 5,494 objects total 53,306,769 at LOD0.

Seven repeated static rock meshes now have four native detail levels. Original
vertices, LOD0 indices, materials and collision detail remain intact. Lower levels
use meshoptimizer's shape-preserving simplification with locked borders and
relative error limits of 0.001, 0.002 and 0.003. Source-level Unreal LOD counts
remain unknown: the headless query returned -1 and was inconclusive.

Calculated all-instance geometry at the authored reference camera falls from
53,306,769 to32,033,349 triangles. These are NOT visibility-filtered GPU draw
counts. Actual same reference-view editor HUD after the LOD change read130 FPS.
The scene was cold loaded and visually inspected with no obvious appearance
failure. All87 texture references resolve, all421 giant rocks remain textured,
and the native collision walk test passed7.09441 metres.

Actual Studio Play used the proven63d2dd4 Runtime. After activating mouse capture,
two views toward the cave/machinery read75 FPS with casting arms visible.
Foregrounding Studio confirmed active Runtime ready=true; closing Runtime then
reported a clean exit and returned to Studio. This does not establish an all-view
minimum or reproduce the owner's exact earlier65 FPS viewpoint.

Owner's40 lights,15 shadow casters and current22 particle emitters were retained.
Current effects are10 flame and12 smoke emitters after owner edits; do not restore
the old16+16 set without reviewing those owner changes. All placed objects and
authored player settings remain intact. Backup before LODs and RockLOD_Report.json
are retained in the project SourceAssets. Full native LOD0 geometry remains the
collision source. The engine fix itself remains compiled source afa5a4c.

Owner acceptance, wider gameplay sampling, independent review and full CI remain
open. No main merge, alpha distribution or release-gate closure.
