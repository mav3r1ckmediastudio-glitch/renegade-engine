# PR #197 UMA/wireframe recovery — 17 November 2026

PR #197 remains draft. Original tip 585407a: both baseline jobs passed;
Studio Debug and Release failed. No merge or release occurred.
Correction: affe8d14ecde5e92302658368dc841b45c3f09e8 composes wireframe on the existing UMA stage into
a distinct final renderer. Both shader targets compile exactly one composed
source. A cold/reconfigure regression rejects the original generator.

Actual WARP: same allocator failure reproduced before correction; corrected
Debug 11/11 and Release 11/11 affected checks pass. Source/help/policy 66/66
pass. Normal NVIDIA allocator and native Release Studio viewport/modal
review pass with test-WARP OFF. Test assertions/deadlines are unchanged.
This is scoped recovery validation, not a new full local 322/335 run.

Read docs/UI_PR197_UMA_RECOVERY_20261117.md and its evidence index.
Next: one corrective push to the existing branch, retain draft, and verify
all four hosted checks SUCCESS at the exact corrected PR head. Do not
recommend merge based on mergeable status or historical local results.
No merge or release is authorized. Existing Wicked overlays are preserved.

---

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

## Terrain props P0/P1 handover, 17 November 2026

**Branch** `feature/terrain-props-p0-p1`. Implementation head `e4cec253360b94edf3a0bce6c3931e1c2cf841b6`, based on `db8d620f75a5219ae5a3906e2837ed2d4165c741`. The handover commit adds documentation and the verified player capture. Run `git log -1 --format=%H -- HANDOFF.md` on this branch for its separate commit. No push, PR or merge was made.

The isolated checkout retains configured Wicked edits to the two particle shader files, particle shader interop header, emitted-particle source/header, allocator header and four archive-overlay files (`wiECS.h`, `wiScene.cpp`, `wiScene_Components.h`, `wiScene_Serializers.cpp`). They are recreated by configuration. There is no uncommitted parent-repository implementation and neither submodule pointer changed. Other working trees were not reset or reused. Build products and raw logs live under ignored `BUILD/`.

[Data formats, archive decision and service contracts](docs/TERRAIN_PROP_P1.md) · [Actual standalone player capture](docs/evidence/terrain-props/runtime-slice.png). The complete source, test, fixture and contract file list is available with `git diff --name-status db8d620 e4cec25`.

**Local evidence.** Windows x64, Intel i9-13900K, 128 GB installed RAM, RTX 4070 Ti, DX12, MSVC 19.51, CMake 4.4.4. A deterministic technical scene imported the original two-part tree, bush and boulder through the normal static-model committer. The 96 placements produced 128 native objects referencing four distinct mesh IDs and four distinct material IDs. Each mesh was reused 32 times. The four native mesh buffers totalled 3,392 bytes for these deliberately small fixtures. The unmodified scene had one LOD per mesh and no impostor atlas. Adding a tiny second subset to a mesh in the disposable benchmark kept its aligned geometry buffer at 608 bytes. Enabling one native impostor allocated an 864-slice atlas with 14,155,776 bytes of texel payload.

The scene had 64 static bodies. Jolt rays hit trunk and boulder, missed crown and bush, and missed a removed trunk while still hitting the boulder. Regular selection, hierarchy queries and duplicate refused a live projected object. Saving while the projections were live produced a raw WISCENE with both authored witnesses and zero projected objects, meshes, materials or rigid bodies. Studio's document service and the Runtime scene loader rebuilt the same population. Save As retained distinct old and new revisions and preserved placement ID 1 through a scale change. The reopened allocation counter returned 97. A withheld page made both save and open fail with the live scene intact. An injected replacement failure retained the previous revision. A separately interrupted replacement recovered both old files from its journal, then retry committed the new page.

The DDS probe observed a 5,592,552-byte backed texture through native resource state. The shared material test reads the current base-colour slot rather than inferring resolution from the request. An exclusive handle held the texture at its cold size, and unlocking the file allowed promotion.

| Native measurement | Cold | Requested 1024 | Released request |
| --- | ---: | ---: | ---: |
| Resident width | 64 | 1,024 | 64 |
| Actual resident / full mips | 7 / 11 | 11 / 11 | 7 / 11 |
| Resident mip payload, bytes | 21,844 | 5,592,404 | 21,844 |
| DXGI device usage in the shared-material run, bytes | 276,824,064 | 293,601,280 | 293,601,280 |
| Process peak working set in that run, bytes | 565,514,240 | 582,778,880 | 582,778,880 |

The DXGI device-usage counter includes pooled allocations and can stay at its high value after the native mip chain shrinks. These are per-texture mip payloads. Physical memory savings, transient peaks attributable solely to streaming, per-frame disk bandwidth and a representative woodland frame-time budget remain unestablished. The original Release probe captured the three states then faulted during process teardown. The final harness drains renderer pipeline work, joins the jobs and waits for the GPU while the device still exists. The accepted test requires normal process exit as well as the descriptor transitions.

