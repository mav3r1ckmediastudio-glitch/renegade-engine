# G8A first depth-driven rocky riffle — 2026-10-25 DESIGN

Owner update2026-10-25: exact7714741e8baa401c9116bbabe3e78f1fe4529d08f7bd0b037f35750a41f63bdb preview visually accepted ("Looks good"), followed by authorization to integrate. Supersedes historical pending-review wording below. G8B is separately documented in RIVER_V2_G8B_EDITOR_RAPIDS_2026-10-25.md.

## Status

Engineering proof PASS; OWNER VISUAL REVIEW PENDING. Isolated native DX12 viewer, not a new production Studio/Runtime feature. Existing Drawing Candidate and owner river project are unchanged. Mountain streams and sloped waterways remain paused.

Current review viewer PID30044; candidate-live.json authoritative. Desktop shortcut: Renegade River V2 - Rocky Rapids Preview. F cycles native WATER / directed normals / flow plus foam;1-5 views; Space pause; R toggles rapid emission (existing foam decays); K all emission; Z zero flow; X resets history; D orange density diagnostic; amount slider0..4. Escape closes only this viewer.

## Implemented

- New EngineBridge/src/RiverRapidsTransport.h, an isolated fork of the accepted conservative transport oracle. Original production RiverWaterwayTransport/Renderer unchanged.
- Actual transformed native bed/boulder triangles rasterize highest submerged surface beneath each water cell. Missing/NaN/over-budget geometry fails closed. Explicit thin-layer resolution floor0.10m; depth above4m refused. This is measured geometry plus documented regularisation.
- Steady depth-weighted pressure projection conserves discharge in m3/s and enforces closed banks/obstacles. Separate surface advection face flux is Q/faceDepth in m2/s; shallow sections accelerate. This is a prescribed-flat-surface kinematic model, NOT a Saint-Venant/free-surface solution.
- Fourth foam source uses a calibrated near/supercritical Froude gate AND resolved bed disturbance/deceleration. It is an aeration proxy, not a prediction of entrained air volume. A fast uniform bed emits no rapid foam. Buried/above-water geometry does not create this source; relevant submerged geometry can.
- Four-source finite-volume GPU density/moment transport retains finite lifetime and zero-speed behaviour. Existing lit bubble coverage shades only the same WATER surface; no foam planes, collars, rock-face shader or scrolling-noise source.
- BUILD-only native WATER shader staging retains original lighting/reflection/refraction/transparency and native normal maps, with accepted two-phase directional sampling and local source-dependent ripple steepness. No upstream or production shader writes.
- Disposable native scene adds a shallow rocky sill and six instances of owner's rounded big_rock_1b. Source .rasset/owner level unmodified. Native scene carries verified texture data. No Unreal content used.

## Files

New: EngineBridge/src/RiverRapidsTransport.h; EngineBridge/shaders/RiverRapidsTransportCS.hlsl; Tests/RiverRapidsTests.cpp; Tests/RiverRapidsProof.cpp. Existing Tests/CMakeLists.txt only appended two isolated targets. BUILD scripts/assets/evidence are under B. Docs/HANDOFF/feature matrix updated.

## Verification

- powershell.exe -NoProfile -ExecutionPolicy Bypass -File B/build.ps1: configure/build Release targets RenegadeRiverRapidsTests RenegadeRiverRapidsProof; ctest -C Release -R ^RenegadeRiverRapidsTests$ --output-on-failure; shader compiler for staged pixel plus new compute shader. Final CPU1/1PASS1.73s; build/shaders exit0.
- CPU: native triangle depth; uniform-depth agreement with accepted unit-depth velocity; shallow mean2.60666m/s vs pool0.723972m/s; Q2.28488m3/s with maximum cell divergence2.23517e-08; doubled input exactly doubles velocity (error0). Fast flat-bed control has Fr>1 but zero rapid source. Submerged top produces source while buried/above controls do not. Missing/NaN/deep bathymetry rejected; deterministic repeat; positive finite transport; zero speed no emission.
- run-gpu-v2.ps1: actual textured boulder scene GPU PASS0/37.59s,74.9723FPS. run-final-reopen.ps1: separate process native saved-scene reload PASS0/39.00s,74.9738FPS.
- Texture validation caught two missing generated PNG sidecars in an early prototype archive. Exact original PNG bytes copied to OWN prototype folder, SHA manifest proof-texture-closure.json. After proper loading, subsequent native archive embeds those bytes. verify-texture-closure.ps1 PASS0/37.04s,74.8955FPS, no engine resource warnings.
- One initial interactive attempt before texture closure exited0xC0000005 before Ready. Do not claim proven crash causality. Subsequent verified viewer reached Ready, showed no resource warnings and closed normally0; current review is pinned to a verified self-contained native fixture. This is still an isolated prototype, not a release gate closure.
- Final compressed native fixture support built via build-compressed-proof.log, exit0. C:/Python314/python.exe -X utf8 B/verify-review-package.py: temporarily renamed only the three OWN loose PNG copies, ran exact review executable against native scene in a new process, restored copies in finally. PASS0/37.05s; no missing/failed/error resource entries. Final GPU 74.9632FPS, CPU/GPU multi-step max error1.33514e-05, source-off and zero-flow density sums0, shader errors0. Native reflection visible on Y0, material WATER4.
- Native-before/restored images are PIXEL IDENTICAL (RGB difference0) in both original and fresh-process proofs. Source density remains inside native fluid raster; no added effect geometry. Shallow model is built once, never scanned/reprojected each frame.
- Native fixture save now enables archive compression; exact existing proof data NTFS-compressed with SHA256 unchanged (proof-compression.json). No owner assets or accepted packages deleted.
- Current owner scene/registry/metadata/descriptor all SHA256 unchanged, G7D candidate binary unchanged. Source HEAD11bbb90e4214776d5e51c8bc4cba2e7f1ccde296; no commit/push/merge. Preservation audit retained separately.

## Physical/production limits

One static flat two-inlet confluence fixture; depth-averaged steady approximation. No evolving water height, breaking-wave geometry, hydraulic jumps, spray or genuine unsteady wake/mixing solver. Those are subsequent engineering work after this visual gate. Prototype R is an emission diagnostic, not a new editor property. No new profile schema or Lua/UI exposure. Native proof save/reopen is tested; production TestLevel/BuildGame integration was intentionally not changed.

References informing the approximation:
- USGS/Magirl et al., Water velocity in rapids on the Colorado River: https://wa.water.usgs.gov/pubs/ja/Rapids_2009WR007731.pdf
- USBR Water Measurement Manual, Froude number/critical flow: https://www.usbr.gov/tsc/techreferences/mands/wmm/chap02_12.html
- NVIDIA GPU Gems chapter38, pressure projection and its free-surface limits: https://developer.nvidia.com/gpugems/gpugems/part-vi-beyond-triangles/chapter-38-fast-fluid-dynamics-simulation-gpu

B=C:/Users/paulw/source/repos/renegade-spline-water-20261020/BUILD/river-v2-G8A-20261025-rocky-rapids
