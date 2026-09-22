# Character Importer V4 - owner-approved UI freeze

Approved by the project owner on 22 September 2026: "v4 is perfect" refers to UI presentation, **not** full functional acceptance.

## Recoverable exact baseline

- Baseline commit: `5152874fa443ce011981e8f0ac50ab1da1d2cfdf`.
- Annotated Git tag: `importer-ui-v4-approved-20260922`.
- Remote checkpoint branch: `checkpoint/approved-importer-ui-v4-20260922`.
- Local V4 executable SHA-256: `14C05F4BCDF6130C3DD5C218DEB6A65DB2ED4530AECAA72A33B7B4F1FE07E1D4`.
- An independent local archive preserves this executable, its Content, shaders, BuildInputs and DX compiler DLL. It is not tracked in Git. An exe hash does not prove that later gameplay is correct.
- Continue only on `feature/a6-animation-wiring-v4` or descendants. Never reset, force-push, retag, amend or overwrite the approved baseline; never merge incomplete A6 into `main`.

## Frozen presentation and interaction requirements

- Keep the V4 right-docked, layered, shaded five-card Animations page: Sources, Available Clips, Preview & Properties, Action Assignments, Validation.
- Preserve the large left preview, persistent previous/next workflow and preview controls; keep the native look, spacing, typography, card dimensions and source/clip-density behaviour as approved.
- Preserve the narrower initial right pane, draggable left-edge horizontal resize, vertical scrolling only and right-side scrollbar/text clearance. Do not permit a second horizontal scrollbar.
- The importer exclusively owns the right-hand inspector during import; ordinary Terrain/Scene/Render inspectors must never overlay it, including after clicking controls, dragging scrollbars or reopening dropdowns.
- Do not silently replace approved UI with an accordion, flatten its visual depth, rearrange sections, or hide controls to make unfinished logic appear complete.
- Necessary minor label/overflow adjustments must be visually reviewed against V4; a substantive layout or styling change requires renewed owner approval. Code-only tests cannot establish visual parity.

## Functional work is separate and incomplete

Wire each V4 control to real import, animation preview, trim, speed, enabled state, action assignments, validation, governed save/reopen and native A6 Runtime behaviour. Next missing workflows include per-source removal (currently remove-last), general expandable action variants and custom actions, frame-number range editing (currently seconds), reopening existing characters, and changed-source clip identity/reimport reconciliation. Do not fake these behind attractive controls or infer completion from the appearance of a dropdown.

## Gate for every wiring checkpoint

Build Studio locally; run focused Import/Recipe/Graphics/AI tests, actual Mutant FBX save/reopen proof when metadata changes, and manually inspect the native UI at narrow/default/wide widths, scrolling, focus, source/clip overflow, preview, action selection, and Import/Cancel. Compare directly against V4. Test gameplay-facing changes in TestGame; record what was *not* tested. Keep the owner's project and other worktrees unchanged. Update HANDOFF and the feature matrix as exposure actually changes.