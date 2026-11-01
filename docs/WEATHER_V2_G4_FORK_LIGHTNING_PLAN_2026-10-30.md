# Weather V2 G4-F1 — Fork Lightning (separate, opt-in)
30 October 2026 | feature/weather-v2-all-gates

## Prior approval
G4 sheet lightning is **OWNER ACCEPTED**, recorded in `docs/WEATHER_V2_G4_SHEET_LIGHTNING_OWNER_SIGNOFF_2026-10-30.md`, root `b0f1de1` with Wicked `c25c9828`. Its shader, 30% feathered radii, reconstruction, timing, and brightness are **frozen**. Fork lightning comes before creator-facing Weather Inspector controls as explicitly requested.

## Fork lightning design contract
- Separate deterministic world-space bolt scheduler and geometry: one varied tapered cloud-to-ground trunk, 3–5 rooted secondary branches. More structural irregularity (offset segments, changing angles/lengths) than repetitive zigzag VFX.
- No combat targeting, damage, static ground mesh rings, or large repeating forks. No artificial constant-brightness rod, contact sphere, or white full-screen overlay.
- Emissive channel consists of a narrow white/blue tapered centre and a faint larger aura, with correct alpha/temporal pulse; world position within storm clouds. Only visible for brief sub-second intervals, with return strokes.
- Opt-in **separate** `RENEGADE_G4_FORK_LIGHTNING_PREVIEW=1`. Leave sheet lighting opt-in as before. This fork experiment must not alter approved sheet shaders or its scene data.
- Maintain deterministic time scheduling, pause behavior, scene-reload cleanup, bounded memory/entity lifetimes, usable at ~1080p/75FPS reference, no gameplay hit logic.
- Begin with pure geometry/timing tests before owner visual review. No push/PR/merge without owner approval. Preserve unrelated uncommitted rain/wind work.
- Deferred: weather Inspector switches, scene persistence, thunder, rain/wind, surface response and Snow V2.

Status: F1 **IN DEVELOPMENT**, not user approved.

## 2026-10-30 — Explicit owner guidance: reuse approved sheet-lightning as fork origin

The owner rejected the fork-specific circular/white-cloud origin glow and pointed out that G4 sheet lightning was already approved and should be reused at the fork origin, rather than reinvented for variety. Implemented directly on DESIGN:

- Fork lightning now owns a separate `bridge::SheetLightningController forkSheetGlow_` using the **identical approved settings** (intensity 0.14; cluster spread 720m; cell range 2340m). A new **additive** `TriggerAtPosition` method places the controller's existing randomized three-cell formation at the exact fork origin; the original sheet event generator/pulse/falloff is unchanged.
- `RuntimeSheetLightning::ApplyFrame` exposes the existing approved WeatherComponent upload path for reuse without duplicating shader packing.
- `RuntimeForkLightning` triggers this sheet pulse only when a new fork strike begins, then advances it normally, sharing the approved shader's cloud scattering and short temporal recovery. Its original custom `SetCloudGlow` and three oversized, fixed illumination pockets were removed. Previous experimental fork-specific cloud shader changes were restored to the pinned accepted Wicked shader with **no remaining Wicked uncommitted changes**.
- Fork mesh remains separate and unapproved, with curved branches and main channel widened approximately 25% relative to secondary branches. No weather authoring, thunder, damage or gameplay interaction.
- New sheet unit test asserts that triggered cell centroid tracks a requested fork origin, preserves seeded formation, pulses and bounded recovery, and never generates unrelated autonomous sheet events.
- **Build PASS:** `BUILD/g4-fork-approved-sheet-reuse-build.log`. **Focused tests 5/5 PASS:** `BUILD/g4-fork-approved-sheet-reuse-tests.log`.
- **DX12 visual capture:** `BUILD/g4-fork-flash-1.png` through `-3.png`, direct preview with cloud structure visible. Sampled overlay at 1920x1009 RTX 4070 Ti: **75 FPS**. Prior custom-origin captures preserved as `BUILD/g4-fork-customglow-g4-fork-flash-*.png`.
- **Owner approval still pending** for the fork lightning look. Approved G4 *sheet* lightning remains protected (root baseline `b0f1de1`, Wicked `c25c9828`); do not merge/push.
- Unrelated WeatherEffectsTransition prototype remains untracked and untouched.

## 2026-10-30 — Fork/cloud pulse synchronization defect fixed (owner visual review pending)
- Owner saw an **extra in-cloud sheet flash after the fork bolt had disappeared**. Root cause: the fork used a triggered but independently advancing `SheetLightningController`; its later third pulse (after ~0.51 seconds) occurred after the fork strike had already stopped (~0.40 seconds).
- Corrected `RuntimeForkLightning` so the fork's `ForkLightningFrame.brightness` and `visible` are the **single source of timing**. The existing approved `SheetLightningController` still supplies its seeded three-cell origin shape, radius and cluster placement via `TriggerAtPosition`, and `RuntimeSheetLightning::ApplyFrame` still uploads its approved scattering shader payload. However the fork no longer advances its own sheet pulse clock. New additive `SheetLightningController::FrameDrivenByStrike(brightness, age)` applies fork brightness to the approved three-cell field and zeroes *all* light on the fork's dark frames. Only a zero-radiance temporal reconstruction cleanup remains until age 0.62 seconds. The normal standalone sheet-lightning scheduler and its approved multi-pulse profile are unchanged.
- Added `RenegadeForkLightningTests` regression exercising actual first return stroke, dark gap, second stroke and disappearance, asserting cloud illumination matches fork visibility at every step and **never emits after the bolt vanishes**.
- Release build **PASS**: `BUILD/g4-fork-synchronized-light-build.log`. Focused CTest **5/5 PASS**: `BUILD/g4-fork-synchronized-light-tests.log`.
- DX12 Runtime preview relaunched and captured `BUILD/g4-fork-flash-1.png` through `-3.png` at 1920x1009 on RTX 4070 Ti, **75 FPS** in sampled screenshots. Latest capture sequence shows a lit fork followed by a dark fading fork and then dark cloud; earlier reference preserved as `BUILD/g4-fork-pre-sync-g4-fork-flash-*.png`.
- This is **NOT fork-lightning owner acceptance**. Preview is opt-in, remains on isolated weather branch; no push, PR or merge to main. Preserve signed-off standalone sheet lightning and the unrelated untracked WeatherEffectsTransition files.
