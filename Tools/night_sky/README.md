# Renegade Night Sky & Aurora — isolated integration checkpoint

Worktree: `C:\Users\paulw\source\repos\renegade-night-sky`
Branch: `feature/night-sky-aurora-v1`. Active UI Designer worktree and Studio binary are untouched.

## Source implementation

- Independent `renegadeNightAtmosphere.hlsli` and `renegadeAurora.hlsli` HLSL modules, composed into Wicked's native atmospheric sky pipeline without a replacement sky.
- Smooth dusk/night sun gating, subtle blue/indigo zenith-to-violet horizon gradient, and animated flowing aurora; zero intensity yields zero contribution.
- Scene-owned `NightSkyState` and versioned weather-entity metadata. Missing metadata retains legacy scene visuals. New environments default to gradient strength 0.7, aurora off.
- Dedicated **Night Sky** Studio workspace: Gradient, Zenith R/G/B, Horizon R/G/B, Horizon Glow, native Stars, Aurora Enabled, Aurora Intensity, Coverage, and Speed. Controls are disabled in skybox texture mode.
- Live preview plus command-backed single-gesture Undo/Redo; zero-change authoring does not create a command.
- Idempotent `cmake/RenegadeNightSkyOverlay.cmake` reapplies the pinned Wicked shader edits from superproject-owned files when configuring this feature branch.

## Verified to date

- Single-file Night Sky/Aurora HLSL module PS 6.0 compilation.
- **Five actual Wicked sky shader variants compiled to DXIL (DX12)** using pinned Wicked Engine DXC 1.9.2602.24: `skyPS_dynamic`, `skyPS_static`, `envMap_skyPS_dynamic`, `envMap_skyPS_static`, `sunPS`. Reproduce: `powershell -ExecutionPolicy Bypass -File Tools/night_sky/verify_wicked_sky_dx12.ps1`.
- **The same five variants compiled to SPIR-V (Vulkan)** with an isolated official SPIR-V-capable DXC 1.9.2609, reproducing the exact binding shifts and descriptor set constants used by `wiShaderCompiler.cpp`. Reproduce: `powershell -ExecutionPolicy Bypass -File Tools/night_sky/verify_wicked_sky_vulkan.ps1`.
- The optional compiler ZIP was downloaded from [microsoft/DirectXShaderCompiler v1.9.2609](https://github.com/microsoft/DirectXShaderCompiler/releases/tag/v1.9.2609) into `BUILD/night-sky-check/shader-check/official-dxc` and verified against the GitHub release's SHA-256 digest `ad31b1fc8443175d204f77a611fdb3ef2ec42759bdc2f1167368de24a4a7e7f1`. **It was not installed globally or substituted into the Wicked source checkout.** This ZIP is NOT checked in.
- Isolated CMake VS18 x64 configure and `RenegadeNightSkySourceContract` CTest pass.
- Targeted MSVC C++ syntax compiles pass for `NightSkyStudioIntegration.cpp`, `StudioApplication.cpp`, `RenegadeStudioChrome.cpp`, `Phase5Gate5RenderWorkspace.cpp`, `NightSkyStateTests.cpp`.

## Mandatory before shipping or owner visual acceptance

- **Recompile the entire Wicked shader package** for the updated `ShaderScene.weather` GPU data layout; reusing old packaged shader binaries is unsafe even if the five sky passes compile. The new fields enlarge weather, which is nested before DDGI and other scene data in ShaderScene. Do not mix old shader cache/shader dump with new ShaderScene layout.
- Complete Release compile, link and **execute** `RenegadeNightSkyTests`, including actual Wicked scene archive save/reopen.
- Build a completely separate RenegadeStudio Release with a consistent, newly compiled complete shader package. No changes to the user's active Studio binary.
- Visual assessment at midday, dusk, midnight, dawn, stars at 0 and 1, clouds, fog, reflections, ocean and Aurora Off/On. Measure GPU frame-time at 1080p/1440p.
- UI polish: full colour-picker swatches and optional aurora palettes are not yet implemented; current night colour controls are linear-HDR RGB sliders.
- Owner sign-off and full Windows CI. **Do not merge into the active UI Designer worktree or claim shipped before all tests pass.**