**Remaining limits.** Only the project workflow and minimal resident projection have been exercised. A moved or exported standalone package without its project asset registry, a detached Test Game snapshot outside the project, cross-process edits to the same layer, asset reimport during an active projection, older backup retention under garbage collection and large forest performance need separate proofs. The slice's colours and deliberately low-poly meshes are test content. No streaming budget, impostor content bake, automatic texture cooker, collision-interest radius or creator UI ships in this change. The 60 FPS / 16.67 ms target remains a proposed performance target. Independent review of the exact commit is outstanding.

**Sol's next change is P2 only.**

1. Add layer recipe and mix persistence, seed, stable candidate keys, clearing/tombstone and override records with v1 read compatibility. Add delta-based bridge editing against the current revision. Preserve the placement ID high-water mark across undo, redo and save.
2. Implement a scene-safe live projection update for changed pages. Commit authoring changes as one logical operation, keep the old display on failure and discard cancelled staging. Verify changed bounds and IDs across adjacent cells and edits at terrain edges.
3. Add the Terrain Props palette using the existing asset picker, linked density slider/numeric input per entry, saved mixes, Generate, Paint, Erase, progress and cancellation. Sample by distance and keep one Undo entry per stroke or Generate Apply.
4. Test deterministic output, cross-cell spacing, manual clearings, retained IDs, terrain expansion/reprojection, narrow-panel layout, Save As and reopen. Leave the proposed collision, asset and texture streaming services at the interface stage.

**Before Sol pushes** refresh `origin/main`, check the combined diff and configuration against the new accepted baseline, verify both gitlinks, and review any local dirt in the correct worktree. From a fresh isolated checkout run:

```powershell
git submodule update --init --recursive
git diff --check origin/main...HEAD
git ls-tree HEAD WickedEngine ThirdParty/imnodes
cmake -S . -B BUILD/native -G "Visual Studio 18 2026" -A x64
$targets = @('RenegadeStudio','RenegadeRuntime','RenegadeTerrainPropPageTests','RenegadeTerrainPropResidencyProbe','RenegadeEditorPointLookupFocusedTests','RenegadeProjectDocumentTransactionTests')
foreach ($cfg in 'Debug','Release') {
    cmake --build BUILD/native --config $cfg --target $targets --parallel 3
    if ($LASTEXITCODE) { throw "Build failed in $cfg" }
    ctest --test-dir BUILD/native -C $cfg -R 'Renegade(TerrainProp|EditorPointLookup|ProjectDocumentTransaction)' --output-on-failure
    if ($LASTEXITCODE) { throw "Focused checks failed in $cfg" }
}
cmake -S . -B BUILD/warp -G "Visual Studio 18 2026" -A x64 -DRENEGADE_TEST_FORCE_WARP=ON
cmake --build BUILD/warp --config Release --target RenegadeTerrainPropResidencyProbe --parallel 2
ctest --test-dir BUILD/warp -C Release -R 'RenegadeTerrainProp(ResidencyProbe|SliceProof)' --output-on-failure
```

Run Sol's new P2 behaviour tests, the repository Studio/Runtime Debug and Release preflight, and the UMA/wireframe overlay regression checks on that combined tree. Run the normal editor workflow and the separate player with a populated project, inspect the captures, and record frame-time evidence at matched settings. A green local build alone does not demonstrate the authoring workflow. After pushing, verify all four hosted checks belong to the pushed head. Merge requires the owner's approval.

**Build record.** Final NVIDIA Release focused checks passed 6/6. The standalone Runtime captured frame 60 from the project and exited with code 0. Studio and Runtime built in Debug and Release. Debug focused CTest passed 4/4 and the separate Debug native slice exited 0. The fresh WARP Release build passed both native tests (2/2, Microsoft Basic Render Driver). Local logs under BUILD include final-Release-test.log, validated-Debug-test.log, debug-confirm.log, debug-confirm-exit.log, warp-test.log and validated-capture.log. The additional baseline renderer-overlay CTest exceeded its 60-second timeout twice on E:. The same cold/reconfigure fixture completed successfully on the system temporary drive, exit 0 (overlay-confirm.log). Repeat the registered overlay check before pushing. All feature and handover edits are committed on this branch. The configured WickedEngine worktree shows ten generated overlay edits; its pinned gitlink and the imnodes gitlink are unchanged.

## Terrain props P2 authoring handover, 17 November 2026

Implementation head: `01d7c012b8b4ebe6caacac74eaf80d5eaa91b0cb` on `feature/terrain-props-p0-p1`, following P0/P1 handover `2853b55c55f7d7aeee8aaa8e9f69d2bba02ca2b8`. The final documentation/evidence commit is separate; obtain it with `git log -1 --format=%H -- HANDOFF.md`. Owner acceptance and merge approval remain pending.

