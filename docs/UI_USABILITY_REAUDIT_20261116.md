# Renegade UI usability re-audit — 16 November 2026

> Follow-up: U01–U07 have a completed implementation and scoped native validation in [UI_USABILITY_COMPLETION_20261116.md](UI_USABILITY_COMPLETION_20261116.md). The findings and screenshots below retain the historical audit state.

This follows the first cohesion pass and approved icon integration. It checks the existing editor against the agreed opaque charcoal/orange direction and Dialogue-style button interaction.

The 65-family source inventory is retained in [UI_USABILITY_COVERAGE_20261116.csv](UI_USABILITY_COVERAGE_20261116.csv). Native coverage is recorded separately: opening a panel does not establish that every advanced workflow, asset state or runtime scenario passes. This is an audit and prioritized correction list, rather than a release acceptance certificate.

## Result

Two implementation commits correct three failures reproduced during this audit:

| Failure | Cause and correction | Receipt |
|---|---|---|
| Striped viewport after resizing with Light/Particle Picker open | Studio paused the main 3D renderer, resized its targets, then sampled the unwritten main image. Light, Particle and Dialogue now share the safe opaque 2D composition boundary while private previews render. | [Modal resize](evidence/ui-modal-resize/README.md) |
| Scrambled inspector after the same transition | Light creation cleared selection without refreshing hierarchy/inspector; resize could retain inconsistent legacy/section geometry. Presentation now reconciles on selection clearing and once after native resize. | Same receipt |
| Advanced fields overlapping basic Dialogue fields after Edit display → Back | The resume path hid conditional children, then native Window show made them visible again. Resume now shows first and reapplies the current mode/selection visibility. | [Dialogue return](evidence/ui-dialogue-resume/README.md) |

Implementation commits: `6a61ab7b97b9438cbcaddb34c2465e33ee05affe` and `d04da64e42880521bed707dc0750de757b20b04e`.

The latest review executable is `RenegadeStudio-Icons9.exe`, SHA-256 `43D1C97E707BEF733EDCED29EB14FC3A1A2EA45D9B4BBB08C1A50C91B5101C01`. Its Dialogue Basic/Advanced/resized-display round trips were verified live. The GPU resize receipt remains explicitly tied to Icons6/Icons8. Icons9 includes that unchanged correction. Binaries were built before commit and retain an older configure-time Git stamp.

These failures explain the recurring pattern in this session: different Studio paths handled pause, resize, show and return differently. A normal static window could look correct while its transition failed. The corrections share the paused composition rule and respect native parent/child visibility ordering.

## Existing capabilities verified

| Capability | Actual check | Result / limit |
|---|---|---|
| Asset-to-inspector handoff | Open Audio; pick a model in the hierarchy, including the same selected asset | Immediate matching inspector; no panel click. The owner also confirmed the earlier fix. |
| Physics target selection | Change hierarchy selection while Physics is open | Lab stays open and updates its authoring target. |
| Panel resizing | Drag the inspector divider at 1280×720, then restore it | Inspector width changes; numeric fields stay inside the panel. |
| Precise numeric entry and Undo | Type `1.25` into Environment time, commit, then Undo once | One command; original `0` and clean document state restored. |
| Toolbar release gesture | Create a disposable navigation grid; hover, hold Undo, then release | Grid remains during hold; one release undoes creation. An earlier fast injected click was not treated as a confirmed UI defect. |
| Main menus | Inspect File, Edit, Add, View, Build and Window | Consistent row faces; File/Edit cover Search Scene; all 12 Add rows share presentation. |
| Prefab save form | Open Save Prefab and cancel with Escape | Opaque form and consistent action buttons; scene markers are suppressed over the modal. No prefab written. |
| Smaller responsive forms | New Project and Screen Editor at 1280×720 | Main controls/actions remain reachable. No project or authored Screen created/changed. |

See [evidence overview](evidence/ui-usability-reaudit/README.md). These are scoped observations, not claims that all related functionality was exhaustively tested.

## Remaining findings

### U01 — P1: Large tool windows lose their bottom actions

At 1280×720, Light Picker and Particle Texture Library extend below the native window. Their primary action and Cancel controls are unreachable within the visible client area. Particle Picker has the same fixed-height limitation. In the Light Picker check, Escape did not dismiss the window. Maximizing recovers the controls, but is not a sufficient small-window layout.

