# Renegade Night Sky & Aurora — isolated implementation checkpoint

Local worktree: `C:\Users\paulw\source\repos\renegade-night-sky`
Development branch: `feature/night-sky-aurora-v1`
Main UI Designer worktree: **untouched**.

## Implemented in source

- Separate `renegadeNightAtmosphere.hlsli` and `renegadeAurora.hlsli`, joined by the short `renegadeNightSky.hlsli` include and called from Wicked's native realistic sky shader.
- Night gradient uses direction-based interpolation from near-black indigo zenith to softly lifted violet/blue-grey horizon; fades in from dusk using existing sun direction. No new skybox, texture, or geometry.
- Aurora uses time-animated procedural curtains with intensity, coverage and speed; does no work when intensity is zero.
- 3 new GPU uniforms for gradient/aurora; named scene metadata authoring persisted on the existing Environment weather entity. Missing metadata preserves legacy scenes. Newly created environments get gradient 0.7, aurora intensity 0.
- A Night Sky workspace in Studio's top navigation, separate from Environment, containing gradient brightness, zenith RGB, horizon RGB, horizon glow, existing Stars, Aurora Enabled checkbox, intensity, coverage and animation speed.
- Authoring sliders preview live, then use one undoable command on commit. No-op changes are filtered. Aurora Enabled maps to intensity 0/0.6. When using a skybox texture, night controls are disabled.
- Existing native stars/twinkling and original physical atmosphere remain intact.
- CMake overlay reproducibly modifies only the branch's pinned Wicked submodule checkout during configure, instead of committing changes to the upstream Wicked submodule SHA.

## Checks that have actually passed

- DirectX Shader Compiler PS 6.0 smoke compilation of the Night/Aurora modules.
- Isolated CMake x64 Visual Studio 18 2026 configure.
- Isolated source-integration CTest (`RenegadeNightSkySourceContract`).
- Targeted MSVC C++ syntax compilation: `NightSkyStudioIntegration.cpp`, `StudioApplication.cpp`, `RenegadeStudioChrome.cpp`, `Phase5Gate5RenderWorkspace.cpp`, and `NightSkyStateTests.cpp`.

## Still required before a testable release

- Full build + link of `RenegadeStudio`, `RenegadeNightSkyTests` executable; actually run the Night Sky unit tests.
- Full Wicked Engine DX12 and Vulkan shader build, plus packaged Release startup.
- Visual sky check at midday, dusk, midnight and dawn on the owner scene; colour values require art-direction tuning.
- Aurora cloud interactions and GPU frametime profiling.
- UX polish: colour-picker swatches and optional palette choices (v1 currently provides linear RGB sliders).
- Visual and functional owner acceptance. **Do not merge into the UI Designer worktree or claim shipped before these pass.**

Build directory: `BUILD/night-sky-check` under isolated worktree; never use or overwrite the active Studio release binary. Build was intentionally restricted to source-level checks to avoid substantial CPU load during concurrent UI Designer development.
