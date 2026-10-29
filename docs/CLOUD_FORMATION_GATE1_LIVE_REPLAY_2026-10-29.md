# Gate 1: complete-body cache and actual Studio replay

Gate 1 remains OPEN. Earlier owner previews remain rejected. This is a Cumulus
architecture slice; the other four profiles are not a completed replacement.
No owner candidate is approved or ready to promote.

Worktree: C:/Users/paulw/source/repos/renegade-cloud-formations-g1-20261028
Branch: codex/cloud-formations-g1-20261028
Root HEAD: 150eca4b761d3f62f122df3ab5e8536ec084f926
Accepted Wicked gitlink and checkout: ceb816b844b4ab2fa56a159ac60a4f65c931bdfc.
Dirty local work only; no commit, push, PR or merge.

## Implemented and measured

Cumulus uses complete connected 3D thermal bodies, not a 2D mask cutting
through unrelated noise. Four immutable morphology fields share one authored
template with an analytic oracle. A 128-cubed RGBA16F atlas caches those fields;
a 64x64 structured buffer caches static site attributes. Fine erosion and
internal water variation remain 3D. Native density fallback and albedo are
preserved. Body bounds enclose geometry; they do not sculpt its surface.

The conservative 3D bounds/candidate masks passed 12 single-body and 36
whole-sky enabled/disabled-skip comparisons across DX12 and Vulkan. Maximum
opacity difference was one byte; whole-sky mean differences stayed below
0.000087 byte. The 320x180 streamed PGM alpha grid exactly matched PNG alpha
quantization in all 12 single-body views. Evidence:
BUILD/body-bounds-single-parity.json and body-bounds-whole-parity.json.

Measured 192 raw body-rank coverage views: 48 per layer/API. Fit ONLY DX12
reference views, then capture held-out poses and Vulkan separately.
BUILD/body-rank-raw-{deep,thin}-{dx12,vulkan}/ contains metrics and transfer.
The new transfer is whole-body rank probability; rejected planar-site tables
and Cumulus mask functions were removed. Negative transient proof values
retain raw admission. Morphology never reads Coverage or the camera.

The fitted default/reference review contains 176 views across both APIs,
including two held-out positions, both layer dimensions and above-layer views.
BUILD/body-rank-fitted-review-summary.json records ranges and GPU timestamps.
Unresolved: steep 80% held-out views can still approach full coverage; broad
undersides look repetitive and soft. Default thin-layer DX12 views cost about
15-16 ms GPU frame before Studio probes were launched. Reference-quality
results are not default Studio performance. Later replay timings overlap live
Studio processes and MUST NOT be reported as isolated performance.

## Studio versus synthetic evidence

New project: BUILD/cloud-body-studio-preview/CloudBodyStudioPreview.renegade
GUID: f39f4b51-1a9e-4ebd-820b-29d5f1409eb1.
The explicit cloud-body-preview + cloud-studio-replay-audit flags capture ONLY
this GUID. Audit mode freezes cloud animation in this private scene without
saving its project. Capture writes a native scene archive, actual nonserialized
camera, render settings, cloud-only opacity and rendered image. Existing
owner projects/windows receive no input or saves.

The first clone was rejected by Studio because its registry was not canonical.
Its CRLF bytes and stale asset hash were repaired in this private copy ONLY;
native validation was not weakened. Failed bytes and repair hashes are retained
in BUILD/body-qa-registry-canonical-repair.json and failed-qa-copy-registry-crlf.bin.

Studio audit 2 crashed in nvgpucomp64.dll (exception 0xc0000005). Audit 3 with
debugdevice logged a compute-PSO E_OUTOFMEMORY, then completed project loading
and successfully captured. This startup shader-creation defect remains open;
63 passing unit/source tests do not resolve it. Live GPU memory later showed
about 2.3 GiB used of 12 GiB, so VRAM exhaustion is not established.

BUILD/body-studio-replay/ is an actual DX12 Studio capture:
render 1920x1009; opacity 960x504; yaw about 45 degrees, pitch about 7.6 degrees;
Coverage .8, Base 2280, Depth 3190, secondary coverage 0, animation 0, FOV 70.
It differs materially from assumed 1920x1080, yaw 0, pitch 35/70 proofs.
The exact archived scene and captured camera replayed on both APIs:
BUILD/body-studio-matched-{dx12,vulkan}/.
Whole-image occupied-fraction difference was below .002, mean native-opacity
byte difference .50/.55 of 255. Edge maximum differences reached 92/93:
this is close geometric correspondence, not pixel identity or complete RGB
parity. Studio grid, temporal phase and post effects need separate treatment.
Evidence: BUILD/body-live-studio-replay-parity.json.
This closes a controlled comparison for the NEW build, not every cause behind
the original owner's screenshots.

## Build, preservation and next bounded experiment

Studio/Runtime Release and Debug built with embedded HLSL6/SPIR-V. Debug's
first final test link ran out of C: space; deleting only owned disposable .ilk
caches and retrying passed. Both configurations passed 63 focused/cloud/night
and source-contract tests. Logs: BUILD/body-replay-apps-*-build*.log and
body-replay-{Release,Debug}-tests.log. These precede the next shader split below.

BUILD/macro-preservation.json reverified all 45 original authored source hashes,
root/fork pins and unchanged native density/albedo function bodies (apart from
the explicit Cumulus dispatch). Its historical RGB/detail capture comparisons
are marked by their original paths; they are not fresh images of the final split.

Next bounded experiment: remove the analytic oracle from production ray
kernels at compile time, while packaging explicit analytic reference variants
for camera/capture/shadow proofs on BOTH APIs. Keeping the oracle in a uniform
runtime branch needlessly increases compiled shader complexity. This is a
performance/compiler-pressure hypothesis, not yet a demonstrated root cause.
RenegadeCloudReferenceOverlay.cmake also names failed embedded shader creation
and offers optional cloud-shader-audit CS begin/end/hash tracing. No fallback
may hide a failed PSO. Rebuild, compare cached image parity, validate reference
permutation loading, reproduce cold Studio startup, and rerun final required
checks before offering a separately named owner preview.

Do not start Weather Gates 2-5 or Snow V2. Do not touch existing Studio windows
or their projects. No push, merge or PR without owner approval.
