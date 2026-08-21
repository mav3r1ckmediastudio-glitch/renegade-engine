# Story Flow Gate 3D — Route condition authoring

Gate 3D completes the remaining semantic route field from the locked Gate 3 Graph-authoring scope: native authoring of Flow route conditions on the dedicated Story Flow render path.

## Contract

- Conditions remain part of the authoritative LP02 `FlowRoute`; there is no Graph-only condition format.
- The editor supports all existing runtime operators: `equals`, `not_equals`, `exists`, and `missing`.
- `equals` and `not_equals` require a value. `exists` and `missing` carry no value.
- Condition keys continue to obey the existing Flow symbol rules and are validated by `ValidateFlowDocument`.
- Multiple conditions on one route remain an AND-set exactly as consumed by the existing Flow interpreter.
- Adding, editing, and deleting a condition calls `StoryFlowAuthoringSession::UpdateRoute`; the UI never mutates the session document in place.
- A candidate that would make the Flow invalid is rejected before it enters Flow history.
- Condition edits therefore participate in the same Gate 3 dirty state, Undo/Redo, and transactional Save boundary as node and route edits.

## Native Graph UX

When a route is selected, a native `ROUTE CONDITIONS` panel is hosted by the dedicated Story Flow `RenderPath2D`.

The panel provides:

- previous/next navigation across every committed condition on the selected route;
- a non-semantic `+ CONDITION` draft state so clicking Add does not alter runtime behaviour before Apply;
- condition key editing;
- operator selection for Equals / Not Equals / Exists / Missing;
- value editing only for operators that require a value;
- Apply Condition through the validated authoring session;
- Delete Condition, or cancellation when the current condition is only an uncommitted draft;
- synchronization after Story Flow Undo/Redo so stale condition input is never silently replayed.

The panel is deliberately placed in the lower Inspector region and overlays only diagnostic text, not the existing route/node action controls. Graph pan/zoom/node positions remain presentation-only and unchanged.

## Boundary

This closes route-condition editing for Gate 3. It does not introduce Level creation, Screen creation, Journey View, or state-variable authoring. The condition editor references runtime state keys already supported by the Flow interpreter; a later variables/state authoring experience may provide richer discovery without changing this Flow contract.

The next Gate 3 step is integrated stack reconciliation, exact-head authoritative Debug/Release validation, and owner acceptance of the dedicated Story Flow render path plus Graph authoring surface.
