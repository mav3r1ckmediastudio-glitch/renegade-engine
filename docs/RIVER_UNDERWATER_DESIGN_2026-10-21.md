# River underwater rendering: implementation contract

Owner requested ocean-like underwater rendering, authorized a river-specific effect, and asked to omit it for shallow mountain streams. Initial eligibility is a linked riverbed depth of at least 0.5 m. A surface-only ribbon has no invented immersion depth. Existing surface materials and ocean parameters must remain unchanged.

## Owned rendering seam

RiverUnderwaterRenderer belongs to each Studio/Runtime render path. It installs one BeforeTonemap CustomPostprocess, using Wicked's public renderer hook. No Wicked source/pointer modifications and no temporary ocean enable/height replacement. Resources are transient, never serialized. Native river material baseColor/alpha and sheenColor (Wicked's water extinction channels) supply optical parameters; existing material commands own persistence/Undo. Linked depth is already persisted against stable river identity by the channel layer service.

Generated native ribbon triangles define finite triangular water columns, extruded vertically by the linked channel depth. Sloped surfaces therefore have local top and bottom planes. Vertical/degenerate faces are excluded so a falling sheet cannot become an infinite underwater pool. The terrain remains geometry authority: world-depth clips visible rays and a terrain query excludes cameras beneath solid ground. This first volume uses the authored depth across each ribbon triangle; it is not a newly simulated fluid volume or an exact volumetric reconstruction of the sculpted banks.

A CPU-built threaded BVH accelerates ray/column intersections. Buffer content is rebuilt only when the captured geometry/depth/material fingerprint changes. The shader merges adjacent intervals immediately to avoid counting every tessellation triangle separately. Near-plane pixels must be inside water; integration stops at the connected water exit or visible opaque geometry. A 2.5 cm transition softens the partial-submersion waterline. Above-water/far cameras do not dispatch the effect. Native ocean-immersed pixels are left to the preceding ocean postprocess; raised inland rivers remain eligible.

Bounds: 16384 nondegenerate native triangles, 64 disjoint ray intervals, 60 m optical integration limit. An over-budget volume is refused rather than truncated. The owned shader is compiled to DXIL by RenegadeRiverShaderCompiler during the native build and deployed through an explicit shader-content dependency even on shader-only edits. Runtime loads the compiled artifact without compiling source during gameplay. Build Game declares it as a governed bundled Runtime resource. Long rivers, dense overlapping volumes and ocean junctions require performance/visual validation. Future optical profiles and per-reach depth authoring are separate from this implementation.

## Validation required

Unit: convex ray exits at waterline/banks/bottom; sloped local elevation; shallow cutoff at 0.499/0.5 m; hidden/unlinked/vertical exclusion; stable identity and persisted depth after archive remapping. No-device unit tests must not recreate GPU meshes: that requires the native graphics proof.

GPU: actual native saved linked terrain; underwater enabled/disabled comparison; partial waterline; above-water dispatch removal; camera below bed/dry bank; scene edits and save/reload. Studio and local Runtime must both compile. Real Test Game enter/exit/return remains mandatory. Preserve the owner's open Studio session; if linking is blocked, use a separately named candidate or wait for owner closure. Compilation and unit success do not establish visual acceptance.

## Current checkpoint

Implementation is being built and tested. No final underwater visual acceptance, packaged distribution or wider waterfall completion is claimed.

## River swimming follow-up

Owner initially approved the appearance as “actually not bad” but reported being forced to the surface despite looking down and pressing W. Runtime land input intentionally flattened pitch, and Wicked MoveCharacter retained vertical velocity while native rigid-body buoyancy supplied upward lift.

PlayerSwimmingService now owns a transient Runtime-player-only river movement mode through public Wicked/Jolt SetLinearVelocity. Waist submersion enters eligible linked river volumes; a lower exit sample gives hysteresis. Mouse pitch supplies forward vertical velocity. Intentional forward movement overrides passive float; idle float is bounded to 0.35 m/s upward and jump requests ascent. Native buoyancy is saved and disabled only during this mode, then restored on river exit. Native gravity, capsule and collision resolution remain active. Shallow walking and ocean movement retain their existing paths. No authored or serialized field is added.

Native DX12/Jolt proof uses an actual WATER ribbon and a solid 3 m-deep bed: initial feet -1.14034, dive -3.00351, idle -2.91192, collision bottom -2.98846, ascent -0.954734 m. It exited 0. The boundary teleport is fixture setup only. Gameplay never teleports. The unlit fixture image is movement evidence only, not a terrain appearance acceptance capture. Focused CTest passes 8/8. Owner Test Game acceptance and normal Runtime executable replacement remain open while their previous Test Game holds the binary.
`nDelivery follow-up: owner closed Test Game; normal Release Runtime rebuilt successfully. Next Studio PLAY uses the swimming implementation; owner dive walkthrough remains pending.
