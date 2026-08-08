# Renegade Engine — Current Handoff

**Date:** 2026-08-08

**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`

**Current main baseline:** `1c9fe841046f4b9a56e7ed4966e251bca31b0330`

**Active branch:** `poc/lp05-gate4-wiscene-typed-walker`

**Wicked pin:** `3a800b7134aafe58461093c8abb2e274d4e64033`

## Critical local safety rule

`Tools/Windows-Build.Common.ps1` contains an unrelated local modification and must not be staged, reset, reverted, stashed, cleaned or overwritten as part of lifecycle work.

Expected SHA-256:

`3CEDE22CB6C8B53404E3A06A26F67D1E8E7C333FE8BECD3EA612AB3E25E0645E`

Temporary LP04 helper patch files may remain untracked. Do not use `git clean` and do not use `git add .`.

## Current truth

LP04 — Unsaved Test Level Snapshot is complete and accepted on `main`. See "LP04 accepted behaviour" below for the summary and `docs/LP04_TEST_LEVEL_ACCEPTANCE.md` for the full record.

LP05 — Representative Dependency Extraction is active. Gates 1-3 are complete and both Windows Debug and Release are proven passing (23/23 tests, `RenegadeDependencyTests` included) on the `Renegade Studio` CI workflow. See "LP05 progress" below and `docs/LP05_REPRESENTATIVE_DEPENDENCY_EXTRACTION.md` for the full technical record, including the Gate 2 Unicode/canonicalization corrections and the Release `CL.exe` crash resolution.

PR #24, `Run Studio checks on every pull request`, fixed the required-check deadlock for docs-only PRs by ensuring the Renegade Studio workflow runs on every pull request targeting `main` while retaining push-to-main path filtering.

- Main merge commit: `1c9fe841046f4b9a56e7ed4966e251bca31b0330`

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

## LP05 progress — Representative Dependency Extraction

LP05 comes before broader asset identity/cooking/packaging work. Its purpose is to establish trustworthy, deterministic dependency discovery from representative Renegade/WISCENE content, producing Renegade-owned dependency/path records that later lifecycle slices can consume.

**Gates 1-3 are complete and merged** through PR #27 at `1c9fe841`. They establish the dependency-graph contract, canonical path/security layer, UI-free provider interface, and concrete project, Story Flow, Runtime Screen and declared-reference providers. Gate 2's path identity was corrected twice after initial landing — once for Windows Unicode/case identity, once for a filesystem-canonicalization leak in that same fix — both proven on real Windows Debug hardware.

**Both Debug and Release are proven.** A separate, pre-existing Release-only `CL.exe` access-violation crash blocked Release proof across two earlier sessions; it was root-caused this session to local CPU hardware instability (corrected machine-check errors confirmed via Windows Event Viewer/WHEA history), not a toolchain or code defect. Release was verified sound on GitHub Actions instead: the `Renegade Studio` workflow passed 23/23 tests in both Debug and Release, `RenegadeDependencyTests` included, no crash.

Full technical detail for all of the above — the architectural decision, the gate-by-gate implementation boundaries, both Gate 2 corrections, and the complete Release crash investigation and resolution — is in `docs/LP05_REPRESENTATIVE_DEPENDENCY_EXTRACTION.md`.

**Gate 4 is implemented on `poc/lp05-gate4-wiscene-typed-walker`, pending GitHub CI and independent verification.** It adds a Renegade-owned read-only WISCENE provider over the validated scene-open seam and walks public Material, Light, Environment Probe, Weather, Sound, Video and Script component resource fields. Tests cover all seven through the direct const-scene walker; the real headless WISCENE reader fixture covers the other six because Wicked's Environment Probe deserializer requires an initialized graphics device. Coverage includes typed provenance, repeated-read order, duplicates, missing resources, metadata exclusion and byte-identical source preservation. Wicked remains unchanged; serializer internals are not intercepted.

### LP05 architectural decision

For WISCENE native-resource extraction, use a **Renegade-owned, read-only typed walker** over the pinned Wicked scene/ECS component structures.

Do **not** modify Wicked's serializer merely to expose its internal resource-registration state. Wicked source remains off-limits unless a later gate demonstrates that no viable Renegade-owned external route exists.

Representative extraction should cover the resource-bearing native components needed by the gate, deduplicate paths deterministically, preserve provenance, and report deliberately missing/unresolved dependencies as structured evidence.

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
- If a local Release build crashes with an unexplained access violation, check Windows Event Viewer/WHEA for hardware-level corrected errors before assuming a toolchain or code regression — see the LP05 Release resolution in `docs/LP05_REPRESENTATIVE_DEPENDENCY_EXTRACTION.md` for the precedent.
