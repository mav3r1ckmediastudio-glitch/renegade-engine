# G13E10 — selected river-node width Inspector and lateral gizmo correction (2026-10-27)

Branch: `feature/river-v2-g6-20261023`; no merge, push, commit or reset. Protect unrelated work and owner projects.

## Owner feedback
G13E9 waterfall and linked receiving river judged to work very well. Two remaining usability issues: Scale gizmo uses the wrong visual axis for width; owner couldn't find a numeric control.

## Confirmed cause
`RiverFlowStudioIntegration.cpp` already created `riverNodeWidth_` and RESET with CommandService undoable `SetWatercourseTransformCommand`. However `TerrainInspectorSections.cpp` omitted those widgets entirely when assembling selected river rows. Base Width existed only in the very long FLOW section, and when flow was disabled it was disabled/hidden. The native spline stores width in `scale_local.x`, independent of which WORLD visual gizmo handle actually points across the river.

## Isolated implementation
- `TerrainInspectorSections.cpp`: show `Base river width (m)` immediately under RIVER // SELECTED PROPERTIES. On blue river control-point selection, show `NODE WIDTH (m)` and `RESET SELECTED NODE WIDTH` immediately below. Both remain available even if spline flow is switched OFF. Eliminate duplicate base-width row in flow section. Existing start-whitewater-on/off and at-selected-node buttons are now present in FLOW & DIRECTION rather than invisible.
- `RiverFlowStudioIntegration.cpp`: keep global/base slider independent of hydraulic enable flag; show per-node label identifying which horizontal gizmo handle changes width and status hint. Maintain original range, commit/Undo/Redo and connected-tributary safety.
- `RiverNodeWidthGizmoAxis.h`: choose world lateral Scale Z when local river tangent is mostly X, otherwise world Scale X when river tangent is mostly Z. Handles bends via immediate adjacent control points. No node geometry/schema changes.
- `StudioApplication.cpp`: while dragging the selected lateral axis, map its scalar delta to persisted scale X, restore Y/Z/rotation/position so a vertical or along-channel drag cannot corrupt widths. Skip no-op history entries. Native linked-bank width transaction, Undo/Redo and scene persistence remain the existing command path.
- Approved G13C falling waterfall and foam shader unchanged, native G11 river transport unaffected.

## Build & evidence
Release `RenegadeStudio` build: `BUILD/waterfall-placeable/evidence/G13E10-WidthUX/build-final.exit=0`. Focused CTest: `ctest-river.exit=0`, 24/24 green. `git diff --check` exit 0 (existing CRLF notices).
New candidate exe in isolated river build directory:
`C:/Users/paulw/source/repos/renegade-spline-water-20261020/BUILD/river-v2-G11F-20261026-continuity/candidate/RenegadeStudioG13E10Width.exe`.
Launched on DESIGN, PID stored in `BUILD/waterfall-placeable/evidence/G13E10-WidthUX/launched-studio.pid`. Owner's earlier G13E9 Studio PID23224 was no longer running when new test build started.
Review project: `BUILD/waterfall-placeable/evidence/G13E9-StableSource/StudioReviewProject/Waterfall.renegade` (validated fresh-process G13E9 export/reopen). Do not write same project from two instances.

## Owner visual test remaining
Open the disposable review project in the newly launched Studio, enter Level One, select a blue control point near the waterfall. The top selected river properties should show two sliders: Base river width and Node width, plus Reset. On the mostly-X receiving river, Scale Z should widen the local stretch; on a river running Z use Scale X. Test slider -> Undo/Redo -> save/reopen, and ensure width/banks remain aligned. Confirm disabled/refusal for connected unsafe edits.
No remote mouse-driven owner-style inspector acceptance, full Test Game frame-time/VRAM benchmark, or packaged-alpha acceptance yet. Do not claim art/performance gate closure.

## Owner visual acceptance (DESIGN)
The owner explicitly confirmed that both the corrected width axis and node-width slider work. G13E10 width UX is **owner-accepted**. Leave this behaviour and the G13C artwork unchanged. This does not authorize branch merge or imply full Test Game performance/VRAM certification.
