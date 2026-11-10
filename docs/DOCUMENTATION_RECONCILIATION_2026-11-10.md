# Documentation reconciliation receipt — 10 November 2026

## Scope and source authority

Base: main 618b2310ec7d6a7889d490e93304e39043f7e5ff (merged PR #193).
Documentation branch: docs/current-main-reconciliation-20261110.
Current Wicked gitlink and Windows-build guard:
82557d439a41d104f559b0d3134d160b7bf6154e.

This is a documentation and matching knowledge-fingerprint change. No engine
source, native assets, submodule pointer, build/workflow behaviour or branch
protection was changed. Existing generated Wicked overlays and owner uncommitted
projects/checkouts are preserved. No binary build or distribution was performed.

## Reconciliation performed

- Replaced stale README, roadmap and handoff orientation with the actual merged
  baseline, source/acceptance boundaries and remaining work.
- Added CURRENT_STATUS.md with the verified merge chain #178 through #193 and
  the four successful pre-merge prefab checks at cb590f0.
- Added CREATOR_GUIDE.md for actual current project/import/selection/prefab/world/
  weather/light/particle/script/Test Game/Build Game workflows.
- Added a complete catalog of repository-owned Markdown, classifying current
  guidance, specifications, historical checkpoints and immutable evidence.
- Reconciled architecture orientation, upstream pin and separate overlay
  provenance, both build paths, module READMEs and test strategy.
- Corrected 39 stale feature-row integration/status records. Full native/backend
  parity, owner acceptance and independent release gates were not inferred from CI.
- Updated current feature/assistant guidance and labelled historical local QA
  catalog statements. No promise of shipping local-only particle presets.
- Preserved historical evidence receipts. The old aggregate README/roadmap/handoff
  content remains accessible at the explicitly linked immutable source-baseline
  GitHub blobs and Git history; no failed receipt was rewritten as a pass.
- Repaired known punctuation/encoding defects without changing source code.
- Reviewed eight changed Markdown workflow fingerprints. All curated evidence
  needles and underlying native controls remain; only corresponding hashes and
  review metadata changed. New orientation pages remain reference-only.

## Local verification

| Command/check | Result |
|---|---|
| All-document local file/image-link audit | 451 existing/new Markdown documents checked before this receipt; zero unresolved file targets |
| Changed Markdown strict UTF-8 and local-link check | PASS |
| Feature CSV validation | 64 unique records; 16 named columns; no malformed rows |
| Curated workflow marker/fingerprint review | Eight reviewed Markdown fingerprints; all curated markers retained |
| python Tests/AssistantWorkflowSuite.py --repo . --output BUILD/docs-assistant-workflow | 84/84 PASS |
| python Tests/AssistantPackagedKnowledgeTests.py --repo . --output BUILD/docs-assistant-package | PASS: Gitless identity/routed answers and changed-source/stale-index rejection |
| ctest --test-dir BUILD/selection -R SourceContract --output-on-failure | 60/60 PASS using the existing configured local tree; no rebuild |
| git diff --check | PASS |

The final index includes this receipt. External URLs and every optional historical
anchor are not exhaustively network-validated; local file targets were checked.
No new native/editor/Runtime behaviour is claimed from these documentation tests.

## CI and integration

The owner explicitly requested no CI. The single documentation commit contains
[skip ci]; no workflow was changed and no manual CI was dispatched. A later merge
must retain a skip instruction if it too is intended to avoid CI. Skipped required
checks can remain pending; they are not green tests. No protection override is
authorized or performed by this task. The documentation PR remains unmerged
until owner approval.

## Reviewed fingerprint references

- surface-type-v1: docs/ARCHITECTURE.md
- animation-add-v1: docs/MODEL_IMPORTER_REBUILD.md
- animation-action-v1: docs/MODEL_IMPORTER_REBUILD.md
- npc-wander-v1: docs/AI_IMPLEMENTATION_HANDOFF.md
- character-prefab-v1: docs/CHARACTER_WORKFLOW_PROGRESS.md
- character-prefab-v1: docs/CHARACTER_WORKFLOW_PROGRESS.md
- character-import-v1: docs/CHARACTER_WORKFLOW_PROGRESS.md
- static-model-import-v1: docs/MODEL_IMPORTER_REBUILD.md

## Encoding/punctuation repairs

- HANDOFF.md
- README.md
- docs/P3_DIRECTIONAL_BLOOD_PROTOTYPE.md
- docs/UI_DESIGNER_FRAMEWORK_CONTINUATION_2026-10-17.md
- docs/UPSTREAM_SYNC.md

## Updated files

- CLAUDE.md
- EngineBridge/README.md
- HANDOFF.md
- README.md
- Runtime/README.md
- Studio/README.md
- Templates/README.md
- Tests/README.md
- Tools/README.md
- Tools/assistant_knowledge/README.md
- Tools/assistant_knowledge/workflows.json
- docs/AI_IMPLEMENTATION_HANDOFF.md
- docs/AI_WORKFLOW.md
- docs/ARCHITECTURE.md
- docs/BUILD_WINDOWS.md
- docs/CHARACTER_WORKFLOW_PROGRESS.md
- docs/CREATOR_GUIDE.md
- docs/CURRENT_STATUS.md
- docs/DOCUMENTATION_RECONCILIATION_2026-11-10.md
- docs/FEATURE_LEDGER.md
- docs/FEATURE_MATRIX.csv
- docs/INVENTORY_CORE_G0_G1.md
- docs/LIGHT_PICKER_AND_BEHAVIOURS.md
- docs/LIVE_DIAGNOSTIC_ACCESS.md
- docs/LP06_NAMED_STANDALONE_SAFE_REBUILD.md
- docs/MASTER_PLAN.md
- docs/MAXIMUM_TERRAIN_RIVER_RECOVERY_2026-11-03.md
- docs/MODEL_IMPORTER_REBUILD.md
- docs/P1_STATUS_AND_RECOVERY.md
- docs/P2_EQUIPMENT_ACTION_IMPLEMENTATION.md
- docs/P3_AUTHORED_PROJECTILES_GATE.md
- docs/P3_CONTINUATION_HANDOFF.md
- docs/P3_DIRECTIONAL_BLOOD_PROTOTYPE.md
- docs/P3_PROJECTILE_AUTHORING_UX.md
- docs/PARTICLE_LIFETIME_CONTROLS.md
- docs/PARTICLE_LUA_API.md
- docs/PARTICLE_PICKER_AND_LAYERED_EFFECTS.md
- docs/PARTICLE_SHAPES_TIMING.md
- docs/PARTICLE_STARTER_EFFECTS.md
- docs/PARTICLE_TEXTURE_LIBRARY.md
- docs/PLAYER_ARMS_COMBAT_FRAMEWORK.md
- docs/PROJECT_CHARTER.md
- docs/README.md
- docs/RIVERBED_PERFORMANCE_2026-11-05.md
- docs/RIVER_WATER_NORMAL_RELOAD_REPAIR_2026-11-05.md
- docs/ROADMAP.md
- docs/SCENE_ASSEMBLY_WORKFLOW.md
- docs/SCRIPTING_S6_LIBRARY_ADOPTION_PACKAGE_CLOSURE.md
- docs/STORY_FLOW_GATE10_RUNTIME_BUILD_CLOSEOUT.md
- docs/TERRAIN_INITIALIZATION_RECOVERY_2026-11-02.md
- docs/TEST_STRATEGY.md
- docs/UI_DESIGNER_FRAMEWORK_CONTINUATION_2026-10-17.md
- docs/UPSTREAM_SYNC.md
- docs/VERIFICATION_CHECKLIST.md
- docs/VSYNC_CONTROL_2026-11-05.md
