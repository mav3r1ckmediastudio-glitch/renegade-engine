# Renegade Night Sky — isolated corrective checkpoint

Branch: `feature/night-sky-aurora-v1`
Worktree: `C:\Users\paulw\source\repos\renegade-night-sky`
The user's normal Renegade Studio build has not been overwritten or merged.

## Critical black-sky regression (fixed in source, visual QA pending)

The first Night Sky preview **was broken**: the entire procedural sky went black in daytime and the existing native stars disappeared.

Root cause: our new `ShaderWeather` GPU data fields inserted a `float4` immediately after three `float` values. HLSL aligned that `float4` to the next 16-byte register, while C++ packed it at the next 4-byte offset. Subsequent fields were wrong: **HLSL sky_exposure offset 136**, **native C++ offset 132**. The shader read the wrong exposure value and multiplied the sky/stars by it.

Fix: an explicit `float night_alignment_padding` in `ShaderInterop_Weather.h`, initialised in `wiScene.cpp` and applied reproducibly by the CMake overlay (including migration of already-patched worktrees). **Native sky_exposure now lives at offset 136**. Permanent C++ `static_assert` checks pin the ShaderScene/Weather offsets, and DXC assembly reflection was cross-checked for the affected fields.

## What is built and verified

- The existing Wicked realistic sky, not a replacement skybox. Procedural indigo/violet zenith-to-horizon gradient, native stars and independent animated aurora HLSL module with intensity, coverage and speed.
- Recovery candidate: one Environment pane, with every group behind a collapsible heading, including Night Atmosphere, native Stars and Aurora. The separate Night Sky workspace is removed. Disclosure is session state; values remain command-backed. Native visual acceptance is still required.
- Versioned settings metadata attached to the existing Environment weather entity; missing metadata preserves legacy scenes.
- DX12 and Vulkan sky shader variants compiled successfully; the entire embedded Wicked Engine shader dump rebuilt after the ABI fix.
- **CPU/HLSL layout corrected and independently verified**. Native Release `RenegadeNightSkyTests` pass, including native archive roundtrip, Undo/Redo and legacy behavior. CTest suite **2/2 passed** after the fix.
- **Corrected embedded-shader RenegadeStudio.exe rebuilt and linked**, isolated Release exit code 0, newer than the shader dump.
- A real standalone preview project was generated via `ProjectService::CreateProject`, not a loose scene import. Its scene disk load, settings and weather hydration validated successfully.
- Executable `--startup-smoke` exit code 0 after the fix. This does not constitute visual validation.
- A guarded launcher runs `CheckNightSkyPreview.ps1` before opening the corrected exe; the launcher refuses old/unverified builds.

## Correct preview instructions

Open `C:\Users\paulw\Downloads\Renegade Night Sky Preview` and double-click `Open Night Sky Studio.cmd`.

In the isolated Studio choose **Open Project**, selecting:
`Renegade Night Sky Visual Test\Renegade Night Sky Visual Test.renegade`

The midnight starter scene is `Content\Scenes\Main.wiscene` **inside that project**. Opening a standalone scene from outside an active project is not supported by the current Studio Story Flow lifecycle; do not repeat that earlier failed workflow.

See `PreviewChecklist.md` for sky, stars, sunrise/sunset, aurora, save/reopen and GPU performance validation.

## Remaining gate

**The corrected build has not yet had owner or independently observed on-screen visual acceptance.** Do not claim the gradient or stars are visually proven until the actual sky is viewed in Studio. Do not merge or ship before that check and regression/performance testing.

Never test this feature by changing the user's real UI Designer project. A dedicated preview project is provided.
