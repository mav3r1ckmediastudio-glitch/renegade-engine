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
- three prebuilt 1024 mipmapped DDS default-grass maps replace the former
  generated 4K TGA payload;
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

The correction enforces a dedicated owner at every entry point:

- new governed Level WISCENEs serialize one named Environment carrier;
- opening Environment on an older blank Level creates it through an undoable
  command;
- Studio establishes the carrier before terrain creation;
- the EngineBridge terrain boundary rejects creation if the precondition is
  absent;
- Terrain and Environment Inspector resolution is mutually exclusive, and a
  legacy dual-role terrain remains visible in the hierarchy for recovery.

The approximately 205 MiB Release artifact is not evidence of missing Studio
code. The Gate 5 foundation artifact is the same size; the reduction from the
older approximately 234 MiB package comes from replacing five large source TGA
terrain maps with three compressed mipmapped DDS runtime maps. The rejected
artifact still contains the complete Studio executable and runtime payload.

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

## Required owner acceptance

Use one exact-head packaged Release build:

1. Create a new governed Level and confirm Environment opens immediately.
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
11. Launch Runtime and confirm DDS materials and snow presentation match Studio.
12. Resize the same build to 1280x720, 1680x945 and 1920x1080.

Green compilation is necessary but cannot replace this visual/behavioural pass.