**Scope delivered.** P2 authoring only: v2 recipes, per-model density, saved layer mixes, stable candidate keys/bindings, clearings/tombstones and overrides; v1 reads; distance-sampled Generate/Paint/Erase and deliberate terrain reprojection/expansion; finite native terrain snapshots; changed-page live projection with rollback; one history command per Apply/stroke; allocation high-water preserved through Undo/Redo/save. The Terrain Props palette uses governed model selection and linked numeric/slider controls. Failure/cancellation retain the old display. Layer-owner deletion/recovery and divergent/stale revisions are guarded.

[P2 contract and format](docs/TERRAIN_PROP_P2.md), [all 34 implementation paths](docs/evidence/terrain-props-p2/implementation-files.txt), and [native evidence and limitations](docs/evidence/terrain-props-p2/README.md). Collision-interest streaming, full asset/texture streaming, budgets/eviction and cooking remain deferred; their proposed interfaces remain in TERRAIN_PROP_P1.md.

**Baseline and concurrency.** Fresh `origin/main` remained `db8d620f75a5219ae5a3906e2837ed2d4165c741` during final source validation. Combined `git diff --check origin/main...HEAD` passed. Wicked remains `82557d439a41d104f559b0d3134d160b7bf6154e`; imnodes remains `eb36902c892548ef94f88f51ad7e7c9c7058a71c`. The documented ten configuration-generated Wicked overlay edits remain uncommitted; neither gitlink changed. Other working trees and the owner's running editor were preserved.

**Source review.** A separate review of the frozen implementation identified and verified closure of four issues: extreme/nonfinite numeric input, cancellation/work accounting inside dense retained-placement scans, changed bounds for legacy placements outside their historical page cell, and repeated owner discovery after an unchanged resolution failure. The final pure authoring test reports 59,752 assertions.

**Native creator workflow.** Final native scripted runs at inspector widths 520 and 280 generated 1,081 placements, erased to 1,062, verified Undo/Redo and that regeneration respects the clearing, painted to 1,076, cancelled staging, then saved and reopened 1,076. Real shared numeric callbacks exercised FLT_MAX, negative FLT_MAX and NaN. Actual full-window captures were inspected after render/layout stabilization; all numeric fields and lower controls are visible. Private test desktops avoided the owner's mouse/keyboard. The enlarged 1600px capture window is separate from the original measured viewport.

The separate player loaded the exact scene bytes from the final editor proof, captured frame 60 and exited normally with code 0. Scene SHA-256: `04F17C9B0C7414A9E921E6573FDB45C5B76F822943E0DD9BFC7300A137B972DC`; Runtime SHA-256: `2152D68025CE50BB46A16C896C5D95C8C2A6851CB5925335B78654F9DA59EB17`. Its Runtime log contained no warning, error or missing-file matches. The disposable player layout staged the existing default-grass fixture support explicitly; this is ordinary project-mode proof, not arbitrary exported-package closure.

**Performance limitation.** The test models are deliberately tiny. Earlier P0/P1 mesh buffers totalled only 3,392 bytes. Roughly 75 FPS / 13ms intervals under the display cap do not establish headroom or performance with detailed assets. No production woodland FPS, geometry/overdraw budget, shadow cost, physical memory saving or real-asset streaming budget is claimed. A representative uncapped benchmark with the owner's actual assets remains necessary before making those claims.

### Final local validation

| Check | Result / receipt |
| --- | --- |
| Release complete preflight | PASS, including 341/341 registered tests, package archives, LP05 dependency proof and LC01 init/update/move/reopen registry proof; `BUILD/p2-preflight-Release-confirmed/build-result.json` |
| Debug complete preflight | PASS, including 326/326 registered tests, package archives and LP05/LC01 subprocess proofs; isolated p2-preflight-Debug-physical/build-result.json |
| Debug pure authoring | 59,752 assertions, exit 0; `authoring-debug.txt` in evidence |
| WARP Release | 4/4 PASS in 120.85s: native allocator, registered overlay, texture residency and slice; isolated `p2-final56-warp-test.log` |
| Release constructor startup smoke | exit 0; `BUILD/p2-startup-smoke-release.json` |
| Final editor / separate player | PASS as described above; captures in `docs/evidence/terrain-props-p2/` |

The Debug registered suite has 326 cases; fifteen native graphics cases are Release-only by existing registration. The documented direct Debug residency and native slice both exited 0, as did the Debug constructor startup-smoke. Receipt: isolated p2-debug-native-physical/result.json. Debug Studio SHA-256: 65BF18FA0CE13CB152A2690B1CCE4BE4D188519DBB6247377BC768E37073A2F8; probe SHA-256: C47B84EC6CA74FF6E6B73D0EF8BDD6A51857283B6CF3FFC4C4AC5365CA66D7CD.

### Commands and local adaptations

The repository Studio preflight builds both Studio and Runtime, runs the unfiltered registered CTest suite, and assembles/verifies both packages. Its task-local adapter preserved the exact upstream pins and checked the exact ten generated overlay paths and SHA-256 bytes instead of resetting them. No test assertions or deadlines were removed.

