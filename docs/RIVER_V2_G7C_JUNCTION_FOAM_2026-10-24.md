## 2026-10-24 G7C lit foam and amount slider OWNER VISUALLY ACCEPTED

Owner exercised the live candidate slider and confirmed "i like it mate". Record bounded visual acceptance of the G7C lit foam appearance and0..4 live amount control in proof-bin-final-v5 (EXE b9e1ecdf1ccdfbab4a419afba95a3a33e37beff2efc1c8a53761957826e289a6). This supersedes earlier OPEN/owner-tuning wording for those two visual features. User chosen current live value is left untouched; restart default remains1. This is not acceptance of future creator topology/snapping/persistence or production Studio/Runtime network integration, general hydraulics, atmospheric startup or release certification. Those require their own bounded implementation/evidence. No commit/push/merge authorized or performed. Mountain streams paused. Full exact commands/results and preservation audit follow in the G7C report/evidence root.

---

## 2026-10-24 G7C live foam amount slider added — bounded proof only

Owner says revised foam is "looking pretty good" and requests a slider to increase/decrease it. Added native Wicked GUI "Foam amount (x)" slider0..4 (default1, step.01), two-decimal number input and Default(1x) button in the same isolated viewer. Only Tests/RiverJunctionFoamGraphicsProof.cpp changes beyond the prior G7C correction; shaders, transport CPU/CS and accepted production binaries remain untouched. Control scales existing render density/coverage, not velocity/emission/lifetime;0 instantly hides foam,1 keeps current response, higher values increase coverage. It retains simulation history, works without reset, retains the value across F mode changes, and is disabled outside lit-foam mode. Numeric callback bounds finite input to0..4; nonfinite returns1. Settings are session-only and restart at1. No authored-state/Studio/Runtime integration is claimed.

Proof now forwards owned foreground WM_INPUT and WM_CHAR to native Wicked input/text handling, with number editing protected from view hotkeys. GUI keeps real dt while native Scene::Update still receives dt*GameSpeed(0) when paused; foam time/dispatch stays independently paused. This fixes frozen GUI interaction without advancing frozen water/foam. Native GUI owns drag/default interactions; no upstream input/GUI source edits.

Exact commands/evidence in original BUILD/river-v2-G7C-20261024-foam: powershell.exe -NoProfile -ExecutionPolicy Bypass -File build-amount-slider.ps1 (VS18 Release target build exit0); stage-amount-slider.ps1 (fresh native archive load/full actual GPU suite EXIT0,shaderErrors0,74.9835FPS at1264x761 capped); C:/Python314/python.exe -X utf8 check-amount-slider.py PASS; check-final-pixels.py amount-slider-proof-v5 PASS. The scripts contain absolute source/build/output paths and are run by full evidence-root paths. Existing87 CPU/bridge tests last passed35.14s; no CPU/CS change and no redundant CTest rerun for the GUI-only addition.

New actual GPU coverage checks at identical frozen simulation clock: amount0 equals directed-water reference RGB max0; changing .5->4 increases whole-view delta from.0846907 to10.3490 MAE; restoring1 equals its pre-change image max0; history before/after byte-identical; negative/above-max values clamp. Native WATER restore/sky/exposed-rock patch max0, existing branch transfer/expiry/parity remain PASS. Pixel shader SHA equals v4. An initial across-PROCESS exact-default-image check failed(max96); unchanged native flow also differed max96 due unfixed live warm-up time before freezing. That is not an identical-clock comparison. Retained the failed oracle and its metric; corrected validation uses exact within-process restoration and identical shader, and reports the cross-process native-water difference instead of claiming exact cross-process pixels. No water shader was changed to make that check pass.

Desktop shortcut now uses proof-bin-final-v5, actual .lnk reopened the viewer at west rock with native TAA, mode2/running/source7/speed1/default1. Owner was already actively dragging the real slider (logged many increasing/decreasing values from0 to4) before automated mouse tests; automation did not take over, reset their chosen value, pause, close or steal focus. GPU P capture confirms readable slider, number and Default button. Prepared click-owned-amount.py was NOT executed. Manual default-button click/paused drag not separately asserted; underlying shared setter and frozen-state rendering/history tests pass. Current live process/session in amount-slider-evidence.json. No visual/creator integration gate inferred from positive WIP feedback.

