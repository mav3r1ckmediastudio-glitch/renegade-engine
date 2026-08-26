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

1. Create terrain and confirm 1 m / 1.25 km presentation.
2. Sculpt across several inner chunks and capture a recognisable shape.
3. Expand once; confirm the existing shape does not change.
4. Undo expansion; confirm only the ring disappears.
5. Redo expansion and sculpt the new outer land.
6. Exercise Stars, rain, snow, Sun and Ocean.
7. Save, return to Story Flow, reopen and confirm terrain/environment state.
8. Launch Runtime and confirm DDS materials and snow presentation match Studio.
9. Resize the same build to 1280x720, 1680x945 and 1920x1080.

Green compilation is necessary but cannot replace this visual/behavioural pass.
