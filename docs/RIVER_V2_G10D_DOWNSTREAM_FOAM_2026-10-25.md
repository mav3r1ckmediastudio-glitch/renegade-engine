# G10D downstream foam recovery

Owner likes the G10C smooth cascade geometry, but noticed smaller gaps downstream than G10B at the same 0.3 amount. Owner approved dense connected whitewater at the landing that gradually opens into larger irregular gaps downstream, automatically, with the existing foam amount control.

## Bounded change

New optional Tools/prepare_river_opening_foam_shading.py builds from unchanged G10C shading. Original owned artwork, geometry, native frames/normals, continuous normal-motion coordinates, source/velocity/decay/transport, surface displacement and parameter layout remain unchanged. No new texture, buffer, simulation dispatch or CPU update is added.

Close to the toe the G10C artwork sampling is retained. Over 0.75–6 metres downstream, optical coverage crossfades to the previous accepted world-space two-phase foam sampling, including its original filtering and two-scale normalization. Crossfading evaluated appearance instead of UV coordinates avoids a stretched transition band. Density is not erased to create the gaps. The same native WATER lighting, transparency, reflection/refraction and bubble lighting remain.

This fixture runs along +X; its shape buffer supplies the actual rounded toe X position. The bounded proof therefore measures downstream offset in X. General Studio/Runtime adoption must use the corresponding native arc-length offset for arbitrary bends; this is not claimed as completed general authoring.

New Tools/prepare_river_opening_foam_graphics_proof.py changes only proof title and A/B labels, retaining the G10C native geometry/edit/save/transport tests. B compares the exact previous G10C foam PS with the new opening PS on the same frozen GPU state. Existing 144-byte parameter prefix/layout is unchanged.

The shader art clock closes at 6000 seconds for both continuous artwork coordinates and the world-space 1.5-second two-phase cycle. The normal-motion code is unchanged.

## Preservation and validation

Distinct G10D BUILD artifact directory and proof target. Existing cache reused; previous running executable and G10C shader directory remain immutable. Only two new owned generators, the optional Tests CMake target block and documentation are changed. No production Studio/Runtime adoption, owner project/main/alpha changes, upstream edits, commit/push/merge. Mountain streams remain paused.

Release build and new pixel shader compile PASS. Unchanged vertex/shadow/transport/detail shaders and PNG are copied byte-for-byte from G10C. Consolidated GPU/fresh-reopen/preservation checks PASS. Exact commands, results and hashes will be recorded in the G10D BUILD manifest. Owner visual acceptance remains pending.

Limits from G10C still apply: calibrated surface/entrainment, not a complete 3D fluid solver; one native planar reflection; synchronous structural rebuild; small-fixture wall timing does not establish whole-game FPS.


## Final engineering result

Release configure/build/pixel compilation all exit 0. All 18 river regressions PASS. Full DX12 run and fresh saved-native-scene reopening exit 0, with no shader errors. Native mesh parity/fold/crease, edit/Undo/Redo, bulk/detail history, one/multi-step transport parity, expiry, zero flow and native restoration checks PASS. No test thresholds, CPU oracle or compute shader were changed.

Density/detail readbacks selected by the preservation audit match the G10C full-final4 run byte-for-byte, including one-step and multi-step bulk/detail, established rapids and surface history. G10C sources, executable/textures/shaders, prior Tests CMake prefix, integration HEAD and private Wicked patch PASS preservation. Active VS/shadow/transport/detail bytecode and original PNG are identical to G10C.

Actual frozen A/B and five-view captures were inspected. The rounded landing is retained, and overhead view shows larger irregular downstream openings without a hard cross-river appearance boundary. Original lighting and amount control remain. The current dense look is available on B for comparison, rather than asking the owner to evaluate separate passes.

Exact command arrays, exit codes, source/executable/shader hashes, readback checks and model limitations are in FINAL_RESULT.json and BATCH_COMMANDS.json. This is engineering acceptance of the bounded proof; owner visual acceptance and production integration remain pending.

## Owner visual acceptance

Owner confirmed "yup, happy with that". Current source/executable/shader hashes were reverified against FINAL_RESULT.json before recording acceptance in owner-visual-signoff.json. This accepts the bounded G10D appearance and preserves this reference candidate. Production Studio/Runtime adoption and ordinary node-dragging integration remain separate work; no commit/push/merge was authorized or performed.
