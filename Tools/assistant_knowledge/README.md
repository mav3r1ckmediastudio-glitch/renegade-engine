# Renegade Assistant — RA-01 Knowledge Database Prototype

**Status:** Read-only local prototype built and tested on DESIGN, 2026-10-13. Not Studio-integrated, packaged, merged, or owner-accepted. No LLM inference or editor mutations.

## Why this exists

The older A1 experiment at `Downloads/Renegade-Assistant-Proof` created a SQLite FTS index of 2,893 mixed documentation passages and six source-checked guides, pinned to revision `380dcc057db2f2470e0f4e946ed50834ffc67ca2`. Those references were useful for finding material, but stale plans and DeepWiki content could be mistaken for currently implemented Renegade features.

This RA-01 slice **reuses the approach and preserves the six guides as historical leads**, rather than importing their old assertions as facts about the current build. It indexes the **current local checkout**, without network access, and adds a small independently validated fact layer.

## Architecture

- `knowledge.py`: standard-library Python 3.12+ SQLite FTS5 builder and read-only query CLI. No dependencies or model files.
- `facts.json`: curated answer cards with literal code/document evidence markers. A source fingerprint is stored at build time, and rechecked before answering.
- `legacy_guides.json`: the original six A1 authored guides, explicitly labeled `historical_needs_revalidation` and **not returned as verified answers**.
- `BUILD/assistant_knowledge.sqlite`: generated local database, not committed to the repository. Also suitable for packaging later once verified.
- `Tests/assistant_knowledge/test_knowledge.py`: isolated regression tests for trust boundaries.

`source_verified` means **the claimed code/document literals were present in the indexed local checkout and have not changed**, *not* that the editor UI, runtime behavior, or creator acceptance has been re-tested. Source needles are an initial verification method; future releases must bind tests/receipts and exact build versions, not rely on literal matches alone.

### Trust policy

1. **Verified fact cards** may support bounded claims about documented/code-defined workflows. Each answer also states missing editor observation/action permissions.
2. **Legacy guides** are preserved for revalidation but cannot be promoted automatically if they match search terms.
3. **Markdown passages** are indexed as `reference_only_not_feature_proof`. They can contain plans, superseded sections, and historical notes. A retrieved passage never becomes automatic authoring guidance or permission to edit.
4. Source files are SHA-256 fingerprinted and rechecked on retrieval. If changed/missing, verified status is withheld until a rebuild and review. Matching current source is still not equivalent to shipping or runtime acceptance.
5. The local checkout/branch/dirty state is recorded for traceability. Do not publish a snapshot built from a dirty or outdated feature branch as authoritative shipped documentation.
6. The model and orb remain separate. The eventual orchestrator should prefer `verified_facts`, cite the underlying paths, and abstain on unsupported questions.

## Run (Windows, offline)

From the repository root, with working Python and SQLite FTS5:

```powershell
python Tools/assistant_knowledge/knowledge.py build
python Tools/assistant_knowledge/knowledge.py ask "How do I set a glass impact?"
python Tools/assistant_knowledge/knowledge.py ask "Can you inspect my selected object?" --strict
python -m unittest discover -s Tests/assistant_knowledge -p test_knowledge.py -v
```

The default database is `BUILD/assistant_knowledge.sqlite`. An explicit repo and output can be specified using `--repo` and `--db`. The builder is offline, read-only to the source tree and atomically replaces only the generated database file. It does not invoke, update, or download an LLM.

## Built checkpoint — DESIGN, 2026-10-13

- Local branch: `feature/p3-original-impact-vfx`; HEAD `60634cec26feb7ffc2d0f941d3381865fa3b41c7`. **Dirty at build time with unrelated uncommitted projectile and RA-02 changes.** Do not treat as evidence of current `main`.
- Build: 195 repo documents, 7,520 reference passages, 6 historical guides, 5 source-verified fact cards. No DeepWiki auto-generated pages promoted to facts.
- Tests: 8/8 passed; evidence and command in this README and the sibling HANDOFF.
- No modifications to existing tracked engine/Studio/Runtime files. No build of Studio, model training, commits, PRs, or pushes performed.

## Necessary follow-up before editor integration

1. Rebuild from an independently reviewed exact integration commit on the intended branch; revalidate A1 guides against that source and actual native UI.
2. Add more source-checked workflows deliberately (importer, animation retargeting, player equipment/projectiles, scripting), backed by runtime tests and creator evidence. Do not automatically label all repository documentation `implemented`.
3. Define a compact machine-readable response format and source locations/line anchors for the eventual RA-02 dock. Guard against prompt injection in retrieved text, output-length limits, invalid citations and branch mismatch.
4. Wire **read-only** knowledge retrieval to the existing assistant orchestrator once accuracy/abstention regressions pass. No editor mutation or fake selection inspection.
5. Subsequently build RA-05 read-only Editor Context Broker and, only after review/Undo/Redo support, the RA-06 approved bounded-action layer.
6. Record independent verification in canonical `HANDOFF.md` and feature matrix at the appropriate integration point; no gate is closed by this prototype.

