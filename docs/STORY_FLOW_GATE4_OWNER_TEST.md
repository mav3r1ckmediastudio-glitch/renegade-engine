# Story Flow Gate 4 — Owner Acceptance Test

Use the Release artifact produced from the exact integrated Gate 4 CI head.

## Test 1 — Add New Level

1. Open the controlled Story Flow project.
2. Enter a Level name in the Story Flow Level field.
3. Click `+ NEW LEVEL`.
4. Confirm a new Level node appears in Story Flow.
5. Select that Level node and click `OPEN LEVEL`.
6. Confirm Renegade switches to the normal 3D Level Editor; Story Flow must not remain visibly/rendering underneath it.
7. Make a small visible scene edit and use the normal Level Editor Save command.
8. Click `< STORY FLOW`.
9. Confirm the same Story Flow returns with the Level relationship intact.
10. Select the same Level and open it again. Confirm the saved scene edit is still present.

## Test 2 — Add Existing Level

1. Return to Story Flow.
2. Enter a different Level name.
3. Click `+ EXISTING...` and choose a project-local `.wiscene`.
4. Confirm a Level node is added.
5. Select it and click `OPEN LEVEL`.
6. Confirm the chosen WISCENE opens in the normal 3D Level Editor.
7. Return using `< STORY FLOW`.

## Test 3 — Stable identity after move

This proof is automated in CI. No manual file surgery is required for owner acceptance. The Gate 4B test moves a governed WISCENE and its `.rmeta` together and proves Story Flow resolves the Level by stable Scene identity even though the stored path hint is stale.

## Pass criteria

Gate 4 passes owner acceptance only if New Level, Existing Level, Level open, normal Scene Save, explicit Return to Story Flow, and reopen all work without losing the stable Story Flow -> Scene relationship.

## Acceptance record

**Owner result: PASS — 2026-08-21.**

The accepted Release artifact was:

`renegade-studio-windows-x64-Release-0e3e13f7fa3fc05659c3c784427759bba8c970b1`

- PR #78 source head: `9afb6377d19cbf206182219953acb852d4ed1cdd`.
- CI merge ref: `0e3e13f7fa3fc05659c3c784427759bba8c970b1`.
- Renegade Studio workflow run: `32469160009`.
- Artifact ID: `9442719486`.
- SHA-256: `6f306903cef2d111023da04caaeb7ab798f15eef113201a98b6cd129db714bd0`.

The owner confirmed the required New Level, open/edit/save/return/reopen lifecycle and Existing Level open/return lifecycle passed on this Release artifact.

Recording this result changes the final candidate tree, so a fresh docs-inclusive exact-tree CI checkpoint is still required before merge authorization can be accepted.