Dialogue uses a different workaround: it scales the layout down. At the same size, labels/actions become clipped or abbreviated, including “Attach to an” and “Add NPC…”, while explanatory text becomes crowded.

Evidence: [Light](evidence/ui-usability-reaudit/light-small.png), [Texture Library](evidence/ui-usability-reaudit/texture-small.png), [Particle](evidence/ui-usability-reaudit/particle-small.png), [Dialogue](evidence/ui-usability-reaudit/dialogue-small.png).

Owners: `LightPickerWindow.cpp`, `ParticlePickerWindow.cpp`, `ParticleTextureBrowser.cpp`, `DialogueEditorWindow.cpp` and their Studio launch/layout paths.

Correction: fit the modal frame to available logical space, keep an action footer reachable, and scroll/reflow the content. Use readable minimum text/control roles. Reapply constraints after resize, rather than retaining a previously clamped position. Escape should cancel the topmost surface subject to its existing dirty-document guard.

Acceptance: primary action, Cancel/close, errors and focused input remain reachable at 1280×720 and 1024×768. Repeat opening, shrinking, maximizing and returning with nested pickers; verify pointer and keyboard ownership throughout.

### U02 — P2: Physics scrolling overwrites its fixed page heading

The Vehicle page does scroll, so its lower controls are not simply missing. However, scrolled controls paint over the Vehicle title/help region. The eight-tab strip also truncates its longest label at the smaller width.

Evidence: [Vehicle after scrolling](evidence/ui-usability-reaudit/physics-scroll.png).

Owner: `RenegadePhysicsLabWorkspace.cpp`.

Correction: give the scroll body a matching drawing and input clip below the fixed title/help region. Fit or overflow the tab strip deliberately. Keep the existing authoring target and scene-overlay suppression.

Acceptance: scroll Vehicle from top to bottom at both tested widths. Controls must neither draw nor accept input over headings/tabs; every tab and lower control remains reachable.

### U03 — P2: Project Hub commands still activate on press

Holding the New Project card opens its modal before mouse release. Source inspection confirms its direct mouse-press dispatch. This differs from the accepted arm-on-press, invoke-on-matching-release interaction.

Evidence: [Captured while mouse remained held](evidence/ui-usability-reaudit/hub-held.png).

Owner: `RenegadeProjectHub.cpp`.

Correction: route Hub command cards through the shared release gesture. Preserve the existing responsive New Project form.

Acceptance: hold does not invoke; release over the same target invokes once; drag away, Escape and focus loss cancel; disabled/busy commands cannot dispatch.

### U04 — P2: Inspector ordering obscures the selected asset's purpose

Selecting a light first presents identity, all 32 layer bits, metadata, Transform and Character setup. The actual Light settings appear near the bottom of the first screen. A plain platform similarly exposes a large Character readiness block before its ordinary rendering/material work.

Evidence: [Selected light](evidence/ui-usability-reaudit/light-inspector.png), [selected platform](evidence/ui-usability-reaudit/model-inspector.png).

Owners: `S1BInspectorSectionMigration.cpp`, section descriptors and `AICharacterInspector.cpp`.

Correction: put identity and the selected asset's primary component controls first. Keep layer masks, generic metadata and optional conversion/setup controls accessible through appropriate collapsed/Advanced sections. Preserve every existing capability and transaction.

Acceptance: selecting Light, Camera, Player and ordinary Model exposes their common edit controls immediately. Unrelated setup must not dominate the initial view.

### U05 — P2: Several active UI families retain different visual roles

The main Scene shell and Journey have the approved icon/action treatment. The remaining seams are concrete:

- Graph nodes still use the small default monospace-style text while surrounding Studio uses its normal UI roles.
- Screen Editor has text controls for common actions such as Back, Save, Undo/Redo, Focus and Fit, and retains a separate chrome accent/typography family.
- Project Hub retains a different letter-spacing/heading treatment and text symbols for commands.
- Light/Particle numeric controls retain their grey native bar treatment and different label roles alongside the orange Studio sliders.

