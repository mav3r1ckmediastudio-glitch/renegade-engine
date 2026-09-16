# Creator Importer A1 — evidence ledger

**Status:** A1 started; no importer defect has been fixed or reproduced on a GitHub runner.  
**Baseline:** `main` `5732d8d460610045275a9149252e47f0a3d75a39`; approved v3 plan: [`CREATOR_IMPORTER_V3_ENGINEERING_PLAN.md`](CREATOR_IMPORTER_V3_ENGINEERING_PLAN.md).  
**Execution boundary:** GitHub only. No access to owner's PC or Runtime. Do not merge PR #162 or disturb PR #158. No proprietary FBX fixtures in the public repository.

## Observed owner symptom (not a root-cause diagnosis)

The #162 Studio artifact reached a displayed `WRITING RASSET PACKAGE` message then returned to Studio without a visible Character asset or physical `.rasset` in `Content/Characters`; plain Model import of Mutant also failed. The full underlying error was not visible. Animation retargeting success is unverified.

## A1 failure-path inventory and evidence requirements

| Boundary | Evidence to record per attempt | Invariant / controlled failure |
| --- | --- | --- |
| Start / source snapshot | attempt ID, selected asset kind and project-relative source | failure shows complete reason; no product/registry mutation |
| Source conversion | source type, imported mesh/object/armature/animation counts | empty/invalid conversion rejects with complete error |
| Recipe and external sources | validated options and source clip identities, excluding absolute personal paths | invalid recipe does not commit |
| WISCENE and `.rasset` serialisation | payload/container created and validated or exact failure | no success claim on serialization failure |
| Atomic project transaction | destination relative to `Content`, commit status | failed commit leaves no partial authoritative `.rasset`; prior bytes remain |
| Registry, metadata and catalogue | stable ID (if available), registration/reload/refresh outcomes | distinguish missing product, missing registry, stale browser projection |
| Studio browser reveal | whether the saved product was found and selected | never report `Imported` based solely on UI close |

A1 implementation must provide the **stage actually reached**, **full untruncated reason**, project-relative destination, safe IDs and a correlated diagnostic record in an accessible Studio view. The stage UI must reflect the backend result, not fabricated percentages; detailed diagnostics must be accessible without opening an external browser or searching hidden backlog entries.

## First focused implementation and proof gate

1. Trace `StudioApplication::ShowStudioMessageBox`, the importer Confirm Import callback, `CreatorAssetWorkflowService::ImportModel`, `ReusableAssetService::ImportModelAsset`, `ProjectDocumentTransaction`, the registry refresh and Asset Browser reveal; record actual function locations and whether each result already carries error/stage.
2. Add complete visible error/detail handling and stage propagation **without changing transaction semantics**. Cover multiline reasons and paths safely.
3. Add focused tests using existing GitHub-safe fixtures and deterministic injected failures wherever facilities already exist; record unavailable injection points as gaps, not passes.
4. Commit small changes on a fresh code branch based on current `main`, review the diff, then run GitHub CI once the scoped code stage is ready. Hosted rendering/owner visual acceptance is **not verified** by CI compilation alone.

**A1 exit:** controlled failures yield actionable Studio-visible errors with the correct stage and no partial-authoritative product. This ledger stays open until those tests and the application behaviour have evidence; A2 repair and B native importer are separate stages.
