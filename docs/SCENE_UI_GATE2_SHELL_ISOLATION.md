# Scene UI Gate 2 — shell, transition and specialist-workspace isolation

**Baseline:** `705aae68a478775a4cc7fa96bad8bd745fd84f42`

**Recovery branch:** `recovery/scene-ui-gate2-shell-isolation`

## Purpose

Repair and harden the Scene Editor shell before any broad Inspector, Asset
Browser, Environment or Terrain usability redesign. This gate is deliberately
bounded: CI is final proof for a coherent candidate, not an iterative debugger.

## Story Flow -> Level Editor seam

The existing `< STORY FLOW` lifecycle control was attached to the Level Editor
GUI after the full Renegade Scene chrome. Wicked renders top-level wiGUI widgets
in reverse registration order, so normal late registration placed the control
behind the chrome. It was also positioned at the upper-left of the viewport,
sharing the same region as the `PERSPECTIVE` viewport chip.

Gate 2 repairs both ownership defects:

- `StudioRenderPath::RegisterStoryFlowLifecycleControl()` deliberately
  registers the lifecycle control and then re-registers the Scene chrome;
- reverse rendering therefore paints the chrome first and the lifecycle control
  afterwards;
- the return control now lives in the scene-tab strip, right-aligned before the
  Inspector rather than inside the viewport chip row;
- its horizontal position stays clear of the 230 px active scene tab;
- its position derives from the live Renegade viewport bounds, so hierarchy and
  Inspector resizing move it with the shell rather than leaving it behind.

## Environment / Terrain isolation audit

A previous failure mode allowed Environment and Terrain option lists to appear
merged in one Inspector after switching between specialist workspaces. That
symptom is not present in the current baseline, so Gate 2 does not rewrite the
working specialist subsystems.

The source audit confirms the current protection chain:

1. `SetEnvironmentWorkspaceActive(true)` explicitly clears
   `terrainWorkspaceActive_`.
2. `SetTerrainWorkspaceActive(true)` explicitly clears
   `environmentWorkspaceActive_`.
3. Scene workspace activation clears both specialist modes through those same
   setters.
4. `RefreshInspector()` chooses its component owner from the active specialist
   workspace rather than retaining the previous selection:
   - Environment -> governed weather entity;
   - Terrain -> first terrain entity, or `INVALID_ENTITY` when no terrain exists;
   - normal Scene -> current selection.
5. Environment and Terrain controls have separate visibility groups.
6. When Terrain is active but no terrain exists, only the Terrain empty-state
   label/create action is exposed; the previous Environment component cannot
   supply the Inspector.

`Tests/SceneUiGate2SourceContract.cmake` locks these invariants so the historical
panel-contamination bug cannot silently return through a later refactor.

## Shell bounds audit

Renegade Studio remains the single viewport-bounds authority. Existing chrome
clamps retain at least a 420 px central Scene authoring surface and a 200 px
minimum open drawer height.

The Gate 2 contract checks return-control geometry at:

- the 420 px minimum central surface;
- 1280x720 default shell widths (`710 px` central surface with current defaults);
- 1680x945 (`1000 px` central surface with current defaults);
- 1920x1080 (`1240 px` central surface with current defaults).

The control remains outside the active scene-tab region and inside the
viewport-right / Inspector boundary at every audited width.

## Input and workspace ownership retained

This gate preserves the existing Scene input cutoff:
`studioChrome_.ConsumedPointerThisFrame()` prevents shell clicks from falling
through into viewport selection, gizmo manipulation or camera navigation.

Story Flow / Level Editor switching also remains post-frame and explicit:

- Level Editor deactivates the Story Flow workspace and exposes the return
  lifecycle control only for an opened Story Flow Level;
- Story Flow activates its own workspace and disables the Level-only return
  control;
- Screen Editor remains an independent render path.

## Explicit exclusions

Gate 2 does **not** attempt the later remedial or authoring work:

- Inspector typography, spacing, wrapping and scrolling — Scene UI Gate 3;
- Asset Browser / placement deep acceptance — Scene UI Gate 4;
- Environment and Terrain control-by-control functional acceptance — Scene UI
  Gate 5;
- terrain datum/global elevation, larger/asymmetric terrain extents, or chunk
  expansion — post-remediation terrain authoring work;
- editable construction grid and placement/transform snapping — post-remediation
  grid authoring work.

## Verification policy

During implementation the branch is source-audited without opening a PR, so the
Windows workflows do not run on every patch. Before an owner candidate:

1. inspect the complete diff against the exact baseline;
2. verify the Gate 2 source contract and all pre-existing Story Flow contracts
   remain compatible;
3. open one candidate PR;
4. require one authoritative four-check Debug/Release CI pass;
5. hand off one packaged Release for consolidated owner verification.

A CI failure is diagnosed from its actual evidence and receives a bounded
correction. CI is not used as a build/patch/build development loop.

## Owner Release check

One candidate Release should be sufficient to verify this gate:

1. Hub -> Story Flow -> open a Level.
2. Confirm `< STORY FLOW` is visible in the scene-tab strip and does not overlap
   `PERSPECTIVE`, `LIT`, `SHOW`, the active scene tab or the Inspector.
3. Resize the Hierarchy and Inspector and confirm the return control remains in
   the correct strip.
4. Scene -> Environment -> Terrain -> Environment -> Scene, repeating the cycle
   several times; confirm option lists never merge.
5. Open Terrain with and without an existing terrain; confirm Environment
   controls never remain in the Terrain Inspector.
6. Open/close the bottom drawer and confirm viewport/shell input remains clean.
7. Return to Story Flow, reopen the same Level, and confirm the same behaviour.

Broader Inspector readability and specialist-control testing deliberately waits
for the later remedial gates rather than multiplying owner builds here.
