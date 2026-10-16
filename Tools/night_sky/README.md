# Night Sky + Aurora: isolated branch checkpoint

**Branch:** `feature/night-sky-aurora-v1`
**Worktree:** `C:\Users\paulw\source\repos\renegade-night-sky`
**Active UI Designer worktree:** unchanged; nothing merged.

## Implemented

- Uses Wicked Engine's **existing realistic sky** (no new skybox).
- Configurable dark-blue/indigo zenith, muted blue-violet horizon and horizon glow, with smooth night/day sun gating. Existing native stars and twinkling are preserved.
- Independent procedural aurora HLSL with animated curtains, strength, coverage and speed; default off.
- Dedicated **Night Sky** Studio pane with gradient, zenith/horizon RGB, horizon glow, existing Stars, Aurora Enabled, intensity, coverage and speed controls. Skybox texture mode disables the procedural controls.
- Versioned settings metadata on the existing Environment weather entity; scene-reopen restoration, runtime weather hydration and Undo/Redo.
- The idempotent CMake overlay applies the Renegade shader fields to the pinned Wicked checkout *in this worktree only*. No unrelated code has been modified.

## Verified

- Actual sky shaders (`skyPS_dynamic`, `skyPS_static`, `envMap_skyPS_dynamic`, `envMap_skyPS_static`, `sunPS`) compile to **both DirectX 12 DXIL and Vulkan SPIR-V**.
- Full Wicked Engine shader package rebuilt and embedded consistently with the expanded `ShaderScene.weather` GPU data layout. Build and link succeeded (exit 0). The generated `wiShaderDump.h` is approximately 40 MB.
- The **final embedded-shader Studio Release build and link succeeded (exit 0)**. Executable: `BUILD/night-sky-check/Studio/Release/RenegadeStudio.exe`. It was built *after* the generated shader bundle.
- Night Sky CTest suite **2 of 2 passed**, including metadata persistence, native scene serialization, defaults, sanitizer, Undo/Redo and source integration.
- `Tests/NightSkyStateTests.cpp --fixture <path.wiscene>` generated the standalone night scene `BUILD/night-sky-check/Preview/NightSky-Midnight-Aurora.wiscene` (~4 KB).
- The generated preview file was reopened **from disk** with Wicked's native archive loader: midnight sun, gradient 0.85, stars 0.85, aurora 0.75 and coverage 0.65 verified, plus runtime hydration. Check command `--verify-fixture <path.wiscene>` passed with exit code 0.
- The official Microsoft DXC `v1.9.2609` archive was checksum-verified and its DLL used **only under BUILD/** for the full shader bundle. No global installation or replacement of the pinned source DLL.

## Runtime startup smoke check

- Launched the isolated embedded Studio Release minimised with no project; it remained alive after 10 seconds and logged Wicked Engine subsystem initialisation and runtime compilation of Studio-owned grid shaders. The preview process was then closed; no RenegadeStudio process remained. This is **not** proof of correct gradient/aurora appearance.

## Next gate: visual owner acceptance

**Do not merge or claim shipped.** The executable has not been launched while the user is running the active Studio/UI Designer. GPU performance and on-screen appearance have not yet been measured.

1. When the user's active Studio/UI Designer session is finished, run `Tools/night_sky/LaunchNightSkyPreview.cmd`. This launcher starts **only the isolated Studio**.
2. Use File > Open to load `BUILD/night-sky-check/Preview/NightSky-Midnight-Aurora.wiscene`, not a working project. Follow `Tools/night_sky/PreviewChecklist.md`.
3. Verify dark blue/violet gradient against the originally pitch-black midnight sky, live aurora curtains, native stars, dusk/dawn fading and existing clouds/fog.
4. Verify save/reopen and controls in Studio and profile GPU frametime at target resolution. Adjust authored colours/animation only after reviewing actual visuals.

**Do not run a new Studio concurrently on the same editable project.**
