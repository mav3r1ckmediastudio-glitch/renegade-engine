Current status (29 October 2026): BOTH prior owner previews were rejected.
The sections below are chronological investigation history, not current approval.
Read CLOUD_FORMATION_GATE1_MACRO_VOLUME_2026-10-29.md for the current experiment,
explicit remaining defects, preservation checks and current-source test scope.

# Weather Gate 1: independent cloud formations
Status: implementation candidate. No owner approval, push, PR or merge.

## Preservation and source boundary
Base: origin/main 150eca4b761d3f62f122df3ab5e8536ec084f926.
Wicked gitlink: ceb816b844b4ab2fa56a159ac60a4f65c931bdfc, accepted river fork.
Branch: codex/cloud-formations-g1-20261028.
The existing Moon, Night Sky, inventory integration and main worktrees remain
untouched. The new worktree ports bounded uncommitted Moon/meteor dependencies;
the four additive CMake conflicts retain BOTH river resources/tests and Moon
sources/assets. No inventory source was transplanted or replaced.
All original working-tree statuses were inventoried before source changes.
The original prototype's tracked and untracked authored sources were snapshotted
outside its checkout with SHA-256 hashes. This is not a backup of unsaved memory.
Owner Studio processes remain open and untouched.

## Diagnosis: established facts versus open questions
The stock renderer already samples 3D shape/detail textures along volume rays,
with shared density for camera and shadow rays. The former V2 patch operated
inside that renderer, but still used a weather-map coverage scalar both to
distribute bodies and to threshold their internal density. Random weather-map
type also selected a vertical envelope. Strong erosion acted on already
thresholded thin bodies. These coupled signals explain why changing distribution
constants simultaneously removed bodies or joined them into a ceiling.

The earlier synthetic fixture is not an exact Studio replay:
- It sets layerSecond.coverageAmount=.16 in its V2 cases; Studio's curated
  primary Coverage control preserves layerSecond, whose native default is zero.
- Many cases use Base 700/Depth 4700 instead of owner Base 1500/Depth 5000.
- Cloud animation continues between sequential cases. Cases do not share a
  frozen world time, wind phase and camera-state manifest.
- Three owner Studio executables from different candidate revisions were running.
- The fixture disables global TAA; Studio's rendering configuration is separate.
- Custom scene weather maps may override the generated default map.
These are proven confounders, not proof of which caused the specific screenshot.
The owner's unsaved camera/weather state cannot be reconstructed from a saved
scene alone. Do not claim that mismatch is completely resolved until a common
scene/camera/state is replayed in the new Studio and capture harness.

## Core-patch decision
A bridge-only change cannot replace the per-sample density model shared by cloud
rendering and cloud shadows. Use the existing bounded CMake shader overlay.
No submodule pointer, raymarch implementation, scattering pass or native archive
version changes. No third-party cloud shaders/textures are imported.

The existing VolumetricCloudLayer.padding0 float becomes a transient formation
ID, initialized to zero in BOTH layers by the overlay. IDs 1..5 explicitly
activate new formations; zero executes the native legacy density path.
CPU/HLSL structure size and all offsets remain unchanged. Every application
and embedded DX12/SPIR-V shader package must be rebuilt consistently.
Version-1 renegade.clouds metadata stores the primary formation ID. Missing,
unknown or invalid schema/type hydrates to Legacy; existing scenes are never
silently reinterpreted. The second layer remains native with its authored fields
preserved. Selecting a profile never changes Coverage, Base, Depth or shadows.

The authored shader has three separate domains:
1. Irregular world-space body centers, sizes and connected lobes, independent
   of the finite weather texture and its random type channel. Coverage admits
   and expands whole footprints; zero clears and one permits connected support.
2. Explicit 3D height/body profiles for Cumulus, Towering Cumulus,
   Stratocumulus, Overcast and anisotropic high Cirrus. Existing 3D noise
   shapes broad billows; Base and Depth remain the authored volume bounds.
3. Subtractive fine 3D erosion restricted to body edges. It cannot create
   material beyond the macro body's support.
The new density path uses one 3D shape lookup and an optional 3D detail lookup.
Macro evaluation adds arithmetic; actual GPU cost still needs measurement.
Cirrus is an incremental anisotropic volumetric approximation, not a claim of
physically resolved ice-crystal filaments.

The editor adds one formation selector in the existing Clouds disclosure.
Legacy is explicit. Formation changes use one command and preserve independent
metadata through Undo/Redo. No new weather gates or snow work.

## Evidence procedure
Native state tests exercise all five selections, independent authored fields,
Undo/Redo, actual WISCENE save/reopen, shared hydration and unknown schemas.
The replay GPU harness uses Base 1500/Depth 5000, layerSecond zero, fixed
camera/FOV, exposure one and animation zero for every comparison. It resets
cloud temporal history after discrete camera/parameter changes.
At 1080p it captures all five formations plus Legacy at 80/100%, day/night;
the Cumulus matrix covers 0/25/50/80/95/100%, daylight/twilight/midnight and
level/up35/up70 views. Offset/rotated views check world-space consistency.
At 1440p it repeats the formation/Legacy 80/100 day/night comparisons.
Each case also saves the cloud-only opacity render target. Coverage metrics
must use this opacity on positive-elevation sky rays, not RGB brightness.
The native scene-inspection option reads saved weather state without writing
the owner's scene. Unsaved owner changes are explicitly excluded.
Serialized CPU+GPU timings are labelled as such; no GPU-only performance claim.