Final EXE SHA256 b9e1ecdf1ccdfbab4a419afba95a3a33e37beff2efc1c8a53761957826e289a6; unchanged pixel shader df5ef04473ee80ea9e162ca2a20ace569351b0cfbe6dec449af42ccf883ae015. Full correction history/candidates retained. Accepted Studio/Runtime, dirty Wicked source/pin and owner projects preserved. No commit/push/merge; mountain streams paused. Next is owner tuning/review before governed creator connection/persistence/network integration.

---

## 2026-10-24 G7C owner found invisible foam; isolated shading correction — visual gate OPEN

Owner reported no visible foam despite liking the WATER. This OVERRIDES the earlier v17 visual-success interpretation. Original live process was in flow-only mode, but switching to foam did NOT resolve the complaint. Do not blame the mode alone or call the prior foam visually accepted.

Actual live GPU readback had about450 cell-density sum and92 downstream; orange density diagnostic proved the data reached visible WATER. Temporary grayscale coverage diagnostic rendered water black and a broad white sheet; its old HUD was misleading. Owner screenshot and feedback "far too large" were about that diagnostic. It is NOT the final material, and neither diagnostic constitutes acceptance.

New G7C-only RiverJunctionFoamShadingBlock.hlsli and prepare_river_junction_foam_shading.py stage native WATER unchanged, then shade the transported bubble fraction using native tiled lights/shadows with a rough opaque dielectric surface. The old ocean-style albedo/refraction edit inherited the water lighting/reflection response and was insufficient in this fixture. New native light evaluation changes only covered water pixels; no emissive white, additive halo, mesh overlay or independent foam emission. Cover response now uses1-exp(-density*2) and a higher patch threshold instead of broad low-density coverage. Existing G5 block/binary, accepted G6 Studio/Runtime, native source/pin and owner projects remain unchanged.

Tests/RiverJunctionFoamGraphicsProof.cpp adds exact live P density readback, explicitly labelled D orange density diagnostic, T TAA toggle, expired-foam/flow equality captures, and starts the live viewer at west contact close-up. Default mode remains2 (lit foam), source7, speed1; F still cycles native/flow/foam. No production integration or authored-state/schema change. Shared transport CPU/CS unchanged.

Exact evidence under BUILD/river-v2-G7C-20261024-foam: build-lit-final.ps1 (VS18 Release target build exit0), compile-lit.ps1 (native SHA guarded staging;125960-byte pixel shader exit0), run-ctest.ps1 (87/87 PASS35.14s), run-lit-v2.ps1 (actual GPU proof exit0,shaderErrors0,74.9662FPS at1264x761 capped). GPU/CPU one-step6.59376e-7 and33-step2.28882e-5; branch transport/expiry/zero unchanged. check-final-pixels.py captures-lit-v2 PASS: native restore/sky/exposed-rock patch max0; west foam MAE.555608 max175, east.481447 max199, motion2.867732 max193. check-lit-visibility.py PASS:8681 west pixels brighten by>20 RGB minima with near-neutral colour; expired-foam vs expired-flow max0. Numerical and threshold passes do not establish aesthetics. Final package/fresh/live results are recorded below.

Final candidate target proof-bin-final-v4; previous binaries/captures retained. Existing Desktop "Renegade River V2 G7C - Tributary Foam" now points to it. Atmospheric LUT issue remains unresolved; controlled native gradient sky remains intentional. CPU numerical diffusion and sparse test fixture remain limits. G7B/G7C owner gates OPEN; no commit/push/merge; mountain streams paused. Next is owner review of properly lit smaller foam, before creator connection/persistence/Studio integration.

