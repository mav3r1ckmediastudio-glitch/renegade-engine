# Current usability candidate - 16 November 2026

Tested engine source: ed2e69ab410045bbad3b6db384fe80b6bc008c17.
Windows x64 Release/DX12. Nine focused suites pass in 4.94 seconds; the portable
core suite passes 50 checks. See HANDOFF.md for exact build/test commands.
The owner reviewed the native build and reported: "that all seems to work now".
PR #195 remains CLOSED; local changes are unpushed. No new CI or merge occurred.

Current UI uses opaque Studio dark/orange colours, native fonts, hover/pressed/
disabled feedback, tooltips, numbered workflow, visible response destinations,
a persistent preview ending, and local recording import/folder/refresh controls.
Unsaved valid edits update preview while preserving progress and the current
voice visit. The UI identifies sample clips as examples, and Runtime names the
configured Interact key. NPC selection excludes Player Start markers.

The simple example now has fixed response links. Its old automatic has_permit
flag made a later Let me through reply take the approval branch. Advanced
conditions/actions are retained and exercised in an explicit separate test
fixture. The owner's captain connection was preserved in the review asset.

Latest native Test Game log:
- work -> captain; sael-captain.wav
- gate -> refused; sael-refused.wav
- Continue -> opening
- bye -> end

Failures remain part of the receipt: 4a39dc3 could activate a replacement preview
button from the same click and allowed outer scrolling (see
[pre-fix capture](usability-before-gesture.png)). 29dcf54 isolated the gesture but
its direct scaled-window attachments hid the editor controls. 7753f94 restored
native unit-scale content attachment and removed the outer scrollbars. Owner
review occurred after those repairs, at ed2e69a.

The older orange screenshots below were recorded before this latest usability
revision; pale-teal screenshots are rejected historical styling. Older package,
save/load and CI evidence does not validate the latest tree. A newly packaged
build and broader manual/controller/file-picker checks remain unclaimed.
---

# Historical native dialogue review - 16 November 2026

Tested code: `41228200d9fa43de1154f58d70ccb2f422684f81` on
`feature/dialogue-editor-20261115`; base main `7093cf4`.
Windows x64, MSVC, Release, DX12, RTX 4070 Ti. Wicked remains pinned at
`82557d439a41d104f559b0d3134d160b7bf6154e`.

This is implementation-side evidence, not independent release acceptance.
PR #195 remains a draft and is unmerged. Other arms/weapon work was not changed.

## Build and tests

```powershell
$env:CL='/MP8'
cmake -S . -B BUILD/native -DRENEGADE_EMBED_SHADERS=ON
cmake --build BUILD/native --config Release --parallel 4 --target RenegadeDialogueReview
cmake --build BUILD/native --config Release --parallel 4 --target RenegadeScreenRendererTests RenegadeRuntimeScreenTests RenegadeDiagnosticServiceTests
ctest --test-dir BUILD/native -C Release -R 'RenegadeDialogue|RenegadeRuntimeGameSaveSlotTests|RenegadeScreenRendererTests|RenegadeRuntimeScreenTests|RenegadeDiagnosticServiceTests' --output-on-failure
```

Release build succeeded. Seven of seven suites passed (0.69 seconds): dialogue
save policy, asset/native scene persistence, core dialogue, diagnostics, Runtime
Screen, ScreenRenderer, and Runtime game-save slots. Core suite includes 32
behavioural checks; the simple single-level save policy has ten cases.
GNU C++17 core checks also pass. The Studio build retains an existing C4834
warning at the Test Level launch call; this is not a warning-free build claim.

## Native editor and Runtime

- [Basic workspace](solid-basic.png): opaque charcoal sections, pale teal actions
  and reply rows, readable native fields, working automatic preview.
- [Advanced workspace](solid-advanced.png): outcomes/conditions/actions revealed
  in the same outline, with authored opacity restored as `1` after save/exit/open.
- Native condition selection shows the correct key/value and rule-type controls.
  Switching Advanced off retains rule summaries and document data.
- Native history: appending `X` to the conversation name made it dirty; Undo
  restored `Gate Guard`, Redo restored `Gate GuardX`, and Discard reloaded the
  saved conversation without changing the file.