## Remaining approval requirements
Actual DX12/Vulkan build/run results, visual inspection, coverage/component/edge
metrics, moving-cloud ghosting, self/terrain shadows, Moon phase regressions,
Studio/Runtime Debug/Release, packaged shaders and native Studio interaction.
The first code slice is not visual acceptance. Gate 1 remains OPEN.
Rollback: close only the separately named candidate when no longer needed;
original programs and source worktrees continue unchanged.

## First visual rejection and architecture correction
The first independent smooth-macro distribution candidate is REJECTED internally.
Its DX12 1080p cloud-opacity buffer measured 97.58% sky coverage at authored 80%
in the up35 daylight view (96.10% at opacity >=0.5). The inspected images show
a broad ceiling. No owner preview was offered from that shader.
Evidence remains under BUILD/formations-dx12-1080.

This proves that separating a 2D occupancy mask from fine erosion alone is
insufficient: extruding broad occupied regions through much of a 5 km layer
still fills the projected sky. The next slice replaces vertical extrusion
with world-space rounded 3D bodies: irregular cell centers/radii, individual
domed height envelopes, level condensation bases, and separate edge erosion.
Coverage admits/expands whole bodies rather than fragmenting internal density.
The last 6% of the Coverage range adds continuous low support to permit full
overcast at exactly one. Cirrus remains an incremental anisotropic volume.

The initial full native source build produced Studio Release. During the
subsequent density-signature change the shader build correctly failed on a
stale generated call with three rather than four arguments. An explicit overlay
migration now updates that signature on reconfigure; no compiler check was
disabled. This failure is retained in gate1-release-current.log.

## Final measured-field and high-layer correction
The initial .24 body cutoff was below the audited native field minimum (.37696).
The new field normalizes measured P10/P95 values (.60244/.85819) and uses the
macro envelope to shift its 3D isosurface, while preserving coherent cores after
fine erosion. Both API distributions agree within about .0002 at these quantiles.

The first all-profile coverage test then correctly failed Cirrus at about 89%.
Its actual high band was being marched from the empty native low band, clipped
by a low-cloud 70 km distance limit and faded by a fixed-distance horizon model.
High-only Cirrus now intersects its own lower shell, uses a geometric tangent
distance and altitude-normalized artistic horizon fade. Actual atmospheric
scattering remains unchanged. All four final matrices pass the original limits.
See the verification document for results and explicit remaining checks.

## Owner screenshot follow-up: sampling and reconstruction

The owner screenshots expose blur and patterned undersampling; the first candidate is not visually accepted. A second bounded core-patch decision changes sampling only for formation IDs 1–5: retain the native checkerboard ratio but trace at half viewport size with full viewport temporal history, deriving dispatch constants from actual allocations. Legacy retains native resolution. Reallocate/reset history on Legacy/formation switches. Upsampling must derive coordinates from actual history size.

The native distance-scaled 96-step march provides approximately 156 m spacing at near and middle distances, including shallow layers. The new formations require finer integration of their density field. Compare a bounded 48 m target (384 sample cap) and disable nonconservative two-step empty-body skips for these formations. This is a sampling experiment requiring visual and GPU cost evidence, not an accepted quality claim. Shape, coverage, noise normalization, weather and lighting remain fixed. Source of truth: cmake/RenegadeCloudQualityOverlay.cmake.

The currently open first preview contains owner edits and must not be closed, saved, reloaded or overwritten. Any subsequent preview needs a distinct executable and disposable project.

### Condensation boundary decision

The high-sample reference still exposed broad smooth silhouettes. The scalar macro mask blended density across an entire radius, coupling edge softness to body size. The next experiment uses approximate ellipsoid interior distances in metres and a fixed 48 m condensation band. Three-dimensional shape noise displaces that surface inside a bounded 384 m support margin; fine noise erodes the surface in metres. Coverage admits/expands bodies, independently of density. The continuous full-coverage stratum remains volumetric. Formation selection, weather archives, Moon lighting and Legacy stay unchanged.

The expensive full-history allocation is now explicitly opt-in through the local QA argument cloud-reference-quality. The ordinary candidate retains native quarter/half spatial allocation while testing finer integration; the reference measures whether more spatial samples materially help. Both must be compared visually and by GPU timestamps before accepting a default.

### Density and footprint follow-up

The first distance representation inflated sky occupancy because the former scalar body 1.35*(1-r*r)*admission reached its median-noise condensation surface at normalized radius .7688, rather than 1. The conversion now derives each body's isosurface from measured stock-volume median .717 and P10/P95; admission keeps its old footprint law. The thin-layer 80% experiment fell from 91–92% sky occupancy to 75–77%, while the 1500/5000 canonical view measured approximately 83.5%.

A uniform interior then produced flat, hard self-shadow patches. Boundary and internal condensate are now separate signals: the measured normalized 3D volume supplies internal density, retaining the earlier .12 coherent-core floor. Surface displacement and a physical condensation band supply geometry. Full coverage retains a low continuous supporting stratum with heterogeneous density. This still requires owner aesthetic acceptance; no source or numerical check closes Gate 1.

Conservative empty-space steps use the bounded macro interior distance, with a factor-two gradient allowance for Cirrus coordinate stretch and a local distance cap. Second-layer combinations retain one-step traversal. March upper bounds include the maximum lobe height and displacement support; high-only Cirrus keeps its actual lower intersection. Source overlays are idempotent and remain local to the accepted river fork checkout.
