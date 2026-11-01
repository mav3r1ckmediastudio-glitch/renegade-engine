Superseded investigation history: see CLOUD_FORMATION_GATE1_MACRO_VOLUME_2026-10-29.md for current source and results. Gate 1 remains OPEN.

# Gate 1 continuous-density investigation — 29 October 2026

The owner rejected the boundary/density candidate as worse. It is frozen under BUILD/rejected-boundary-source (76-file SHA256 snapshot), with its separate executable/project retained. Gate 1 remains OPEN.

## Architecture decision before implementation

Replace explicit ellipsoid/interior-distance bodies with a continuous weather potential, independent vertical formation profiles and a genuine sampled 3D condensate/erosion field. Coverage moves a calibrated weather-field percentile; it must not multiply optical density. Type selects a height/morphology profile; Base and Depth retain their separate authored controls. Keep existing metadata schema, Legacy fallback, Undo/Redo and save/reload.

The rejected model combined a narrow 48 m boundary with values up to .35 against native extinction coefficients .071–.1 per metre. A 48 m sample can therefore have optical depth up to 1.68, consuming most transmittance in one step. This makes integration and sparse light sampling expose opaque sculpted surfaces. The replacement uses gradual density variation through the interior and a bounded maximum condensate chosen for sub-unit optical depth per step. Shape and density are independent signals.

Sample spatial frequencies according to projected footprint, preserving broad cloud structure while filtering subpixel distant erosion. Replace former signed-distance empty-space skipping, which is inapplicable to the new weather potential. Keep low-overcast support at Coverage 1 and clear at 0. Do not change the accepted river gitlink, engine ABI, native scene archives or Legacy density path.

## Reference study and source boundary

Studied the owner-supplied UniStorm v5.3.4 archive: cloud include, CloudProfile, cloud rendering/compositing and quality/reconstruction control. Its relevant general principles are separate density/coverage/edge/base controls, continuous weather/height fields, 3D erosion, near-to-far detail treatment and volumetric optical integration. The reference's coverage is itself documented as nonlinear. It is not a direct drop-in solution to Renegade's coverage contract.

No Unity shader, script, texture or asset is copied into Renegade. The new hash, weather field, percentile calibration, profiles and density functions are authored independently and reuse only the existing pinned Wicked noise resources and renderer. Private reference extracts stay outside the repository.

## Validation approach

Start with small deterministic day/night profile captures in the existing renderer. Reject obvious solid blobs, tiny fragment fields, flat ceilings or lighting regressions before another owner preview. Match the study fixture's lunar orbit to the Studio preview and record actual weather/camera values. Only a visually credible candidate proceeds to the full DX12/Vulkan 1080p/1440p matrix and Studio/Runtime Debug/Release checks. Numerical success cannot override the owner's rejection.

## Controlled findings before a new preview

The first continuous weather-envelope study produced an unacceptable blurred deck; the second, with an explicitly sampled 3D body field, still looked like broad blurred banks. Neither was promoted to Studio. Their sources and captures are retained under BUILD/continuous-*.

A native-kernel control now holds the pinned engine SampleCloudDensity and SampleAlbedo implementations unchanged (whitespace/comment-normalized comparison against the accepted submodule HEAD). BUILD/native-density-control-verification.json records this. Only weather admission and formation height/anvil inputs differ. The initial control revealed another modelling error: mapping admitted weather directly to 1 bypassed most native density carving, turning occupied regions into decks. Admission and local native cloud weather are now separate signals: a stable .8 native local weather state grows more cloud columns, with coalescence reserved for the upper .88–1 request range. This is a controlled distribution/profile experiment, not another density-kernel rewrite.

The earlier optical-depth explanation is a hypothesis, not a proven root cause: the stock kernel can also produce high optical depth while retaining its own morphology. Its native curl warp, detail erosion and height/anvil response were lost in the rejected replacements. Keeping those mechanisms fixed is necessary to separate their contribution from coverage distribution.

## Native local-weather sweep

With noise, kernel, profile and light held fixed, the .25–.85 local-weather sweep exposes a structural trade-off: low values create sparse isolated fragments; high values create coherent bodies but excessive screen occupancy. At local .50 the reference view is about 81–83% occupied but visibly contains many small disconnected cloudlets. At .60 it is already about 94% occupied. These are diagnostic results, not acceptable presets.

The next control keeps local native weather at .80 and varies only spatial admission across three world positions and 35/70 degree views. Requested sky occupancy cannot be equated directly with the fraction of weather columns: a slanted ray can pass through several columns. A measured monotonic transfer from spatial admission to sky opacity is being evaluated before another owner preview. The native 3D density and albedo kernels stay unchanged.

A second reserved layer padding float (offset 28) now carries the initialized transient local-weather calibration. It is not a new archive field or user-facing control. The existing formation metadata schema and Legacy fallback are retained, with a static ABI assertion. All render applications must rebuild together before a new candidate is offered; existing owner executables remain untouched.