Final correction verification: stage-lit-final.ps1 fresh process native archive reload EXIT0,shaderErrors0,74.9811FPS. Both fresh-lit-final-v4 pixel suites PASS: native restore/sky/exposed-rock patch max0, expired-foam vs flow max0,8673 distinct near-neutral bright west pixels. Final package EXE SHA256 db4b0db0e813fe79b869001a3edbcd9a4943c27063085d93d545a552121737f8; lit pixel SHA256 df5ef04473ee80ea9e162ca2a20ace569351b0cfbe6dec449af42ccf883ae015. Current desktop shortcut uses proof-bin-final-v4; launched through the actual .lnk, live session interactive-20261024-123015-099 PID26832 READY, mode2/running/source7/speed1 at west contact with native TAA enabled. Live P GPU capture inspected: normal WATER plus sparse broken white trails, no diagnostic black/orange. lit-completion-evidence.json and lit-final-audit.json hold exact current hashes/results. Owner visual acceptance remains OPEN.

---

# River V2 G7C — isolated tributary foam transport
2026-10-24. Implemented and self-verified on DESIGN; owner visual gate OPEN.
Isolated integration worktree: renegade-river-v2-integration-20261023,
feature/river-v2-g6-20261023, HEAD11bbb90e4214776d5e51c8bc4cba2e7f1ccde296
plus preserved uncommitted changes. No commit/push/merge or production integration.
Evidence root in original river: BUILD/river-v2-G7C-20261024-foam.

## Outcome and boundary
Both tributaries feed one shared conservative foam grid across the G7B union of actual
Wicked spline ribbons. Actual native triangle contacts generate foam; it travels around
solid footprints, reaches the common outlet, decays and expires. Accepted G5 native
WATER foam shading is reused unchanged. No foam overlay/collar/rock-face shader,
Unreal content, replacement water renderer, authoring graph serialization or Studio/
Runtime deployment. G6 remains owner accepted; G7B/G7C owner visual gates remain OPEN.
Mountain streams, waterfalls, rapids, lakes/ocean and general hydrodynamics stay paused.

## Exact source changes
New files:
- Tests/RiverJunctionFoamTransport.h
- Tests/RiverJunctionFoamTransportTests.cpp
- Tests/RiverJunctionFoamGraphicsProof.cpp
- EngineBridge/shaders/RiverJunctionFoamTransportCS.hlsl
- Tools/prepare_river_junction_foam_proof.py
- docs/RIVER_V2_G7C_JUNCTION_FOAM_2026-10-24.md

Existing edits: Tests/CMakeLists.txt (two targets, one CTest and build-only staging);
HANDOFF.md; docs/ARCHITECTURE.md; append-only docs/FEATURE_MATRIX.csv row.
Original river HANDOFF receives navigation only. No Wicked source/pin, production
bridge renderer, Studio, Runtime, accepted binary or owner-project edits.

## Derived state and solver
The accepted G4 actual-triangle classifier is copied only into BUILD with a namespace
change and templated read-only query adapter; its algorithm/source remain unchanged.
Classifier provenance records the original SHA256. One640x320 float32 domain uses
the G7B exact native-footprint union and concatenated actual native ribbon rows.
The saved native bed has28000 triangles; each of three licensed native rock instances
has2506 triangles. World transforms and LOD0 triangle ranges are used.

A bounded pressure projection constructs anti-symmetric face fluxes, with no flux into
banks/solid rock and open native inlet/outlet caps. Unit-depth kinematic inflow from
both branches is balanced at the outlet. Every port must remain represented; substantial
disconnected water is refused. Two tiny isolated bank-raster slivers are stagnant, never
bridged. This is NOT a varying-depth/discharge, turbulence, erosion or flood solver.

Upwind finite-volume transport conserves its density integral apart from known inlet/
outlet, source, exponential decay and explicit tiny-density/mean-age expiry terms.
GPU steps subdivide to positivity CFL<=0.75 (CPU refuses >0.8).
Float4 history = density, age-mass, material-X-mass, material-Z-mass.
Contact impact uses incident native reach velocity before obstacle deflection; transport
uses projected no-penetration flow. West/east/outlet sources are independently selectable
for verification. Six-second decay, mean-age ceiling18s, cutoff1e-7; no density clipping
that would silently destroy the mass budget. Density is a normalized concentration,
not a physical kilogram quantity or bounded coverage fraction.

