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
  No geometry, lighting, particles, material or render-setting quality changes.
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

## Remaining performance work

The source GLTF contains approximately 53,389,629 triangles summed across all
instances. This is potential source geometry, NOT a measured visible draw count.
The converted mesh path currently provides only the base detail level. Investigate
native LODs and GPU shadow cost without removing the owner's remaining lights or
reducing the accepted close-up appearance. A headless Unreal LOD query returned -1
and is inconclusive; do not claim the pack's original LOD count is established.

No main merge, alpha distribution or release-gate closure.
