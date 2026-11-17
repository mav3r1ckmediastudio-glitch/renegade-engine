# Combined UI/wireframe ready for PR — 17 November 2026

Application implementation: 7fd95b5b3607974e7275673fa0bcc3e1b1c6af2a.
Configure/diagnostic stamp: c2eccc33fa1c2e13c06bc50b94f16ffa308d12a5.
Base main fceda08 integrated. The implementation commit adds one final
Screen Editor tooltip render pass and three reviewed test-harness corrections
to the configure checkpoint. The documentation/evidence commit changes no
application inputs. All four tested file hashes are recorded. See
docs/UI_COMBINED_PR_VALIDATION_20261117.md and
docs/evidence/ui-pr-validation/README.md for commands hashes and scope.

Earlier complete CI aggregate builds pass in Debug and Release. The final
Screen tooltip change is rebuilt in Studio, along with five interrupted-link
recovery targets. Complete unfiltered CTest is rerun after that final build:
Debug 321/321; Release 334/334.
Studio/Runtime packages and dependency/registry reopen proofs pass for both.
Both Studio startup-smoke runs exit 0; packaged atlas/manifest hashes match
source and all 65 approved SVGs are present. Final source/help/policy pass:
66/66 including packaged help and the 84-case workflow suite.

Preflight fixed reviewed help fingerprints; kept the example-audio notice;
corrected literal-ellipsis inspector title caching and retained detailed
action help during icon assignment. Final Screen Editor toolbar hints render
after ordinary controls and before child pickers. Native before/after and final-candidate
receipts are indexed above. Owner acceptance of UIFinish7 remains scoped.
The original owner application/project and configured Wicked overlays remain
preserved; Wicked pin unchanged.

Local exFAT validation required explicit CMake configuration with auto-
regeneration suppressed and bounded build workers. The task-local canonical
flow adapter retained all tests and package proofs. No hosted workflow change.
The three reviewed test-only corrections preserve all assertions and deadlines;
full details and initial failures are retained in the receipt. No partial or
aborted run is counted as PASS.

Next: verify final documentation-only diff and current main; push this branch
once and open one combined UI/wireframe PR. Confirm all four hosted jobs are
queued for the exact PR tip. No merge or release is authorized. Avoid an extra
documentation push after opening the PR because CI takes over two hours.

---

# Remaining UI audit corrections completed — 16 November 2026

Implementation/evidence commit: d55b61ac28266c11550e338faa72312175da0b72
on fix/ui-cohesion-pass1-20261116. This follows the already-local whole-scene
wireframe commits fada1d6 / 2ddc4ca. All seven U01-U07 findings have implemented
corrections and scoped native receipts. Read
docs/UI_USABILITY_COMPLETION_20261116.md and its evidence index; the historical
re-audit now links to that follow-up and retains its original findings.

Changed StudioModalWindow and StudioNumericControl (new header/source pairs),
LightPickerWindow, ParticlePickerWindow, ParticleTextureBrowser and
DialogueEditorWindow header/source, S1BInspectorSectionMigration,
StudioApplication header/source, RenegadeProjectHub header/source,
RenegadePhysicsLabWorkspace, RenegadeImGuiBridge,
RenegadeScreenEditorWorkspace, RenegadeStudioChrome header/source,
Studio/CMakeLists and ScreenRenderer/src/ScreenRenderer.cpp.
Architecture and the cohesion/icon feature rows were updated.

Key behavior: readable scrollable modal body with pinned footer; one topmost
Escape per physical press; Dialogue dirty guard retained; embedded Screen host
clip for both input and rendering; Physics matching clip and wrapped tabs;
Hub release gestures; primary-first inspectors with fresh selection scroll;
shared numeric rails/bounded help; approved common-action icons and UI font.
Private picker previews render shaded, then restore main scene wireframe mode.
Authored Screen appearance and native document transactions are preserved.

Final native review app remains open on DESIGN:
BUILD/native/Studio/Release/RenegadeStudio-UIFinish7.exe
SHA256 DB13A9DAA42D17206D7ACCEAE826C4B5BCD8A9F8A274EC7432E07182D2447C5F.
It was built before commit and has configure-time Git stamp 2ddc4ca; identify
it by hash and this implementation commit, not its older UI stamp.
Native review used a disposable seven-entity DialogueReview project and
isolated preferences. No owner project, authored dialogue or Screen saved.
Temporary dirty-name guard test was discarded to the saved conversation.

Release build passed (69.30 s); current four Screen suites were rebuilt and
passed separately; final 71 selected CTests passed (11.96 s); diff check passes.
Exact commands, hashes and binary-specific screenshots are in the completion
receipt. Broad resize/Screen/dirty-guard proof uses UIFinish5; final UIFinish7
adds only note spacing and inspector scroll reset, then separately verifies
those changes, Graph/Hub, numeric help, Physics and all four asset handoffs.
Do not attribute all prototype screenshots to the final binary.

No Debug, hosted CI, push, PR, merge, distribution or release-gate closure.
Narrow modal content intentionally scrolls instead of shrinking all text.
OS DPI 125/150%, all Physics pages at 1024, path-traced transitions and
independent exact-commit verification remain outside these receipts.
Existing configured Wicked overlays remain dirty/unstaged and the gitlink
was not changed. No owner work was removed.
Next: owner review of the open candidate; prepare one combined UI/wireframe PR
only when requested, with the required broader build validation before pushing.

---

# SHOW whole-scene wireframe modes — 16 November 2026

Implementation commit: fada1d6353691a14508b9b748ec8335631984cfb,
on fix/ui-cohesion-pass1-20261116. Owner requested whole-scene modes only
and gave positive feedback during native review.

