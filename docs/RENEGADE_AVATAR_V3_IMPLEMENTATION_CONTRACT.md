# Renegade Engine Avatar — V3 Real-Time Implementation & Look-Lock Contract

**Decision status:** **LOOK LOCKED TO V1** by owner decision on 2026-10-13 after V3 HTML visual comparison. V3 visually rejected; technical/performance and release acceptance remain separate/open.
**Date:** 2026-10-12
**Owner decision (resolved 2026-10-13):** V1 is the **selected and locked visual baseline** for the Renegade engine orb. V2 was rejected for overexposure and poor visual fidelity; the one authorised V3 HTML implementation was completed and **failed the owner visual comparison** ("the orb looks nothing like the concept"). The side-by-side comparison was useful, but V3 does not displace V1. Owner described performance as "as expected" without supplying a measured acceptance result; **do not claim performance passed**. Subsequent pre-release polish is possible without reopening an indefinite V4 design search.
**Programme:** RA-03 visual manifestation, coordinated with RA-02 detachable chat and RA-07 optional voice.
**Reference:** Owner-approved concept art from the 2026-10-12 Renegade avatar design conversation (large expanded orb with a distinct central speech waveform; cool cyan/blue/violet interwoven filaments; restrained amber ribbons; dark transparent core). Preserve the actual reference image for side-by-side review before acceptance.
**Scope:** A standalone, **actually animated and executable renderer**, not an AI image, screenshot, video loop, or purely static concept. Do not add autonomous engine/game creation, modify combat systems, or require the experimental Qwen service.

## 1. The decision rule (non-negotiable)

1. Preserve V1's files, behaviour, and appearance as an immutable comparison reference. Do not replace, overwrite, or discard the only working V1 artifact.
2. V2 is historical experimental evidence only; it is NOT the V3 source of truth. Do not carry over its large white torus, muddy interior or expensive full-screen effect.
3. Produce one coherent V3 implementation and allow focused **defect corrections** (crashes, broken shaders, bad scaling, unintended clipping) before owner review. Do not repeatedly reinvent the art direction.
4. Show **live** V1 and V3 at the same on-screen diameter, resolution, zoom, background, state, hardware and brightness context, including a speaking animation and collapsed idle animation.
5. The owner makes a clear, binary call: **V3 preferred and performance-accepted -> lock V3**; otherwise **lock V1**. Neither engineering effort spent nor a flattering still image counts as visual approval.
6. A locked **look** is not a claim that the avatar, detachable windows, TTS or packaged Studio integration are release-ready. Those remain separate acceptance gates.

## 2. Recognisable visual identity — hard pass/fail requirements

### Silhouette and composition
- Spherical **living energy presence**, not a face, humanoid, mascot, ball with eyes, fairy or desktop-paperclip analogue. It represents the Renegade engine itself.
- Readable sphere with **dark central negative space**. Fine luminous, irregular strands wind around and through it at different depths; do not let one circular band define the whole orb.
- Noticeable asymmetry, varied strand thickness and different directions/speeds of circulation. Maintain an elegant, contained silhouette rather than noisy turbulence everywhere.
- Clear outer boundary, rich but translucent inner volume, a distinct luminous nucleus and sparse particles. Some detail must remain visible when the orb shrinks to its normal collapsed size.

### Filaments and plasma
- Several **visibly separate** interweaving filaments/plasma ribbons, with tapering thickness, irregular curvature and controlled turbulence; no uniform torus, mechanically repeated ellipse set, or equal-width concentric rings.
- Prioritise the **fine filament structure visible in V1**; add carefully restrained flowing translucent matter BETWEEN filaments, not a bright blanket that erases them.
- Layer foreground and background strands so they occlude/blend differently and convey parallax/depth. The effect must actually move through time; rotation of a flat sprite is insufficient.
- A few exceptional highlights may glow intensely, but they must not fuse into continuous white arcs. The owner must still identify cyan, blue, violet and amber independently.

### Core and colour
- Dark navy/translucent interior; preserve contrast between thin bright trails, softer plasma and the empty core. Avoid the V2 cloudy/opaque centre.
- Dominant palette: electric blue + cyan + violet; controlled pink/magenta transitions; restrained warm amber/gold ribbons, not an arbitrary RGB rainbow.
- Clamp exposure and use soft-knee bloom/controlled alpha compositing. **Automatic visual fail** if the brightest plasma merges into the wide featureless white band seen in V2.
- Colour shifts follow the engine state smoothly; do not hard-swap whole palettes or flash colours without a state change.

