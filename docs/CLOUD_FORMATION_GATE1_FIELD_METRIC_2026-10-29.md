# Gate 1: distance-field units under independent scale

Bounded implementation experiment, not appearance approval. Gate 1 remains open.
Read LIVE_REPLAY and the body-cache decision first.

The Cumulus cache stores a scalar signed morphology field. Runtime currently
multiplies it by min(horizontal scale, vertical scale). That is conservative
for empty-space reasoning but is not the surface-normal distance under a
nonuniform transform. The same 100 metre density transition becomes much wider
along horizontal sides when vertical scale is small. Flattened formations
would inherit this problem. Noise constants are not the cause of that error.

For a local implicit field f and diagonal shape scale S, use the first-order
world-distance estimate f / length(inverse-transpose(S) * gradient(f)).
This preserves the zero surface and uses physical metres for fine erosion and
the density transition. Near a horizontal side of a shape scaled by 1 in width
and .25 in height, the old min-scale method reports only one quarter of the
normal distance. A 100 metre transition can therefore spread across 400 metres.

Bake field value plus its three local derivatives into RGBA16F. Four immutable
variants occupy a 2x2 arrangement of 128-cubed tiles in one 256x256x128 texture.
This uses about 73 MiB with the existing seven mip levels. Native detail-noise
RGB, weather/archive layouts, fork pin and lighting remain unchanged.

The generator derives tile resolution and derivative spacing from actual UAV
dimensions. Runtime converts derivatives under the independent height/width
transform. It clamps each sample to its own tile's texel centres at both mip
levels used by trilinear filtering; variants must never bleed across tiles.
Allocation, dispatch and producer/consumer interpretation change together in
the authored overlay. No public SDK layout or scene-format revision is needed
for these derived private resources.

Keep one cached texture read per body lookup. The analytic derivative oracle
belongs only in the explicit reference shader permutation. It uses the same
local field and finite-difference spacing as the cache generator. Production
kernels must not carry that expensive oracle.

Before this change, the height/wind-fixed Cumulus baseline passed the 256-case
opacity/motion matrix across DX12/Vulkan at 1080p/1440p. It still fails visual
acceptance: some steep 80% views overfill and broad bases look repetitive/soft.
The other four profiles are still the earlier unapproved implementation.
Preserved authored sources: BUILD/cumulus-live-replay-source-checkpoint/.
Preserved executable and metrics: BUILD/body-cumulus-baseline-binary.json and
body-cumulus-baseline-{dx12,vulkan}-{1920,2560}/.

Validate cached versus analytic six-view geometry on both APIs, both layer
dimensions, physical-scale behavior, source/fork preservation and production
startup before using this cache for additional formation profiles. Refit/check
coverage only if measurements show it changed. Do not label a different image
as more realistic merely because shader creation or numerical checks pass.