Changed CMakeLists, cmake/WickedWireframeOverlayFix.cmake, StudioApplication
header/source, RenegadeStudioChrome header/source, Phase5Gate9RenderDiagnostics,
and Tests/Phase5Gate9SourceContract. Documentation: WIREFRAME_VIEW_MODE,
FEATURE_MATRIX, ARCHITECTURE, UPSTREAM_SYNC and evidence/wireframe-show.

SHOW exposes Shaded / Wireframe / Shaded + Wireframe beside independent Grid.
One setter shares native state with diagnostics and persists only editor prefs.
Fixes native overlay edges failing the strict depth test using six dedicated
read-only coplanar PSOs in a generated renderer. Original PSOs remain unchanged.
Do not add the renderer to WickedEngine_common: embedded/external targets compile
it individually. Build common and embedded library before relinking Studio.

Exact commands and hashes are in docs/WIREFRAME_VIEW_MODE.md. Release builds pass;
67/67 focused checks pass (2.58 s); diff check passes. Native Wireframe4 checks
cover all three modes, clean scene/history, SHOW indication and fresh-process
restoration. Final Wireframe5 differs only in ASCII active-state markers.
No hosted CI, push, PR, merge, Runtime distribution or Debug proof.

Existing owner Studio windows, project data and dirty configured Wicked overlays
are preserved. No gitlink change. Complex terrain, transparent overlay parity,
MSAA and path-trace transition native checks remain scoped limitations.
Next: owner review of final candidate; broader checks before any requested PR.

---

# UI usability re-audit and modal lifecycle corrections — 16 November 2026

This supersedes the native-coverage limits in the Audio handoff below where
explicit new checks are recorded. Owner subsequently confirmed Audio-to-asset
switching. Native review now also covers model-row, same-asset and Physics
authoring-target selection. No additional feature duplication was proposed.

Local implementation/evidence commits:
- 6a61ab7b97b9438cbcaddb34c2465e33ee05affe: shared suspended-modal composition,
  immediate cleared-selection presentation, one-time resize inspector refresh.
- d04da64e42880521bed707dc0750de757b20b04e: show Dialogue parent before restoring
  conditional child visibility on return from Edit display.
- 57b58f51361535422db3278cb0f8a04b92f365df: audit, coverage, pinned source inventory
  and screenshots. Read docs/UI_USABILITY_REAUDIT_20261116.md and the coverage CSV.

Latest reviewed executable:
BUILD/native/Studio/Release/RenegadeStudio-Icons9.exe
SHA-256 43D1C97E707BEF733EDCED29EB14FC3A1A2EA45D9B4BBB08C1A50C91B5101C01.
Source is pinned to d04da64 for this receipt; the build predates that commit and
its configure-time Git stamp is not the review identity. Modal GPU before/after
proof is explicitly Icons6/Icons8; Icons9 adds the Dialogue return correction.
See docs/evidence/ui-modal-resize and docs/evidence/ui-dialogue-resume.

Release Build target Icons9 completed exit 0 (CL=/MP6):
MSBuild BUILD/native/Studio/RenegadeStudio.vcxproj /t:Build
/p:BuildProjectReferences=false /p:Configuration=Release /p:Platform=x64
/p:TargetName=RenegadeStudio-Icons9 /v:minimal.
ctest --test-dir BUILD/native -C Release
-R 'SourceContract|StudioUiPolicy|RenegadeDialogue|^RenegadeStoryFlowJourneyUiLayoutTests$'
--output-on-failure: 67/67 pass, 5.70 s. Existing C4834 warning remains.
Native Basic/Advanced/Edit display/resize/Back cases pass on exact Icons9.
Native Icons8 Light/Particle/Dialogue resize and close cases are documented.
Docs staged whitespace check passes. These are selected checks, not hosted CI.

All 65 original UI families are inventoried; 30 have scoped fresh native
observations. Remaining asset states and Runtime/DPI scenarios are explicit in
the CSV. Seven open findings: small-window modal footers/readability (P1);
Physics scroll clipping/tabs; Hub press-time commands; inspector ordering;
remaining visual-role adapters; numeric tooltip overflow; stale Window wording.
Do not claim these remaining findings are fixed or every family native-tested.

Review used copied DialogueReview project and isolated preferences. Disposable
numeric/navigation edits were undone. No owner project, authored dialogue,
authored Screen, Runtime gameplay or save schema changed. The Icons9 review app
was left open. No push, PR, hosted CI, merge or release was performed.

Concurrent wireframe/view-mode work appeared after the reviewed build in
CMakeLists.txt, Studio application/chrome/diagnostics files, source contracts,
a new cmake overlay and docs/WIREFRAME_VIEW_MODE.md. Preserved without staging
in these audit commits. That evolving tree is outside this receipt. Wicked
overlays remain dirty and unstaged; no gitlink update was made by this audit.
Do not rebuild mixed concurrent source and reuse the Icons9 test identity.

---

# Audio-to-asset inspector handoff — 16 November 2026

Implementation/evidence commit: 1663001dd76a2f5962d557182dd149f55e262544
on fix/ui-cohesion-pass1-20261116. This supersedes the toolbar-only correction
below for the new report that Audio stays active after an asset click.

Explicit viewport, legacy/modern hierarchy and completed box selection now
release specialist inspector ownership after updating the existing selection.
Sound/legacy particle/navigation entities retain their typed specialist;
ordinary assets immediately use the Scene inspector. Opening global Audio with
an old ordinary selection remains possible. Physics Lab keeps hierarchy targets.

