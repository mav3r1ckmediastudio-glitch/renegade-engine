# Story Flow Gate 5 — Integrated Closeout

## Locked purpose

Gate 5 gives first-class Story Flow Screen nodes a governed project-content
lifecycle. The visual Screen Editor is explicitly out of scope and remains Gate
8. Gate 5 ends at a stable open/handoff boundary that Gate 8 can consume without
re-resolving paths or inventing editor-only semantics.

## Implementation slices

### Gate 5A — governed Screen creation

- Draft PR #79, reconciled directly onto merged Gate 4 `main`.
- `StoryFlowScreenLifecycleService` creates a real `runtime-screen` document and
  its Flow Screen node as one `ProjectDocumentTransaction`.
- creator templates are scaffolds over one Screen document architecture:
  Custom, Title/Main Menu, Loading, Options, Pause, Failure/Death, Victory,
  Save/Load and Credits;
- Screen document stable identity is authoritative;
- rollback prevents orphan Screens and dangling Flow relationships;
- generated template actions are real Runtime Screen actions/outcomes.

### Gate 5B — stable Screen reference and outcomes

- Draft PR #81, stacked on Gate 5A.
- `StoryFlowScreenReferenceService` resolves Screen nodes by stable document ID;
- stored project-relative path is only a hint;
- moved Screens resolve with an explicit stale-hint diagnostic;
- missing, wrong-project, wrong-type and ambiguous/duplicate identity cases fail
  closed;
- successful resolution reads the governed Runtime Screen and exposes its actual
  authored action IDs back to Story Flow.

### Gate 5C — native Studio lifecycle and Screen Editor boundary

- Draft PR #83, stacked on Gate 5B.
- native Story Flow controls provide Screen name, purpose/template, `+ SCREEN`
  and `OPEN SCREEN`;
- creation delegates to Gate 5A and refreshes the same authoritative Story Flow
  authoring session/model/layout;
- opening delegates to Gate 5B and produces a typed
  `StoryFlowScreenEditorHandoff` containing stable node/document identity,
  resolved project path, path-hint movement state and authored action IDs;
- a callback boundary is available for Gate 8 to consume;
- Gate 5 deliberately does **not** switch to or fake a Screen Editor workspace.

## Integrated validation

Validation-only PR #80 is reused as the exact-tree Gate 5 checkpoint. It is
parented directly on the current merged Gate 4 `main` and must exactly match the
latest Gate 5C tree before acceptance.

Required authoritative checks:

- Renegade Studio Windows x64 Debug — pending final docs-inclusive checkpoint;
- Renegade Studio Windows x64 Release — pending final docs-inclusive checkpoint;
- Windows x64 Debug — pending final docs-inclusive checkpoint;
- Windows x64 Release — pending final docs-inclusive checkpoint;
- Gate 5A lifecycle/rollback test — required;
- Gate 5B resolution/outcome test — required;
- Gate 5C Screen Editor handoff test — required;
- Runtime Screen/Flow regressions — required;
- Studio startup smoke — required in Debug and Release.

The known standalone-package test skip remains acceptable only if it is the same
pre-existing skip and all executed tests pass.

## Owner acceptance

Owner audit is defined in `docs/STORY_FLOW_GATE5_OWNER_TEST.md` and is currently
**PENDING**. Gate 5 is not DONE until the exact docs-inclusive tree is green and
the owner audit passes.

## Merge rule

No Gate 5 PR is to be merged without explicit owner authorization after final CI
and owner acceptance. The validation-only PR #80 must never be merged as a
substitute for the stacked implementation review path.
