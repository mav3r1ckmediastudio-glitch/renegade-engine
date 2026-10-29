# Weather Gate 1: complete 3D body slice - 29 October 2026

Status: implementation study. Gate 1 remains OPEN. Both prior Studio previews
were rejected by the owner. No new Studio preview has been offered.

The isolated branch remains codex/cloud-formations-g1-20261028 at base
150eca4b761d3f62f122df3ab5e8536ec084f926, with accepted river Wicked gitlink
ceb816b844b4ab2fa56a159ac60a4f65c931bdfc. No commit, push, PR or merge.

## Rejected distribution model

The 8 km regional field had excessive view-to-view occupancy variance.
At raw admission .7, six canonical reference views ranged from about 43% to
99% cloud cover. A locally stratified 2 km footprint reduced that variance:
at raw .4, the canonical mean was about 80.5%, range 59.2%-92.5%; the thin-layer
mean was about 73.3%, range 54.7%-85.5%. Both APIs closely agreed.

However, the stratified version visibly cuts cylindrical sides into the cloud
volume. Independent 2D support clips unrelated 3D density. This is a geometric
failure, irrespective of the better coverage statistics. Neither its raw nor
fitted transfer version is suitable for an owner preview.
Rejected source: BUILD/rejected-stratified-column-reference.hlsli.
Captures and all metrics remain in the stratified-* folders, with contact
sheets and retained representative full-size captures.

## Current implementation slice

First validate a complete finite 3D cloud without a spatial weather mask.
The analytic template combines connected thermal-shaped volumes, subordinate
3D cellular billows/erosion, and independent internal condensate variation.
It is an authored shape approximation, not a physical convection simulation.
Density fills the interior and has a gradual transition in metres.

The explicit single-cloud-body-study proof uses transient padding1=2.8 only
inside the test process, then renders the same body from below, side and above,
at 9am and midnight. This sentinel is not serialized or exposed in Studio.
Normal runtime hydration still initializes the calibration value to .8.
The six-view proof runs on DX12 and Vulkan; captures are in BUILD/single-body-*.
Its .8 label identifies the fixture request, not 80% occupancy of a whole sky.

Next, place this same body definition as whole formations at independently
jittered sites. Balance admission ranks locally, vary stable body height/width
independently of coverage, and avoid cutting body interiors with a 2D mask.
A narrow activation transition may grow a newly admitted body. Established
bodies retain their morphology. Keep a late volumetric overcast stratum at 1.

The first placement implementation may evaluate the analytic body directly.
If profiling warrants a cache, generate a dedicated 3D formation volume and
sample it for camera and shadow rays; do not sacrifice native Legacy noise
or replace the river fork. Record any new resource/binding core patch before
implementation. No new resource/binding or cloud-atlas cache exists yet.

Only Cumulus is being evaluated. The other four labels remain unapproved
prototypes. They require their own formation morphology before Gate 1 can close.

## Evidence scope and preservation

The macro-volume stage before stratified support passed four application builds,
seven focused tests in both configurations, 58 source contracts, both Studio
constructor startup smokes and four rendered Runtime starts (Debug/Release,
DX12/Vulkan) in a disposable GUID-remapped project. These results PRECEDE
the stratified and single-body shader changes and are not current-source
release acceptance. Repeat required builds/tests before a replacement preview.

All original 45 authored-source hashes and the accepted fork pin were verified.
Matched Legacy captures and native detail RGB were identical before/after the
macro-channel change. Repeat preservation checks after further implementation.
No original worktree or owner preview was saved, closed, overwritten or reset.
No Gates 2-5 or Snow work.

Capture CSV now includes Base, Depth, transient calibration, lunar mode/phase,
reference-quality flag and animation multiplier. Reference quality remains
opt-in and differs from the default quarter-ray/half-history allocation.
Direct GPU density extraction remains a diagnostic, not aesthetic approval.