Release build command (CL=/MP6):
MSBuild BUILD/native/Studio/RenegadeStudio.vcxproj /t:Build
/p:BuildProjectReferences=false /p:Configuration=Release /p:Platform=x64
/p:TargetName=RenegadeStudio-Icons6 /v:minimal
Completed with exit 0, including a final incremental confirmation.
BUILD/native/Studio/Release/RenegadeStudio-Icons6.exe SHA-256:
83FC83470A2798ACA3E5398C7A17DE1C4DA4CD477435583C0509C9BA165EF6ED.
Built before commit; its configure-time Git stamp is not a release identity.

ctest --test-dir BUILD/native -C Release
-R 'SourceContract|StudioUiPolicy|RenegadeDialogue|^RenegadeStoryFlowJourneyUiLayoutTests$'
--output-on-failure
All 67 pass in 12.06 s. git diff --check passes.

Native exact-binary screenshots verify Player selection, opening Audio with
retained selection, and clicking Point Light in the viewport immediately showing
LIGHT // Point Light without a panel click. Reopening Audio and expanding a
hierarchy category also preserve mix ownership until an asset is picked.
The review process closed before model-row, same-asset and Physics native checks;
those are not claimed as passes. Exit cause was not established (no matching
Application Error/WER event found). See docs/evidence/ui-icon-integration.
OS DPI 125/150 percent and owner acceptance remain pending.

The existing Icons5 instance was preserved; it is the previous toolbar-only
build and does not contain this fix. The native check used a separate copied
project/preferences in BUILD/ui-audio-selection-review. No owner project edits,
push, PR, hosted CI, merge or release. Dirty Wicked overlays remain unstaged.

---

# Audio/prefab context correction — owner confirmed 16 November 2026

Implementation commit: 239be816e821e7040a59ffb4d3997f927c93ba37.
StudioApplication.cpp now excludes Audio/particle/navigation and Physics from
the selected-object prefab toolbar and its pointer hit targets. The scene editing
guide/footer follows that context; viewport box-selection feedback is preserved.
Audio had reused Scene mode internally, exposing the toolbar for a retained asset
selection. The owner subsequently confirmed the reported issue was fixed.

Release build BUILD/native/Studio/Release/RenegadeStudio-Icons5.exe passed:
ACFD66371311AC6096A27BBB8DBD75AE406840E248A44946BCE4ED6CD5562EF3.
Same MSBuild configuration as below, TargetName=RenegadeStudio-Icons5.
The same 67-test CTest filter passed in 2.75 s. git diff --check passed.
No new screenshot receipt is claimed for this small owner-confirmed correction.
Docs: UI_ICON_INTEGRATION, ARCHITECTURE and FEATURE_MATRIX.

The owner's Icons4 window had unsaved scene edits: do not close or discard it.
Icons5 uses BUILD/ui-audio-context-review and separate preferences. No owner
project or existing window was closed. No push, PR, hosted CI, merge or release.
The existing dirty Wicked overlays remain unstaged. Remaining UI/DPI scope below
is unchanged; owner confirmation applies to this reported context issue.

---

# Approved icon integration and owner corrections — 16 November 2026

Implementation/evidence commit:
5bfe2d4e2ea2f2412d539d91eae8df4d2baac4f0
on fix/ui-cohesion-pass1-20261116, following UI checkpoint 9eb8410.
Source and asset files are unchanged after that commit. This handoff update is
documentation only. No push, PR, hosted CI, merge or release was performed.

Integrated the owner-approved 65-icon SVG catalogue and one padded atlas,
shared square icon rendering, matching-release input, delayed title tooltips,
keyboard focus and compact native controls across Studio and Story Flow.
Changed StudioIcons, Studio chrome/header, Physics chrome, Story Flow chrome/
layout/render path/workspace and return control, Studio asset installation,
four obsolete source-contract expectations, feature matrix and architecture.
Full detail and binary-specific screenshots: docs/UI_ICON_INTEGRATION.md and
docs/evidence/ui-icon-integration/README.md.

Owner review then requested clear space below Fit and reported Audio disappearing
when a viewport menu opens. A shared 192 logical px Journey header now gives Fit
a 9 px gap above the lower row and keeps canvas hit/scissor bounds aligned.
Audio stays rendered during Perspective/Lit/Show menus; popup input ownership
and foreground composition remain. All three dropdown cases and the row gap
were visually verified in the latest native build.

Latest live review executable:
BUILD/native/Studio/Release/RenegadeStudio-Icons4.exe
SHA-256 E6596CA2670F860C0F05B02AB7AA353BA1EF21034E4374D3AE32C7F491A8E2DC.
It uses BUILD/ui-icon-owner-review/DialogueReview.renegade and separate review
preferences. Existing owner-operated Studio windows were preserved. Do not
close them or discard edits. The reviewed project retains seven entities/two
lights. Previous native receipts also cover hold/cancel/release, tooltips,
keyboard Tab/Enter, narrow 1280/1024 layouts and save/reopen. The binary was
built before commit, so its embedded configure-time Git stamp is not the new
implementation hash; do not treat it as a release/package identity receipt.

Configure: cmake -S . -B BUILD/native -DRENEGADE_EMBED_SHADERS=ON.
Build: MSBuild BUILD/native/Studio/RenegadeStudio.vcxproj /t:Build
/p:BuildProjectReferences=false /p:Configuration=Release /p:Platform=x64
/p:TargetName=RenegadeStudio-Icons4 /v:minimal, with CL=/MP6. Exit 0.
Also built RenegadeStoryFlowJourneyUiLayoutTests; its existing tests pass unchanged.
Combined validation:
ctest --test-dir BUILD/native -C Release -R
'SourceContract|StudioUiPolicy|RenegadeDialogue|^RenegadeStoryFlowJourneyUiLayoutTests$'
--output-on-failure
67/67 pass in 9.73 seconds. This is 60 source contracts, UI policy, five dialogue
suites and Journey UI layout. All 65 atlas crops exactly match the approved PNG
artwork. Source and installed atlas hashes match. git diff --check passes.

