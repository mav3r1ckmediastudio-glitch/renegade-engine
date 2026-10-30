# Renegade Weather V2 — Gate 3 scripting and authoring contract
29 October 2026 — DEVELOPMENT / NOT FINAL SIGN-OFF

## Existing shared state
Weather transitions reuse `bridge::WeatherState`, `MakeWeatherPreset`, `ApplyWeather` and `WeatherTransitionController`. There is one scene weather state, no second weather shader, no separate Lua virtual machine.

## Lua during a running game
Only the existing governed creator-script Runtime owns the bindings, as `renegade.weather`:

```lua
-- Only from an attached, running creator script.
local ok, err = renegade.weather.set_preset("overcast", 15) -- 15 game seconds
if not ok then print(err) end

local w, queryError = renegade.weather.get_state()
-- w.transitioning, w.progress (0..1), w.duration
-- w.cloud_coverage, w.cloud_speed, w.fog_density, w.sky_exposure

renegade.weather.cancel() -- stop at CURRENT visual state, no abrupt rollback
renegade.weather.set_preset("clear", 0) -- apply instantly
```

Valid names: `clear`, `scattered`, `overcast`, `storm` (case-insensitive). Valid duration: finite 0 through 300 seconds. Invalid script calls return `false, explanation`, not access to raw engine pointers. Lua uses the existing Governed Script Runtime and lifecycle: scene load starts it, pause freezes transitions, stop/reset discards any transition, and no script performs scene mutation after its owning scene closes.

Script API contract version 1; query returns an ordinary table rather than any raw ECS handle. This is G3-weather-specific exposure, **not full Lua exposure for all engine systems**.

## C++ API
`bridge::WeatherTransitionController::Start(from,target,seconds)`, `Advance(dt)`, `Current()`, `IsActive()`, `Progress()`, `Duration()`, `Cancel()` and `BlendWeatherStates(from,to,progress)`. A render-independent deterministic value system. The caller owns the active `WeatherState`, so the same controller powers Studio and Runtime.

## Studio persistence semantics
- APPLY NOW: one SetWeatherCommand, Undo/Redo like other editor actions.
- BLEND TO PRESET: game-time smoothstep preview with one SetWeatherCommand on completion. No one-command-per-frame storm of Undo entries.
- CANCEL BLEND: restore the weather as it was before the preview; not an Undo.
- SAVE/Save As while blending: **finish the intended target immediately, commit it as one Undo action, then save the target weather**. Never silently save an intermediate preview or discard the user's choice.
- Undo, Redo, Open Scene, Reopen and Project Hub during blending: cancel and restore the authored source before processing navigation.
- Time-progress persistence across editor process exit is **not included**: Studio authoring persists the resulting weather state, not a suspended editor tween. Runtime saved-game weather state and a persistence/resume contract remain to be designed if game-save support is required.

## Regression gates
1. Pure C++ test `RenegadeWeatherTransitionTests` (passed prior).
2. Governed Lua integration test `RenegadeWeatherLuaRuntimeTests` (pending Release build / run) covers valid/invalid names, duration constraints, progress, pause, cancel, immediate change and script lifecycle.
3. Built real Studio GUI test performed in G3 initial preview: Storm -> Clear 17 sec yielded one Undo and 75 FPS in Clear; Cancel Blend restored prior Clear, generated no extra Undo; Apply Now -> Undo -> Redo restored desired weather with one history entry. Screen evidence `BUILD/g3-blend-*.png`, `BUILD/g3-storm-blend-*.png`, `BUILD/g3-roundtrip-*.png`.
4. Storm profile is materially more demanding: observed approximately 53–70 FPS in the test 1920-pixel desktop screenshot (not a synchronized GPU benchmark). **Performance optimization or performance acceptance still required before G3 sign-off**; do not misstate all presets run 75.
5. G3 current Test Game weather Lua, save/reopen, scene packaging, Lua fallback and performance regression require real user-path tests in updated Studio/Runtime build. No push or PR/merge.

Accepted Gates 1 and 2 and the Wicked river fork remain intact.
