# Renegade recovery pre-alpha — 2026.11.02.1

This private testing candidate restores approved importing and repairs riverbed linking on normal 19 x 19 terrain. It does not claim every reported defect is fixed. RELEASE_MANIFEST.json records the exact source/dependency revisions and file hashes; VALIDATION.json records the checks performed.

## Start safely

Extract the entire ZIP into a new writable folder. Keep the previous alpha and use copies of projects. Run START_RENEGADE_ALPHA.cmd. On a fresh profile, Living Core opens automatically for the name/handshake after the logo reveal; separate manual assistant startup is not required. START_LIVING_CORE.cmd can reopen it.

The V4 tester handbook is under Documentation. These notes supplement that earlier version. Planning documents in KnowledgeSource are references, not claims that future features are implemented.

Riverbed metadata saved by this version uses schema 2. It reads earlier schema 1, but older alphas cannot edit schema 2 riverbed layers. Back up projects before saving with this build.

## Included repairs

- OBJ is restored in the normal model picker/converter, including OBJ/MTL textures and retained source dependencies. Alternating material runs (A / B / A) retain their face assignments. Material/map selection, CHOOSE / REPLACE MAP and source warnings are restored. FBX/GLB remain supported. Missing textures can be assigned in the importer; passing fixtures do not prove every third-party FBX compatible.
- Link / Update Bed supports the default 361 resident chunks using sparse riverbed storage and local segment filtering. The 81 edited-chunk, 32 MiB document, 32-layer and 32-million composition-work limits remain. External height edits and unsupported transforms are refused instead of overwritten. The Inspector shows backend refusal reasons.
- Living Core has source-backed river/waterfall answers and explicit Git-free package identity. Existing workflow records were revalidated against current controls, static OBJ guidance restored, and the complete offline workflow suite added to CTest. Whole-file text fingerprints tolerate Git LF/CRLF checkout conversion while rejecting every other source change. New packages bake knowledge for their own source revision. Existing extracted alphas do not update themselves when Git changes.
- Tools/Package-RecoveryAlpha.py replaces the earlier ad hoc staging procedure. It preserves old archives, stages current binaries/companion source, rebuilds and verifies packaged knowledge, hashes files and requires recorded CI/package checks before creating a versioned ZIP.

- Build Windows Game rebases verified built-in grass particles and river normal references when a project was saved under another Studio installation. Native grass resources are included in the export. Discovery and staging operate on detached scenes; unrelated external assets and custom maps remain blocked.
- A Story Flow Start action or quick load can replace a scene after its native update. Runtime retains the completed frame until the new scene has updated, so enabled volumetric clouds never dispatch with uncreated resources.
- Studio can start when Windows has no default audio output device. Native sound remains unavailable; configure an output device and restart Studio to enable it.

- Coherent-memory DX12 adapters use valid read-only upload buffers for native mesh suballocations. Normal GPU-memory allocations retain unordered-access support. This repairs the hosted WARP allocation failure without reducing terrain extent or bypassing assertions.

## Draw and link a river

1. Create terrain with normal defaults in a disposable level.
2. Open Terrain > Rivers > Painting, arm DRAW RIVER, drag across terrain and release. Turn drawing off when finished.
3. Select River Water or a control point. On freshly generated flat terrain with a zero height-encoding floor, use ALLOW DEEPER BEDS +10M before carving below that floor.
4. Open Riverbed & Banks and click LINK / UPDATE BED. Check the visible water/channel and Inspector result. A refused safety check must show its reason.
5. Change a point/width and verify the linked bed, Undo/Redo, Save, Reopen and Test Game.

Water is intentionally below bank height. Spline control spheres alone are not a successful visible-river test.

## Weather V2 and saved Legacy clouds

Environment > Clouds > Cloud formation selects the density model. Older scenes without V2 metadata open as Legacy (saved scene), preserving their previous appearance. Weather presets change coverage, exposure and fog while preserving formation and sun schedule.

Choose Cumulus for the accepted V2 Cumulus model, then apply Clear, Scattered, Overcast or Storm and save. Storm on Legacy can retain scattered old clouds; choosing Storm does not migrate the formation. Verify save/reopen and Test Game. Compare screenshots at the same time of day: the supplied accepted storm reference was midnight and the reported current scene was noon.

## Performance and known issues

- Owner observed 75 FPS in Studio and Test Game on DESIGN. An image-specific Windows Full PageHeap debugging setting caused local overhead. Registry settings were preserved; this is not a universal engine optimisation or graphics-quality reduction.
- Intermittent terrain darkness after grass removal remains open. Paint/delete and persisted-grass deletion did not reproduce it during recovery; that is not a repair. The earlier bundled-grass packaging path fix operates on a detached build scene and does not establish a live VT/BVH fix.
- Tester-specific failing FBX files are still needed for compatibility cases beyond the supplied fixtures.
- Wider UI Designer usability work and future weather/tree/artwork features are outside this recovery release.

## Report a failure

Include VERSION.txt, exact steps, whether the level was fresh or reopened, screenshots/recording and whether Test Game or Build Windows Game differs. For darkness, capture before/after grass removal and whether reopening clears it. For rivers, include selection, the Inspector result and refusal text. Preserve the failing project copy.

## Reproduce this package

Build RenegadeStudioCI Release and run complete CTest. The PR requires Windows baseline Debug/Release and Studio Debug/Release on the final head. Compilation alone is not visual acceptance.

Stage command: python Tools/Package-RecoveryAlpha.py stage --checkout <clean-commit> --build <validated-build> --base-zip <previous-private-alpha> --output <new-stage> --version 2026.11.02.1 --manual-zip <V4-handbook-bundle>

Test the actual stage: startup/onboarding, importer, default-terrain rivers, weather, Test Game, Build Windows Game and assistant. Record screenshots/receipts in the PR and validation JSON. Archive command: python Tools/Package-RecoveryAlpha.py archive --stage <tested-stage> --validation <exact-source-validation.json> --output <new-versioned.zip>

The script requires all four successful exact-head checks, successful local CTest and packaged workflow receipts. It validates staged hashes/ZIP CRC and writes a SHA-256 sidecar. Fresh extraction and launcher checks on the final ZIP remain required before handoff.