Keep the standalone Qwen A3, approved V1 orb and Daniel voice prototypes intact. This stage adds **no model weight** to base installation.

## 2026-10-13 continuation: Projectile manual connection

- Added `docs/manual/PROJECTILES_FIRST_WEAPON.md` and `Tools/assistant_knowledge/projectile_guide.json` as the first human/AI paired source record.
- Knowledge builder now adds `manual_guides` and its full-text lookup. It fingerprints the manual and every evidence file, checks each cited source marker at build, and rechecks fingerprints at query time.
- Retrieval exposes the guide as `draft_source_matched`, `draft_evidence_failed`, or `draft_stale_rebuild_required`, **never** as fully implemented or published. `publication_approved` remains false, including when all source markers match.
- Live database on DESIGN: 196 Markdown files, 7,547 passages, 5 source-backed facts, 6 historical guides, 1 linked manual draft.
- `python -m unittest discover -s Tests/assistant_knowledge -p test_knowledge.py -v`: 9/9 PASS, including detection of a stale changed manual.
- This does **not** constitute a live Studio walkthrough or packaged Runtime proof. The existing native Debug/Release executables were located, but no GUI simulation or owner acceptance was conducted. The source checkout has unrelated uncommitted feature work and no push/merge was attempted.
- Next: run the complete tutorial in Studio on an isolated disposable project and capture actual UI screenshots before promoting it; then connect read-only retrieval to the model conversation transport.

## 2026-10-13 evening: beginner projectile manual verification

- Added `Tests/assistant_knowledge/test_projectile_manual_contract.py` covering exact native UI labels, initial Bullet preset vs selectable Arrow preset, single-launch policy visibility, explicit asset save vs apply-to-player, release-time validity and draft/publication boundary.
- Knowledge/source tests: `C:\\Python314\\python.exe -m unittest discover -s Tests/assistant_knowledge -p 'test_*.py' -q`: **15/15 PASS**.
- Existing native Release tests: `C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\CMake\\bin\\ctest.exe --test-dir BUILD/renegade -C Release -R '^(RenegadeProjectileAssetTests|RenegadeRuntimeProjectileSessionTests|EquipmentAsset|LaunchSocket)$' --timeout 35 --output-on-failure`: **4/4 PASS (1.44s)**.
- Release Studio executable at `BUILD/renegade/Studio/Release/RenegadeStudio.exe` exists (last modified 2026-10-09). Disposable Bow Playground fixture exists under BUILD. **No interactive Studio walkthrough was performed**: this remote file/terminal connection does not provide reliable visual GUI interaction. No actual screenshots captured. Manual remains a DRAFT; neither full end-to-end authoring nor latest working-tree runtime parity has been claimed.
- No gameplay/Editor source modifications, no extra LLM installation, no commits or pushes. Do not mix these untracked docs/tests into the active projectile feature work without review.

## 2026-10-13 late: actual Studio UI validation recovered

Remote GUI automation was recovered using Pillow, pywin32 and the left monitor's negative screen coordinates. Launched local `BUILD/renegade/Studio/Release/RenegadeStudio.exe` (last modified 2026-10-09), entered Project Hub, opened existing disposable `BUILD/bow-projectile-playground/Bow-Playground.renegade` and the Main Level editor. Selected Player Start through Characters hierarchy. Opened Starting Equipment and Weapon Firing; saved bow uses **Primary fire / use**, **Flaming Arrow - Stick**, **Bow_Nock**, **After a delay** at **0.133333 seconds**. NEW defaults to Bullet; changing to Arrow yielded 45 m/s, gravity 1 and no visible model. SHOW ADVANCED displayed lifetime, rotations, damage and particle properties. SET FIRE TIME IN ANIMATION opened live bow preview showing 0.133333 s marker and 0.833333 s total duration. Cancelled timing preview and projectile draft without saving/applying.

Five real screenshots (cropped from actual local Studio screenshots) are now under `docs/manual/images` and embedded in `docs/manual/PROJECTILES_FIRST_WEAPON.md` (draft 0.2). Fixed duplicate/misleading advice that bows always use Release; verified current example uses Primary fire/use. Companion `projectile_guide.json` carries observed evidence and explicit outstanding tests. Added automatic screenshot-existence checks to `test_projectile_manual_contract.py`. Full local assistant/manual suite: **16/16 PASS**. Rebuilt SQLite index: 196 documents, 7,554 passages, 5 source-backed facts, 1 linked draft manual guide. Existing unrelated engine files and active feature branch unchanged; no commit/push.

**Limits:** This is now an actual native editor inspection, not yet end-to-end fresh authoring acceptance. Did not create/save a new projectile, apply a new loadout, save/reopen the scene or run Test Level with that new projectile. Manual remains draft. The existing Release Studio executable predates the current dirty working tree; screenshots are evidence for that executable only.

## 2026-10-13 continuation: expanded projectile knowledge, isolated e2e fixture

