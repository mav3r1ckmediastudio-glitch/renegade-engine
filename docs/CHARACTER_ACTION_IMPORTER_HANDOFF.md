# Character action assignments — local A6 checkpoint

Date: 22 September 2026. Branch: `feature/complete-a6-recovery-20260921`.
Worktree: the isolated recovery worktree for this branch (not `main`).
Original `main`, all other worktrees, private FBXs, owner project and currently running Studio must remain untouched. No merge or remote push is authorised by this checkpoint.

## Implemented in this checkpoint

- Character Importer Animations page has an explicit action dropdown per imported clip: Unassigned/Reference, Idle, Walk, Run, Attack, Reload, Hit and Death. Multiple clips can be assigned Attack by choosing Attack on each. Source clips retain their existing rename, duplicate and trim controls; embedded and external clips enter the same recipe.
- New Character imports mark each native AnimationComponent entity with a governed action metadata string, including Unassigned. The recipe stores this in optional `animations[].action` for reimport; old recipes without the field still parse as before.
- A6 selects assigned native clips through this metadata instead of relying on the filename or source order. When assignments exist, unassigned reference-pose clips are never considered gameplay Idle. Older Character products lacking any assignments retain legacy filename inference, but no longer use unnamed clips as implicit Idle.
- An active one-shot attack/reload/hit/death clip cannot be replaced by another request for the same action before it completes. No Idle clip is optional, and the missing-Idle path does not repeatedly request an unusable bind clip.
- Character import confirmation checks for duplicate/empty enabled clip names case-insensitively and displays a naming explanation. The Animations page indicates whether an Idle assignment is missing.

## Evidence and exact validation

- Local Release build of `RenegadeRuntime`, `RenegadeCharacterAiAnimationTests`, `RenegadeReusableAssetReimportRecipeTests`: PASS, exit 0. Log: ignored `BUILD/recovery/action-assignment-build.log`.
- Real Mutant graphics-backed governed Character import/reopen and AI transition proof: PASS, exit 0. Untouched source FBXs; output under ignored `BUILD/recovery/a6-actions-real-proof`. Log: ignored `BUILD/recovery/action-assignment-real-mutant-proof.log`.
- Focused Release CTest: three tests PASS (3/3), exit 0, including the Creator Asset graphics proof. The AI06 test now checks that a second Attack request does not cut off an active one-shot, and that explicit metadata overrides misleading clip names and excludes the reference-pose clip.
- Studio is open from `BUILD/recovery/Studio/Release/RenegadeStudio.exe`; to preserve its active executable, Studio Release ClCompile PASS, exit 0, using `/t:ClCompile /p:BuildProjectReferences=false`; existing Studio executable was not overwritten. Log: ignored `BUILD/recovery/action-assignment-studio-compile.log`. A separate side-by-side `RenegadeStudio_ActionAssignments.exe` linked successfully, exit 0; log: ignored `BUILD/recovery/action-assignment-side-by-side-link.log`. The original running Studio was not overwritten and the side-by-side binary has not been opened or visually inspected.

## Explicitly NOT complete

- A separate `RenegadeStudio_ActionAssignments.exe` has linked successfully but has not been launched or visually inspected. The running original Studio predates this checkpoint. No visual owner acceptance or live Mutant TestGame proof has occurred on these changes.
- This is the first functional assignment slice, not the full approved Animation Library UX: expandable named action slots, free-text custom actions, intuitive *frame-number* trim controls, reopening an existing Character in the importer, source/clip identity during changed reimports, and a future root-motion warning remain to implement and test. Existing clip trim values are native time values, not frame numbers.
- Existing owner `.rasset` products have not been rewritten or migrated. New imports require explicitly selecting intended actions; an unassigned Idle remains empty. Auto-assignment from filenames would defeat the point of explicit authorship.
- The actual owner Mutant and three untouched external walk/run/swipe FBXs PASSED a disposable governed import, RAsset reopen, native action-metadata assertion, Character wrapper placement, A6 unassigned-pose -> Walk -> Run -> Swipe -> Run transitions and autonomous melee event proof. Packaged Build Game, current-head full CTest and visual owner behaviour remain unverified.

## Next safe actions

1. Use the safely linked side-by-side Studio executable for visual verification, but first close the currently running Studio to avoid conflicting diagnostic endpoints and concurrent edits to the same project. Never overwrite the protected original while it is open.
2. The real Mutant disposable graphics proof is implemented and passed. Preserve and repeat it after importer or Runtime changes; output is only under ignored `BUILD/recovery`.
3. Finish the approved Character-specific animation library/slots and editable frame-range UX in small local checkpoints; preserve every accepted importer page, thumbnail, head mapping, terrain/hierarchy and current world placement.
4. Run focused builds/tests, full Release regression when the feature is integrated, and visual hands-on TestGame acceptance before proposing a PR or merge. Never infer success from only synthetic tests.

Do not stage the unrelated untracked `Tools/__pycache__/` or `log.txt`; they predate this change.