Evidence: [Graph](evidence/ui-usability-reaudit/graph.png), [Screen Editor](evidence/ui-usability-reaudit/screen-editor.png), [Hub](evidence/ui-usability-reaudit/hub.png), plus the picker screenshots under U01.

Owners: `RenegadeImGuiBridge.cpp`, Graph editor, Screen Editor chrome/control classes, Project Hub and picker control adapters.

Correction: migrate these existing consumers onto explicit shared typography, neutral/action and control-state roles. Apply approved catalogue icons to matching common actions already represented in the library. Preserve authored Screen colours/fonts, preview pixels, graph role colours and meaningful selection/handle colours.

Acceptance: compare the same title, action, numeric, help, disabled, held and focused roles across these families. Do not flatten creator-authored appearance into editor chrome styling.

### U06 — P2: Numeric help can extend beyond the window

The Environment numeric-entry tooltip overflows the right edge at 1280×720. It also presents raw `FLT_MAX` syntax prominently. Precise entry works; the help presentation needs attention.

Evidence: [Numeric entry/help](evidence/ui-usability-reaudit/numeric-tooltip.png).

Owners: Studio numeric-input adapters and tooltip presentation.

Correction: constrain and wrap help to the logical canvas. Lead with the useful interaction and units; retain expression/reset support and place specialist syntax in an appropriate secondary explanation.

Acceptance: hover/focus fields at each window edge. Full help remains readable without covering unrelated commands unnecessarily.

### U07 — P2: Window menu labels contradict working dividers

Window shows disabled “HIERARCHY // FIXED” and “INSPECTOR // FIXED” rows. Native dragging proves both panel widths can be adjusted.

Evidence: [All six menus](evidence/ui-usability-reaudit/menus.png), [working divider](evidence/ui-usability-reaudit/panel-resize.png).

Owner: `RenegadeStudioChrome.cpp` menu model.

Correction: remove stale placeholder wording or replace it with accurate, useful guidance for the current layout behaviour.

Acceptance: the menu and tooltip wording accurately describes the interaction users can perform.

## Recommended next implementation pass

1. Correct responsive modal frames, reachable footers and topmost Escape behaviour across Light/Particle/Texture/Dialogue.
2. Fix Physics scroll clipping and narrow tab presentation.
3. Complete Hub command gestures and the remaining typography/control adapters, then improve inspector ordering and stale help/menu wording.

Use the working Screen Editor/New Project sizing behaviour and shared action foundations where applicable. Validate transitions as well as static pages: open → edit/inspect → nested picker/editor → resize → return → cancel/close → selection → Undo.

## Coverage, provenance and boundaries

All 65 original UI families have a row in the coverage CSV, with explicit native observations and remaining scenarios. The accompanying source inventory is pinned to implementation commit `d04da64e42880521bed707dc0750de757b20b04e`; fingerprints use normalized text when comparing with the original audit snapshot. Shared theme, icon, gesture and composition foundations are listed separately.

The reviewed project was a copied seven-entity dialogue test level with isolated preferences. Native checks cover the observed empty/no-terrain states, not terrain sculpting, populated waterfalls, hair, real animation rigs, every weapon/socket/importer state, every UI Designer inspector, packaged Runtime menus or companion voice/chat. Those remain explicit coverage items rather than implied passes. The isolated Particle effect preset list was empty while the included texture browser contained 50 textures; this is not evidence of lost owner presets.

Window-size tests do not certify Windows OS DPI at 125% or 150%, path-traced preview mode, or all supported hardware. No runtime gameplay or save schema changed. No owner project, authored dialogue or authored Screen was modified. Disposable numeric/navigation edits were undone. No hosted CI, push, PR, merge or release was performed.

Concurrent wireframe/view-mode source edits appeared after the reviewed build. They were preserved and are outside these receipts. The audit does not certify the evolving working tree or attribute its changes to the tested Icons9 executable.

Final Release build and the selected 67 checks pass. Exact commands, binary hashes and native before/after screenshots are retained in the two correction receipts. The 67-check filter was:

```text
ctest --test-dir BUILD/native -C Release
-R 'SourceContract|StudioUiPolicy|RenegadeDialogue|^RenegadeStoryFlowJourneyUiLayoutTests$'
--output-on-failure
```