### Speaking waveform (a defining feature)
- A clear, horizontal **luminous waveform suspended through the centre** of the orb, with enough contrast against the dark core to remain visible during the brightest speaking highlights.
- Smooth audio envelope drives height/energy; very low signal falls back to a quiet line rather than randomly spiking. The surrounding plasma reacts gently to envelope peaks.
- The waveform may use layered glow and light diffraction, but must remain a waveform, not a chunky equaliser overlay. It should appear to exist INSIDE the energy volume.
- Accept a test audio file as input and prove the waveform follows real audio playback; simulated speech mode is useful for testing but NOT sufficient for final acceptance.
- No facial animation, mouth shapes, blendshapes or lip-sync subsystem.

## 3. Motion, UX and accessibility

**Collapsed / idle:** small, lower-right by default; slower filament flow, low light output, sparse particles, faint core, no visible large waveform unless listening/speaking. No bouncing, bobbing for attention, forced speech or regular unsolicited notifications.

**Listening:** soft cyan focus and a clear bounded input-response cue; never imply microphone capture when none is occurring.

**Thinking:** orderly internal violet/blue current and subtle tightening of the field, not frantic spinning or constant alarming flashes.

**Speaking / expanded:** smooth diameter increase, richer plasma, purposeful colour blending, waveform from actual audio, restrained outer reaction to speech. On speech completion, ease back to quiet idle rather than snapping.

**Success / warning / unavailable:** short, purposeful confirmation accent (teal); warm attention accent (amber/red, never flashing continuously); muted low-energy presentation for unavailable. Text is the accessible companion to any colour cue.

- State transitions: visually smooth, reversible, interruption-safe, no reinitialisation flicker.
- User can independently move, minimise/hide or mute the presence and suppress optional speech bubbles. Provide reduced-motion mode and a static/text-only fallback.
- Default collapsed location is lower-right, but the user can drag the orb anywhere. Do not steal keyboard focus while animating or cause editor controls to miss input.
- Planned Studio integration needs a **real detachable native top-level window** for avatar and chat on a second or third monitor, independently movable, optionally snapped/docked, with saved layout, DPI support and off-screen recovery. This cross-monitor behaviour is an RA-02/RA-03 integration gate, not a fake promise based on canvas dragging.
- Orb and chat share ONE conversation/state/voice controller; the visual renderer cannot become a separate AI or authoring stack.

## 4. Real rendering architecture (implementation guidance, not an excuse to redesign)

- Implement actual frame-to-frame rendering using a testable GPU path. A native Windows renderer or a genuinely efficient shader-based standalone proof is acceptable for the **look lock**; don't equate a browser screenshot with a finished desktop runtime.
- Prefer a small number of intentional render layers: **dark volumetric-looking core -> inner flowing currents -> spatial filament/ribbon paths -> sparse particle motes -> central waveform -> tightly controlled bloom/composite**.
- Use coherent procedural motion, temporally stable noise and eased parameter transitions. Interleaved strands should move at distinct angular speeds and phases; avoid noisy frame-to-frame randomness.
- Maintain non-blown alpha/colour: pre-multiplied alpha or equivalently correct blending; clamp HDR values before tone mapping; bloom with bright-pass threshold and knee. Inspect intermediate passes when diagnosing blowout.
- Avoid excessive independent draw calls, oversized render targets, full-screen heavy fragments, unnecessary updates and per-frame file scanning. Consider scalable quality levels rather than deleting vital design features.
- Renderer must animate without network access, an LLM, Unreal, Unity or external asset licences. Audio capture requires explicit user action; local audio-file testing works offline.
- Keep the prototype separate from the active Renegade Studio/game source until its visual and performance gates pass. Reuse bridge/services at the future integration seam, do not further inflate StudioApplication.cpp.

## 5. Performance budget and instrumentation — proposed acceptance thresholds

These are **engineering targets to verify on the owner's RTX 4070 Ti**, not claims about the current prototype. Capture true frame presentation times as well as CPU/GPU timing: the V2 screenshot reported **10 FPS** despite an approximately **3.2 ms reported draw time**.

| Metric | Collapsed / idle target | Expanded / speaking target |
|---|---|---|
| Visual smoothness | Quiet 30 FPS update budget acceptable, smooth user dragging | At least 60 FPS at normal test resolution; 95th-percentile presented frame interval at or below ~16.7 ms under uncontended test conditions |
| GPU elapsed time attributable to orb renderer | <= 1 ms per updated frame | <= 2 ms per updated frame |
| CPU render/update time attributable to orb | <= 1 ms per updated frame | <= 2 ms per updated frame |
| Incremental GPU memory (working targets) | <= 128 MiB | <= 128 MiB |
| Studio interaction | No perceptible input lag; idle editor FPS decrease <= 2% after controlled comparison | No perceptible input lag; active editor FPS decrease <= 5% after controlled comparison |

