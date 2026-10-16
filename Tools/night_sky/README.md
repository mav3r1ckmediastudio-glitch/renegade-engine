# Night Sky renderer prototype — isolated worktree

Worktree: `C:\Users\paulw\source\repos\renegade-night-sky`
Branch: `feature/night-sky-aurora-v1`

## Current scope

- `renegadeNightSky.hlsli`: modular linear-HDR night gradient and procedural animated aurora.
- `cmake/RenegadeNightSkyOverlay.cmake`: reproducibly adapts the pinned WickedEngine source at CMake configure time, keeping the parent submodule revision unchanged. The overlay is applied **only** when building from this branch.
- C++ NightSky shader uniforms use the native WeatherComponent, with zero as the default intensity for both effects. No extra skybox or particle emitter.
- `NightSkyService.h`: v1 namespaced metadata on the weather entity; restores transient WeatherComponent values on SceneService load and Studio document adoption. New environments author gradient 0.70 and aurora off. Existing scenes without v1 metadata do not change.
- `Tests/NightSkyStateTests.cpp`: metadata validation, legacy defaults, hydration and rejected entity checks.

## Verified

- Shader-module smoke test compiles with Windows SDK DXC `ps_6_0` (tested 16 Oct 2026).
- CMake `-P cmake/RenegadeNightSkyOverlay.cmake` passes twice (idempotent).
- CMake Visual Studio 18 2026 x64 configure passes in `BUILD/night-sky-check`, with `RENEGADE_EMBED_SHADERS=OFF`.
- Full Wicked sky shader compilation requires the engine's own shader toolchain. Direct compile with old Windows SDK DXC is **not** a valid gate for this pinned Wicked HLSL dialect and reports pre-existing unsupported syntax.

## Not yet complete

- Dedicated Night Sky editor pane with Zenith/Horizon colour, Brightness, gradient and aurora controls; no editor controls have been added.
- Undo/Redo command transaction for the new NightSky metadata.
- Zenith/horizon colours are currently constants in the shader and must become configurable authored parameters.
- Full integrated Wicked shader compilation and packaged Studio DX12/Vulkan testing.
- Screenshot matching, sky/cloud/water integration and GPU profiling.
- Only the standalone module has passed shader compilation; the current build must pass before testing in Studio.

## How to validate without affecting UI Designer work

1. Configure the isolated worktree, not `renegade-engine`, to a separate build directory.
2. Run `RenegadeNightSkyTests` in Release; do not launch or replace the user's running Studio executable.
3. Compare new and legacy scene rendering in a separate packaged Release build once the full shader pipeline is validated.
4. Merge only after CI, save/load, Undo/Redo and owner visual acceptance.

No changes to the original UI Designer worktree, existing WickedEngine checkout, or Studio release artifact are required for this prototype.
