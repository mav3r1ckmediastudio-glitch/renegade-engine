# Weather V2 — Gate 3 execution plan: modular weather and transitions
Date: 29 October 2026
Status: NEXT GATE. Owner explicitly requested one last G2 lighting pass followed by G3.
Do not begin Gate 4 environmental effects, Snow V2 or alter accepted Gate 1 geometry.

## Starting point
Use the owner-approved G1 cloud baseline and the final isolated G2 lighting candidate only after preserving its exact source and review binary. Existing WeatherPreset enum, MakeWeatherPreset and StudioRenderPath::ApplyWeatherPreset already provide **Clear, Scattered, Overcast, Storm**. These are immediate assignments, not a complete weather-transition system. Avoid implementing competing weather models.

Existing references:
- EngineBridge/include/renegade/bridge/CommandService.h (WeatherState, WeatherPreset)
- EngineBridge/src/CommandService.cpp (CaptureWeather/ApplyWeather/MakeWeatherPreset)
- Studio/src/StudioApplication.cpp::ApplyWeatherPreset()
- Runtime scene.weather, NightSky/Moon service; authored cloud formation settings
- Existing Scene/Environment/Weather controls, Story Flow level lifecycle and Test Game.

## G3 objective and gates
1. **Workflow first**: Environment > Weather shows a compact Preset selector with Clear, Scattered, Overcast, Storm, Custom; an obvious *Apply Now* action and a **Transition** control. Simple defaults, advanced settings collapsible; never clutter the viewport or duplicate the current weather controls. Preset preview does not irreversibly overwrite artist settings.
2. **Deterministic transitions**: time-based blend (duration in game seconds, e.g. 0–300, 0=instant), easing curve, saved target and progress where appropriate, pause/resume with game time. Keep same result in Studio and Test Game; no jumps at start/end or resets of sky/Moon direction. Cloud cover/movement, fog density, exposure and sun/ambient colour blend safely; handle discrete toggles explicitly and avoid sky mode/no clouds going blank mid-transition.
3. **Weather state ownership**: Source and target captured via existing bridge WeatherState; scene/weather components updated through one authoritative pipeline; no concurrent competing updater. Undo/Redo should treat preset authoring as one operation, not one per frame; avoid dirtying scene every update tick. Persist preset selections and current transition across save/load/restart as specified.
4. **Scripting API**: Lua and C++ have equivalent bounded functions to set preset, duration, query active/target/progress, cancel transition, and subscribe to completion; errors visible and safe; no prompt-driven world generation. Do not fabricate 'all Lua engine systems' as a deliverable of just weather G3.
5. **Presets**: use the four existing presets as foundation; settings reflect current scene and are editable. Preserve developer-authored cloud-type, Moon and Sun schedules unless explicitly overridden by the preset; custom profile support should not require reworking weather shaders.
6. **Scope**: G3 controls transitions between existing weather states. Do not expand to thunderstorms, lightning, rain/wind physics, terrain snow deposition, seasons or Snow V2 (G4 and later). Existing rain controls may be referenced, not redesigned.
7. **Validation**: source/target stable hashes; all four presets & Custom, intermediate progress, rapid switching, transition cancel, Test Game runtime, scene reopen, restart, Lua/C++ contract tests, Undo/Redo, plus actual user project open. Test different cloud formations and noon/twilight/moonlit night. Preserve accepted G1 75 FPS in isolated DX12 comparison and G2 visual baseline.
8. **Release boundaries**: separate isolated G3 worktree after final G2 source snapshot/manifest, no automatic push/PR/merge. Owner visually checks at each gate; one clear ready-to-open Studio project and exact file path, pre-verified *Main Level opens* (including registry/sidecar project IDs).

## First engineering checkpoint
- Audit the already-wired preset UI and bridge calls and identify controls/fields not yet exposed.
- Design explicit WeatherTransitionState (source/target/duration/elapsed/status), deterministic blend and ownership.
- Add pure state-machine tests before UI; make compact inspector beginner-first.
- Wire Lua/C++ surface and save/reopen, then a real Studio and Test Game acceptance project.
