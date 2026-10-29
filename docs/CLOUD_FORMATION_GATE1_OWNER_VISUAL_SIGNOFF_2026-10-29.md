# Renegade Weather V2 — Gate 1 owner visual sign-off
**29 October 2026 | DESIGN**

The owner explicitly said: **"im giving you visual sign off of G1 right now, happy to go straight into G2"**.

**Decision:** Gate 1 cloud visuals are **owner-approved** at the current preview baseline. This supersedes older Gate 1 handovers saying "owner has not accepted cloud appearance". The previously confirmed moving-cloud **75 FPS at speed 8**, current Cumulus/Towering appearance and 0–12 speed control are part of the accepted preview baseline; exact preview executable SHA256 and evidence are in `docs/CLOUD_FORMATION_GATE1_PERFORMANCE_OWNER_ACCEPTED_2026-10-29.md`.

**Scope:** This is the **owner's visual sign-off**, not a claim that four full application builds, packaged tests, every formation at every setting, moon/cloud lighting, Night Sky dynamics, or automated PR/CI have passed. The full five-gate weather system remains unfinished. **No merge, push, PR or integration is automatically authorized.** G2 is now explicitly authorized by the owner. Snow V2 and Gates 3–5 remain out of scope.

**Freeze/preservation:** Preserve `BUILD/gate1/Studio/Release/RenegadeStudioCloudPerfNear64G1Oct29.exe`, `BUILD/perf-near64-speed8-solo.png`, `BUILD/Run-Gate1-Approved-75FPS-Clouds.cmd`, all accepted Cumulus sources and controls, and the accepted river-enabled Wicked fork `ceb816b844b4ab2fa56a159ac60a4f65c931bdfc`. Do not overwrite any owner Studio window or unsaved scene. New work must compare to this exact accepted baseline and provide a separate owner preview.

**Gate 2 scope:** Atmospheric illumination: coherent solar and lunar direct/scattered illumination on volumetric clouds, phase-aware moonlit rim light, correct cloud self-shadowing and relationship to night-sky/aurora/stars, physically plausible dusk/dawn transitions, and no cloud movement or 75Hz capped FPS regression. Work only in a new isolated Gate 2 working area after reproducing full dirty G1 source without committing/pushing it. Do not re-open Gate 1 visual design without the owner's request.