```powershell
git fetch origin main
git diff --check origin/main...HEAD
git ls-tree HEAD WickedEngine ThirdParty/imnodes
cmake -S . -B BUILD/native -G "Visual Studio 18 2026" -A x64
powershell -ExecutionPolicy Bypass -File BUILD/Build-P2-Preflight-Local.ps1 -Configuration Release -ArtifactRoot BUILD/p2-preflight-Release-confirmed
ctest --test-dir BUILD/native -C Release -R '^RenegadeAssistantRiverKnowledgeTests$' --output-on-failure
cmake -S . -B BUILD/warp -G "Visual Studio 18 2026" -A x64 -DRENEGADE_TEST_FORCE_WARP=ON
cmake --build BUILD/warp --config Release --target RenegadeTerrainPropResidencyProbe RenegadeGpuUploadBufferTests --parallel 2
ctest --test-dir BUILD/warp -C Release -R 'Renegade(TerrainProp(ResidencyProbe|SliceProof)|GpuUploadBufferTests|RendererOverlayComposition)' --output-on-failure
```

The registered renderer-overlay fixture now performs only its disposable cold/reconfigure work under system temporary storage, keeping logs at the requested build-tree output root. Its original 60-second timeout and UMA/wireframe assertions remain. The negative control still rejects a missing UMA witness.

Validation originally filled the system drive with this task's generated Debug link maps. Only owned generated outputs were moved and regenerable ILK files removed; source/projects/owner files were preserved. The system drive recovered about 55 GiB. Local Debug linking used `/debug /INCREMENTAL:NO`, retaining Debug CRT/assertions and compiler optimization settings. A one-off MSVC C1001 in an existing Story Flow boundary test recovered with the unchanged source in a serial invocation; its registered test passed 1/1. The task-local extra `/MP3` was removed for the final Debug run; standard upstream settings were retained.

The moved validation build used a C-to-E directory junction. Package tests reproduced a working-directory-dependent failure through that junction; the same unchanged executable passed from the physical build directory. All thirteen affected package/fixture tests passed when CTest used the physical directory. The final Debug adapter changes only that test-directory path; the full registered suite is rerun with all checks enabled.

The first frozen Release full suite retained one assistant-knowledge timeout (340/341). Its unchanged isolated retry passed in 3.26s. A complete subsequent preflight passed 341/341, with that test at 2.37s. Earlier disk-full, compiler, path and screenshot-tool failures remain in ignored raw logs; they are not green receipts.

### Remaining scope and next work

Saved mixes are layer-local. The first palette requires one finite terrain and refuses ambiguous multiple authored layers. Legacy key-zero placements remain manual; erase clearings persist until Undo. Cross-process editing, asset reimport during active projection, arbitrary moved/exported populated projects and representative forest performance still need separate proofs. Proceed to later streaming stages only under a new scoped task. No merge without owner approval.


## Terrain props P2 hosted-path correction, 18 November 2026

Native implementation: `59c4e926b4a34e9d1523474e8e06b440ff691867`, following failed hosted PR head `a2522190ff920be4dd48be807b6d10b8f9d7bd56`. The final evidence commit also contains a reviewed architecture-document fingerprint refresh in `Tools/assistant_knowledge/workflows.json`; that exact metadata change was present in both full-preflight working trees. The C++ sources match the implementation commit. Obtain the final documentation/evidence head with `git log -1 --format=%H -- HANDOFF.md`.

**Hosted failure.** Studio compiled in both configurations. The longer hosted checkout path made terrain revision transaction artifacts exceed ordinary Windows file-open limits. Release failed TerrainPropSliceProof and TerrainPropEditTests; Debug failed TerrainPropEditTests. The registered overlay passed in both hosted configurations. The original native edit binary reproduced the hosted staging failure at matched checkout depth.

**Correction and source files.** `EngineBridge/src/ProjectDocumentTransaction.cpp` now uses compact indexed sibling artifacts with journal schema 2, while retaining version-one recovery and its version across an interrupted retry. `Tests/ProjectDocumentTransactionTests.cpp` exercises long digest filenames, unchanged-document filtering, validation, commit, rollback, interrupted recovery, invalid artifact indexes and legacy recovery. `docs/ARCHITECTURE.md` records this choice. The assistant's existing UTF-8/LF fingerprint for that document is refreshed without changing its Surface Type instructions or stale-evidence checks. The original transaction implementation fails the new regression; the correction passes.

| Correction check | Result |
| --- | --- |
| Complete Release preflight | PASS; 341/341 tests, both packages and LP05/LC01 process proofs |
| Complete Debug preflight | PASS; 326/326 tests, both packages and LP05/LC01 process proofs |
| Matched hosted-depth native repro | Original Release edit fails; corrected Release edit/slice and Debug edit exit 0 |
| Long-filename regression negative control | Original code fails four checks; correction passes |
| WARP corrected C++ snapshot | 4/4 PASS in 126.62s; allocator, overlay, residency and slice |
| Fresh editor and separate player | PASS; 1,076 placements retained, exact saved scene bytes, frame 60, exit 0; captures inspected |

