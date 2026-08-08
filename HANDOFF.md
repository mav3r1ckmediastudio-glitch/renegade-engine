# Renegade Engine — Current Handoff

**Date:** 2026-08-08

**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`

**Current main baseline:** `a14e3dc0a056a03e7c51e2fc38c1bb3786442d1e`

**Documentation close-out branch:** `docs/lifecycle-roadmap-refresh`

**Wicked pin:** `3a800b7134aafe58461093c8abb2e274d4e64033`

## Critical local safety rule

`Tools/Windows-Build.Common.ps1` contains an unrelated local modification and must not be staged, reset, reverted, stashed, cleaned or overwritten as part of lifecycle work.

Expected SHA-256:

`3CEDE22CB6C8B53404E3A06A26F67D1E8E7C333FE8BECD3EA612AB3E25E0645E`

Temporary LP04 helper patch files may remain untracked. Do not use `git clean` and do not use `git add .`.

## Current truth

LP04 — Unsaved Test Level Snapshot is complete and accepted.

PR #21, `Add LP04 unsaved Test Level snapshot gate`, merged the implementation into `main`.

- Accepted implementation head: `f35ffef588f01a928ba833aea7262d70a67ee1b8`
- Main merge commit: `fbf572e01bff04106b081c72e2ea14ec5fc22bb3`

PR #23, `Record LP04 Test Level acceptance`, added the formal acceptance record.

- Main squash merge commit: `51232c8df22e0f510167a1b7400643298afb5b6d`

PR #24, `Run Studio checks on every pull request`, fixed the required-check deadlock for docs-only PRs by ensuring the Renegade Studio workflow runs on every pull request targeting `main` while retaining push-to-main path filtering.

- Main merge commit: `a14e3dc0a056a03e7c51e2fc38c1bb3786442d1e`

The four Windows PR checks completed successfully on the corrected PR #24 head:

- `Renegade Studio Windows x64 Debug`
- `Renegade Studio Windows x64 Release`
- `Windows x64 Debug`
- `Windows x64 Release`

Branch protection is intentionally being left disabled until this lifecycle close-out is complete. Restore the intended protection at the end of the lifecycle rather than during the documentation refresh.

## LP04 accepted behaviour

LP04 proves that Studio can run the real standalone Runtime from the editor's current unsaved scene state without first overwriting the authoritative WISCENE.

Manual end-to-end acceptance on real Windows hardware proved:

- Studio remained open with the authoring scene visible.
- A newly imported crate remained an unsaved live-scene change.
- PLAY created a disposable Test Level snapshot.
- Studio launched the real `RenegadeRuntime.exe`.
- Runtime reached the explicit READY handshake and Studio entered RUNNING.
- Runtime displayed the unsaved imported crate.
- No ordinary scene save was required to make that change appear in Runtime.
- STOP terminated Runtime correctly.
- Studio remained open and usable after STOP.

See:

- `docs/LP04_TEST_LEVEL_ACCEPTANCE.md`
- `docs/LP04_GATE3B2_RUNTIME_HANDSHAKE_EVIDENCE.md`

### LP04 gate summary

**Gate 3A — process lifecycle**

Established real Win32 process launch/observation, startup timeout handling, bootstrap failure observation, STOP/termination handling, snapshot cleanup and deterministic synthetic fixture coverage.

**Gate 3B1 — Runtime readiness**

Runtime accepts a Renegade-owned `--renegade-ready-event=<name>` argument and signals it only after successful startup.

**Gate 3B2 — real Runtime handshake**

Executed twice on a real Windows GPU/DX12 device. Both runs reached Running, stopped successfully, cleaned the snapshot session and left no lingering Runtime process/window. The Debug suite remained 22/22.

The GitHub-hosted Runtime handshake probe cannot prove this path because the pinned Wicked adapter selection rejects the software/basic adapter available on the hosted runner. The probe remains a manual diagnostic. Wicked source is intentionally unchanged.

**Gate 3B3 — Studio PLAY/STOP wiring**

PLAY snapshots the current scene, launches Runtime using Studio's backend, distinguishes STARTING from RUNNING, polls non-blockingly, exposes STOP correctly, discards unrelated editor actions while Test Level is active, and resolves Runtime relative to the Studio executable.

**Gate 3B4 — manual acceptance**

PASS. The real editor-to-Runtime workflow displayed a newly imported unsaved crate and returned control cleanly to Studio.

### LP04 non-blocking follow-up

Runtime currently starts from its own fixed default camera rather than inheriting the current Studio viewport camera. This can make visual comparison awkward but does not affect snapshot correctness. Treat this as later Runtime/gameplay-camera work, not an LP04 defect.

## Phase position

The active roadmap is now **Phase 4 — Project and asset pipeline**.

The master-plan outcome is that assets become repeatable project resources rather than ad-hoc file opens. Phase 4 still requires:

- project-relative asset database and stable asset IDs;
- Content Browser with folders, filters, previews and drag/drop;
- repeatable import/reimport;
- source-file tracking and import settings;
- texture/audio/video/script/font handling;
- thumbnails and dependency/reference reporting;
- missing/moved asset recovery;
- background import jobs and visible error reporting.

Existing GLB/GLTF import and scene placement are valuable foundations but are not yet a reusable project asset system.

## Next bounded milestone

**LP05 — Representative Dependency Extraction**

LP05 comes before broader asset identity/cooking/packaging work.

Its purpose is to establish trustworthy, deterministic dependency discovery from representative Renegade/WISCENE content. It should produce Renegade-owned dependency/path records that later lifecycle slices can consume.

### LP05 architectural decision

For WISCENE native-resource extraction, use a **Renegade-owned, read-only typed walker** over the pinned Wicked scene/ECS component structures.

Do **not** modify Wicked's serializer merely to expose its internal resource-registration state. Wicked source remains off-limits unless a later gate demonstrates that no viable Renegade-owned external route exists.

Representative extraction should cover the resource-bearing native components needed by the gate, deduplicate paths deterministically, preserve provenance, and report deliberately missing/unresolved dependencies as structured evidence.

LP05 is not permission to build the full Content Browser, cooker or standalone packaging pipeline in one slice.

## Existing foundations relevant to LP05/Phase 4

Already established in merged work:

- native Renegade Studio chrome over Wicked subsystems;
- project-aware scene workflows;
- protected Save/Open scene persistence;
- command-backed Undo/Redo;
- Environment, precipitation, sun/time-of-day and native ocean authoring;
- Terrain Authoring V1;
- native Light authoring and hierarchy markers;
- Model Import V1 GLB/GLTF conversion and WISCENE round-trip proof;
- imported-model scene placement and automatic scale correction;
- stable UUID-v4 project/document/entity identity;
- transactional Renegade-owned project/flow/screen document writes and recovery;
- Runtime screens and stable action dispatch;
- LP04 unsaved Test Level launch.

These do not remove the need for stable **asset** IDs, source tracking, dependency extraction, reimport and a reusable Content Browser.

## Repository rules

- Do not edit Wicked or move its pin without an explicit, justified core-patch decision.
- Do not claim behavioural success from compilation alone.
- A visible or behavioural failure overrides green CI.
- Persistent scene mutations belong behind Renegade-owned command/service boundaries and require Undo/Redo plus Save/Open evidence where applicable.
- Keep lifecycle slices bounded and independently verifiable.
- Do not touch `Tools/Windows-Build.Common.ps1`.
- Do not use `git add .`.
- Do not use `git clean`.
- Hosted CI GPU limitations must be documented rather than worked around by altering Wicked.
- At lifecycle close-out, restore the intended `main` branch protection once documentation and next-milestone state are settled.
