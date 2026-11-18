# Terrain prop CI path correction

Implementation commit: `59c4e926b4a34e9d1523474e8e06b440ff691867`.
Original failed PR head: `a2522190ff920be4dd48be807b6d10b8f9d7bd56`.
Hosted workflow run: `44170123875`, testing GitHub merge tree `af998359f83f4e7c2fb02049888881e12ce826ea`.

## Failure and reproduction

Both Studio configurations compiled successfully. Release then failed TerrainPropSliceProof and TerrainPropEditTests; Debug failed TerrainPropEditTests. Staged revision paths were 266–270 characters long. The transaction implementation appended its stage/backup/restore token to an already 64-character revision digest filename, crossing the ordinary Windows file-open limit at the hosted checkout depth.

The unchanged Release edit executable reproduced the same staging_failed error locally under a directory tree matching the hosted checkout depth. Earlier local preflights used shorter paths and missed this case. This was not a failed renderer-overlay assertion: that hosted check passed in Release (5.75 seconds) and Debug (6.60 seconds).

## Correction

Journal schema 2 names each same-directory artifact by its sorted changed-document index, artifact role and transaction token. It does not repeat the destination basename. Final authored filenames, page/recipe formats, stable IDs, atomic replacement and containment rules remain unchanged. Existing version-one journals retain their original naming and version while recovering, including if recovery is interrupted again.

The regression exercises long digest filenames, unchanged-document filtering, validation, commit, immediate rollback, interrupted recovery and rejection of a mismatched artifact index. A separate version-one journal fixture verifies recovery interruption and retry. The original implementation, compiled against these new tests as a negative control, fails the long-path regression; the corrected implementation passes.

This is not unrestricted long-path support for arbitrary project roots. It removes avoidable transaction suffix growth from otherwise valid destinations. Collision, asset and texture streaming remain deferred.

## Local evidence on the implementation commit

- Release focused suite: 6/6 PASS.
- Release native edit and slice executables at hosted directory depth: both exit 0.
- Debug transaction regression and native edit at hosted directory depth: both exit 0.
- Original-code negative control: expected exit 1, four regression checks fail.
- Release complete preflight: PASS, 341/341 registered tests in 900.32 seconds; both package archives, LP05 dependency proof and LC01 init/update/move/reopen registry proof PASS. Receipt: BUILD/ci-path-repro/release-preflight-fast-knowledge/build-result.json.
- Debug complete preflight: PASS, 326/326 registered tests in 1847.07 seconds; both packages and LP05/LC01 subprocess proofs PASS. Copied receipt: BUILD/ci-path-repro/debug-preflight-result.json.
- WARP Release: 4/4 PASS in 126.62 seconds (allocator, renderer overlay, residency and native slice).
- Fresh native 280px editor Generate/Erase/Undo/Redo/clearing/Paint/Cancel/Save As/reopen PASS, retaining 1,076 placements. Fresh separate player loaded the exact scene bytes, captured frame 60 and exited 0. Both captures were visually inspected; Runtime log had zero warning/error/missing matches. Executable hashes match the complete Release preflight.

Raw receipts are under ignored `BUILD/ci-path-repro/`; the isolated Debug preflight uses the physical directory of the existing validation build. Both native source checkouts are at the implementation commit. The final evidence change also refreshes the single reviewed architecture-document fingerprint in Tools/assistant_knowledge/workflows.json; that same metadata refresh is present in both local full-preflight working trees. Their established configuration-generated Wicked overlays remain unchanged. The ten overlay identities and original gitlinks remain guarded by the preflight adapters. No test assertions or timeouts were weakened.

The separate woodland benchmark is unchanged and is not release or full-game performance acceptance. Hosted checks on the corrected final pushed head and owner merge approval remain required.

Local stock-asset I/O: the existing stock grass packer delayed the local build through small reads/writes on the asset volume. The unchanged Release and Debug packer executables were also run on a temporary system-volume copy of the four inputs (each input hash checked), with the same tile count of 32. All three output hashes in BOTH configurations matched the already completed original generated products. These byte-identical generated files were cached into the owned validation output directories. The slow original jobs had completed before any interruption was necessary; no packer was stopped, and the unfiltered preflights continued. Raw proof: BUILD/ci-path-repro/stock-texture-proof.json. No production source, assertion, deadline, pixel format or input asset was changed for this adaptation.