**Evidence and commands.** [Correction record, commands and limitations](docs/evidence/terrain-props-p2/ci-path-correction.md). All full-suite assertions and original timeouts remain. The first local correction Release attempt retained three assistant scratch-I/O timeouts; the complete subsequent run uses system-volume scratch database/output arguments for those three knowledge cases. The stale architecture evidence failure discovered on retry is retained separately; the refreshed complete 84-case workflow suite passes. Stock terrain packing used unchanged executables and hash-verified, byte-identical generated products as described in the evidence. No original packer was stopped. The Debug preflight retained the established physical-directory test adapter and nonincremental Debug linking.

**Baseline and scope.** Fresh main is `db8d620f75a5219ae5a3906e2837ed2d4165c741`; Wicked remains `82557d439a41d104f559b0d3134d160b7bf6154e`, imnodes `eb36902c892548ef94f88f51ad7e7c9c7058a71c`. The ten governed configuration overlays remain uncommitted. Collision-interest, full asset/texture streaming and representative full-game forest acceptance remain deferred. Compact artifacts remove avoidable suffix growth from otherwise valid destinations; arbitrary overlong project roots are not newly supported. Hosted validation of the corrected pushed head, independent/owner acceptance and merge approval remain required. No merge is authorized.

## Terrain props P3 source-freeze checkpoint, 18 November 2026

**Implementation candidate; full frozen preflight and acceptance remain pending.** P2 PR #198 was owner accepted and merged as `38196e9869fabd36c9255c64cd248d1000b68285`. P3 proceeds on `feature/terrain-props-p3` from that baseline. The final validation receipt must identify the tested implementation commit. No P3 push, hosted green or merge claim is made here. Do not merge without owner approval.

Implementation covers document-derived shared capsule/box collision, an all-map query BVH, bounded native residency with actual ID reclamation/reserve, player/NPC/vehicle/scripted and wide sleeping-support interests, target terrain readiness, ordinary-world/prop nearest-hit merging, swept projectiles, perception and authored navigation. Collision fit, friction/surface and uniform scale controls use the existing editor mutation/history boundary. Schema 3 reads v1/v2 defaults and keeps placement pages at v2. See [P3 implementation and boundaries](docs/TERRAIN_PROP_P3.md).

Native integration composes guarded build-tree physics, Scene and terrain adapters with the accepted pin. Gitlinks and preexisting generated Wicked bytes are preserved. Terrain interests use the existing generation queue; zero-time simulation holds allow that same queue to prepare collision. No additional scene/physics step, terrain generator or asset/texture streaming service is introduced.

### Observed implementation checks

- Release Studio and Runtime build. Seven focused suites passed: PhysicsLua, ProjectileWorld, Phase6NativeNavigation, TerrainPropEdit, TerrainPropPhysics, TerrainPropCollisionIndex and TerrainPropCollisionWorld. These include actual 65,536 native-ID exhaustion/reuse, publication failure, late revision conflict, anchor replacement, terrain-owner deletion, legacy manifest reads, Undo/Redo and save/reopen.
- Native SliceProof plus CollisionWorld passed 2/2 after the legacy fixture was corrected to emit actual v1 profile fields. The earlier fixture failure is retained.
- Wide sleeping support, native character support identity, removed-support handling and late-global teardown regressions pass. The teardown negative control reproduces Runtime's `0xc0000374`; process-lifetime weak/request bookkeeping fixes it while controllers continue explicitly releasing native bodies.
- Isolated native Studio retained 3,950 placements through Generate, Erase, Undo/Redo, Paint, cancellation, Save As and reopen. Both 520- and 280-pixel Inspector layouts were inspected. The narrow proof also exercises the shared preview-focus callback and visibly frames the orange box around a scaled boulder.
- The separate native Runtime proof passes player support on the selected authored boulder, an NPC wall contact, pending teleport source retention, destination terrain admission and landing. It reports four waiting updates, 3,597 indexed colliding placements, 834 resident prop bodies, 917 total native allocations and 8/8 required terrain chunks. The copied saved scene's SHA-256 is `2bbc6ab7a96ca643c50f5ee510bf922385bfc40705cd03e8b74f48bc32619bbb`. Runtime executable SHA-256 is `64717ebda18e80601e4eeb7e1c2508abcf148dabf1c194a8cc5cd45d4c9f6466`. Its regular close exits 0 with no unexpected warning/error matches. Four expected collision-preparation warnings are counted separately.
- Earlier native attempts retain an incorrect capsule-centre proof assertion, pointer-valued support userdata handling, excessive proof-NPC acceleration and the shutdown crash. The fixed proof uses the pinned foot-origin convention, locked native support identity and the normal acceleration/friction convention. Failures are not green receipts.

