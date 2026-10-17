# Codex implementation handoff — Renegade Environment / Night Sky recovery

## Ownership and non-negotiable scope
You are the implementation and QA owner for this task. Work ONLY in `C:\Users\paulw\source\repos\renegade-night-sky`, branch `feature/night-sky-aurora-v1`. This is an isolated worktree of the user's Windows C++/Wicked Engine Renegade editor. Do NOT modify `C:\Users\paulw\source\repos\renegade-engine`, any other worktree, the active user's UI Designer source, their live project, or the normal Studio binary. Never merge/push automatically. Preserve unrelated modifications (including WickedEngine source overlay) and inspect status before edits.

## Latest requirements (supersede old Night Sky spec)
- **NO separate NIGHT SKY pane or top-navigation tab.** The user explicitly rejected it.
- Put all sky controls inside the **existing ENVIRONMENT panel**.
- **EVERY Environment option group must have a collapsible heading**, including existing groups: inspect current UI and group all existing options logically (Time of Day/Sun, Realistic Sky, Clouds, Stars, Fog, Ocean, Weather, Night Atmosphere, Aurora, and any other extant groups). Do not hide or drop existing controls. Native look and feel. Persist expand/collapse state for the editor session or preferences if trivial, defaults sensible. Collapse must actually hide children and reflow remaining inspector controls without gaps or overlap.
- Night Atmosphere: smooth deep navy/indigo zenith and muted violet-blue horizon, brightness/gradient intensity, horizon glow, proper dusk/midnight/dawn gating. Consider ergonomic colour picker UI rather than awkward channel-only RGB if existing editor supports it.
- Stars: preserve the EXISTING native star control, render, twinkling; no regression at midnight.
- Aurora: enable/disable, intensity, coverage, speed, procedural animated curtain integrated with existing Wicked atmospheric sky (not a replacement skybox); default OFF.
- Real-time preview, scene autosave through existing editor lifecycle, Undo/Redo, scene reopen, DX12/Vulkan support.
- If a skybox texture is active, procedural controls should be appropriately disabled or explained.

## Known failures — DO NOT assume earlier success
Prior assistant reported 'complete' several times without visual verification. The user tested and found: zero new controls visible, black sky even in DAYTIME, and native stars disappeared. Do not trust earlier assurances. Specific confirmed root cause was a four-byte C++/HLSL ShaderWeather layout mismatch (`sky_exposure` C++ byte 132 vs HLSL byte 136). Commit `9263993` on this branch adds explicit `night_alignment_padding` and static ABI assertions; audit the fix, GPU layout, build CMake overlay idempotence and shader caching rather than blindly relying on it. Verify bright daytime sky and visible twinkling stars FIRST, before aurora refinements.

Another failure: earlier loose WISCENE in Downloads was rejected because it lay outside the active Renegade project. A proper standalone preview project is at:
`C:\Users\paulw\Downloads\Renegade Night Sky Preview\Renegade Night Sky Visual Test\Renegade Night Sky Visual Test.renegade`
and `Content\Scenes\Main.wiscene` within it. Do not write to original user projects.

## Implementation approach
1. Audit current branch source, actual Studio navigation/actions, Environment inspector lifecycle, existing collapsible widget mechanisms, Scene/Weather GPU shader integration, serialization/undo. Take note of any active running Studio before launching tests. Read `Tools/night_sky/README.md`, shader modules, tests and CMake overlay. Verify actual behaviour rather than just static text.
2. Remove the separate Night Sky top-nav tab, workspace state and controls. Refactor into the existing Environment inspector with collapsible headings for ALL sections (including existing options); avoid tool duplication and unused fields. Use maintainable section abstractions and correct visibility/layout when selection changes, when Environment reopened and when toggled.
3. Fix functional issues and explicit ABI tests and ensure legacy baseline stars/day sky. Keep shader modules separate and optional. Minimise GPU overhead, preserve original sky when new controls zero/default.
4. Add automated UI/state and renderer regression tests practical in this codebase. Build isolated Release with shader package fully rebuilt after ShaderScene weather ABI changes (DX12 + Vulkan). Never use old shader dump with new CPU struct. Tests must cover section collapse/reflow, day sky ≠ black, stars > 0 at midnight, aurora ON/OFF, save/reopen, Undo/Redo, zero-intensity legacy.
5. Run the isolated editor and **actually verify the UI visible and sky rendering on screen**, ideally collecting screenshots/logs/artifacts in an isolated test-output folder. If you cannot inspect framebuffer, state that limitation and do not claim visual acceptance. No edits to active user project. Do not claim success based on compilation alone.
6. Commit auditable focused changes to branch (not main). Provide a factual final summary identifying files, commit, steps run and actual pass/fail, screenshot or evidence, remaining blockers. No PR/merge until owner approval.

## Key source pointers
- `Studio/src/StudioApplication.cpp`, `Studio/src/StudioApplication.h`: inspector widgets, Environment, Night sliders, layout, weather state.
- `Studio/src/RenegadeStudioChrome.cpp`, `.h`: top navigation; remove Night Sky tab/action.
- `Studio/src/NightSkyStudioIntegration.cpp`: NightField preview/command callbacks.
- `Studio/src/Phase5Gate5RenderWorkspace.cpp`: workspace exclusivity.
- `EngineBridge/include/renegade/bridge/NightSkyService.h`: persisted values, undoable command, runtime hydration.
- `cmake/RenegadeNightSkyOverlay.cmake`: pinned WickedEngine source overlay and ABI alignment migration.
- `Tools/night_sky/renegadeNightAtmosphere.hlsli`, `renegadeAurora.hlsli`, `renegadeNightSky.hlsli`: shader modules.
- `Tests/NightSkyStateTests.cpp`, `Tests/NightSkySourceContract.cmake`: existing tests; amend tests that still require obsolete separate Night Sky pane.
- `BUILD/night-sky-check`: isolated VS 2026 x64 Release build with full embedded shader package, historic passing two CTests and startup-smoke; do not regard this as visual proof.
- Windows-specific current toolset supports MSVC + CMake and both Wicked DXC DXIL/Vulkan SPIR-V backend; see README.

## Definition of done
User opens **Environment** (no Night Sky tab), sees every existing option within usable collapsible sections, night gradient/stars/aurora are within those sections and responsive. Midday sky not black; stars visible/twinkle at midnight. Aurora animates, fades at dawn and can be turned off. Save/reopen and Undo/Redo work. Genuine visual validation documented with evidence, performance checked. Only then request user acceptance. Do NOT announce completion earlier.