Local knowledge-test I/O: the first correction Release suite retained three assistant-knowledge timeouts on the asset volume. The unchanged river script with its repository input unchanged passed in 8.27 seconds when only its SQLite scratch database moved to system temporary storage. Both registered river and packaged knowledge tests then passed, 2/2 in 5.86 seconds, with only their generated scratch database/output arguments relocated. The final full local preflight adapter reapplies the three knowledge scratch-path substitutions after configuration/build. It retains the original unfiltered suite, test source, assertions and timeouts; the initial failed receipt remains preserved. This adaptation does not move terrain fixtures or replace the separate hosted-depth failure reproduction.

Correction commands (paths relative to the relevant validation checkout):

```powershell
cmake --build BUILD/native --config Release --target RenegadeProjectDocumentTransactionTests RenegadeTerrainPropEditTests RenegadeTerrainPropSliceProof --parallel 3
ctest --test-dir BUILD/native -C Release -R 'Renegade(ProjectDocumentTransactionTests|TerrainProp(PageTests|SliceProof|AuthoringTests|TerrainTests|EditTests))$' --output-on-failure
powershell -NoProfile -ExecutionPolicy Bypass -File BUILD/ci-path-repro/Build-Preflight-Fast-Knowledge.ps1 -Configuration Release -ArtifactRoot BUILD/ci-path-repro/release-preflight-fast-knowledge
ctest --test-dir BUILD/warp -C Release -R 'Renegade(TerrainProp(ResidencyProbe|SliceProof)|GpuUploadBufferTests|RendererOverlayComposition)' --output-on-failure
```

The relocated offline assistant workflow suite then exposed the stale whole-file architecture evidence fingerprint introduced by the new transaction note. The cited Surface Type content and native source were unchanged. Only that document fingerprint is refreshed, using the existing UTF-8/LF hash format. Workflow verification, stale-source rejection, native acceptance status and guide wording remain unchanged. The four stale-evidence assertion failures are retained in knowledge-workflow-relocated.log; they are not passing evidence. The complete 84-case offline suite and both full preflights passed with the refreshed metadata.

Refreshed standalone offline assistant suite: 84/84 PASS in 29.366 seconds (`BUILD/ci-path-repro/knowledge-workflow-refreshed.log`). The architecture document's reviewed canonical UTF-8/LF fingerprint is `d05bf0c568a48c8af07dfd8d043db72c4cf0cf4f9c63fc22f95620833fb642ec`. Its Surface Type passage is unchanged; the added paragraph concerns only transaction artifacts. The single corresponding fingerprint replacement is included in the final evidence commit. Both full-preflight working trees contain this same metadata change while their C++ sources match `59c4e926b4a34e9d1523474e8e06b440ff691867`.

## Retained evidence

- [Machine-independent result summary and artifact hashes](ci-path-validation.json).
- [Fresh 280px palette capture](ci-path-editor.png), with the private window enlarged after interval sampling so all controls are visible.
- [Fresh separate player frame 60](ci-path-player.png).

The WARP run used the identical corrected C++ snapshot before the isolated source checkout was advanced to the implementation commit; the subsequent documentation fingerprint refresh has no native-renderer code effect. Its original 60-second overlay timeout and all four tests remain enabled.

The new editor's initial interval sample includes cold startup and concurrent validation work. Its intervals are not comparable performance measurements. These deliberately low-poly technical fixtures establish workflow/render continuity only, not detailed-asset quality or full-game woodland headroom.

Additional exact local commands, run from the corresponding validation source checkout:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File BUILD/Build-P2-Preflight-Physical-Local.ps1 -Configuration Debug -ArtifactRoot BUILD/ci-path-debug-preflight
powershell -NoProfile -ExecutionPolicy Bypass -File BUILD/ci-path-repro/negative-control.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File BUILD/ci-path-repro/native-workflow-final.ps1
```