Raw attempt logs and local launch/capture/build adapters remain ignored under BUILD. Final full Debug/Release suites, both packages/process proofs, WARP, isolated renderer-overlay checks, deeper Windows-path reproduction, frozen-commit review and final creator/player capture receipts remain outstanding. These technical fixtures establish no production-asset forest performance claim. Asset, geometry and texture streaming remain later stages.


## Terrain props P3 local validation and review candidate, 18 November 2026

Product implementation is `9989b3f1599781c26d7ee5c62287919348976295`, with independent Runtime scene-transition fixture link correction `ed959f9f1aeea98e2311ffd4cd8e8183e367f089`. Final native Studio/Runtime and bridge product hashes match that validation snapshot. The publication also contains four updated source contracts, test-only native crash diagnostics and a Studio CI environment setting; exact tested working-tree source hashes are in the evidence. Obtain the final publication identity with `git log -1 --format=%H -- HANDOFF.md`. Main remains the accepted P2 merge `38196e9869fabd36c9255c64cd248d1000b68285` at final local validation.

**P3 implementation candidate, not an accepted release gate.** Collision fit/friction/surface/scale authoring, bounded native collision residency, actor/destination terrain readiness, all-map queries, swept projectiles, perception and authored navigation are implemented. Asset/geometry/texture streaming and arbitrary hull cooking remain deferred. [Implementation and boundaries](docs/TERRAIN_PROP_P3.md); [validation commands, captures and limitations](docs/evidence/terrain-props-p3/README.md); [changed implementation paths](docs/evidence/terrain-props-p3/implementation-files.txt).

| Local check | Observed result |
| --- | --- |
| Release complete unfiltered CTest | 344/344 PASS in 559.28 seconds |
| Debug registered test set | 328/329 in the initial full run; sole unchanged 250 ms deferred-launch case passed 10/10 targeted rechecks with hash-identical fixtures on fast scratch; all 329 unique cases verified |
| Studio and Runtime packages | Both configurations PASS; LP05 dependency and LC01 init/update/move/reopen process proofs PASS |
| Constructor startup smoke | Debug and Release exit 0 |
| WARP and renderer overlay | 4/4 PASS in 63.91 seconds; overlay 5.35 seconds under its unchanged 60-second timeout; final native Release registered overlay also PASS |
| Hosted-depth native checks | Debug and Release Edit plus Release Slice exit 0 at matched checkout depth |
| Source-contract controls | Four positives and eleven deliberately broken-source negative controls PASS |
| Fresh native editor/player | Both inspector widths and separate exact-scene player PASS; captures inspected; normal close exits 0 |

The editor proof generated 3,933 placements, erased to 3,925, verified history and persistent clearings, painted to 3,950, cancelled staging, saved/reopened 3,950 and focused the collision preview. Inspector widths 280 and 520 were inspected. The enlarged capture window is separate from the measured viewport. The player loaded the exact wide-editor scene, SHA-256 `BB2021057EF1A3C3725CA74AE91C4B561D4F0E95C7F0EDEAAB55597914EAAEC7`. Runtime SHA-256 `1FCF09EC09D9009B81DC68EE67DF6011C1990C12B2C21DB16AFDFFC12B2DB8F5`. It passed native boulder support, NPC wall contact, four pending teleport updates with source support held, destination admission and landing with 8/8 terrain chunks ready. Counts: 3,597 indexed colliding placements; 834 resident props; 917 total native allocations. No unexpected warning/error/missing-file matches; four expected collision-preparation warnings are separate.

**Failure disposition.** The earlier intermittent Debug maximum-terrain access violation remains unexplained. It also occurred in an isolated run after overlapping suites had ended, so concurrency alone is not an established cause. It did not recur in the final full Debug/Release native terrain checks, registered repeat or debugger runs. Those passes are not a root-cause fix. Opt-in first-chance dump capture is verified by an intentional failing process and enabled for Studio CI; normal exception propagation/assertions/timeouts remain. Keep this risk explicit in review. The earlier editor driver's 150-second timeout did not recur in the fresh complete proofs.

Earlier local harness failures from shared fixed temporary directories and overlong replacement scratch roots are retained in ignored raw logs. Current runs use separate short roots and serial configurations. The Debug receipt explicitly preserves its initial failed full run and successful targeted recheck; it does not claim a single 329/329 invocation. Native aggregate build logs/product hashes and separate test-diagnostic rebuild witnesses guard reuse of already compiled products. Three knowledge scratch arguments and the process fixture's hash-identical local execution copies are the documented environment adaptations; no test threshold was raised.

Wicked remains `82557d439a41d104f559b0d3134d160b7bf6154e`; imnodes remains `eb36902c892548ef94f88f51ad7e7c9c7058a71c`. All ten preexisting generated Wicked overlay paths/bytes remain uncommitted and preserved. Technical fixtures establish no performance headroom with production woodland assets. Hosted validation and independent exact-publication/owner acceptance remain pending. No merge without owner approval.

## T1 material texture cooking and native streaming