Remaining: owner acceptance of the latest live UI, real Windows OS DPI checks at
125%/150%, remaining specialist UI audit families, and hosted Debug/Release CI
only when requested. No universal UI/release gate is certified. The six existing
Wicked overlays and gitlink were not staged or modified by icon integration.
Preserve original dialogue and concurrent arms work.

---

# Studio UI cohesion first pass — 16 November 2026

Isolated checkout `E:\RenegadeUI-20261116`, branch
`fix/ui-cohesion-pass1-20261116`, based on dialogue checkpoint `7d4839a`.
The original dialogue checkout and concurrent arms work were not modified.
No push, PR, merge or release is authorized/performed by this UI pass.

The owner requested fixing the audited UI before toolbar icon integration.
Changes: shared opaque charcoal/orange action faces and release-only callbacks;
one twelve-row Add menu; world overlays before GUI; final foreground popups and
loading cover; Physics Lab marker suppression; flow-owned Pickup/metadata row;
reserved Audio heading; bounded heading/Character text; scrollbar-safe content
width across Scene/Player, Environment, Terrain/River, Material, Render and
Waterfall. The protected importer keeps its accepted native control types.

Native owner review found an inspector occlusion regression caused by promoting
the whole chrome for popup input. This is corrected by exclusive popup updates
without chrome promotion. The owner also exposed separate Environment/Render
sizing paths; all discovered inspector width paths now use the shared policy.

Release Studio and five dialogue regression targets built successfully. Six
CTest suites pass (UI policy plus dialogue core/assets/save policy/display/
scripting). All 60 source contracts pass after preserving importer types and
updating obsolete menu-count/typography expectations. Existing JP01 Physics Lab,
Marker Icons, S1B and protected importer source contracts pass. Pure policy tests
also pass with g++ C++17; width tests cover 280/360/480 px and tiny bounds.

Latest native binary SHA-256:
`9E7E371B96B0EDBF769535D210DB09F2E9E86780E7862499197072A0F1F221AD`.
It is staged as canonical `BUILD/native/Studio/Release/RenegadeStudio.exe`.
The owner closed the preceding session and authorized reopening. Final review
uses `BUILD/ui-correction-review/DialogueReview.renegade` and isolated local
preferences. Do not close an owner-operated Studio session or discard edits.
Native review confirms complete numeric fields in Environment/Render/Player,
immediate newly placed light inspection, and single-click light selection from
both Environment and Render. The saved review scene reopened with seven entities
and two lights. Narrow layer bits wrap into four columns. Evidence records each
binary separately; earlier menu/Physics screenshots are scoped to their capture.
See `docs/UI_COHESION_PASS1.md` for current scope and acceptance targets.

The six pre-existing Wicked working-tree overlays were copied unchanged into
this independent dependency worktree. Do not stage the Wicked gitlink or its
dirty overlay, change pins, or include unrelated arms work. The old dialogue PR
instructions below remain historical and belong to its separate checkout.

---

# PR #196 CI failure correction - 16 November 2026

Failed head: `7d4839ab1b048a8c423dbed76654b3d86d0eeac5`; Studio run
`43906758099` failed the same four tests in Debug and Release. Both Windows
baseline jobs in run `43906757994` passed. Compilation succeeded; the failure
was in the full CTest run, which the earlier 16-suite selection did not cover.

Corrections:
- DialogueDisplayService.cpp now uses filesystem-relative resolution for both
  path spellings instead of comparing a canonical file with a lexical root.
  The added noncanonical-root regression failed on the old implementation
  (exit 1, invalid Content-relative hint) and passes with the correction.
  This covers equivalent root spellings, including Windows short-path aliases.
  Existing project identity/containment/resource validation remains in place.
- Phase5Gate3SourceContract.cmake now checks the ten-item Add menu, including
  the appended Dialogue action, while retaining Decal/Probe/Projectile checks.
- workflows.json refreshes 17 affected UTF-8/LF whole-file fingerprints after
  reviewing dialogue-only changes in StudioApplication, StudioChrome and the
  architecture notes. All existing evidence needles remain present. Recipe
  steps, acceptance limits and fail-closed hash checks are unchanged.

Validation before updating the PR:
`cmake --build BUILD/native --config Release --parallel 4 --target RenegadeDialogueDisplayTests`
succeeded. `ctest --test-dir BUILD/native -C Release -R 'SourceContract|Assistant|RenegadeDialogue' --output-on-failure`
passed all 70 selected tests in 41.80 seconds (exit 0), including all four CI
failures, the complete offline assistant suite, Git-free packaged knowledge,
and the display-root regression. The local Git safety override was supplied
per process for this ownership-less drive; no production/test checks were
disabled. Python caches generated by these checks are removed before staging.

Changed files: the three code/test files above, the assistant workflow registry,
this handoff and the feature-matrix validation field. Existing native UI acceptance
is unchanged. Hosted Debug/Release reruns are still required at the corrective
head; local passes do not establish four green hosted checks. Keep PR #196
unmerged. Preserve the unrelated UI-audit/arms work and existing Wicked overlay.

---

# Dialogue PR preparation - 16 November 2026

The owner authorized a separate dialogue PR and its four hosted CI checks.
Merge is not authorized. Latest fetched main is `7093cf4da660cebd7c686aac1b44d6e00181f556`
and is already an ancestor of the dialogue branch (0 commits behind). The
previous validated checkpoint is `db14f27`; runtime/editor behaviour is unchanged.

