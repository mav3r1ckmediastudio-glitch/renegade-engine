# Phase 3 — Native Ocean Authoring V1

## Outcome

The Environment workspace now authors Wicked Engine's camera-relative FFT
ocean without requiring terrain tools or a generated water mesh. The surface is
effectively infinite and remains centred around the camera, making this slice
appropriate for oceans, islands, coastlines and flooded worlds rather than
bounded lakes or rivers.

## Complete native parameter coverage

`OceanState` captures every field in the pinned `wi::Ocean::OceanParameters`:

- enabled state;
- FFT displacement-map dimension (`64`, `128`, `256`, `512`, `1024`);
- patch length;
- simulation time scale;
- wave amplitude;
- ocean wind direction and speed;
- wind dependency;
- choppy scale;
- water RGBA;
- extinction RGB;
- water height;
- surface detail; and
- screen-edge displacement tolerance.

The four presets — Calm, Coastal, Storm and Alien — are curated native states,
not alternate renderers. Presets, toggles, resolution changes and completed
slider edits each produce one `SetOceanCommand` Undo/Redo transaction. Slider
dragging previews live, restores the before-state, then commits once.

Wicked's spectral inputs and FFT dimension require recreation of its lazy ocean
GPU resources. Colour, water level, choppiness, simulation speed and rendering
detail remain cheap live edits. `1024` resolution and high surface detail are
labelled expensive in Studio.

## Reflection and honest boundaries

Studio already enables Wicked's reflection rendering and the native ocean
shader reflects the scene/sky automatically. Reflection is therefore real
renderer behaviour, not a decorative Renegade checkbox.

The pinned native ocean does not expose shoreline foam, foam masks, underwater
caustics, refraction controls, terrain intersection, lake volumes, river flow,
buoyancy authoring or underwater post-processing. V1 contains no dummy controls
for those features. Foam and caustics require a later Renegade-owned shader or
an upstream engine extension.

## Packaged Windows acceptance

1. Enable Ocean and confirm an animated horizon-spanning surface appears.
2. Move Level above and below the Proving Ground deck.
3. Confirm Calm, Coastal, Storm and Alien are visibly distinct.
4. Confirm the water reflects the current physical sky and scene lighting.
5. Change wave amplitude, choppiness, simulation speed and ocean wind.
6. Change water colour, opacity and extinction colour.
7. Confirm `64` and `512` resolution work; treat `1024` as an optional GPU test.
8. Confirm one completed slider drag creates exactly one Undo step.
9. Confirm Undo/Redo restores enable state, presets and all authored values.
10. Save, close and reopen; confirm Ocean and its settings persist in WISCENE.
11. Recheck clouds, fog, precipitation, sun preview and Scene workspace.
12. Pass Debug/Release and packaged DX12/Vulkan checks before merge.
