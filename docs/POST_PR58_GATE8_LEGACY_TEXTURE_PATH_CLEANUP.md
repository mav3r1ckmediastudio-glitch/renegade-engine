# Post-PR58 Gate 8 — Legacy Governed Texture Path Cleanup

## Baseline

Gate 8 starts from merged `main` commit `ba7eb764e334337e0e60b616ecb2008b8a7d9f81`, containing the accepted Project Hub, Gate 5 transactional switching, Gate 6 save safety and Gate 7 responsive real-phase loading experience.

Those behaviours are locked for this cleanup gate.

## Root cause

Older governed scenes can contain two different forms of texture identity at the same time:

- Renegade stable governed texture metadata (`renegade.material_texture.*.asset_id`) — authoritative;
- Wicked `MaterialComponent::TextureMap::name` — an old external/source filename retained from the original importer workstation.

Wicked deserialises `TextureMap::name` while reading a WISCENE and can therefore probe that obsolete external path before Renegade subsequently restores the correct governed resource by stable ID. Gate 7 made the stable-ID restoration visible and responsive, but it intentionally did not rewrite the creator's scene on open.

## Gate 8 rule

At the normal transactional scene-save boundary, Renegade inspects governed material texture bindings. For every slot that has valid authoritative governed metadata, it clears only the redundant Wicked `TextureMap::name` before serialisation.

The cleanup does **not** remove or rewrite:

- stable asset IDs;
- Asset Registry records;
- source/import provenance;
- `.rasset` products or payload hashes;
- the live `wi::Resource` already bound to the material;
- ungoverned authored texture filenames.

A malformed governed binding fails the save instead of silently stripping unrelated data.

## Existing-project migration

Gate 8 does not silently modify a creator's WISCENE merely because it was opened. Therefore a legacy project may still emit its historical source-path warnings on the **first** open after upgrading.

After that scene is saved normally, the validated replacement WISCENE contains no obsolete filename for governed slots. Subsequent reopen/project-load operations should no longer make those old external probes.

This preserves Gate 6's ownership rule: disk content changes only through the normal save path.

## Regression proof

`MaterialTextureAssetTests` injects a legacy absolute source filename into a material that already carries valid governed stable-ID metadata and verifies that cleanup:

1. identifies the governed binding;
2. clears the stale filename;
3. preserves an unrelated ungoverned authored texture filename;
4. leaves stable metadata available for normal governed restoration.

`SceneDocumentService::Save()` performs this cleanup before the temporary WISCENE is serialised and before both save-validation reopen passes, so the newly written archive itself is clean.

## Owner Release acceptance

Use the populated V3 project that previously printed obsolete texture source paths.

1. Open the project once. Historical warnings from the old on-disk WISCENE are allowed on this first migration open.
2. Confirm all governed textures/materials render correctly.
3. Save the scene normally.
4. Close/reopen the project through the Gate 7 loader.
5. Inspect the log.

Pass conditions:

- the previously reported original-source texture path probes are absent after the save/reopen cycle;
- all materials still render correctly;
- Gate 7 governed-resource counts/loading presentation still work;
- project load does not regress materially from the accepted mid-teens baseline;
- moving/copying the project does not require the original importer texture folders;
- ungoverned authored texture paths remain unchanged;
- Gate 5/6 project lifecycle and save-safety behaviour remains intact.

## Exclusions

Gate 8 does not redesign the Hub or loader, change asset provenance, alter resource payloads, introduce automatic scene saving, or perform another load-time optimisation pass.

## Merge rule

Do not merge until exact-head Renegade Studio Debug/Release and Windows baseline Debug/Release are green and owner Release acceptance confirms the save/reopen migration removes the legacy probes without visual or lifecycle regression.
