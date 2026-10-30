# Weather V2 Gate 3 — creator Lua scripting surface (development checkpoint)
Date: 29 October 2026
Status: Under test. Not a Gate 3 acceptance statement.

## Scope
The governed creator Lua runtime shares the existing Renegade `WeatherState`,
`MakeWeatherPreset`, and `WeatherTransitionController` with the C++ bridge.
It does not expose arbitrary Wicked globals or introduce a second weather engine.

## Creator API
- `renegade.weather.set_preset(name, seconds)`: Select `clear`, `scattered`,
  `overcast`, or `storm`. Names are case-insensitive; optional `seconds`
  defaults to 0 (instant) and must be a finite number in [0, 300].
  Returns `true` or `false, error_message`.
- `renegade.weather.get_state()`: Returns a table with `transitioning`,
  `progress` (0 to 1), `duration` (seconds), `target_preset` (when set),
  `target_cloud_coverage`, `cloud_coverage`, `cloud_speed`,
  `fog_density`, and `sky_exposure`. Returns `nil, error_message` when
  called without an active Level.
- `renegade.weather.cancel()`: Stops an active blend without snapping weather
  back to its source; preserves the interpolated state. Returns `true`,
  or `false, error_message` when unavailable.
- Lifecycle callback `on_weather_transition_complete(self)`: Optional,
  once per completed transition, including instant transitions (delivered in
  a subsequent script-update tick). A cancellation does not fire completion.
  A callback can safely begin a new transition.

## Example
```lua
return {
  on_start = function(self)
    assert(renegade.weather.set_preset("overcast", 8))
  end,
  on_weather_transition_complete = function(self)
    local weather = renegade.weather.get_state()
    -- weather.target_preset == "overcast"
  end
}
```

## Ownership and current limits
- Game transitions advance only while Runtime creator scripts are running,
  using the bounded, pause-aware game delta.
- Changing or completing a runtime blend does not create an editor Undo step.
  Studio authoring uses its own separate command-backed preview/Undo workflow.
- Preset operations change cloud and fog parameters but preserve the author's
  Moon/Sun schedule and custom formation parameters.
- In-progress gameplay blend state is not yet serialized as a resumable game
  checkpoint. Test Game loads the saved Level and begins from its authored weather.
- Weather G3's full standalone Runtime, project reopen, FPS, save semantics,
  and owner visual acceptance must still be verified before sign-off.

## Latest validation
- Release Runtime build: PASS (`BUILD/g3-lua-validation-build.log` linked `RenegadeRuntime.exe`; the first combined command failed only when compiling an added test fixture).
- Corrected Release Lua test build: PASS (`BUILD/g3-s3-lua-final-build.log`).
- Exact focused CTest: **5/5 PASS** (`BUILD/g3-s3-lua-final-ctest.log`): `RenegadeS3GovernedLuaRuntimeTests`, `RenegadeWeatherLuaRuntimeTests`, `RenegadeWeatherTransitionTests`, `RenegadeMoonTests`, and `RenegadeCloudFormationTests`.
- Owner reports the currently running **Storm preset at 75 FPS** (30 October 2026, shortly after midnight London time). This updates/supersedes the earlier ~53–70 FPS screenshot estimate for that test scene. Not yet a controlled, repeatable heavy-scene benchmark.
- Keep G3 open until current Studio build, full project save/reopen, Test Game and visual owner acceptance are verified.