GPU ABI: explicit48-byte push block (uint4 resolution/mask, float4 bounds, float4
step/time/lifetime/speed). Shader layout and C++ offsets/size are asserted. Pixel ABI:
unchanged80-byte G5 block and material magic0x52464c35; current state index and simulation
clock are transient userdata. Normal motion uses projected signed world-XZ direction/
speed with bounded two-phase sampling. Native material optics are restored on disable.
All transient resources and visibility are rebuilt on native archive adoption.

## Commands and authoritative results
Run scripts with PowerShell -NoProfile -ExecutionPolicy Bypass -File <script> from the
evidence root. Full exact local commands are retained inside each script.
CMake/CTest are VS18 BuildTools' bundled executables.

build-v15.ps1:
- cmake -S . -B BUILD/g6
- cmake --build BUILD/g6 --config Release --target
  RenegadeRiverJunctionFoamTransportTests RenegadeRiverJunctionFoamGraphicsProof --parallel2
- RenegadeRiverShaderCompiler.exe <new CS> <actual pinned Wicked shaders> <owned output>
Configure/build exit0; compute compiler exit0,7036-byte CS.
Final executable hashes and exact final staging are in final-audit.json.

run-ctest.ps1: selected river/bridge/import/snapshot/Runtime-process/source contracts,
87/87 PASS,39.41s. The CPU test uses actual Wicked spline samples including reversed
west authoring, sliced solid boxes, global conservation/decay/outflow budget, zero-flow
decay, each branch reaching outlet, nonnegative/finite/no-solid density, exact expiry
and unsafe-step rejection. Final CPU results:11423 fluid cells; inflow/outflow8.24617;
max divergence9.38307e-8; projection residual9.55068e-9; max speed2.53208m/s;
step0.0389546s. West/east peak downstream integrals0.172569/0.172096; final both0.
Original complete CTest output remains in BUILD/g6/Testing/Temporary/LastTest.log.

run-v15.ps1: actual DX12 standalone proof exit0; shaderErrors0;74.980FPS at1264x761,
display capped. Domain45744 fluid cells,301 rock-contact,165 solid,2 stagnant slivers;
max flux divergence3.39613e-7; max speed2.97668m/s. West/east source cell-rate sums
59.2135/40.6857. One-step CPU/GPU maximum component error6.59376e-7;
33-step error2.28882e-5 (all four transported quantities compared).
At20s after8s emission, west-only density sum20.8563,11.9069 downstream of x=5;
east-only14.8538,12.2451 downstream. Both reach exact0 after further32s source-off.
Established density sum450.094,91.2362 downstream, max concentration8.77209.
Zero-speed empty history emits0. No foam geometry is added.

run-fresh-final.ps1: separate process loads the saved native-junction.wiscene with no
external weather-reference scene; asserts WATER and no transient custom binding,
rebuilds visibility/domain/resources, repeats the full proof and exits0.
Fresh shaderErrors0 and74.9652FPS. This is a native archive/proof lifecycle check,
not a new Studio project/export certification.

check-final-pixels.py captures-v15 and fresh-final both PASS:
- Native before/restored RGB MAE0/max0.
- Same-time whole eye-view foam contribution MAE0.106166/max13.
- West/east contact-view contribution MAE1.234284/max36 and0.328165/max48.
- Sampled exposed-rock patch and sky unchanged: MAE0/max0.
- Actual3-second close GPU animation difference MAE1.852986/max150.
- Five eye-height stages have nonblack blue sky, including pre/post parity and fresh
  reopen; numerical assertions explicitly reject black-sky captures.
Readback/statistics establish transfer; pixel differences alone do not prove realism.
Full-size contact/eye/join/overhead images and30 actual GPU frames are retained;
motion GIF and contact sheet derive only from those captured frames.