Implementation commit `adcab60dbd63238d984aa563f0d607f55f77d6bd` follows P3 on merged PR #199. The complete code change list is available with `git diff --name-only 3e0c69e..adcab60`. The shared texture importer, material binding, native file reader and Build Game dependency providers are described in `docs/TEXTURE_PRODUCTS_T1.md`. Stock grass now has one content-addressed cook across Studio, Runtime and the proof executable.

On Windows x64 Release, the six focused cooking, upload, overlay, streaming and terrain checks passed. The five corresponding WARP graphics checks passed. The renderer overlay took 6.65 seconds on hardware and 5.22 seconds on WARP against the existing 60-second limit. Run the registered tests with `ctest --test-dir BUILD/native -C Release -R "MaterialTextureCookTests|GovernedTextureStreamingProof|TerrainPropResidencyProbe|TerrainPropSliceProof|GpuUploadBufferTests|RendererOverlayComposition" --output-on-failure`. The native proof directly requests resolutions and verifies GPU readback across Save As, reimport, project relocation and package binding.

Two production Build Game exports passed their packaged Runtime smoke. The populated scene package contained two revisions, 399 placement pages and three reusable assets. The governed boulder package contained one revision, one page, one model and its texture. All audited products matched their project bytes, and the governed package contained no retained source image. The previous exporter reproduced Runtime exit 24 without the prop dependencies. A private relocated player run of the original 3,950-placement scene passed with four expected collision-readiness messages and zero unexpected warnings, including zero missing grass files. See `docs/evidence/texture-products-t1/runtime-boulder.png` for the actual DX12 frame. The technical assets do not establish production forest performance.

First bind still reads the selected DDS and retained input. Embedded textures in legacy model archives keep their existing native path. T2 memory-pressure policy and whole-library migration are separate work. Full Windows Studio preflight at 111f290 passed 330/330 Debug and 346/346 Release tests. Release startup-smoke exited 0. One earlier intermittent maximum-terrain-preview access violation remains unexplained. Six focused reruns and the final full Debug run passed. Hosted checks and independent acceptance are pending.

The horizontal strip also appears in the pre-T1 P3 capture. In a disposable diagnostic Runtime, native camera rays hit the fixture objects Proof ground and chunk_0_0 at the same world height (y=0). Hiding only the renderable Proof ground plane exposed continuous grass across the strip at the original camera angle. See docs/evidence/texture-products-t1/runtime-plane-hidden.png and runtime-wire.png in the same directory. The latter uses the lower camera angle and shaded wireframe. The original QA scene and shipped Runtime were preserved.

## P4 Runtime cell experiment

Local implementation checkpoint b558a564f10b4f05249b9d7e07facad5f7f85e16 on the merged T1 source tree. Use git show --stat b558a564f10b4f05249b9d7e07facad5f7f85e16 for the six changed paths. Release player experiments set RENEGADE_P3_RUNTIME_PROOF to a writable log path and RENEGADE_P4_RESIDENCY_PROOF=1 on a disposable TerrainProof project. Set RENEGADE_P4_CELL_RESIDENCY=1 for the experimental run and leave it unset for the control. The driver uses the normal player input for the 48 m walk each way after a 198 m teleport. The fixture ground overlay is hidden for both screenshots.

Both runs completed and exited successfully. The control held 4297 prop render objects. The experiment sampled 1539 at the boulder, 1466 after teleport, 1556 at the walk turnaround, 1466 after walking back, and exactly the original 1539 entities upon returning to the boulder. Player movement crossed three cells. The hardware reported 1828847616 bytes of application GPU memory at every milestone in both runs. One sampled run per mode peaked at 1434 versus 1754 MiB working set and 4290 versus 4422 MiB private bytes (experimental versus control). These process-wide samples do not isolate prop allocations. The earlier projection regressions passed with ctest -C Release -R "^(RenegadeTerrainPropEditTests|RenegadeTerrainPropSliceProof)$" --output-on-failure.

The path is opt-in. Distant cells retain representative placements using the original meshes. All cells are still materialised at scene load and all four source meshes remain resident. Remaining gate work includes independently loadable coarse geometry, asset lease and lifetime management, total population bounds, reload/cancellation and representative production-asset visual and performance checks. Full local preflight and independent review are outstanding.

## P4 scale-aware draw distance and measured model-loading hitch

Implementation 407b4cfd2ae831e8c738627dcff5f7588b3de76d follows e648b1b. Run git show --stat on each revision for changed paths. Studio terrain Prop Settings now accepts a unit-scale draw distance in metres. Zero keeps unlimited range. An optional v5 profile field persists the range. V1 through v4 inputs retain their prior behaviour. The renderer applies its native fade and distance checks to coarse, fine and single-model placement parts. This setting does not unload meshes or change gameplay collision.

On Windows x64, build RenegadeTerrainPropEditTests in Release and run ctest -C Release -R "^RenegadeTerrainPropEditTests$" --output-on-failure. Passed including save/read, Undo and a 3x scaled placement with a 125.5 m profile receiving 376.5 m native range.

