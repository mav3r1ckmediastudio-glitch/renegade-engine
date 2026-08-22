# Story Flow Gate 8C — Screen Editor Authoring Shell

## Outcome

Open a governed Screen from Story Flow into a first-class native Screen Editor
where the creator can select existing elements, edit their content and resolved
layout, preview the exact Runtime presentation, use Screen-specific history,
save transactionally and return to the same Story Flow project context.

Gate 8C is the first real Screen Editor UI. It consumes the accepted Gate 5
stable handoff, Gate 8A schema-v2 document and Gate 8B shared renderer. It does
not invent a second preview renderer or keep the Story Flow/3D editor paths
running behind the Screen Editor.

## Locked implementation boundary

- `ScreenAuthoringSession` is the UI-independent mutation authority.
- Every accepted edit validates the complete candidate `ScreenDocument` before
  entering bounded Screen-specific Undo/Redo history.
- Save uses the existing transactional `WriteScreenDocument` path and clears
  dirty state only after the validated commit succeeds.
- Absolute elements write their authored rectangle directly.
- Anchored elements preserve anchor values and translate an edited resolved
  rectangle into deterministic parent-relative offsets.
- `RenegadeScreenEditorRenderPath` is a first-class `RenderPath2D`.
- The central preview instantiates `Renegade::ScreenRenderer`; Studio does not
  duplicate Runtime canvas, font, resource, state, border or layout rules.
- Hierarchy and canvas selection share one stable widget ID.
- Inspector editing in this slice covers element name, visible content for
  Text/Button records, resolved X/Y/width/height and visible/enabled state.
- Return to Story Flow refuses to discard unsaved Screen mutations silently.

## Deliberate Gate 8C exclusions

Gate 8D owns element creation/deletion, image/resource browsing, the broader
control catalogue, reusable components, complete visual-state/style editing and
typed data/action binding. Gate 8E owns Story Flow outcome and packaged
standalone parity closeout.

Gate 8C does not redesign Story Flow. The recovered Journey View concept remains
the visual reference for Gate 9.

## Automated acceptance

- opening a valid Screen establishes clean Screen history;
- absolute content/layout editing enters exactly one history state;
- editing an anchored element preserves the requested resolved rectangle;
- an editor viewport translates the exact shared Runtime frame geometry;
- an identical edit does not pollute history;
- malformed geometry fails before mutation;
- Undo/Redo restores exact validated Screen snapshots;
- transactional Save clears dirty state;
- a second authoring session reopens the saved content and anchored layout;
- Studio, renderer and integration sources compile in Windows Debug and Release;
- the existing Runtime/Story Flow/Screen suites remain green.

## Owner Release acceptance

Use only visible Studio workflows for the primary test:

1. Create or open a project and enter Story Flow normally.
2. Add a Title Screen, select its card and open it by double-click or the
   explicit **OPEN SCREEN** action.
3. Confirm Studio switches to a dedicated **SCREEN EDITOR** showing:
   hierarchy, central live Runtime preview, Inspector, Save/Undo/Redo and
   **< STORY FLOW**.
4. Select the title from the hierarchy and then directly on the preview. Both
   selections must identify the same element and draw a clear selection frame.
5. Change its displayed text and X/Y/width/height in the Inspector, apply the
   edit and confirm the live preview changes visibly.
6. Press Undo and Redo and confirm both the Inspector and preview restore the
   corresponding states.
7. Make an edit and confirm Return refuses to discard it silently. Save, then
   return successfully to the same Story Flow.
8. Reopen the same Screen and confirm the saved text/layout persisted.

A missing editor transition, invisible preview, misleading selection, lost
edit, broken history, silent dirty-state loss or failed reopen rejects Gate 8C
regardless of green CI.
