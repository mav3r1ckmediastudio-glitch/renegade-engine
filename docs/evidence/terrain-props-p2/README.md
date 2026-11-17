# Terrain-prop P2 authoring evidence

Implementation: `01d7c012b8b4ebe6caacac74eaf80d5eaa91b0cb`, following handover `2853b55c55f7d7aeee8aaa8e9f69d2bba02ca2b8`. The separate final HANDOFF commit contains documentation and captures. [Implementation files](implementation-files.txt) lists all 34 changed paths.

## Native editor and player

- [Wide palette](ui-wide.png): 520-pixel inspector, with the private test window enlarged after measurement to expose every control.
- [Narrow palette](ui-narrow.png): 280-pixel inspector, captured after layout/render stabilization; linked numeric values and lower controls remain visible.
- [Player frame 60](runtime-frame60.png): actual separate Runtime loading the editor-saved P2Review scene.

The scripted editor uses production authoring/transaction services and shared widget callbacks. It generated 1,081 placements, erased to 1,062, verified Undo/Redo and persistent clearings, painted to 1,076, cancelled staging, then saved and reopened 1,076. Shared numeric handlers rejected/clamped extreme and nonfinite input. These are scripted native actions, not a claim of manual pointer testing.

The player used the exact scene bytes saved by the final editor proof: SHA-256 `04F17C9B0C7414A9E921E6573FDB45C5B76F822943E0DD9BFC7300A137B972DC`. Runtime executable SHA-256: `2152D68025CE50BB46A16C896C5D95C8C2A6851CB5925335B78654F9DA59EB17`. It captured frame 60 and closed normally with exit 0. The Runtime log had zero warning, error or missing-file matches.

The disposable player layout explicitly staged the three existing default-grass support maps required by this technical terrain fixture. It preserved the complete project products and scene bytes. This proves ordinary project-mode loading; it does not prove exported-package dependency closure for an arbitrary populated project.

## Performance limit

These are deliberately tiny, low-poly tree/bush/boulder fixtures. The earlier P0/P1 four mesh buffers totalled only 3,392 bytes. Matched editor interval samples used 120 frames, the same camera and viewport, with the display capped around 75 Hz and other validation work active. The screenshots and nearly unchanged frame intervals do not establish rendering headroom, real-asset woodland performance, physical memory savings or a 60 FPS woodland budget.

A representative forest still needs actual asset triangle/material/texture counts, foliage overdraw and shadows, uncapped frame-time measurements and native residency observations. No performance forecast for the owner's detailed assets is made here.

## Validation receipts

The latest P2 section in [HANDOFF](../../../HANDOFF.md) is the acceptance record. Raw machine logs remain under ignored build directories. `authoring-debug.txt` records the final 59,752 pure authoring assertions; it is not a substitute for native integration or full preflight.

Capture tooling and validation-directory workarounds were task-local. No streaming service, collision-interest radius, automatic cooking, eviction policy or impostor bake was added by P2.