Pre-publication inspection found two registered suites absent from the explicit
CI aggregate. CMakeLists.txt now includes RenegadeDialogueScriptingTests and
RenegadeDialogueDisplayTests in both RenegadeStudioCI and RenegadeDialogueReview.
Configure succeeded; the generated Studio CI project explicitly references all
five dialogue executables. Both newer targets rebuilt successfully and the
same 16-suite regression set passed again in 9.95 seconds (exit 0). Commands:
`cmake -S . -B BUILD/native -DRENEGADE_EMBED_SHADERS=ON`, then
`cmake --build BUILD/native --config Release --parallel 4 --target RenegadeDialogueScriptingTests RenegadeDialogueDisplayTests`,
then the same 16-suite CTest filter recorded below. The feature matrix now
records owner acceptance and pending hosted checks.

Publish the existing feature/dialogue-editor-20261115 branch with a normal
fast-forward push, open a focused PR against main, and verify both Debug/Release
jobs in each automatic Studio and Windows baseline workflow. No workflow edits,
Wicked gitlink changes, unrelated arms work or UI-audit changes are included.
The pre-existing Wicked working-tree overlay/copied baseline remains untouched.
Do not merge or claim hosted CI success until the checks complete.

---

# Basic dialogue beta validation - 16 November 2026

Validated checkout: `00c2146de0668f9a5d3b65cd246535760352df70`.
Functional Studio source: `d38d23eafc7e38c8a86b3ee7fec59d2ff98b2e58`;
subsequent commits contain documentation only. The owner confirmed "works
perfectly" after the Advanced correction, closing its pending owner review.
This supersedes the older live-session/unsaved-state notes below: Studio was
closed at the start of final validation. A fresh isolated review was launched;
input stopped when another window had foreground focus. No owner project or
unsaved conversation was discarded, and no new automated visual proof is claimed.

Release Studio, Runtime and all selected test targets built successfully.
All 16 related CTest suites PASS in 10.07 seconds: dialogue core/assets/save
policy/scripting/display, governed Lua and its source contract, Screen runtime/
renderer/authoring/history/completion, Test Level snapshots and native reload,
game save slots, scene transitions and native window startup. Build/check
process exited 0. No implementation changes were needed during this pass.

Commands (repository root, existing embedded-shader native configuration):
```powershell
cmake --build BUILD/native --config Release --parallel 4 --target RenegadeStudio RenegadeRuntime RenegadeDialogueTests RenegadeDialogueAssetTests RenegadeDialogueSavePolicyTests RenegadeDialogueDisplayTests RenegadeDialogueScriptingTests RenegadeS3GovernedLuaRuntimeTests RenegadeScreenAuthoringSessionTests RenegadeScreenAuthoringCompletionTests RenegadeScreenRendererTests RenegadeRuntimeScreenTests RenegadeTestLevelSnapshotTests RenegadeTestLevelSnapshotRuntimeTests RenegadeRuntimeGameSaveSlotTests RenegadeRuntimeSceneTransitionTests RenegadeNativeWindowSurfaceTests
ctest --test-dir BUILD/native -C Release -R 'RenegadeDialogue|RenegadeS3GovernedLuaRuntime|RenegadeScreenAuthoringSessionTests|RenegadeScreenAuthoringCompletionTests|RenegadeTestLevelSnapshot|RenegadeRuntimeGameSaveSlotTests|RenegadeScreenRendererTests|RenegadeRuntimeScreenTests|RenegadeRuntimeSceneTransitionTests|RenegadeNativeWindowSurfaceTests' --output-on-failure
```

SHA-256 of the validated binaries:
- Studio: `9CBA89A276EC9B1F3FBEAECE9E0380A5D3FEC73C05B57B2A820716706A7663B3`
- Runtime: `AE904D6B689FB31889C5BF6A1F2DBE863B83420BC93EE8AA2B2F955F5B9EB95F`

Basic dialogue implementation and local validation are complete. Existing native
game/display and scripting receipts remain below. This is not all-engine Debug/
Release CI, packaged distribution, or an independent release-checklist pass.
Next integration step is a separately authorized PR/CI and exact-commit review.
No push, PR, CI, merge or release was performed. Preserve the existing Wicked
overlay/copied baseline and unrelated arms/Blender work.

---

# Advanced controls visibility correction - 16 November 2026

Latest LOCAL Studio source: d38d23eafc7e38c8a86b3ee7fec59d2ff98b2e58.
The owner reported that Advanced appeared to expose no controls while an NPC
line was selected. Conditions/outcomes belong to player replies and actions to
outcomes; the existing tree retained Basic mode's collapsed reply state.
DialogueEditorWindow.cpp now expands rule branches when Advanced is enabled,
selects the first player reply when invoked from an NPC with replies, and shows
selection-specific instructions. Show in Advanced also forces rule expansion.
This changes authoring selection/visibility only, not conversation assets/runtime.

Release Studio built successfully. The five focused dialogue CTest suites PASS
in 0.75 seconds. The earlier shared display build has 14 passing suites and native
UI/game evidence below. The new automatic selection needs final native owner
review; the currently open review window still holds the previous UI and the
owner's UNSAVED conversation edits. Preserve it. The updated normal Studio
executable and on-disk review executable are ready for the next launch. The old
loaded review binary was renamed to a timestamped before-advanced backup, leaving
the live process and its unsaved edits intact. Do not restart that active review
session without preserving its edits. In that current session, Advanced then selecting a player
reply exposes the existing tools; selecting an outcome exposes actions.

An annotated guide was generated from the exact selected screenshot. It is a
labelled instructional image, not native software verification. No generated
image is used as implementation evidence. No push, PR, CI, merge or release.

