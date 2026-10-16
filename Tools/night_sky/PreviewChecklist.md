# Night Sky visual acceptance (isolated editor)

Open the isolated build only when your active UI Designer session is finished. **Do not open and edit the same project simultaneously in two Studio instances.** This branch has not been merged.

1. Build the embedded shader version of Studio (`RENEGADE_EMBED_SHADERS=ON`) and verify the resulting executable is newer than the generated `wiShaderDump.h`. The external-shader version is **not** an acceptable preview.
2. Generate the standalone `NightSky-Midnight-Aurora.wiscene` fixture in `BUILD/night-sky-check/Preview` with `RenegadeNightSkyTests.exe --fixture <destination>`; confirm it is an ordinary WISCENE file.
3. Start `Tools/night_sky/LaunchNightSkyPreview.cmd` (not the normal Studio shortcut). In Studio, open the fixture from the isolated Preview folder.
4. In the new **Night Sky** tab, compare Gradient 0 with 0.7–0.9 at midnight. Expect a nearly-black indigo upper sky fading to subtly lighter blue/violet near the horizon—not a solid-colour sky. Verify existing stars twinkle and remain visible.
5. Test Aurora Enabled off/on, then Intensity 0.4–0.8, Coverage 0.4–0.8 and Speed 0.2–0.7. The procedural curtain is centered near the +Z horizon; rotate the camera around if initially outside its field of view. Confirm gradual animation, no abrupt flickering.
6. Test day/dusk/dawn transitions. Gradient and aurora should fade out as the sun rises. Confirm the original daylight and cloud render remains acceptable.
7. Test the legacy scene with no Night Sky metadata. It must look unchanged. Verify Skybox mode disables Night Sky controls, and Undo/Redo and save/reopen restore settings.
8. Compare GPU frametime before/after enabling aurora, including high-resolution and fog/cloud coverage; record any regression.

**Current status:** Release C++ scene tests and all actual sky shader variants passed for DX12 and Vulkan; full shader dump built. Final embedded Studio build/link and visual acceptance are separate gates, do not mark complete until verified. No source changes were merged into the active main worktree.
