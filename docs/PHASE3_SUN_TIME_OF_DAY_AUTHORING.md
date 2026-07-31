# Phase 3 — Sun and Time-of-Day Authoring

## Scope

This slice extends the accepted Environment workspace with a serialized sun
authoring model. It deliberately stops at editor authoring; runtime Lua clock
progression remains the next bounded phase.

## Creator workflow

- `ENVIRONMENT` contains a `SUN // TIME OF DAY` section.
- Time is authored from `00:00` to `24:00` through a slider or its numeric
  value field.
- Azimuth and elevation remain independently editable for art direction.
- Dawn, Midday, Golden Hour, Dusk and Midnight presets each produce one
  Undo/Redo command.
- `PLAY DAY` previews the complete path at an adjustable editor-only speed
  from `0.001` to `24.000` hours per second.
- `PAUSE` commits the preview's final position as one Undo/Redo command.

Play and Pause are deferred through Studio's `EditorAction` queue. This keeps
scene mutation, command creation and inspector refresh outside Wicked's active
button update, matching the safety rule used by the rest of the workspace.

## Engine ownership

`SunService` is the single bridge between Renegade's clock/angle model and
Wicked Engine. It updates both serialized scene state:

1. `WeatherComponent::sunDirection`, which drives the physical atmosphere.
2. The primary directional-light transform, preferring the entity named
   `Sun`, which drives world lighting and shadows.

The Weather component remains the canonical direction. Clock time is derived
from that direction after save/reopen, so this version adds no sidecar file or
editor-only scene metadata. Preview speed is intentionally not serialized.

## Solar path

The initial path is deterministic and art-directable rather than geographic:

- sunrise crosses the horizon at 06:00;
- solar noon reaches 75 degrees at 12:00;
- sunset crosses the horizon at 18:00;
- the sun continues below the horizon through the night.

Latitude, longitude, date, seasons, moon and astronomical accuracy are future
systems. The later Lua day/night feature should call this same bridge rather
than reimplementing sun rotation.

## Packaged acceptance

1. Time slider and numeric input move the atmosphere and directional light.
2. Azimuth and elevation can be authored independently.
3. All five presets visibly select distinct times.
4. Play advances continuously, supports three-decimal preview speeds and
   Pause stops immediately without switching workspace tabs.
5. A completed slider drag produces exactly one Undo step.
6. A Play/Pause preview produces at most one Undo step.
7. Undo and Redo restore both sky direction and world shadows.
8. Save, close and reopen preserve the sun position.
9. Existing fog, clouds, Rain, Snow and Scene workspace behaviour remain intact.
10. Debug/Release and DX12/Vulkan checks pass.