---

# Editable dialogue displays - 16 November 2026

Latest LOCAL engine source: 4e8f1fa44c565411d00ccc2f494ceb6cb9de7927.
Branch feature/dialogue-editor-20261115, main baseline 7093cf4.
Owner required creator-editable dialogue boxes before signoff and approved the
shown direction with "this looks good". This is visual approval, not permission
for a PR, CI, merge or distribution. All work remains local; PR #195 stays closed.

The Dialogue Editor now offers a project display selector and Customise/Edit
Display. New creates a normal project-owned Screen template and opens the existing
native UI editor through its standalone handoff. Speaker/text/reply/navigation
roles are separate from names, content, styles and geometry. The normal Style /
Font, graphic browser, layout, Undo/Redo and save workflow applies. The project
selection and typed packaging roots persist; changing display removes the old
selection's explicit root. The shared panel adapts the template for preview,
Interact and scripted conversations, preserving game logic, audio and memory.

Changed code: DialogueDisplayService.h/.cpp; ScreenService.h/.cpp;
ScreenAuthoringSession.h; TestLevelSnapshotService.cpp; DialoguePanel.h/.cpp;
RuntimeDialogue.cpp; DialogueEditorWindow.h/.cpp; StudioApplication.h/.cpp;
RenegadeScreenEditorRenderPath.h; RenegadeScreenEditorWorkspace.cpp; bridge/test
CMake lists; DialogueDisplayTests.cpp. See docs/DIALOGUE_DISPLAYS.md.

Validation:
- Release Runtime and native Studio builds succeed; the latest delivery targets
  the normal RenegadeStudio.exe without closing the owner's review window.
- 14 focused CTest suites PASS in 7.61 seconds at the final source. Receipt:
  docs/evidence/dialogue-display/README.md.
- Integration tests cover roles, renamed elements, custom styles/geometry, reply
  pagination, Continue/Close, native Screen history/Undo, save/reload, assignment,
  foreign project rejection, missing roles/hidden parents and staged fonts/artwork.
- Native Runtime at 68c7dcc showed a deliberately different project layout using
  Georgia, gold text, a brown opaque background, changed size and screen position.
  Enter selected work -> captain with matching Sael audio and a visible Continue.
- Native Studio at 2de8dff opened the assigned display through Edit display in the
  actual UI editor. Full artboard, Layers, role-bearing text and font controls
  were inspected. The owner's input moved to another window during the final
  Apply/save/return interaction, so automation stopped. This last mouse-driven
  round trip remains for owner review; automated persistence checks passed. The
  owner subsequently returned to the conversation workspace and reported its
  header overlap, confirming the native return handoff was reachable.
- Initial native review exposed the audio-example note overlapping the new
  selector/button, then the selection heading. Final placement is in the preview
  caption area. The editor's return label is now generic Back for either origin.
- Missing resources or renderer rejection keep runtime dialogue playable via the
  built-in display; staging rejects incomplete assets so creators can repair them.

Reproduce:
cmake -S . -B BUILD/native -DRENEGADE_EMBED_SHADERS=ON
cmake --build BUILD/native --config Release --parallel 4 --target RenegadeRuntime RenegadeDialogueDisplayTests RenegadeDialogueScriptingTests RenegadeS3GovernedLuaRuntimeTests RenegadeScreenAuthoringSessionTests RenegadeScreenAuthoringCompletionTests RenegadeScreenRendererTests RenegadeRuntimeScreenTests RenegadeTestLevelSnapshotRuntimeTests RenegadeRuntimeGameSaveSlotTests
cmake --build BUILD/native --config Release --target RenegadeStudio -- /p:BuildProjectReferences=false
ctest --test-dir BUILD/native -C Release -R 'RenegadeDialogue|RenegadeS3GovernedLuaRuntime|RenegadeScreenAuthoringSessionTests|RenegadeScreenAuthoringCompletionTests|RenegadeTestLevelSnapshot|RenegadeRuntimeGameSaveSlotTests|RenegadeScreenRendererTests|RenegadeRuntimeScreenTests' --output-on-failure

No current package-distribution or independent release acceptance is claimed.
The owner then reported the NPC LINE overlap from the earlier review candidate.
The review executable was refreshed from the final normal Studio binary; both
now have identical contents. The example-audio note lives in the preview caption
area, clear of NPC LINE and the display selector. This was visually confirmed
in the refreshed live review at the owner's resolution; see header-fixed.png in
the receipt. The owner currently has unsaved conversation edits open in the
review window: preserve them and do not restart it again. Preserve the
owner's original projects, unrelated arms work and the existing Wicked overlay
state. The separate review project is disposable and contains copied example
recordings; none were generated for this task.

---

# Dialogue scripting beta integration - 16 November 2026

Tested LOCAL engine source: bedcf30f744f2b4ce9c3432b96084573879f6199.
Branch feature/dialogue-editor-20261115; main baseline 7093cf4.
Owner chose to retain the current simple editor for beta and authorized scripting
support. No node redesign, generated voices, push, PR, CI, merge or release was
performed. PR #195 remains closed and unmerged. Original owner project and the
separate arms/weapon work were preserved; Wicked pin/overlays were not changed.

Added renegade.dialogue contract 1 to the existing governed creator Lua runtime:
- Start a saved conversation by NPC EntityRef, exact name or asset ID, optionally
  at an authored line ID. An active conversation cannot be silently replaced.
- Stop/query active state and get/set shared flags and signed integer values.
- Lifecycle events dialogue.started / dialogue.finished / dialogue.closed through
  the existing FIFO, plus the existing authored dialogue.action event.
