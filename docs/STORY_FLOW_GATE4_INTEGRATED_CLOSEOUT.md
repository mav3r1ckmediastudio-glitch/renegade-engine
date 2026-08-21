# Story Flow Gate 4 — Integrated Closeout

**Status:** integrated candidate awaiting exact-head CI and owner Release acceptance.

Gate 4 is implemented as four reviewable slices:

- **4A:** atomic governed New Level creation;
- **4B:** existing Level adoption, stable Scene identity resolution and moved/missing diagnostics;
- **4C:** native Story Flow Level controls plus Story Flow -> 3D Level Editor -> Story Flow lifecycle;
- **4D:** cumulative acceptance proof and owner-test closeout.

## Gate 4 acceptance contract

The complete candidate now supports:

`Story Flow -> Add Level -> open Level -> edit/save Level -> Return to Story Flow -> reopen same Level`

The relationship is stable-ID authoritative throughout. A Level Flow node points to the Scene document ID stored in `.wiscene.rmeta`; filenames are repairable hints only.

## Transaction boundaries

New Level creation commits WISCENE, Scene identity and Story Flow as one project transaction. Existing Level adoption commits Scene identity creation/retargeting and Story Flow as one project transaction. Failure paths therefore cannot leave an orphan Flow node or newly governed sidecar without its corresponding semantic relationship.

## Render-path boundary

Story Flow remains the dedicated 2D render path established by Gate 3. The existing 3D Level Editor is activated only after a Level is explicitly opened. Returning uses an explicit native control and restores the still-live Story Flow authoring session/model/layout.

## Automated proof set

- Gate 4A: new WISCENE + identity + Flow atomicity and rollback.
- Gate 4B: existing WISCENE adoption, move recovery by stable identity, duplicate/missing diagnostics.
- Gate 4C: governed Scene open while Flow authoring state remains alive and unchanged.
- Gate 4D: Add New Level -> resolve -> open -> edit/save -> reopen while retaining the same Scene document identity.

## Merge policy

The stacked development PRs remain Draft for review. The integrated exact-head CI checkpoint is validation only and must not be merged independently. Gate 4 remains unmerged until the authoritative Windows Debug/Release checks pass and the owner accepts the Release artifact.
