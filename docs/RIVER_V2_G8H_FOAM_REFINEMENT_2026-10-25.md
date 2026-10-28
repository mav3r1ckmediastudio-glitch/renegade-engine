# River V2 G8H: bounded foam appearance refinement, 2026-10-25

Owner agreed G8G was much closer and requested finishing the pocket-scale and distribution refinements. G8G source, shaders, original artwork and executable are preserved byte-for-byte. This is an optional native WATER proof, not a production Studio/Runtime promotion or a completed release gate.

## Result and scope

Two differently scaled, rotated samples of the SAME original Renegade artwork retain fine froth while reducing single-scale pockets. Both layers use the existing bounded dual-phase actual-velocity coordinates; derivative/temporal mip filtering remains. A broader 14m artwork birth projection with additional filtering feeds the existing source-born scalar tracer. This tracer is born only at actual active sources, transported by the existing bounded RK2/MacCormack path, and controls cluster packing and the fine/coarse mixture. It generates no physical foam mass. Optical density response changes from 1-exp(-1.8*density) to 1-exp(-1.3*density), preserving more water between weak trails.

No per-frame CPU rebuild, new texture allocation, proprietary artwork, mesh overlay, emitter or physical solver change. Two extra fine texture reads per shaded foam pixel; source birth retains two reads. Parameters stay128B and existing6MiB tracer storage is unchanged. Native WATER reflection/refraction/transparency/lighting/normals and existing bounded geometric waves remain.

The measured rapid SOURCE zone still spans the shallow rough bed across the channel. Appearance now varies within that zone; this does not claim a wake-turbulence solver or eliminate the physical source footprint. No mountain-stream, slope or cascade work.

## Exact reproducible commands on DESIGN

I denotes the integration worktree; B denotes the G8H BUILD directory under the spline-water worktree. Python is the installed3.14 executable; cmake is the Visual Studio18 BuildTools bundled executable. Actual absolute argument arrays and paths are retained in B/prepare.py and B/review-exit.json; no machine-specific paths added to repository documents.

- cmake -S I -B I/BUILD/g6
- cmake --build I/BUILD/g6 --config Release --target RenegadeRiverRefinedFoamProof --parallel 2
- python B/stage.py
- Native compiler: RenegadeRiverShaderCompiler B/shader/RiverContactFoamPS.hlsl I/WickedEngine/WickedEngine/shaders B/shader/RiverContactFoamPS.cso ps
- Native compiler: RenegadeRiverShaderCompiler B/shader/RiverFoamArtworkCS.hlsl I/WickedEngine/WickedEngine/shaders B/shader/RiverFoamStrandsCS.cso
- python B/run.py review
- python B/validate.py review
- python B/preserve.py
- python B/audit-all.py
- python B/launch-preview.py

Configure/build PASS0. Initial PS compile caught inherited variable-name collisions small0/small1; renamed new UVs smallArtUV0/1; final PS and CS compile PASS0. One initial evidence-script write failed because new BUILD directory did not exist; directory created and script written before execution. No old files replaced.

## Actual native-geometry and GPU evidence

Full DX12 proof PASS0 in72.45s, CPU/GPU parity error1.33514e-05. Uses actual native spline geometry/triangles/depth/source/obstacles and prior refined native WATER mesh, not an independent spline approximation.

All five bulk-state dumps are SHA-identical to protected G8D baseline: one-step; multi-step; rapid-only-established; rapid-source-off-expired; zero-flow. Scalar detail is bounded, changes over motion, expires with physical mass, and remains zero with zero flow. Actual surface and appearance A/B preserve bulk AND detail history exactly. Native-before/restored RGB pixels identical, no resource errors or shader errors, entity/mesh counts unchanged. Saved native scene reload into fresh live candidate PASS with original artwork resolving.

Small-scene warm wall measurements: previous appearance with detail simulation disabled1.26916/1.26264ms; refined appearance with detail simulation enabled1.28291/1.28109ms. Approximately0.016ms difference is below reliable isolation; this is not GPU timestamps or a whole-game FPS claim. Normal preview uses normal VSync settings. No new build cache.

Owner review amount0.75 entered through native numeric control; default remains0.3. Close/overhead live captures inspected. Six paused captures across both appearances/cameras share EXACT bulk/detail hashes at clock135.548; B history checks pass. Returned new/refined appearance, view4, RUNNING. Current distinct title and desktop shortcut: Renegade River V2 - Refined Foam. candidate-live.json is the process/launch authority. Previous disposable G8G viewer normally closed only after new readiness; accepted editors untouched.

## Preservation and status

B/validation.json records source/executable/shader hashes, exact launch args, parity and live state. B/before.json protects G8G. Both preservation audits PASS, including accepted binaries, owner projects, dirty prior sources and Wicked private diff/HEAD. Existing CMake content remains a hash-identical prefix; only optional H target appended. No commit/push/merge.

Owner visually ACCEPTED this exact candidate on2026-10-25: "i think we can sign off on your refinements visually". Executable SHA256 7aa455432003783ac40e48d8e43af9d1f43a5b01b001d9c9855054bdc14fe87a; current source/shader hashes reverified and recorded in B/owner-visual-signoff.json. The bounded visual refinement pass is accepted; do not silently declare the wider River V2 programme, Studio integration, free-surface hydraulics, particle spray, wake turbulence, mean-plane reflection/collision or deformation motion-vector limitations solved. Next engineering work should be deliberate Studio/Runtime adoption and packaging parity, not more unbounded proof-only visual iterations.
