# Character Importer V4 - animation editing wiring checkpoint

Date: 22 September 2026. Branch: `feature/a6-animation-wiring-v4`.
Code checkpoint: `85408871aecead786fca52692e151b3796db73c4`.
Approved UI remains immutable at tag `importer-ui-v4-approved-20260922` (`5152874fa443ce011981e8f0ac50ab1da1d2cfdf`). No main merge.

## Real functionality added

- Start/End fields validate finite native seconds, source clip bounds and nonzero duration before mutating the pending recipe; rounded displayed endpoints are clamped safely. Rejected edits retain the previous source, action, name and speed.
- Invalid trim/speed feedback now appears in the *visible existing* Validation card rather than the hidden old readout. Valid edits clear the warning; switching/searching clips clears stale warnings. The UI states assignments are **ready for import**, not saved before confirmation.
- A real-Mutant graphics-backed proof now duplicates the Walk source into an independent trimmed, 0.75x, unassigned clip, commits/reopens the governed Character, checks both native range and speed, retains action metadata, then verifies A6 Walk/Run/Swipe transitions and autonomous melee. This exercises the real recipe/persistence path, not a clicked Studio UI test.

## Validation (local Windows Release)

- Built `RenegadeReusableAssetReimportRecipeTests`, `RenegadeCreatorAssetWorkflowGraphicsProof`, `RenegadeStudio` successfully; log `BUILD/recovery/v4-wiring-duplicate-proof-build.log`.
- Focused CTest ReusableAssetReimportRecipe, CreatorExternalAnimationImport, CreatorAssetWorkflowGraphicsProof, CharacterAiAnimation: **4/4 pass**.
- Actual owner `Mutant.fbx`, walking, run and swiping FBX graphics proof: pass; log `BUILD/recovery/v4-wiring-real-mutant-proof.log`. Original source FBXs and owner project untouched; test output disposable under BUILD.
- V4 source guard `Tests/ApprovedImporterUiContract.ps1`: pass; `git diff --check`: pass. No V5 visual/UI click-through or packaged TestGame check yet.
- Separate test executable `BUILD/recovery/Studio/Release/RenegadeStudio_ImporterWiring_v5.exe`; original accepted V4 executable/archive unchanged. Do not open both Studio instances on the same project.

## Remaining and next bounded task

The approved appearance is preserved, **not all controls are feature-complete**. Still needed: independent per-source removal instead of remove-last; general expandable variant slots and custom actions; frame-number (not native seconds) trim UX; reopen/edit an imported Character; stable changed-source identity/reimport reconciliation. Confirm native preview, trim rejection feedback, action assignment, import/cancel and A6 gameplay by actual owner click-through on a disposable project before claiming end-to-end acceptance. Preserve frozen V4 UI, `main`, existing owner projects and unrelated untracked `Tools/__pycache__/` / `log.txt`.
## A6 one-shot variant selection checkpoint

Code commit `ef8dcce1bec8af8529877484bc665738d2f9debd` adds stable-character-ID-seeded pseudo-random choices for Attack, Reload, Hit and Death, each with a separate sequence and no consecutive repeat when multiple clips exist. Looping Idle/Walk/Run requests do not change one-shot variant sequences. A repeated request during an active one-shot leaves that native clip playing and does not consume the next variant. The selection is reproducible, not cryptographically random.

Release builds `RenegadeRuntime`, `RenegadeCharacterAiAnimationTests`, `RenegadeCreatorAssetWorkflowGraphicsProof` and `RenegadeStudio`: pass, exit 0. Focused CTest: 4/4 pass. Unit regression exercises five attack options for 20 successive requests, no immediate repeats, stable replay and no sequence consumption during an active attack. Actual owner Mutant Walk/Run/Swipe graphics-backed import/reopen, duplicated trimmed Walk clip and autonomous melee proof: pass (`BUILD/recovery/v4-wiring-variant-real-mutant-proof.log`). V4 UI source guard remains passing; owner TestGame and packaged release were not run.

Named side-by-side Studio executable: `BUILD/recovery/Studio/Release/RenegadeStudio_ImporterWiring_v6.exe`; the separately built Runtime binary resides in `BUILD/recovery/Runtime/Release`. Do not infer that running Studio alone validates standalone AI behavior. Preserve V4 approved executable/tag/branch and avoid simultaneous Studio processes on the same project.