Private 1920x1080 RenderPath3D benchmark: same seeded 200 supplied trees and 40 rocks, shadows on, route near/far/near. Trees used 360 m and rocks 100 m at unit scale for this experiment. At the far checkpoint, objects passing renderable and range checks fell from 241 to 38; corresponding mesh triangles from 935936 to 167375. These are pre-frustum range counts. Near and return matched exactly in each mode. The far skyline was thinner with these values. GPU allocation remained 780140544 bytes at all checkpoints.

A separate 601-update moving-camera trace crossed 300 m and returned. At z=-224 on return, owner-thread scheduler time reached 4752.5 ms with unlimited range and 4743.02 ms with distance culling. Product reopening and native scene preparation accounted for seconds in the asset log. The expensive work runs in the fine-model acquisition path. Do not treat the last still frame, average FPS, or draw distance as evidence of smooth traversal. Gate remains open pending bounded background preparation, stale-result handling, real-asset standalone player timing, full preflight and independent review.

## P4 background model preparation and standalone player trace

Implementation 5e34fbc58ef18c0974ff6b0ec61f45eb28a2b8dd. See git show --stat for four changed files. The runtime starts at most one detached product preparation at once and polls its future. Only the owner thread installs or evicts render objects. Returning from an obsolete scene or leaving the desired region invalidates the installation. Per-operation staging filenames isolate concurrent uses of the same asset.

Release Studio and Runtime built. Build RenegadeTerrainPropResidencyProbe in Release and run it with PROJECT_ROOT --mesh-detail PROJECT_ID FINE_ID COARSE_ID. The private oak fixture passed departure during load, scene revision during load, return to fine, and revision while fine was resident. Coarse-only views measured 132530 bytes; fine plus coarse 486130. The normal standalone 3950-placement player regression exited zero, reported P3_RUNTIME_PROOF_PASS and had zero unexpected warnings. It logged 105 expected terrain readiness messages on this run.

The native 200-tree / 40-rock moving camera route peaked at 0.9013 ms of scheduler work versus 4743.02 ms for the prior culling run. All three endpoint geometry counts matched. A separate instrumented Release standalone Runtime used the actual physics player and an automated 300 m out-and-back position trace, one metre per frame. Its returned scene settled at 19 meshes, down to 12 at the far checkpoint. Scheduler maxima were 0.482 ms outbound, 0.032 ms inbound and 0.766 ms while settling after return. Maximum full frame intervals were 24.66, 14.99 and 16.82 ms respectively. The initial near-view phase included a 377.35 ms frame interval. Fine detail took about 13 seconds after return to finish loading. The final DX12 window capture showed textured woodland, shadows and a 75 FPS overlay. The diagnostic movement and timing instrumentation were confined to the verification executable. Full preflight, fine-detail latency improvements, total population bounds and independent gate acceptance remain open.

## P4 queue, player speed comparison and population scaling

Changes b6051f7 and c8cb327fd5b5bd5cfeda008036ced5428dca9050 preserve authored placement draw distance when legacy cells return, order new cells by player proximity, share pending work by scene revision and profile, and cap preparation at two jobs. See git show --stat for changed paths. The native oak probe also exercises single-model distance restoration, cancellation on departure and revision change, fine eviction and re-entry. Release Runtime and RenegadeTerrainPropResidencyProbe built. The real-asset probe passed. Run ctest -C Release -R "^(RenegadeTerrainPropSliceProof|RenegadeTerrainPropEditTests)$" --output-on-failure; both passed in 8.36 seconds.

Private standalone DX12 player, 200 supplied trees and 40 rocks, same 300 m out-and-back route. At 75 m/s, two one-worker runs settled 12.87 and 13.00 seconds after stopping. Two two-worker runs settled in 11.48 and 11.54 seconds. Full-frame startup maxima were variable: 377/396 ms with one worker and 1276/282 ms with two. The two-worker repeat had a 156 ms frame during return settlement, while the timed scheduler maximum in that phase was 0.80 ms. At 7.5 m/s, six of seven fine meshes were back on arrival; the last arrived 2.08 seconds later. P99 inbound frame interval was 16.24 ms, maximum 17.04 ms. The outbound maximum was 45.87 ms. These are automated player-position traces on the owner GPU. Fast traces exercised the queue change; the 7.5 m/s trace also included the single-model distance fix.

Native render benchmark at a fixed camera, 1920x1080. Compare one 240-placement tile against nine identical tiles spaced 1024 m apart (2160 placements). Both produced exactly the same RGB frame, 212 range-eligible objects, 1595662 range-eligible triangles, 17 scene meshes and 780140544 device bytes. Scene objects increased from 304 to 2224 and transforms from 307 to 2227. Every additional out-of-range placement still became a native object and transform. The measured blocker is the coarse/single-model projection created at scene restore. The model scheduler bounds fine loads but does not yet bound that initial projection or its steady-state population. Remaining acceptance includes paging native placement creation by view/interest with conservative transformed bounds and fallback coverage, startup tracing, full local preflight and independent review. No hosted run for this checkpoint.
