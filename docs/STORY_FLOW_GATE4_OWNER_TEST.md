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

Do not merge Gate 4 on CI alone; owner Release-artifact acceptance remains required.
