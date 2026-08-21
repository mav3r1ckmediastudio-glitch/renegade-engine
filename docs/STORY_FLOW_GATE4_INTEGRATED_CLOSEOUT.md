# Story Flow Gate 4 — Integrated Closeout

**Status:** owner Release acceptance passed; final docs-inclusive exact-head CI pending.

Gate 4 is implemented as four reviewable slices:

- **4A:** atomic governed New Level creation;
- **4B:** existing Level adoption, stable Scene identity resolution and moved/missing diagnostics;
- **4C:** native Story Flow Level controls plus Story Flow -> 3D Level Editor -> Story Flow lifecycle;
- **4D:** cumulative acceptance proof and owner-test closeout.

## Gate 4 acceptance contract

The complete candidate supports:

`Story Flow -> Add Level -> open Level -> edit/save Level -> Return to Story Flow -> reopen same Level`

The relationship is stable-ID authoritative throughout. A Level Flow node points to the Scene document ID stored in `.wiscene.rmeta`; filenames are repairable hints only.

## Transaction boundaries

New Level creation commits WISCENE, Scene identity and Story Flow as one project transaction. Existing Level adoption commits Scene identity creation/retargeting and Story Flow as one project transaction. Failure paths therefore cannot leave an orphan Flow node or newly governed sidecar without its corresponding semantic relationship.

## Render-path boundary

Story Flow remains the dedicated 2D render path established by Gate 3. The existing 3D Level Editor is activated only after a Level is explicitly opened. Returning uses an explicit native control and restores the still-live Story Flow authoring session/model/layout.

## Automated proof set

The accepted pre-closeout checkpoint was PR #78 head `9afb6377d19cbf206182219953acb852d4ed1cdd`, validated through PR merge ref `0e3e13f7fa3fc05659c3c784427759bba8c970b1` against `main` `12e4aebbf92208850d80c475cc28a6aca6da1925`.

- Renegade Studio workflow run `32469160009`: **passed**.
  - Windows x64 Debug: **passed**.
  - Windows x64 Release: **passed**.
  - Release CTest: **73/73 passed**.
  - Gate 4A, 4B, 4C and 4D tests all passed.
  - Release startup smoke passed.
- Windows baseline workflow run `32469159920`: **passed**.
- Exact Release artifact: `renegade-studio-windows-x64-Release-0e3e13f7fa3fc05659c3c784427759bba8c970b1`.
  - Artifact ID: `9442719486`.
  - Size: `244782122` bytes.
  - SHA-256: `6f306903cef2d111023da04caaeb7ab798f15eef113201a98b6cd129db714bd0`.

## Owner acceptance

On **2026-08-21**, the owner reported **PASS** after testing the exact Release artifact against `docs/STORY_FLOW_GATE4_OWNER_TEST.md`.

Accepted owner path:

1. create a new Level from Story Flow;
2. open it in the normal 3D Level Editor;
3. make and save a visible Scene edit;
4. return explicitly to Story Flow;
5. reopen the same Level and confirm the saved edit survives;
6. add and open a project-local existing `.wiscene`;
7. return to Story Flow with the relationship intact.

Stable identity after a physical Scene move remains covered by the automated Gate 4B proof rather than manual file surgery.

## Final exact-tree closeout

Recording owner acceptance changes the candidate tree. Therefore Gate 4 is not merge-ready until a fresh validation-only #78 checkpoint, rebuilt directly on the unchanged `main`, proves the final docs-inclusive tree in authoritative Windows Debug and Release CI.

## Merge policy

The stacked development PRs remain Draft for review. The integrated exact-head CI checkpoint is validation only and must not be merged independently. Gate 4 may be merged only after the final docs-inclusive exact-tree CI passes **and** the owner gives explicit merge authorization.