## Failures retained and corrections
All failed scripts/logs/captures are retained under earlier v1-v14 names.
An initial native raster rejected disconnected slivers; component/port validation now
handles tiny stagnant pockets and refuses meaningful separation.
A64-byte compute block did not read its fourth float4 correctly on this device.
A GPU sentinel exposed that layout failure. Compact48-byte layout now passes explicit
CPU/GPU parity; no native graphics-device patch was made or inferred necessary.
Frozen startup originally produced black output; native rendering is warmed first.

G7B saved three original spline ribbons that restore visible during archive adoption.
They overlapped the union, creating large triangular surface patches. The G7C proof
explicitly hides those originals before/after native updates; the single union remains.
Do not assume transient visibility survives WISCENE. A future governed network renderer
must own and reversibly restore it at its load/update boundary.

Cached G7B weather was not serialized as an Environment component. The disposable
fixture now carries native Weather state. Earlier overhead-first frozen harness runs
also produced stale black sky despite valid CPU sky/plane settings. Short eye-height
runs stayed correct across reset, source-off/expiry, readback and camera changes.
Final sequence warms native WATER at eye height and records native eye reference
before the overhead/CPU comparison, plus an eye capture after parity. Full final and
independent fresh-process sky assertions pass. The exact lower-level cold/frozen
harness cause is not claimed resolved in Wicked; no core renderer was changed.
That sequence did NOT cure all later cold runs: captures-v16 and fresh-final-v2
failed the explicit sky-pixel assertions despite successful numerical checks.
Those failed runs are not promoted as visual passes. Final controlled fixture lighting
therefore uses Wicked's native horizon/zenith sky gradient, retaining native WATER,
planar scene reflections, refraction, lights and all material sampling. No atmospheric
shader, native source or accepted Studio environment was altered. Atmospheric LUT
startup in this disposable fixture remains an unresolved separate issue. Final
gradient-lit runs, independent native reopen and live paused/running captures must
all pass; see final appended verification below.

Initial launch wrapper put log files into the proof's output folder, correctly causing
its nonempty-output guard to refuse startup. Launcher now puts captures in a separate
empty child; previous launch folder retained. No engine crash is inferred from that.

## Candidate and remaining gates
stage-final-v3.ps1 stages a separate proof-bin-final-v3, unchanged accepted G5 pixel bytecode,
new compute, native embedded fixture and its own dxcompiler.dll. Content is a read-only
usage junction to the accepted complete G6 package Content (no copying over it).
create-shortcut.ps1 creates Renegade River V2 G7C - Tributary Foam on DESIGN's desktop.
launch-foam.ps1 allocates a unique session/captures path every launch.

F native/flow/foam;1 junction eye,2 overhead,3 west contact,4 east contact,5 join;
Space pause; K emission; Z zero flow; X history reset; Escape close.
This is a standalone connected-water visual proof, not an editable Studio network tool.
Creator endpoint snapping, split/connect commands, graph metadata with Undo/Redo,
arbitrary multi-junction handling and shared Studio/Runtime controller remain future
gates after owner visual review. The kinematic projection and first-order transport
are bounded prototypes; no general hydraulic or uncapped many-network performance claim.

C: initially had40.7MB free. Reversible NTFS compression of only owned generated G7B
evidence and g6 .lib/.exe artifacts recovered about2.5GB without deleting file data;
exact scripts/byte accounting retained. No user pack/project was moved or deleted.
Final preservation evidence is final-audit.json and diff-check.log, including source
baselines, original dirty patch, accepted G6 binary hashes, unchanged HEADs and the
candidate/source-build executable identity. No release/owner acceptance is invented.


## Final controlled-lighting verification
The native sky gradient is an explicit disposable-fixture choice, not a repair claim
for Wicked atmospheric rendering. Native horizon linear RGB(.32,.44,.58), zenith
(.055,.12,.22), ambient(.3,.35,.4), same directional light and WATER materials.
Build-v17 changes only the owned graphics proof; CPU solver/shader/test source remains
exactly the final87-test version. Run-v17 is the final complete numerical/native render
proof; fresh-final-v3 independently reopens its saved native fixture without an external
weather reference. Exact final logs/pixel JSON and preservation hashes are authoritative.

