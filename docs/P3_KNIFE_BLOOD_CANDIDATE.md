# P3 KNIFE blood candidate — 2026-10-09

Status: experimental private-project candidate. The owner rejected the earlier
generated static spray, generated 16-frame atlas and furry floor splat. No visual
acceptance or P3 gate closure is established. No SPH was enabled.

## Scope and design

Character impacts keep the existing projectile/Hitscan routing. A scene material
tagged internally with `renegade.blood_sheet` supplies the blood sheet material.
This is an engineering fixture installation seam, not a new creator workflow.
Normal creators still select an object and choose Surface Type; Character is
automatic. No material/profile/object-override authoring chain is introduced.

The material's base-colour and normal textures are governed LP08 .rasset products.
Existing material stable-ID metadata, scene save/reload, Test Level snapshot and
Build Game resource dependency paths own their restoration and package closure.
Third-party artwork remains in the private test project; it is not added to
Runtime resources or the engine repository.

`Tools/Prepare-KnifeBloodCandidate.ps1` adapts the supplied pack's
`Particles/Textures/Sheets/Blood_1-8.png` and matching `Blood_1-8_n.png`.
It reads the mask as sRGB, computes the Liquid/Errosion shader's linear red-mask
alpha at softness 0.2, and bakes the Skin Impact prefab Custom1.x Hermite curve
(first key 0.25768325/0.008621216, last key 1/1, flat tangents).
It interpolates the 16 authored tiles into an 8x8/64-frame sequence at 256px per
tile, with matching normalized interpolated normal tiles. This is asset adaptation,
not a complete Unity shader/prefab port. Unity's reflection cubemap/cloud/stretched
particle layers are not ported.

Two bounded native PBR mesh cards present each blood hit, using native material
UV frames, normal maps and ordinary depth testing. They face the incoming side;
reversed contact normals are corrected. Lifetimes are 0.38 and 0.48 seconds after the second owner speed correction.
There is no soft-particle intersection fade. These are flat liquid sheets,
not volumetric liquid reconstruction. Grazing/observer angles remain a limitation.
Native Wicked plane winding is retained. Card half-widths are 1.04 and 0.62m;
the visible liquid occupies only part of each authored tile.

Existing mesh droplets trace actual world segments and leave native attached
decals. The rejected AI floor texture is replaced by sixteen asymmetric procedural wet
footprints with a solid irregular centre and a few satellite drops. These are
explicit fallback masks, not final realistic authored floor splats or a pool solver.
They persist for 600 gameplay seconds, darken/dry over 90 seconds and fade in the
last 30 seconds. Water does not receive these floor decals.

Budgets: 24 liquid sheets, 96 droplets, 128 stains, with oldest eviction.
Pause freezes animation/age; reset removes transient meshes/materials/objects,
drops and stains while retaining the authored material. Projectile queries exclude
the transient sheet objects. Diagnostics include blood_sheets, blood_drops,
blood_stains. GPU cost/VRAM have not been profiled.

## Asset findings

The supplied KNIFE pack has 15 impact prefabs covering skin, water, glass,
wood/plywood, metal, concrete, rock, asphalt, brick, tile, ground/mud/sand.
The four Skin decals are wound textures with surrounding skin; they are not
suitable floor blood splats. Puddle material GUIDs have no matching texture metadata
in either supplied pack copy. Do not substitute skin wound decals for floor pools.

## Reproduction