- Preview: `has_permit=0` produces “Come back with a permit.”; setting it to `1`
  produces “Everything seems in order.”
- [Test Game](test-game.png): assigned review object offers the Interact prompt;
  E opens the same opaque panel; Down/Enter select the refusal branch. The
  permit-granting reply changes memory and the next gate reply succeeds.
  Escape closes the conversation without opening Pause and returns player input.
- Native asset suite verifies transactional save failures, ownership rejection,
  assignment Undo/Redo without unrelated metadata loss, and WISCENE reopen.

The fixture uses a cube as its assigned review object; it is not character-art,
voice, portrait or cinematic acceptance.

## Standalone package and saved memory

The actual Studio **Build > Build Windows Game** path completed at the tested
revision. Build report records `gate5_validated_for_final_path`, DX12 smoke
`passed_gate4`, native Story Flow entry, and package isolation/parity checks.
The package includes the saved `.rdialogue` asset with text size `22` and
opacity `1`, its Level identity companion, and the three-destination
Game Start → Main Level → Complete Game flow. Distribution readiness remains false.

[Standalone dialogue](standalone.png) was opened from the packaged executable
without source-project arguments. Native keyboard replies work. F5 saved before
obtaining a permit; the permit reply then made the gate succeed. After Escape
and F9, reopening and selecting the gate reply returned to the refusal branch,
proving the earlier dialogue memory was restored
([capture](standalone-memory-restored.png)). This live save/load proof used an
inventory-free Level. Inventory payload integration has automated coverage;
a populated-inventory live dialogue save/load case is not claimed.

## Original Test Game failure and resolution

At `4122820`, quick-save in the temporary **Test Game** project failed for this script-free
fixture because `SnapshotCreatorScripts` skips the Level `.rmeta` sidecar when
there are no script attachments. The Runtime correctly refused a save without
scene identity. The packaged game includes that identity and passed the native
save/load proof above. This failure is preserved as historical evidence; the Runtime identity check
was not weakened.

### Resolution at `63cc73d373162de4f6cf3e490cb3f9804d7c7bec`

Scene identity is now staged independently of creator scripts. The bridge
retains the source document/project IDs, retargets the path hint, and stages the
transactional write under short OS temp storage before copying into the disposable
snapshot. A script-free Runtime snapshot regression checks that identity exists,
its path is correct, source bytes are unchanged, and foreign project ownership
is rejected. Missing identity is still allowed for ordinary unsaved Test Game
play; Runtime save requirements remain unchanged.

Windows Release Studio/Runtime rebuilt successfully. Nine focused suites passed
(2.64 seconds), using the earlier commands with `RenegadeTestLevelSnapshot` added
to the CTest expression and the two snapshot targets added to the aggregate.
The following additional scripted-scene and inventory checks passed, 3/3 (1.72 s):

```powershell
cmake --build BUILD/native --config Release --parallel 4 --target RenegadeS4BScriptAuthoringTests RenegadeS5ReusableScriptOwnerTests RenegadeInventoryTestLevelProjectionTests
ctest --test-dir BUILD/native -C Release -R 'RenegadeS4BScriptAuthoringTests|RenegadeS5CoreGameplayLuaTestsReusableOwner|RenegadeInventoryTestLevelProjectionTests' --output-on-failure
```

Actual Studio Test Game created a scene identity companion with the original
project/document IDs and `Content/Scenes/TestLevel.wiscene` path hint. The native
Runtime log reported `QUICK SAVE COMPLETE` for F5 before obtaining a permit.
The permit reply then caused the gate success response. Escape/F9 reported
`QUICK LOAD COMPLETE`; E, Down, Enter returned the gate refusal response,
proving that the earlier dialogue memory was restored
([native capture](test-game-memory-restored.png)). Runtime reported zero creator
script instances for this inventory-free fixture. Both review Runtime processes
closed cleanly. Earlier standalone evidence above remains at `4122820`.

Controller hardware, reply pagination in the live UI, larger DPI/resolution
layouts and independent owner/release verification remain open. Automated tests
alone do not close those checks.
