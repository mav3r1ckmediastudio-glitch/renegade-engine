# Renegade Studio

Renegade's Windows-first creator application. Current integrated baseline:
main 618b231, 10 November 2026. See [current status](../docs/CURRENT_STATUS.md)
and [creator guide](../docs/CREATOR_GUIDE.md).

Studio owns Project Hub, Story Flow/Journey, Level/Screen authoring, hierarchy,
Inspector and Asset Browser workflows, private modal previews, native gizmos,
Undo/Redo and save/reopen. It uses EngineBridge services for durable state and
transactions rather than embedding the stock Wicked Editor.

Current authoring includes terrain/rivers/waterfalls/weather, rendering/audio,
model/Character imports, Player/equipment, lights and layered particles.
Multi-selection and functional scene prefabs are integrated with the pickers.
Camera/terrain/river tool gestures retain exclusive ownership when appropriate.

Test Game captures native state on its owning thread, then performs owned
snapshot writing/validation and launch preparation in the background with an
animated activity indication. The initial capture is still synchronous.
Point visibility/membership queries use native lookups; selection-derived
outlines update on selection changes. Frame-rate claims require populated scenes
and matched settings.

Build with [BUILD_WINDOWS](../docs/BUILD_WINDOWS.md). The early PHASE2/PHASE3
documents preserve their acceptance history; they are not today's feature list.
