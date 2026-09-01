# WD01 — Clean Native Wicked Vegetation

## Authority

- Base: `main` at `350927ee2e065405ff9d4c0c59ff791fa4df7c84`
- Wicked pin: `3a800b7134aafe58461093c8abb2e274d4e64033`
- Implementation branch: `wd01/wicked-vegetation-clean`
- PR #119 is not an implementation source and no commits are inherited from it.

## Creator outcome

The existing Terrain inspector gains a **VEGETATION // WICKED GRASS** section beneath terrain sculpting. The creator can select PAINT or DELETE, set one world-space brush size, and drag continuously across terrain-chunk boundaries.

The panel exposes the native Wicked terrain grass controls:

- Density
- Length and width
- Stiffness, drag and gravity power
- Randomness and random seed
- Segments and billboards
- View distance
- Uniformity
- Camera bend

## Native boundary

Renegade does not introduce a vegetation renderer.

The pinned Wicked `wi::terrain::Terrain` owns one authored `HairParticleSystem` per chunk. Wicked remains responsible for GPU simulation, wind sampling, collider response, culling, fading, strand geometry and WISCENE serialization. Renegade contributes:

- a bounded EngineBridge authoring service;
- a Renegade Terrain-inspector workflow;
- completed-stroke Undo/Redo;
- packaging of Wicked's bundled `grass.wiscene` and `grassparticle.png`.

## Paint and performance contract

1. The brush tests its world-space sphere against every transformed terrain-chunk mesh AABB, so one stroke is not constrained to the picked chunk.
2. Exact mesh vertices are tested only for chunks passing that broad phase.
3. Pointer movement is interpolated to prevent gaps at low frame rates.
4. During a live stroke, Renegade edits the existing native `vertex_lengths` array and requests Wicked's `REBUILD_BUFFERS` path.
5. The live HairParticle component and material are retained. They are not replaced for every brush sample.
6. The authoring emitter distribution is finalized once per touched chunk when the gesture ends.
7. Empty chunks do not retain live grass entities.
8. Newly generated terrain chunks are switched to manual vegetation exactly once using serialized Wicked metadata markers.
9. Idle Studio frames compare only the active terrain identity and native chunk
   count. A full chunk synchronization pass runs only when the terrain is
   replaced or generation/expansion changes that count.

The system therefore scales with touched chunks and touched mesh vertices, not with the total terrain size.

## Persistence

Wicked's existing terrain serialization writes each chunk's authored `HairParticleSystem`, including the native length mask. The same WISCENE is consumed by Studio, Test Level and packaged Runtime. Renegade's manual-terrain and manual-chunk markers are ordinary serialized Metadata components.

## Owner acceptance

Before merge:

1. Paint continuously across at least four visible chunk boundaries.
2. Delete through the same boundaries.
3. Verify no persistent hitch or progressive FPS collapse during a sustained stroke.
4. Verify completed-stroke Undo and Redo.
5. Save, reopen and confirm masks and settings.
6. Confirm Wicked wind and camera bend.
7. Confirm Test Level and packaged Runtime render the same grass.
8. Compare an empty terrain and a representative painted terrain using Phase 5 diagnostics.
