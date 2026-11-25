# Approved foliage night reference

Owner-approved on 24 November 2026 in the normal DX12 Runtime. Open
`FoliageNightReference.renegade` with Renegade Studio or Runtime.

This preserves the exact approved scene bytes, six foliage plots, terrain,
materials, camera, shelter, sky, authored directional moonlight and warm lantern.
The WISCENE embeds the artwork, including the meadow atlas. Embedded resource
names retain their original relative identifiers; no external review directory
is required for the six foliage images. Project and scene identities are fresh.

Surfel GI is enabled, exposure is 3, flat ambient is zero, eye adaptation and
bloom are disabled. The moonlight is a separate authored light; automatic
moonlight tracking is not implemented by this sample. Wind is frozen for the
lighting comparison. This is a visual reference scene, not a gameplay level.

`Reference/approved-state.json` records file hashes and settings.
`Tools/FoliageNightReferenceVerify.cpp` cold-loads the scene, checks the saved
lighting/material/plant contract and embedded foliage resources. Run:

    FoliageNightReferenceVerify.exe <reference-Content/Scenes/Main.wiscene>

`Reference/night-shelter.png` is the settled native GI view;
`Reference/standalone-start.png` is the original normal-player startup view.
The lighting study and its limits are documented in
`docs/FOLIAGE_TERRAIN_NIGHT_GI_2026-11-24.md`. Future material/palette integration
should be compared against this approved reference at matched lighting, camera,
exposure and GI warmup.