The live viewer records its own native startup image and supports P for independent
GPU 3D/HUD captures, avoiding overlap from other applications on the owner's desktop.
Owner coin-pusher processes and windows were not closed or modified. Native/flow/foam,
pause, view, source and zero-speed controls are checked using messages only to the exact
owned executable path/PID/window; no generic process termination or global keystrokes.
The desktop shortcut delegates to launch-foam.ps1, which now uses proof-bin-final-v3
and unique session/captures children. Previous candidates/logs remain intact.


Final controlled native-gradient results: build-v17 exit0; run-v17 exit0;
fresh-final-v3 exit0; both final pixel scripts PASS including explicit nonblack sky.
Exact native restore remains MAE0/max0, sampled exposed-rock/sky differences0.
Final contact-view contribution west1.700684/max66, east0.552099/max85;
motion MAE2.250370/max153. Fresh results west1.700933/max66, east0.551700/max85;
motion2.253459/max153. Source/flow/expiry statistics match the final solver above.
Final executable SHA256 7b0b3179cefa965a47ab4640f5aac5f39f05e16b22a2907183f78ac6ae372f36.
Live exact-executable/PID control sequence verified modes0/1/2, pause/view3,
source-off, zero-speed and restored running/source-on; seven own GPU/HUD captures
all pass blue-sky checks. See live-control-checks.json and unique session viewer.log.
Screen-grab overlap was not used as river evidence; own GPU captures isolate the view.

Preservation audit PASS:1593 integration baseline source files; only the four listed
existing files changed plus the six intended new files. Original river1562 source files
unchanged except HANDOFF navigation; accepted combined1468 unchanged; dirty native
patch unchanged; all HEADs unchanged; accepted G6 Studio/Runtime hashes unchanged.
An owned compiler-generated root log.txt was moved, intact, into the evidence folder
as compiler-root-log.txt rather than treated as source. git diff --check exit0.
Candidate/source-build binary hashes match; accepted G5 pixel shader and new compute
hashes are recorded in final-audit.json. Human/owner visual acceptance remains OPEN.


Final proof statistics under controlled native lighting: run-v17 74.972FPS,
fresh-final-v3 75.000FPS (both capped, not an uncapped performance claim).
Owned final interactive process4640 graceful Escape exit0 is recorded in
interactive-exit.json. The existing new review shortcut was reopened through Windows
after that check; latest candidate-launch.json identifies the new review process.
The exact accepted G6 and other owner applications were never terminated.

Before future Studio/Runtime network integration, recheck the still-open atmospheric
fixture behavior in the accepted native Studio environment. Controlled native-gradient
foam acceptance does not certify atmospheric startup or replace that integration gate.


Correction reproduction commands (run on DESIGN; scripts retain exact absolute source/build/output paths):

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File BUILD/river-v2-G7C-20261024-foam/build-lit-final.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File BUILD/river-v2-G7C-20261024-foam/compile-lit.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File BUILD/river-v2-G7C-20261024-foam/run-ctest.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File BUILD/river-v2-G7C-20261024-foam/run-lit-v2.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File BUILD/river-v2-G7C-20261024-foam/stage-lit-final.ps1
C:/Python314/python.exe -X utf8 BUILD/river-v2-G7C-20261024-foam/check-final-pixels.py fresh-lit-final-v4
C:/Python314/python.exe -X utf8 BUILD/river-v2-G7C-20261024-foam/check-lit-visibility.py fresh-lit-final-v4
C:/Python314/python.exe -X utf8 BUILD/river-v2-G7C-20261024-foam/lit-final-audit.py
```

Relative command paths above are from the original river repository root; build scripts explicitly change to the isolated integration root. Earlier diagnostic compile using relative compiler input failed exit1; corrected absolute-path compile passed. That intermediate launch reused the previous diagnostic binary and was not promoted. One diagnostic process had already been closed when the safe exact-PID Escape helper ran; its process identity assertion refused the absent PID, and no unrelated process was touched. Diagnostics/captures/prior packages remain retained. Pillow getdata emitted a future deprecation warning in the metric helper; checks passed.
