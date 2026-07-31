# Phase 3 — Environment workspace and precipitation

## Outcome

Environment authoring is a dedicated Studio workspace rather than a special
row in the scene hierarchy. The top-right `SCENE / ENVIRONMENT` switch opens
the primary serialized `WeatherComponent` without discarding the creator's
current object selection. Returning to Scene restores the normal Transform
Inspector, outline and gizmo.

The Weather entity remains part of WISCENE because it is the runtime carrier
for sky, fog, clouds, wind and precipitation. Studio simply omits that service
entity from the creator-facing hierarchy.

## Functional precipitation

Wicked's pinned renderer provides one native GPU precipitation emitter through
the Weather component's rain fields. Renegade exposes it as three authored
modes:

- **Off** — particle amount is zero;
- **Rain** — fast, narrow streaks with native ground splashes; and
- **Snow** — slow, larger, unstretched pale particles with splashes disabled.

Rain and snow expose intensity, fall speed, particle size, horizontal wind
direction, wind strength and turbulence. All fields are serialized by Wicked
inside WISCENE and refresh the resolved runtime weather immediately.

Snow is an honest visual precipitation profile over Wicked's native emitter,
not a claim that surface accumulation exists. The pinned renderer also treats
all non-zero native precipitation as wet weather internally. Snow accumulation,
footprints, temperature, material coverage and a snow-specific collision pass
require a later Renegade-owned renderer slice.

## Command discipline

Mode changes execute one `SetPrecipitationCommand`. Slider drags follow the
same capture/preview/restore/commit contract as the accepted cloud and fog
controls, so every drag creates exactly one Undo entry. Unsurfaced native
particle values are captured and restored rather than overwritten accidentally.

## Packaged acceptance

1. Environment does not appear in Scene Hierarchy.
2. The top-right Environment tab opens weather controls without changing the
   selected scene object.
3. Returning to Scene restores that object's Inspector, outline and gizmo.
4. Rain visibly renders, responds to intensity and produces native splashes.
5. Snow visibly renders as slower, larger, unstretched flakes with no splash.
6. Wind direction, speed and turbulence visibly affect both modes.
7. Every slider drag produces one Undo step; mode changes, Undo and Redo restore
   the correct precipitation profile.
8. Save, close and reopen preserve the selected mode and authored values.
9. Existing sky, fog, clouds, panel resizing and drawer workflows remain intact
   under DX12 and Vulkan.
