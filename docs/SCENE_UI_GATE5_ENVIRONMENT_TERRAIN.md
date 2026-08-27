# Scene UI Gate 5 — Environment and Terrain Recovery

## Candidate boundary

Gate 5 recovers the accepted Environment/Terrain authoring surfaces while
establishing an honest finite-terrain baseline. It does not introduce a MAX
clipmap or an infinite world.

Delivered in the draft candidate:

- one-metre standard terrain vertex spacing;
- radius 9 / 19x19 standard chunks / 1.254 km square dimensions;
- creator-facing current-size readout instead of the raw generation radius;
- one-ring non-destructive expansion with Undo/Redo;
- existing native per-chunk height/blend WISCENE persistence retained;
- physics remains bounded by Wicked's smaller physics radius;
- native distance LOD, frustum and occlusion culling remain active;
- the accepted 4K TGA grass sources and build-time surface packer remain the
  validated default-material payload;
- native realistic-sky Stars authoring through the weather command;
- the accepted snow motion retained with a dedicated static snowflake DDS;
- Studio and Runtime package the same terrain and snow resources.

## Owner-reported rejection and lifecycle correction

The packaged Release at published head
`cdd4dea6934438745faaa8f0bfc54ec167838641` passed CI but failed owner
interaction testing: a newly governed blank Level left Environment inactive,
and creating Terrain crashed Studio. That build is rejected regardless of its
green jobs.

The failure was a scene-owner lifecycle hole. New Level creation serialized an
empty `wi::scene::Scene`. Environment therefore had no Weather entity to edit.
When Terrain generation ran in that same blank scene, pinned Wicked created its
fallback Weather component on `terrainEntity`, making one entity own both
Environment and Terrain presentation.

The first lifecycle correction was still owner-rejected at published head
`901d3b90e1fe5666dacaebbc89e3e067a897d00d`. It created a partial Clear Weather
component when Environment was clicked but did not create a directional Sun.
Stars therefore rendered at midnight while the realistic atmosphere stayed
black at midday. The same candidate replaced the accepted terrain TGAs with
malformed DXT5 files. Each DXT5 file was shorter than its declared top mip, and
the shared binding was used by both new Terrain and old terrain-project open.

The corrected boundary enforces a complete owner and validated assets at every
entry point:

- new governed Level WISCENEs serialize one named Environment carrier and Sun;
- opening Environment on an older blank Level captures the live atmosphere and
  creates Environment and Sun through one undoable command;
- existing authored directional lights are preserved;
- Studio establishes the carrier before terrain creation;
- the EngineBridge terrain boundary rejects creation if the precondition is
  absent;
- Terrain and Environment Inspector resolution is mutually exclusive, and a
  legacy dual-role terrain remains visible in the hierarchy for recovery.
- Studio and Runtime use the accepted TGA surface-packer output for Terrain
  creation and old-project material rebinding.

The approximately 205 MiB Release artifact was materially incomplete: it had
lost the three approximately 64 MiB packed TGA runtime maps. The restored packer
produces complete 4096x4096 RGBA outputs of 67,108,882 bytes each. Artifact size
is therefore part of the exact-head package audit for this correction.

## Expansion contract

At one-metre spacing a native chunk spans 66 metres. Radius 9 therefore gives:

`(9 * 2 + 1) * 66 = 1,254 metres`.

Expansion changes the radius to 10, adding exactly one missing outer ring. It
does not call `Generation_Restart()`. Wicked's generator observes that all inner
coordinates already exist and creates only the missing ring asynchronously.
Undo cancels generation, removes chunks whose Chebyshev distance exceeds the
former radius, and leaves every inner height/blend entry untouched. Redo asks
the native generator to recreate the ring.

## Streaming limitation discovered by source audit

The pinned Wicked `Terrain::Generation_Update()` removal path calls
`chunks.erase(it)`. `ChunkData::heightmap_data` and blend layers are the same
data serialized into WISCENE. Stock removal therefore deletes authored state;
returning later procedurally regenerates the chunk instead of restoring the
creator's sculpting.

Gate 5 keeps `centerToCamera=false` and `removeDistantChunks=false`. Distant
chunks still receive native LOD/frustum/occlusion culling, and rigid bodies are
present only inside `physicsChunkRadius`, but authored CPU chunk data remains
resident. Claiming safe edited-chunk unloading would be false until Renegade
adds a separate authored-data cache or adopts an explicit Wicked core patch.

## Accepted result

Corrected head `93659c274fa1ce6edf68e2478dfbfa8c9e3298a4` passed all four
required Windows checks. The project owner then accepted the packaged Release
after exercising the corrected Environment, realistic sky, stars, Sun, Terrain
creation, old-project open, sculpting, expansion, Undo/Redo, dirty state and
save/reopen paths. PR #105 was merged to `main` as
`1e0470a9e530dd20c42ddf16662c3771aaede825` on 2026-08-27.

The rejected heads and incomplete approximately 205 MiB artifact remain failure
evidence only; they are not accepted Gate 5 builds.

## Required owner acceptance

Use one exact-head packaged Release build:

1. Create a new governed Level and confirm Environment opens immediately with
   realistic sky at midday, stars at midnight, and a named Sun in hierarchy.
2. Open Terrain, create it, and confirm Studio stays alive and shows Terrain
   controls rather than Environment controls.
3. Confirm Environment still opens and remains independent after Terrain exists.
4. Confirm 1 m / 1.25 km presentation.
5. Sculpt across several inner chunks and capture a recognisable shape.
6. Expand once; confirm the existing shape does not change.
7. Undo expansion; confirm only the ring disappears.
8. Redo expansion and sculpt the new outer land.
9. Exercise Stars, rain, snow, Sun and Ocean.
10. Save, return to Story Flow, reopen and confirm terrain/environment state.
11. Open a pre-Gate5 terrain project, then launch Runtime and confirm TGA
    materials and snow presentation match Studio.
12. Resize the same build to 1280x720, 1680x945 and 1920x1080.

Green compilation is necessary but cannot replace this visual/behavioural pass.
