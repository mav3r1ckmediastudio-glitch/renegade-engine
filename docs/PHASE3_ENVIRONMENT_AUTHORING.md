# Phase 3 Environment Authoring

## Outcome

Selecting the primary weather entity replaces the Transform inspector with a
curated Environment inspector. It authors Wicked Engine's existing atmosphere,
fog, volumetric clouds, and cloud-shadow system without editing Wicked source
or exposing its full weather parameter block.

## Creator controls

- Clear, Scattered, Overcast, and Storm presets.
- Realistic sky, realistic sky with volumetric clouds, and existing skybox
  texture modes.
- Aerial perspective, sky exposure, and ambient intensity.
- Distance fog, height fog, and fog-layer bounds.
- Primary cloud-layer coverage, start height, and thickness.
- Volumetric clouds casting shadows onto the world.

Every accepted field edit and every preset is one `SetWeatherCommand`.
Undo/Redo restores the complete curated state. Values not represented by
`WeatherState`, including the second cloud layer, wind, rain, ocean, colours,
weather maps, and advanced scattering parameters, are left untouched.

## Automated acceptance

`RenegadeBridgeTests` must prove:

1. `SceneService::WeatherEntity()` returns the primary weather or
   `INVALID_ENTITY`.
2. A weather command applies, undoes, and redoes the curated fields.
3. An unchanged edit does not enter command history.
4. Hidden Wicked weather values survive an edit.
5. Clear and Storm presets contain their defining state.
6. Volumetric clouds, cloud shadows, exposure, coverage, altitude, and
   thickness survive a headless WISCENE save/reopen round trip.
7. The Wicked submodule remains pinned at `3a800b71`.

## Packaged Windows acceptance

Test both launchers:

```text
Run-RenegadeStudio-DX12.cmd
Run-RenegadeStudio-Vulkan.cmd
```

In each:

1. Select `Environment` in the hierarchy.
2. Confirm the Environment inspector replaces Transform controls.
3. Apply each preset and confirm the viewport responds immediately.
4. With volumetric clouds active, change coverage, base height, and thickness.
5. Toggle cloud shadows and confirm the world lighting responds.
6. Undo and redo the preset and individual changes.
7. Save, reopen, and confirm the authored state persists.
8. Select a normal object and confirm the Transform inspector and gizmo still
   work.

Required report:

```text
DX12 ENVIRONMENT PASS / CLOUDS PASS / CLOUD SHADOWS PASS /
UNDO-REDO PASS / SAVE-REOPEN PASS / TRANSFORM REGRESSION PASS /
VULKAN ENVIRONMENT PASS
```

Green CI alone cannot provide the cloud or cloud-shadow visual acceptance.

## Deferred

Sun and light-component authoring, material authoring, skybox asset selection,
the advanced second cloud layer, cloud weather maps, rain and wind controls,
viewport post-processing, arbitrary local fog volumes, and custom cloud
shaders remain outside this bounded vertical slice.
