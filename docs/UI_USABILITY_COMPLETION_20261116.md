# UI usability correction pass — 16 November 2026
Later combined pre-PR validation: [17 November receipt](UI_COMBINED_PR_VALIDATION_20261117.md). The results and submission state below describe the historical UIFinish7 checkpoint.


The seven findings U01–U07 from [the re-audit](UI_USABILITY_REAUDIT_20261116.md) are implemented. This closes that bounded correction list; it does not certify every state of the 65-family inventory or mark an independent release gate complete.

| Finding | Result | Native evidence |
|---|---|---|
| U01: oversized tool windows | Light, Particle, Texture Library and Dialogue share a canvas-bounded frame, readable scrolling body and pinned action/status footer. Escape closes the enabled topmost modal once; Dialogue retains its unsaved-draft guard. | Light/Particle/Texture/Dialogue at 1280×720 and 1024×768; nested Texture → Particle Escape; Light Advanced; Dialogue Edit display → Back, dirty-name Escape and Discard. |
| U02: Physics overlap | Scroll controls share a drawing/input clip below the fixed heading. Eight tabs wrap into rows according to available width. | Vehicle scroll and wrapped tabs at 1280×720. |
| U03: Hub press-time commands | Hub commands use the shared release gesture, opaque action faces and held state. | Hold New leaves Hub open; drag-away release cancels; same-target release opens one New Project form; Escape cancels without creating a project. |
| U04: inspector ordering | Primary asset controls precede generic setup. Layers/metadata stay available in a collapsed section. Ordinary models start with Character conversion collapsed; actual Characters retain their setup workflow. Changing selection/view resets inspector scroll offsets. | Model → expanded/scrolled metadata → Player immediately shows Player controls at top. Camera and Light selections immediately show their own primary controls. |
| U05: visual seams | Graph uses Windows Segoe UI Semibold where available. Hub/Screen Editor use shared chrome roles and approved common-action icons. Picker numeric controls share Studio rails, labels and states. | Graph, Hub, Screen Editor and Light/Particle comparisons. Authored dialogue Screen colours/fonts remain unchanged. |
| U06: numeric help overflow | Studio numeric help wraps and clamps to the logical canvas, leading with exact entry, expressions and reset. Native parsing/quantization remain intact. | Environment right-edge numeric field at 1280×720; full help stays visible. |
| U07: stale Window labels | Menu now explains divider dragging and adjustable inspector width. | Window menu at 1280×720. |

## Shared implementation decisions

`StudioModalWindow` owns the scroll body and pinned footer. Body and footer geometry is positioned in absolute pixel coordinates before native Window attachment, preserving native scaled-transform semantics. The same body owns drawing and input clipping. Embedded Screen previews inherit the host clip and reject pointer input outside it.

`StudioNumericControl` preserves native numeric input and provides explicit Studio presentation/help. Physics uses an identity clip parent so rendering and hit testing have the same content boundary. The Project Hub uses `ui::ReleaseAction`, including cancellation while hidden/disabled or after capture loss.

Private Light/Particle previews temporarily use shaded rendering and restore the main scene's wireframe preference immediately. This branch already includes the whole-scene Show modes from `fada1d6353691a14508b9b748ec8335631984cfb`; no renderer/submodule pointer change was made in this correction pass.

## Validation and provenance

Final native candidate: `BUILD/native/Studio/Release/RenegadeStudio-UIFinish7.exe`.
SHA-256: `DB13A9DAA42D17206D7ACCEAE826C4B5BCD8A9F8A274EC7432E07182D2447C5F`.
Release build passed (69.30 s). The configure-time stamp is the prior `2ddc4ca` checkpoint because the executable was built before committing these changes; use the binary hash and handoff implementation commit together.

The broader modal/Screen transition evidence uses UIFinish5, SHA-256 `4A79C8859B94C118BF37D316FECFB0B107ED7EBEBF744A5ACE14AA723A0A711D`. UIFinish7 adds only the Dialogue explanatory-note spacing and inspector-scroll reset. The final candidate separately verifies those changes plus Graph, Hub gestures, numeric help, Physics and asset inspector handoff. [Evidence index](evidence/ui-usability-completion/README.md) identifies each binary's receipts.

Build commands, from the repository root with its existing native Release configuration and configured Wicked overlays:

```powershell
cmake -S . -B BUILD/native
MSBuild BUILD/native/Studio/RenegadeStudio.vcxproj /t:Build /p:BuildProjectReferences=false /p:Configuration=Release /p:Platform=x64 /p:TargetName=RenegadeStudio-UIFinish7 /v:minimal
```

The current ScreenRenderer and authoring dependencies/tests were rebuilt before final Studio linking:

```text
cmake --build BUILD/native --config Release --parallel 4 --target RenegadeScreenRendererTests RenegadeScreenAuthoringSessionTests RenegadeScreenAuthoringCompletionTests RenegadeRuntimeScreenTests
ctest --test-dir BUILD/native -C Release -R 'SourceContract|StudioUiPolicy|RenegadeDialogue|^RenegadeStoryFlowJourneyUiLayoutTests$|^Renegade(ScreenRenderer|ScreenAuthoringSession|ScreenAuthoringCompletion|RuntimeScreen)Tests$' --output-on-failure
```

Final selected checks: **71/71 pass, 11.96 s**. The four freshly rebuilt Screen suites also passed separately. `git diff --check` passes. Build logs and test output are retained under ignored `BUILD/`; evidence copies accompany this receipt.

Native checks used a disposable seven-entity DialogueReview copy and separate preferences. No owner project, authored dialogue or authored Screen was saved or changed. A temporary name edit exercised the dirty guard, then Discard restored the saved conversation. No gameplay or serialized format changed. Existing configured Wicked overlays remain untouched and unstaged.

At narrow sizes, large modal content scrolls horizontally/vertically at readable size; it is not all reflowed into one visible column. Numeric help is bounded; this is not a claim that every native nonnumeric tooltip has been migrated. Physics 1024-wide/all-page states, Windows OS DPI 125/150%, path-traced transitions, Debug, hosted CI and independent exact-commit verification remain outside these native receipts. No push, PR, merge or release has been performed.