- Measure on the **same build, same scene, same GPU/driver, same resolution and focus state**, with warm-up before capture. Collect averages, p95 frame intervals, GPU timestamps and explicit process-level CPU/GPU/VRAM changes across idle and speaking.
- Measure a minimum 5-minute steady speaking/active run and 15-minute collapsed soak. Report maximum frame stalls, resource drift and any sustained thermal/power throttling.
- The active Studio currently has **Full Page Heap** enabled for its executable from an earlier investigation. That setting is paused pending owner approval: **DO NOT toggle it** as part of avatar testing. Keep performance comparisons controlled and record Debug/Release and verifier status.
- If the proof remains near **10–12 FPS**, it fails regardless of screenshot quality. Do not treat a low shader-only draw time as proof of good performance.
- If a budget is infeasible without destroying appearance, report the conflict to the owner. Do not silently relax these thresholds or fake a benchmark.

## 6. Evidence and comparison procedure

Produce all the following from the **working renderer**, not from an image generator:
1. **Idle screenshot + 15-second captured animation** with the orb at default collapsed size.
2. **Speaking screenshot + 15-second animation** at the approved expanded size, including waveform and colour transitions.
3. **Identical-size V1 and V3 side-by-side** on both a dark background and a realistic dark Studio-like UI background. Include the owner-approved concept art in the review set.
4. **Audio demonstration:** load a local voice clip; record waveform synchronisation, silence handling and stop/interruption.
5. **Performance sheet:** the budgets above, version/driver/resolution, actual presented FPS (not just simulated FPS), frame time percentiles and CPU/GPU/VRAM; include failures honestly.
6. **Interaction demonstration:** collapse/expand, start/stop audio, drag, focus safety and reduced motion. For cross-monitor integration later, independently drag chat and avatar to different monitors, save/reopen layout and test unplug recovery.
7. **File/build evidence:** source files, exact launch instructions, dependencies and a repeatable test command; recorded failures and limitations; no production/release claim from a local demo.

### Visual rejection triggers

- Main orb silhouette dominated by one bright white ring.
- Opaque or muddy ball with no readable dark core.
- Mostly mechanical wireframe hoops with little organic energy flow.
- Central waveform too faint to be immediately visible while speaking.
- Jitter, harsh palette switching, incoherent flash/bloom, or obviously flat layered graphics at normal size.
- Real-time performance far below the target, or materially worse editing responsiveness.
- Looks attractive in a still but unconvincing when viewed moving.

## 7. Implementation sequence — one focused attempt

**V3.0 | Freeze references:** Copy V1 into an unmodified comparison folder; archive V2 as rejected. Collect accepted concept artwork and fixed review capture settings.

**V3.1 | Core appearance:** Build filament/ribbon paths, inner dark core and low-opacity flow. Test static frames and motion without bloom; ensure colours remain separable.

**V3.2 | Controlled polish:** Add selectively graded glow, depth cues and sparse particles. White-blowout regression check before advancing.

**V3.3 | Voice and states:** Integrate existing audio envelope/waveform behaviour, state transitions, collapse/expand and drag without feature creep.

**V3.4 | Profile and repair:** Measure whole-app presentation and GPU/CPU/VRAM, optimise any costly layers; run the capture/soak tests.

**V3.5 | Owner look-lock review:** Present V1 vs V3 side by side, with performance evidence. Record the **owner's actual choice** in the roadmap. **No V4 design programme** is implied or authorised by this contract.

## 8. Owner decision and look lock — 2026-10-13

- [x] V1 artifact was preserved in the V3 comparison package (retain original, no edits to V1).
- [x] V3 was delivered as functioning standalone HTML code, rather than generated concept art.
- [x] V1 and V3 side-by-side running-render stills were provided; owner explicitly appreciated this comparison.
- [x] Owner visually rejected V3: **"the orb looks nothing like the concept"**.
- [x] **Owner-selected visual baseline: V1 — LOOK LOCKED.** V2/V3 are not approved production looks.
- [ ] Full GPU/CPU/presented-frame budget acceptance on owner hardware: **OPEN**. "Performance is as expected" does not specify or establish a passing measurement.
- [ ] Final locked V1 artifact captured into repository with immutable hash/commit: **OPEN**. Original download exists outside the repository; do not invent a source-control commit.
- [ ] Native Studio integration, RA-02 separate cross-monitor windows, RA-07 optional voice, audio-driven waveform, reduced-motion modes and packaged release: **OPEN**.

**Implementation instruction:** Progress with the **V1 animated look** as the reference implementation. The spectacular concept art remains aspirational for **carefully scoped pre-release polishing** that does not replace the selected silhouette and energy language. Do not initiate V4 or claim V3 visual acceptance. Look approval does **not** mean feature or performance acceptance.
