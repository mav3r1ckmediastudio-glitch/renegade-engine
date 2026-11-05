# Riverbed locality and progress candidate — 5 November 2026

Base: 8fcc02a53fd0b2b392b4cbec7cb7c5e7bd2cdecc. Local uncommitted candidate;
not merged, released, or accepted against the owner's unsaved river.

## Problem and implementation

The owner's live Link Riverbed operation stopped the application heartbeat
for about 8.8 seconds, consuming 9.17 process CPU seconds with no file reads
or writes. This identifies CPU-bound editor blocking, not an exact function.

The default native fixture independently exposed full-snapshot Undo/Redo
rebuilding unchanged terrain. Initial linking changed three of 361 chunks.
ApplyTerrainSculpt now compares exact target heights before building its grid.
Only changed chunks rebuild mesh data, BVH, height textures and collision.
The normal/tangent lookup includes the changed chunks' one-ring neighbours
using the existing canonical global grid coordinates. Full layered-snapshot
topology/range validation and metadata restoration remain intact. Valid no-op
restoration succeeds without reconstructing resources; invalid nonresident
targets are not newly classified as applicable.

TerrainOperationProgress is optional, scoped, thread-local bridge telemetry.
Callbacks cannot change transaction success. Reports describe real preparation,
validation, height calculation, terrain updates and collision reconstruction.

Studio's Link Riverbed handler installs a bottom-centre Windows progress
overlay. A separate UI message loop paints actual stages and chunk counts
while the existing scene-safe thread performs atomic live scene/GPU changes.
Editing is disabled during this operation and restored on scope exit, including
failure. This does NOT move scene mutation to a worker, provide cancellation,
or make the whole editor render loop asynchronous. Unknown-total stages display
an indeterminate indicator rather than invented percentage completion.

## Validation

- Instrumented Release A/B, same default native fixture and terrain tracing:
  initial Link Riverbed 603 ms before, 160 ms after.
- Instrumented history chunk rebuilds: 8.9–10.7 seconds before,
  63–148 ms after across the observed restore operations.
- Final noninstrumented Release native proof additionally asserts:
  restoring identical heights rebuilds zero chunks; Undo rebuilds exactly the
  three changed chunks. It passes existing height/history, masks, external-edit
  rejection, source GPU readback and cold-reopen checks.
- Final expanded-first and linked-first maximum-terrain fixture runs exercise
  1,089 chunks, three rivers/99 sparse patches, existing fail-closed budgets,
  expansion/sculpt/history, rocky/grass paint, collision and cold hydration.
  See the external final-results receipt for final exit codes.
- Combined Studio preview links successfully using the changed application
  object and changed bridge translation units against the baseline libraries.
  This is a targeted incremental engineering build, not a full clean CI build.
- Progress-window standalone check passes painting at 20/100 and 53/100
  while the owning test thread sleeps, plus editing lock and input restoration.

## Outstanding acceptance

The controlled fixture is not the owner's unsaved 8.8-second river. Repeat the
actual Studio Link Riverbed operation on a saved disposable copy with this
preview before claiming that specific freeze fixed. Integrated Studio visual
acceptance, a clean build, Runtime/package checks and independent review remain
open. Preserve the running owner session and existing alpha. No CI, merge or
external distribution is authorized by this checkpoint.


## Owner rejection and progress-lifecycle revision — 5 November 2026

The owner reports over a minute on real Link Riverbed in preview 1; the bar never appeared. The preview hash was verified. The application recovered and its unsaved scene was preserved. Preview 1 is rejected regardless of native test passes. No profiler was recording that interval, so the exact blocking stack is not established. The progress class had a synchronous readiness wait and a UI-thread join that could block the engine Windows-message thread. Both are removed. A detached painter owns shared state; its callbacks and Windows loop never require Studio to wait. Window bounds are captured by the caller, and completion is signalled atomically. RENEGADE_CARVE_TRACE records operation entry, input lock, readiness, UI creation, actual terrain stages/chunk counts and return. The Studio preview 2 link passes; standalone live progress and input lock/restoration pass. The backend objects are unchanged from the final native regression passes above. The actual Studio minute-long case remains UNVERIFIED and must not be called fixed. The separate TEST_RIVERBED_PREVIEW2 launcher enables the trace. Do not close or overwrite the owner session or replace the alpha executable. No merge, CI or external distribution.


## Preview 2 owner follow-up and focus regression — 5 November 2026

Owner reports progress worked, then apparent minute-long freeze. Actual preview2 trace operation-start 90634843, operation-returned 90636500, ui-window-closed 90636515: 1.657 seconds for the recorded operation, one changed chunk; no minute-long gap inside these stages. The post-operation minute is outside this trace. Two Studio instances were found (40116 and 32636); neither was closed or mutated by the investigation. Only the first instance owns the usual diagnostics endpoint. Focus regression reproduced independently: the progress scope re-enabled its owner but left GetFocus unequal to owner (exit 5). Studio handles WM_KILLFOCUS by setting is_window_active=false, and the engine pauses updates while inactive. Progress completion now restores activation/focus only if the editor initiated the foreground operation and the user has not explicitly switched away from the fallback foreground window. Regression now passes focus restoration and intentional-switch preservation (exit 0). Test source: Tests/RiverbedProgressFocusTests.cpp; this is an interactive Windows regression, not a headless CI claim. It is not yet established that focus loss explains every part of the owner minute-long pause. Preview 3 is a separately linked Studio candidate with unchanged backend objects, optional stage/focus logging and TEST_RIVERBED_PREVIEW3 launcher. Integrated owner acceptance remains pending. No main merge, CI or external distribution.


## Owner acceptance and merge preparation (5 November 2026)

The owner confirmed Preview3 carving/progress worked well, then confirmed the combined water-normal preview works a treat and authorized merging these fixes to main. This supersedes the earlier pending-owner/no-merge checkpoint restrictions above. Four-way exact-branch CI remains required before merge. No cloud optimisation is included.