- Expanded `facts.json` from five to **12** source-checked cards. Added native physical/instant hitscan fire modes, NEW vs EDIT COPY, Arrow starting defaults, single/multiple launch-point visibility, animation marker use, save-vs-apply-to-player, and Stick requiring a visible mesh. Each new record was checked against exact Studio/EngineBridge source text before writing.
- Retrieved answers on the actual built SQLite database. "How do I assign my new projectile to the player?" retrieves specific supported steps; "Can Renegade create an entire world automatically?" now returns no verified facts. The relevance-filter regression found and repaired during this work prevents the generic word "create" from satisfying an unrelated capability question.
- **16/16 source, knowledge and manual tests PASS**; rebuilt SQLite: 196 indexed Markdown files / 7,554 passages / 12 verified source facts / six historical guides / one linked manual draft. The 2026-10-09 native build's previously run four focused CTest checks were green; no new full gameplay acceptance is implied.
- Duplicated the existing ~820 MB Bow Playground to `BUILD/manual-projectile-e2e-20261013` for an isolated save/reopen test. The source and clone descriptors, scene, registry, metadata and storyflow were hashed and matched; no newly saved arrow or equipment was produced.
- GUI attempts were **stopped** when window focus shifted from Studio to a browser/Runtime on the two-monitor desktop. It was not possible to establish conclusively that Studio had opened the cloned project. The copied editor-state file preserved the original timestamp, so it is not evidence of a successful clone open. Full test status and safe restart instructions are at `Tools/assistant_knowledge/MANUAL_E2E_STATUS.md`.
- No existing Studio/EngineBridge/Runtime source files were edited, no PR/commit/push, and the manual's `publication_approved` is still false. The connected Qwen model has not been updated or integrated in this step.

## 2026-10-13 final continuation: native projectile manual E2E passed

Supersedes the previously recorded pending GUI e2e checkpoint. The remaining limitation is **creator acceptance and broader coverage**, not this core workflow.

- Opened the explicitly renamed **MANUAL QA COPY - Projectile Playground**, copied independently under `BUILD/manual-projectile-e2e-20261013`. Original project files were not modified.
- In Studio Release (dated 2026-10-09), authored a NEW Arrow projectile with imported `Content/Projectiles/Models/Arrow.rasset`, gravity 1, speed 45 m/s and Stick into the object. Saved via SAVE AS NEW as `Manual QA Arrow E2E`, asset ID `c030b651-00c0-4af7-9827-3a29ed42e46e`.
- Applied to the existing Player Start's **Primary fire / use** action, using `Bow_Nock` at 0.133333 s. New equipment asset ID `87a28639-a59f-4bd4-b428-bb75ee82591a` exists only in the QA copy.
- Saved via Ctrl+S; SHA-256 of the original scene remained `68adb4ecd391e5cf1ec8f667ea9f0c46990ce21082c1494c259056553680c5e0`, while the test clone changed to `b542716bf4d0012560c9f2f2b9930872d9e38e0360b9e69b923a27e86088dc0f`.
- Reopened the saved scene, confirming the selected arrow, Bow_Nock, 0.133333 s delay and new equipment identity remained. Launched Test Level: the standalone runtime title confirmed the QA-copy project, the bow fired, impact feedback occurred and a dark retained target mark remained when the player moved sideways. Runtime closed normally; Studio displayed TEST LEVEL // COMPLETED.
- Returned to Project Hub and reopened the QA project; Starting Equipment still displayed the new equipment identity.
- Manual updated to **draft 0.3**, with six new native QA screenshots (`06` through `11`) totalling ~703 KB, and `projectile_guide.json` now records the specific native acceptance plus outstanding creator/surface-type/blank-project tests. **Publication remains unapproved**.
- Full receipt, asset identifiers, evidence and limitations: `Tools/assistant_knowledge/MANUAL_E2E_STATUS.md`. No core engine/editor/runtime source changes, new model weights, commits, PRs or pushes performed as part of this QA.

### Final documentation build and regression run (after QA evidence)

- `C:\\Python314\\python.exe -m unittest discover -s Tests/assistant_knowledge -p 'test_*.py' -q`: **17/17 PASS**, including screenshot references, native receipt status, and separation between source verification, QA and publication approval.
- `C:\\Python314\\python.exe Tools/assistant_knowledge/knowledge.py build`: **12 source-verified facts, 1 linked draft manual, 196 indexed Markdown files, 7,563 searchable passages, 6 historical guides** on current local branch HEAD `60634ce`.
- `ctest --test-dir BUILD/renegade -C Release -R '^(RenegadeProjectileAssetTests|RenegadeRuntimeProjectileSessionTests|EquipmentAsset|LaunchSocket)$' --timeout 35 --output-on-failure`: **4/4 PASS (1.27 s)**.
- Documentation and QA test changes remain untracked and uncommitted in `docs/manual`, `Tools/assistant_knowledge`, and `Tests/assistant_knowledge`. Preserve unrelated active work. QA clone under ignored BUILD contains the saved test assets and should remain available for independent creator review; it is not intended for core distribution.
