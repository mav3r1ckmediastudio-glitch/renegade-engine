# S5A Reusable Script Owner Regression

## Owner-test failure being guarded

The real Studio owner test exposed an entity-script attachment whose persisted owner ID no longer resolved in the active Scene when Test Level attempted to snapshot live creator scripting state.

The failing path was not the S5A Lua transform API itself. The failure was in creator ownership: viewport picking can select an imported child inside a reusable asset payload, while that payload hierarchy is replaceable during refresh/reimport. Creator-authored script state must not use a replaceable payload child as its durable owner.

## Required ownership rule

For an ordinary Scene entity, entity scripts retain that entity's persistent Renegade ID.

For a selected entity inside a reusable asset hierarchy, `ScriptAuthoringService` walks upward to the nearest entity carrying `renegade.reusable_asset_id` and records that stable reusable instance wrapper as the script owner. Inspector attachment queries apply the same resolution so selecting the imported child still displays the wrapper-owned script.

The strict `ValidateScriptDocumentAgainstScene()` check remains unchanged. No orphan-owner validation is weakened or bypassed.

## Regression acceptance

`RenegadeS5CoreGameplayLuaTestsReusableOwner` reproduces the ownership boundary rather than using a synthetic standalone transform:

1. Create a reusable wrapper with an imported payload child.
2. Save the Scene so both receive persistent identities.
3. Attach the S5A movement script using the payload child's persistent ID, matching the viewport-selection path.
4. Require the resulting `ScriptAttachment.ownerEntityId` to be the stable wrapper ID.
5. Remove the original payload child and create a replacement payload, simulating refresh/reimport.
6. Require the live scripting document to remain valid against the Scene.
7. Create the real Test Level snapshot, resolve the shadow project through Runtime, start the governed Lua companion and require the wrapper-owned script to move the reusable barrel.

This test is a dependency of the existing fast S5A Lua target and its CTest name begins with `RenegadeS5CoreGameplayLuaTests`, so the S5A fast-validation workflow executes it before full Studio/baseline acceptance is allowed.

## Owner acceptance

After the fast regression and the four normal Windows acceptance jobs are green on the exact repair head, the owner must repeat the real Studio test on an imported/reusable barrel: remove any attachment created by a pre-fix build, attach `S5A Move Barrel` again, set a non-zero offset, launch Test Level and verify the barrel moves exactly once on `on_start` without a snapshot identity error.
