# Terrain collision compatibility decision — 2026-11-04

The pinned terrain renderer splits each grid cell along the (0,1) to (1,0) diagonal. Jolt heightfields split along (0,0) to (1,1). Vertices can agree while the surfaces inside a sculpted cell differ. Independent review confirmed the inherited ~9.7 cm stress-pit discrepancy; a four-by-four saddle regression against ec0c38a renders Y=0.5 m and returns Jolt Y=0.00147057 m at the same off-grid point.

Extend the existing Renegade-generated physics allocator overlay at the shape factory. Static, indexed, non-flat heightfields use the existing native triangle-mesh creation branch with LOD0 indices. Constant-height chunks retain compact heightfields. The public shape enum, mesh LOD setting, mass, transforms, vertex/index data and serialization are unchanged. The correction applies to all factory callers, including generation workers, brush/river refresh, missing-precompute fallback and cold loading.

No Wicked source file or submodule revision is edited. Configure requires exactly one match for each native anchor and fails closed if upstream changes. The generated copy remains under the build directory. The reusable allocator correction is retained.

This is a narrow core compatibility decision under AGENTS.md. A bridge-only post-update repair would leave first-frame/fallback holes; changing generated/rendered geometry would alter authored surfaces. Exact native triangles avoid both, including heightfield quantization on slopes. Non-flat chunks cost more collision memory/build time; maximum-extent and actual Studio/Runtime acceptance remain mandatory. No capacity or safety budget is increased.

Required regression: fail before repair at off-grid positions on both triangle halves; pass precompute, fallback, flat control, save/cold and native updates after repair. Existing sculpt Undo/Redo, rivers, expansion, maximum 1089-chunk, preservation and standalone acceptance remain required. Implementation alone does not accept an alpha.
