Historical macro/column investigation. Read CLOUD_FORMATION_GATE1_WHOLE_BODIES_2026-10-29.md for current source. Four-build results here precede the latest shader experiments.

# Weather Gate 1: macro volume investigation - 29 October 2026

Status: OPEN. The owner rejected both previous Studio previews. No replacement
Studio preview has been offered from this source. This is an experimental
Cumulus slice, not an approved five-profile implementation or release gate.

Branch: codex/cloud-formations-g1-20261028.
Base: 150eca4b761d3f62f122df3ab5e8536ec084f926.
Accepted river Wicked gitlink and checkout:
ceb816b844b4ab2fa56a159ac60a4f65c931bdfc.
All changes remain uncommitted. No push, merge or PR. No G2-G5 or Snow work.

## Findings and rejected models

The pinned renderer already performs true 3D camera/shadow density sampling.
The native weather value is also a carving threshold: at the unprofiled base
kernel, Remap(body,1-weather,1,0,1)*weather reduces to body+weather-1.
The actual 64-cubed GPU texture has positive base density in 100% of samples
at local weather .8, versus about 90.48% at .4 and 35.25% at .25. Height,
anvil and detail still affect the final density; these base percentages alone
are not a final cloud-opacity prediction. See BUILD/base-support-audit.

Removing the medium anvil dip did not remove the slabs. Native local-weather
and physical body-width controls still produced either overfilled banks or
small fragments. The anvil curve is not established as the main cause.

The continuous 3D threshold model had floating pockets and a visible periodic
noise repeat in its unlit slice. A supported horizontal moisture-column model
reduced disconnected mass but produced terrain-like pointed tops. Both are
rejected as owner previews. Raw ray captures show the same broad smooth forms
before temporal reconstruction, so reconstruction is not the sole defect.

The stock detail texture is inverted cellular noise and has compressed
contrast: weighted RGB P05/P95 are .640196/.867647 in the actual GPU data.
Unconditionally subtracting it can remove cell centres and leave ridges.
Changing polarity/contrast helped edge shape but did not make the column
model acceptable. No claim is made that this is a universal stock-renderer bug.

## Current bounded core-patch decision

The primary Cumulus path now has separate signals:
- Regional weather admission: a nonperiodic world-space potential and percentile.
- Macro body: a pure 3D cellular distance field stored in the previously unused
  alpha channel of the existing detail resource.
- Existing native RGB channels: subordinate 3D billow and erosion scales.
- Optical density and edge/base transitions: independent, measured in metres.
- A late coalescing low volume permits full overcast at Coverage 1.

The new macro channel is generated once with the existing resource, bindings
and mip lifecycle. Native detail RGB data is byte-identical before/after on
both DX12 and Vulkan (32,768 voxels each). The native density function body,
after removing the explicit new Cumulus dispatch, and native SampleAlbedo are
unchanged. Two matched Legacy day/night captures are pixel-identical. These
are scoped preservation checks, not proof of every environmental feature.

Source of truth:
Tools/night_sky/renegadeCloudFormations.hlsli,
cmake/RenegadeNightSkyOverlay.cmake and
cmake/RenegadeCloudQualityOverlay.cmake.
No external shaders, textures or reference-pack assets were imported.
Formation metadata, archive layout, ABI offsets, Undo/Redo and Legacy fallback
retain their existing design. The accepted river fork pointer is unchanged.

Only Cumulus uses this new volume. Towering, Stratocumulus, Overcast and Cirrus
retain earlier experimental profile overrides and are NOT newly accepted.
Once the Cumulus architecture is credible, implement their own vertical extent,
aspect, coalescence and anisotropic filament morphology in the same separated
domains. Do not present the existing five selector labels as five approved looks.

## Reproducible evidence

Run RenegadeNightSkyTests from BUILD/gate1/Tests/Release:
- --formation-proof OUTPUT dx12|vulkan 1920 macro-body-review cloud-reference-quality
  gives 18 captures: 1500/5000 at 9am/3pm/midnight and 0/35/70 degrees,
  two offset/rotated positions, clear/full endpoints, and the owner's thin layer.
- --formation-proof OUTPUT dx12|vulkan 2560 quality-comparison cloud-reference-quality
  gives matched Legacy/Cumulus thin-layer day/night comparisons.
- Add cloud-raw-study to save the pre-reprojection ray texture.
- --density-audit OUTPUT dx12|vulkan 1920 dumps actual native base/detail data.
- --formation-proof OUTPUT dx12|vulkan 1920 density-volume-study cloud-reference-quality
  extracts the actual shared primary density into 128-cubed float32 volumes at
  Coverage 0/.8/1. It regenerates the same native resources, uses density LOD 0,
  and bypasses illumination, ray integration and temporal reconstruction.

The volume audit is a test-only compute shader; Studio/Runtime do not load it.
Runtime audit compilation needs the already installed matching dxcompiler DLL
available on the process PATH (the EngineBridge Release output supplies it).
An initial missing-DLL failure is retained in density-volume-dx12-missing-compiler.log.

Tools/night_sky/analyze_density_volume.py reports six-neighbour topology and
orthogonal slices with physical aspect ratio. Horizontal voxels are 93.75 m;
thin links can be missed. Base connection within 150 m is a diagnostic,
not a fluid simulation or proof of realistic cloud dynamics.

Current evidence:
BUILD/macro-preservation.json,
BUILD/macro-body-review-dx12 and -vulkan,
BUILD/macro-owner-dx12-2560 and -vulkan-2560,
BUILD/macro-volume-dx12 and -vulkan,
BUILD/macro-noise-dx12 and -vulkan.
All outputs are isolated from owner projects and executables.

## Remaining defects and acceptance boundary

The old screen-admission transfer was calibrated for a different model.
It is provisional and must be recalibrated, then checked at held-out positions,
angles and both layer depths. The current thin-layer night 80% view measures
about 56.8% sky opacity coverage. Numeric endpoint checks are insufficient.

Broad rounded bodies replace the previous small fragments in the first current
captures, but surface quality, distant bands, lighting, coverage consistency and
performance still require review. Full-history reference quality is opt-in:
default new-mode rendering still uses native quarter-ray/half-history resolution.
Do not pass a high-quality capture off as the default Studio result.

Current 18-case/4-case studies are not the full 256-case gate matrix.
The current application rebuild results and scoped tests must be recorded
separately from the previously rejected candidate's four-build evidence.
No owner preview should be opened until the slice is materially better and
its exact Studio state can be replayed. Original unsaved owner state remains
unrecoverable from saved files alone; the original mismatch is not fully closed.

## Preservation and rollback

All 45 snapshotted original authored files still match their SHA256 values.
No original worktree was reset, cleaned, stashed, saved or overwritten.
Do not close, save, reload or overwrite an owner's running preview.
A replacement requires a new executable alias, new project GUID in all sidecars,
and a new preview path/flag.

Rejected source variants remain under BUILD, including the 76-file boundary
snapshot and the native/continuous/column references. Use these only within
this owned worktree; never restore the old Wicked pin or copy a whole submodule.
Only selected disposable PNGs from nine explicitly named superseded studies
were pruned after retaining canonical 80/100 day/night images, contact sheets,
logs and metrics. The hash/size manifest is
BUILD/owned-superseded-study-pruning-20261029.json (about 1.63 GB reclaimed).