- State, Screen and voice use the same RuntimeDialogue session as NPC Interact.
  Scene preparation precedes Lua on_start. Setters refresh reply availability
  without replaying the spoken line. Saved-state codec/schema remains unchanged.
- Reject stale/deleted NPC references, ambiguous names, bad line IDs, embedded NUL,
  invalid keys/types, integer overflow and memory growth beyond saved capacity.
- Fix native Runtime startup requested as minimised: wait for a drawable window
  before Run can start engine initialisers with a null graphics device.

Validation at the tested source:
- Release Runtime and focused targets built successfully.
- 13 focused CTest suites PASS in 5.28 seconds; GNU C++17 core 50 checks PASS.
- New integration test executes governed Lua against real saved dialogue assets
  and RuntimeDialogue, not a callback-only mock: conditional permit/refusal,
  authored action event, shared memory/codec, lifecycle counts, deleted NPC,
  duplicate names, invalid inputs and capacity limit.
- Separate native Runtime: minimised launch responsive, restore, on_start starts
  captain by name, stop, start permit by asset/line ID, two start/two close events,
  retained flag/value, matching captain/permit voice clips, graceful exit 0.
  QA uses a deliberate terminal Lua error marker only after all assertions pass;
  this disables the disposable proof script, not a production error acceptance.
- Native receipt: docs/evidence/dialogue-scripting/README.md.
- Initial aggregate could not replace the owner's open Studio executable
  (LNK1104). A separate Studio candidate is built alongside it; the open editor
  is not terminated. RenegadeStudio-DialogueScripting.exe linked successfully
  alongside it. Runtime changes are available to its next Test Game launch.

Reproduce from repo root with the normal embedded-shader configuration:
cmake -S . -B BUILD/native -DRENEGADE_EMBED_SHADERS=ON
cmake --build BUILD/native --config Release --parallel 4 --target RenegadeRuntime RenegadeDialogueScriptingTests RenegadeS3GovernedLuaRuntimeTests RenegadeRuntimeSceneTransitionTests RenegadeNativeWindowSurfaceTests
ctest --test-dir BUILD/native -C Release -R 'RenegadeDialogue|RenegadeS3GovernedLuaRuntime|RenegadeRuntimeSceneTransitionTests|RenegadeTestLevelSnapshot|RenegadeRuntimeGameSaveSlotTests|RenegadeScreenRendererTests|RenegadeRuntimeScreenTests|RenegadeNativeWindowSurfaceTests' --output-on-failure

Creator API and copyable NPC example: docs/DIALOGUE_SCRIPTING.md.
The editor remains the same simple/Advanced authoring UI accepted at ed2e69a.
New package distribution, new owner visual acceptance and independent release
acceptance are not claimed. Automatic conditional NPC continuation, quest-specific
services and graph editing remain outside this beta integration.

---

# Dialogue editor usability review - 16 November 2026

Tested LOCAL engine commit: ed2e69ab410045bbad3b6db384fe80b6bc008c17.
Branch feature/dialogue-editor-20261115; base main 7093cf4.
The owner tested this native build and reported: "that all seems to work now".
PR #195 remains CLOSED unmerged. No commits were pushed and no CI/merge/release
was started. Owner review of this UI does not authorize those further actions.
Other arms/weapon work and the pinned Wicked revision are unchanged.

Current behaviour:
- Studio dark/orange styling and native fonts; distinct hover/pressed/disabled
  feedback, one mouse-release action, tooltips and three numbered workflow steps.
- Valid edits refresh preview without Save. Progress/private memory/voice visits
  remain stable; unfinished fields retain the last valid preview with a hint.
- Preview end/stop keeps a visible terminal card with Restart. A replacement
  button cannot consume the click that ended the previous line.
- Reply rows show their next response or ending. Unused NPC lines explain that
  a reply's Continue to connects them; Add response creates a linked line.
- Local WAV/OGG import adopts a validated copy without overwriting clips. Open
  voice folder and Refresh clips expose local recordings. New lines can stay
  text-only; a visible note identifies example clips and invites creator audio.
  Existing Sael take-2 recordings remain example assets; no new voices generated.
- NPC chooser, attachment status, Save & attach and Test in game provide the
  complete authoring path. Player Start markers are excluded as dialogue targets.
- The Runtime prompt names the project's actual Interact binding (E by default).
- The basic guard example uses literal fixed responses. Its old hidden permit
  flag/actions caused a later Let me through choice to approve entry. Conditional
  behaviour remains tested separately and available through Advanced. The review
  asset was updated without changing the owner's new captain connection.

Changed code in the latest revision: DialogueService.h/.cpp, DialogueEditorWindow.cpp,
RuntimeDialogue.h/.cpp, RuntimeApplication.cpp, DialogueTests.cpp. Earlier local
usability commits added voice adoption, asset tests, the dialog launch integration,
button/tooltip feedback and shared preview input isolation.

Validation:
- GNU C++17 core suite: 50 checks PASS.
- Release Studio/Runtime and aggregate/focused targets built successfully.
- Nine focused CTest suites PASS in 4.94 seconds at the tested engine source.
- Tests cover live unsaved text/connection changes, voice-visit and memory
  preservation, incomplete edits, deleting the displayed line, basic refusal even
  with an old permit flag, Advanced conditions/actions/save memory, clip adoption,
  serialization/snapshot/binding Undo/Redo and native scene reopen.
- Native 7753f94 restored all controls with no outer scrolling. Its pressed Test
  in game button showed depressed fill and the normal launch path saved the
  current conversation/scene and launched the separate Runtime.
- Native ed2e69a owner Test Game log recorded work -> captain and matching audio,
  then gate -> refused and sael-refused.wav, continuation -> opening, and bye ->
  end. Owner reported that the changes work after reviewing the new build.

