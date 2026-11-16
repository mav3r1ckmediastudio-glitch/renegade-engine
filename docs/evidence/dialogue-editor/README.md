# Native dialogue review — 16 November 2026

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

## Open issue

Quick-save in the temporary **Test Game** project fails for this script-free
fixture because `SnapshotCreatorScripts` skips the Level `.rmeta` sidecar when
there are no script attachments. The Runtime correctly refuses a save without
scene identity. The packaged game includes that identity and passed the native
save/load proof above. Do not claim Test Game quick-save acceptance or weaken
the identity check. Follow up by staging scene identity independently of creator
script attachments and adding a snapshot regression.

Controller hardware, reply pagination in the live UI, larger DPI/resolution
layouts and independent owner/release verification remain open. Automated tests
alone do not close those checks.
