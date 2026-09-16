# Creator Importer v3 — GitHub-only engineering plan

**Status:** implementation brief / proposed staged delivery; **not** an implementation or acceptance record.  
**Prepared:** 16 September 2026.  
**Baseline:** `main` at `5732d8d460610045275a9149252e47f0a3d75a39`; pinned Wicked `3a800b7134aafe58461093c8abb2e274d4e64033`. Recheck both before implementation.  
**Owner direction:** skip the local Runtime work; do subsequent engineering through GitHub only. Do not access or modify the owner's PC, local checkout or Runtime, and do not run local builds. Do not start CI until a bounded code change is ready for its planned GitHub validation.

## 1. Authority and evidence

- The owner's approved **Renegade Importer Interactive Concept v3** HTML attachment defines the visual/interaction intent. It is a design prototype: its clip motion, file selection and success screen are simulated; it does not parse FBX, retarget animation, write `.rasset` or update the Asset Browser. The attachment is **not committed to this public repository**. Review it directly when implementing UI; do not infer a working backend from its appearance.
- The owner's **Renegade next-chat engineering handoff, 16 September 2026**, records witnessed failures, safeguards and suggested implementation order. It is historical evidence, not proof that the current repository state is unchanged.
- Existing repository contracts remain relevant: [Creator Model Import Workflow](CREATOR_MODEL_IMPORT_WORKFLOW.md) and [LP07 governed `.rasset` import transaction](LP07_GATE3_RASSET_IMPORT_TRANSACTION.md). Preserve source retention, governed identities, transactional rollback, reusable products and placement as a separate operation. Where v3 intentionally changes older UI layout, document and implement the revised owner-approved behaviour rather than preserving an obsolete layout.
- This plan adds the owner's clarification: **Mixamo is not the retargeting limit**. Maintain Wicked's general humanoid mapping/retargeting path; account for Unreal, Unity-compatible humanoids, Blender Rigify deform exports and other compatible humanoids. Source-family detection and actual compatibility are to be verified, not advertised as tested merely because code contains candidate bone names.
- A separately recovered GGMAX Retargeter v0.6.12 reference includes UE4/UE5/Mixamo mapping, rest-pose/limb-direction handling, specialised GGMAX finger compression and visual validation. It is an **external, separately archived reference**, not an imported engine, universal rig profile, verified Renegade dependency or licence to upload private files. Do not copy its GGMAX-specific export scale, finger angles or reduced skeleton assumptions into general Renegade retargeting.

## 2. Current GitHub boundary (revalidate before each code stage)