Commands:
cmake -S . -B BUILD/native -DRENEGADE_EMBED_SHADERS=ON
cmake --build BUILD/native --config Release --parallel 4 --target RenegadeDialogueReview RenegadeRuntimeSceneTransitionTests RenegadeScreenRendererTests RenegadeRuntimeScreenTests
ctest --test-dir BUILD/native -C Release -R 'RenegadeDialogue|RenegadeRuntimeSceneTransitionTests|RenegadeTestLevelSnapshot|RenegadeRuntimeGameSaveSlotTests|RenegadeScreenRendererTests|RenegadeRuntimeScreenTests' --output-on-failure
g++ -std=c++17 -I EngineBridge/include EngineBridge/src/DialogueService.cpp Tests/DialogueTests.cpp -o dialogue-tests
./dialogue-tests

Preserved failures: 4a39dc3 could click through to the replacement preview button
and allowed outer scrolling. 29dcf54 fixed the preview gesture but directly
parenting controls to the scaled window hid them off-screen (owner screenshots).
7753f94 restored the native unit-scale content parent and removed only the outer
scrollbars. The opaque 2D composition resize fix remains; no unwritten suspended
3D targets are sampled. Earlier responsive exits had code 0, not an application
fault receipt. No blanket crash/performance pass is claimed.

Limits: Debug/CI have not run for these local revisions. A new packaged build
has not been verified at ed2e69a; older package receipts retain their original
scope. Runtime was exercised as the actual separate Test Game player, not an
embedded simulation. Controller hardware, live pagination, explicit manual
file-picker save/reopen coverage and populated-inventory native save remain
separate checks. Do not promote these into an independent release-gate PASS.

---

# Renegade engineering handoff

**Current reconciled baseline:** main `618b2310ec7d6a7889d490e93304e39043f7e5ff`, 10 November 2026.
**Wicked pin:** `82557d439a41d104f559b0d3134d160b7bf6154e`.
The prefab/selection branch has been merged as PR #193. Lighting/particles were
already merged as PR #192. There is no unresolved integration conflict between
those streams. Do not restart prefab implementation from an old local checkpoint.

## Completed latest work

PR #193 feature head `cb590f06267e3407633d4e8f68bbde9265dd6ec1` includes ordered
multi-selection, Shift-toggle in hierarchy/viewport, directional box selection,
shared-pivot transforms, batch duplicate/delete, Group/Edit Pieces/Unpack and
native reusable scene assemblies. Scripted prefabs retain typed properties,
resources and modules; placement assigns fresh instance identities and remaps
internal references. Delete/duplicate/Unpack use existing history/companions.
Packaged load preserves authored instance edits rather than replacing them.

Latest validation: Release Studio/Runtime/proof builds; 11 focused checks;
60 source contracts; all six earlier failures rechecked; original geometry proof;
complete functional brazier proof; actual Studio save/card drag/Unpack/Save/Reopen;
package-relative standalone render with four governed scripts started. Evidence:
[functional prefab receipt](docs/evidence/scene-prefab-functional/README.md).

Four pre-merge checks succeeded on `cb590f0`:
[Studio Debug/Release](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/actions/runs/42857306536) and
[Windows baseline Debug/Release](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/actions/runs/42857306652).
Owner approved merging and merged #193 on 10 November. This records source
integration and scoped acceptance, not every independent release gate.

## Current documentation task

Branch: `docs/current-main-reconciliation-20261110`, based on the main SHA above.
Scope: current summaries, roadmap, architecture orientation, feature ledger,
build/upstream guidance, module READMEs, active creator/assistant guides,
documentation index and stale status/encoding reconciliation. No engine code,
workflow behaviour, assets, executable outputs or submodule pointer changes.

The documentation commit uses `[skip ci]` at the owner's explicit request.
Run local documentation, source-contract and assistant trust checks; do not
dispatch CI. Skipped required checks can remain pending. Do not change branch
protection or manufacture a green result to merge documentation.

## Boundaries and known follow-ups

- Placed prefab copies are independent; linked updates/override management are
  not implemented. External entity targets must be included or cleared.
- No explicit prefab entity-count cap; large castle assemblies are untested.
- Plugin/marketplace/third-party shader-editor discussions are proposed work,
  not implemented features or an instruction to begin them.
- Preserve existing castle assets, owner worktrees, private projects and backups.
  The separate prefab and picker test projects are not distributable releases.
- Preserve the pinned Wicked gitlink. Main's CMake-generated sky/weather,
  allocator and particle overlays are documented core-patch decisions, not a
  reason to commit a dirty submodule or reset another checkout.
- Main/alpha distribution and future merges require the owner's approval.
  No fresh alpha distribution is authorized here.

## Working context and history

Read [CURRENT_STATUS](docs/CURRENT_STATUS.md), [documentation index](docs/README.md),
[ROADMAP](docs/ROADMAP.md), [ARCHITECTURE](docs/ARCHITECTURE.md) and the relevant
[FEATURE_MATRIX](docs/FEATURE_MATRIX.csv) rows before acting.
The [complete pre-reconciliation handoff](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/blob/618b2310ec7d6a7889d490e93304e39043f7e5ff/HANDOFF.md) preserves
the earlier commands, failures, snapshots and checkpoints. Dated evidence remains
in the repository. Historical “no merge”, “CI pending” and machine observations
apply to their original checkpoint, not automatically to today's main.

Final follow-up: Wireframe5 fresh-process mode restoration, readable ASCII
markers and corrected shaded overlay were visually verified. Its review window
is left open. Source implementation remains fada1d6353691a14508b9b748ec8335631984cfb.