From the repository root, prepare the private candidate textures:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Prepare-KnifeBloodCandidate.ps1 -Pack <supplied-pack-folder> -Output BUILD/knife-blood-candidate
```

Build Release targets RenegadeRuntime, RenegadeRuntimeProjectileSessionTests and
RenegadeImpactAudioTests. Install only into the disposable playground:

```powershell
BUILD/renegade/Release/RenegadeImpactAudioTests.exe --install-knife-blood BUILD/bow-projectile-playground/Bow-Playground.renegade BUILD/knife-blood-candidate
```

The helper imports the two governed textures, binds the material, saves/reloads,
and captures native rendering at five ages. Its Character position/controller is
explicitly initialized and frozen for the read-only diagnostic after saving;
that freeze is not serialized into gameplay. It does not prove gameplay behaviour
or performance. Standalone owner inspection remains required.

Focused checks cover reversed-normal placement, native sheet animation/pause,
budget eviction and transient-resource cleanup in addition to previous projectile,
blood, glass/water, impact audio and launch socket cases.

## Rendering diagnosis

An earlier candidate installer incorrectly called native metadata Create on an
entity that already had metadata. Seven duplicate records accumulated in the
private fixture. Reload could select the older 4x4/16-frame material binding while
presentation expected 8x8/64 frames, producing chopped corner fragments that
appeared to be behind the dummy. Hiding the dummy reproduced identical gaps;
GPU texture readback then confirmed the old 4x4 atlas. This was an installer bug,
not established evidence of a Wicked depth/transparency defect.

The installer now reuses existing metadata, repairs only its fixture's duplicated
blood entity records, and checks one metadata record plus the two expected stable
IDs after native save/reload. Normal alpha blending is restored; the opaque
alpha-test experiment did not fix the underlying problem and is not retained.
The read-only helper supports RENEGADE_BLOOD_HIDE_DUMMY for occlusion comparisons
and captures the loaded atlas for inspection. No native Wicked code changed.

## Playback timing revision

Owner found the enlarged candidate better but too slow and stuttery. Main and
secondary playback were shortened from 1.25/1.5s to 0.55/0.70s. The 64-frame atlas
contains interpolated versions of 16 authored poses, not 64 new motion samples;
stretching those poses over long lifetimes exaggerates their stepping. Faster
playback is an initial correction, not proof of eliminated stutter or GPU cost.
Floor stain timing remains unchanged. Session checks cover early UV advancement,
pause and complete spray retirement within 0.71 gameplay seconds.

## Verification checkpoint

Final Release Runtime/session-test/helper build passed. Four focused tests passed
in 0.53s (ProjectileWorld, ImpactAudio, RuntimeProjectileSession, LaunchSocket).
Repeat native installation/save/reload and separate cold reopen passed. Build Game
staged both active 64-frame texture products through existing material dependency
edges; staged SHA256 matched the content manifest. Standalone package opened and
was responsive with authored equipment; observed owner gameplay retained 77 stains
without projectile errors. No injected input, visual acceptance or performance
claim. Logs are recorded in HANDOFF.md.

## Outstanding acceptance

- Owner check of moving spray against the actual dummy, including front/side shots.
- Proper authored floor splatter/pool artwork; procedural wet patches remain below
  the requested realistic quality target.
- Other surface art replacement, melee blood, skinned wounds and full content UX.
- SPH on/off GPU experiment and measurements, only after baseline visual review.
- Broad working-tree provenance review before commit, push or merge.

## Second speed / droplet / stain revision

Spray lifetime is now .38/.48s. Shared droplets use 12x24 tessellation and
age-based visual shrink without removing their collision traces. Floor marks
vary shape, size, aspect, tint and opacity and retain these differences during
drying. Native captures and four focused tests pass; owner motion/artifact
acceptance remains pending. These still use procedural floor fallback artwork.
See the latest HANDOFF entry for exact commands and limits.

## One-frame giant blood sphere: confirmed timing defect

Owner screenshot and native late-sync reproduction establish that the artifact
comes from changing transient CPU entity/component slots after GPU instance
upload. Runtime now creates, updates and retires projectile visuals before native
Scene/render update. Same-age frame29 old/new native captures reproduce/remove
the giant sphere. Smoother droplet tessellation alone did not solve it. Camera
aiming samples the completed scene before physics and refreshes for display after
physics. Owner moving-shot regression and gameplay acceptance remain required.

## Airborne / floor balance revision

Owner accepted the flash repair and requested more droplets with less excessive
floor coverage. Emission is now33 per reference arrow or60 at full strength.
Continuously varied3-10mm droplets produce much smaller collision stamps
(.025+5*radius metres before existing shape/scale variation). Live budgets and
persistence are unchanged. Candidate still needs owner gameplay appearance review.