- `main` includes isolated Story Flow default-navigation-tile persistence from #164. That functionality must not be duplicated.
- [PR #158](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/pull/158) is the open, unmerged Character AI programme. Preserve its branch and changes.
- [PR #162](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/pull/162) is open and unmerged, targets `wip/cw05-owner-validation-fixes` rather than `main`, and mixes navigation, importer file-picker and other work. **Do not merge it wholesale** or treat its checks/source contracts as importer owner acceptance. Review any importer-related code there and deliberately carry forward only needed, independently audited changes on a clean branch from current `main`.
- Relevant read-only source seams: `Studio/src/CreatorImportPreviewWindow.h`; `Studio/src/StudioApplication.cpp` (`ShowStudioMessageBox` / visible failure reporting); `EngineBridge/src/CreatorAssetWorkflowService.cpp`; `EngineBridge/src/ReusableAssetService.cpp`; `EngineBridge/src/CreatorModelImportRecipe.cpp`; `EngineBridge/src/CreatorExternalAnimationImportService.cpp`; `EngineBridge/src/HumanoidRetargetService.cpp`; `EngineBridge/src/AssetBrowserService.cpp` and registry/catalogue services. File names are investigation starting points, not a conclusion about root cause.

## 3. Confirmed symptom versus unknown cause

Owner's #162 artifact test: `Mutant.fbx` queued with **12 external animation FBXs**; preview reported one mesh, one material, two textures, one native animation and 37 bones. The last visible stage was `PROCESSING // WRITING RASSET PACKAGE`, after which Studio returned to the editor without a `.rasset` in `Content/Characters` or a Character Asset Browser entry. Plain **Model** import of Mutant also failed; another FBX lacked a browser entry but its on-disk state was not established. Thus the **shared import/save/registration path is the first diagnostic target**. Whether the failure occurs during conversion, preparation, recipe validation, serialization, transaction, registry refresh or UI reporting is **unknown**. No evidence yet proves that the 12 clips retargeted successfully.

Current code contains external-animation snapshot and humanoid-retarget paths, but code presence is not functional acceptance. The existing Studio error path can send the detailed reason to backlog while presenting only the first line in the visible status: expose the actual reason and failing stage without claiming a speculative fix.

**Fixture boundary:** the owner supplied the Mutant plus 12 clip FBXs privately for investigation. This repository is public. **Do not commit, publish in a PR, or upload the owner's FBXs, textures, personal paths or archived GGMAX source to the public repository or CI artifacts** without a separate explicit decision on permissions and distribution. Use existing licensed/public fixtures and synthetic fixtures in GitHub tests. State clearly which Mutant-specific tests cannot run in GitHub until an approved, secure fixture route exists.

## 4. Track A — shared import/save reliability (first code priority)

### A1. Evidence-rich, non-modal diagnostics

Instrument the existing transaction without changing its semantics initially. Return a structured per-attempt stage, full underlying reason, destination, source/asset stable IDs where safely available and whether each of these actually occurred: source snapshot, imported scene, recipe preparation, WISCENE/container serialization, atomic commit, registry/catalogue refresh, browser reveal. Show a readable complete failure in the Studio importer (with a detailed expandable diagnostics view); preserve a correlated log record, without leaking sensitive absolute paths in shareable logs. Never display `Imported` merely because processing returned to Studio, or hide `Reason:` behind a one-line status. Preserve last-good products and leave no authoritative partial product on failure.

**Gate A1:** a controlled failure at each available stage produces an actionable Studio-visible error and the correct disk/registry rollback outcome. Document any failure that cannot be injected on GitHub runners.

### A2. Repair and prove the common transaction

Trace `CreatorAssetWorkflowService::ImportModel` through `ReusableAssetService::ImportModelAsset`, import conversion, recipe, `.rasset` write, registry/metadata transaction and Asset Browser refresh. Start with a plain Model and an existing GitHub-safe FBX fixture; do **not** mix in external-animation retargeting yet. Make only the smallest proven repair(s), with regression tests distinguishing: no package written, package written but not registered, registry entry missing from browser and a stale browser projection. Test deterministic name/destination collision and failed/retried import without deleting an unrelated asset.

**Gate A2:** a real supported static or skinned model imports into a physical valid `.rasset` at the intended `Content/Models` destination, has a durable stable-ID registry/metadata entry, is shown in the actual browser projection, can be reopened and survives a reload; forced commit failures preserve previous bytes. The same shared path works for Character destination without requiring any external clips. Source-fixture limitations and hosted-runner graphics limitations are reported honestly.

## 5. Track B — owner-approved native v3 experience (after A is observable)

### B1. Dedicated native Wicked preview workspace

Implement the approved layout in native Studio, **not** an embedded HTML stand-in: isolated preview scene, useful camera/lighting/grid, large central character/model viewport, **resizable right inspector**, six right-side navigation headings only: **Asset Setup, Transform, Materials, Rig & Retargeting, Animations, Review & Import**. A selected heading exposes its controls and collapses the other sections; the inspector scrolls internally without growing the page or overlapping ordinary Studio UI. Do **not** add a duplicate left-hand stage toolbar. Preserve pending model/material/animation edits on section changes.

Choose **Model** or **Character** once in a prominent mutually exclusive control. Model workflow omits inapplicable rig/animation sections; Character exposes them. Maintain the imported asset's original scale by default, explicit transforms/unit conversion, and an **independent fixed 1.82 m reference** that never inherits model transform. Restore editor view/state on cancel and commit; never insert an asset into the level as a side effect of import.

**Gate B1:** native viewport/inspector interaction is demonstrated by the relevant CI/build evidence and direct application review where supported. No source-contract-only or HTML demonstration is an owner-visible native UI pass.

### B2. General humanoid mapping and genuine clip playback

Queue external animations with reliable retained source provenance and action identity, including source filenames, duration/range and include/rename/remove controls. On clip selection, retarget (or directly bind if skeletons demonstrably match) and **play the real resulting clip on the actual imported character** in the isolated preview scene. Provide mapping diagnostics: source rig, target rig, mapped/unmapped required bones, conflict or ambiguity, rest/axis mismatch when known, and a truthful per-clip status (`not analysed`, `mapped`, `retargeted`, `failed`, etc.). Allow review/correction of bone assignments via existing Wicked humanoid mapping commands rather than a Mixamo-only converter.

Retain support goals for **Mixamo, UE4/UE5, Unity humanoid-compatible exports, Blender Rigify deform-skeleton exports and other compatible humanoids**. Validate each family using representative licensed fixtures before claiming it works. Rigify control/helper bones are not automatically guaranteed to be suitable deform/export bones. Reuse Wicked's generic `HumanoidComponent` / `RetargetHumanoidAnimationsCommand` where proven; audit the current auto-mapper's naming and hierarchy assumptions. Preserve clip time range, the target rig/skin and source rest pose; do not copy incompatible local bone rotations naively. Review hands/fingers and joint deformation visually as well as structurally, drawing on the separate GGMAX reference only when technically appropriate and appropriately licensed.

**Gate B2:** selecting each tested clip demonstrably animates the imported character; source-to-target mapping, failed/partial states and playback remain inspectable. For the owner's identical-skeleton Mutant + 12 clips, a private acceptance route must prove all 12 separately; GitHub-safe fixtures must additionally exercise **different humanoid skeletons**, not only Mixamo-to-Mixamo or identical bone names. Every untested family is marked unverified, not passed.

### B3. Durable commit, reopen and placement integrity

Review displays the true intended name, destination, included clips and their verified mapping states. Commit only after a successful governed `.rasset` transaction to `Content/Characters` or `Content/Models`. Confirm the real file, registry/metadata identity and Asset Browser projection; reveal the resulting asset and support close/reopen with clips, transforms, materials and skin intact. Scene placement is separate; authored position/rotation/scale and gizmos must survive placement, save and reopen. Investigate the separately reported barrel-to-Character promotion `(0,0,0)`/gizmo regression without disguising it as an importer failure.

**Gate B3:** valid package + browser + reopen + placement proof and rollback on failure. A simulated success modal or a green compile alone cannot satisfy this gate.

## 6. Delivery and GitHub validation rules

1. **Planning artifact:** this document on its isolated docs branch, with no code, binaries, proprietary FBXs or changes to `main`/PR #158/#162. Do not open a PR solely to trigger CI.
2. **Implementation branches:** start each bounded code stage from then-current `main`; audit the current branch/PR references first. Keep A diagnostics/repair separate from B native UI/animation changes. Port a needed #162 importer change selectively only after examining its diff, never by merging the mixed branch wholesale.
3. **Recoverability:** commit small coherent changes directly to GitHub with test evidence and explicit known gaps; preserve parallel programme branches and avoid force pushes/rebases/deletion. Keep this plan and acceptance ledger updated as code lands.
4. **CI:** when a code stage is ready, open a scoped PR targeting `main` and use the repository's GitHub Actions Windows Debug/Release jobs. Avoid repeatedly triggering lengthy full builds on speculative edits. Run source/unit/integration tests on appropriate GitHub runners, attach non-sensitive failure reports and inspect exit status, test totals, built artifacts and package completeness. Do not infer native animation/visual quality from CI compilation.
5. **Hosted environment limitation:** if GitHub-hosted Windows cannot exercise a graphics-dependent Studio test, record it as **not verified**; do not quietly substitute local PC execution or claim full owner acceptance. Agree a GitHub-compatible validation method separately before expanding assertions.
6. **Runtime scope changed by owner:** the earlier local matched-Runtime-build step is explicitly **skipped**. No local packaging or TestGame claim is part of this plan. Runtime/TestGame end-to-end acceptance remains a clearly deferred dependency rather than a completed gate.

## 7. Overall acceptance ledger (initial state)

| Evidence | Status at plan creation |
| --- | --- |
| Approved v3 layout and interaction intent | Owner-approved concept; **not native implementation** |
| GitHub `main` / #158 / #162 separation | Checked at document baseline; recheck before code |
| Recovered GGMAX reference | Read-only external reference; not imported into Renegade |
| Shared Model/Character `.rasset` save diagnosis | **Unresolved** |
| Native v3 workspace | **Not established as implemented** |
| Mutant 12-clip retarget/play/save/reopen | **Not verified** |
| Unreal / Unity / Rigify cross-rig retargeting | **Compatibility to prove individually** |
| GitHub Windows build and functional proof for v3 | **Not run** |
| Matching Runtime / TestGame acceptance | **Deferred by owner instruction** |

**Next executable engineering task:** on a fresh, narrowly scoped GitHub branch, inspect the shared import failure path and add Studio-visible stage/full-error reporting plus a GitHub-safe real-asset regression. Establish the actual failure before attempting an animation-only fix or rebuilding the importer UI.
