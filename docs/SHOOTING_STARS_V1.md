# Shooting stars inside Stars — development checkpoint

Approved 2026-10-20: put occasional shooting stars in the existing Environment >
Stars disclosure. No new Environment heading, tab or workspace. Native stars
and Milky Way intensity remain in that section. An enable checkbox is always
present; when enabled it reveals sightings/minute, brightness, trail length and
an explicit preview button. Disabled controls consume no blank inspector rows.

Sightings average across the entire sky, not just the current view. The default
is two per minute; .1 is rare and 120 is a meteor shower. Events use four
independently jittered schedules, varied compass bearings, downward diagonal
paths, .45–.9-second lifetimes and soft fading trails. All contributions fade
with the existing night weight and are composited before native fog/clouds.
Shooting stars are independent of native star density, Moon, aurora and galaxy.

The preview is a transient viewer-relative streak: look toward open night sky.
It does not create a scene command or change saved settings. Natural events
are world-oriented. The animation clock is ephemeral, like other sky effects;
exact meteor event positions/timing are not saved or added to Undo history.

## State and core-patch decision

ShootingStarsService.h owns independent versioned renegade.meteors.* metadata,
sanitation, capture/store/hydration and SetShootingStarsCommand. Sliders preview
runtime values and commit one Undo action per completed drag. Legacy or unknown
schemas default off. The existing Studio/Runtime load hydration restores them.

Extend the documented pinned-Wicked sky overlay with transient Weather fields
only; no upstream SHA or scene archive version change. Reuse the three reserved
ShaderWeather.night_padding floats (rate, brightness, trail degrees), and the
reserved night_alignment_padding float at offset 76 (transient preview epoch).
Their names remain for overlay compatibility. Weather offsets and total GPU
structure size stay unchanged, including sky_exposure=136 and weather size1040.
A complete consistent C++ + HLSL6/SPIR-V rebuild is still required.

The shader branches out when disabled, brightness zero or daylight. No per-frame
CPU entity creation, particle emitters, asset allocation or persistent scene
mutation. Existing Moon texture and artistically accepted sky modules remain
unchanged. Four scheduled lanes are bounded; preview is a single optional event.

## Validation plan/status

Source and implementation are in progress. Five actual sky/reflection pixel
variants compiled on DX12 and SPIR-V. The first full native build hit an MSVC
compiler process access violation while compiling offlineshadercompiler.cpp;
a serial retry built that target successfully, and the full rebuild resumed.
No source/test failure was hidden or disabled.

ShootingStarsStateTests covers disk WISCENE roundtrip, hydration, Undo/Redo,
preview independence, invalid values, unknown/legacy schemas and other sky
contributions. The disclosure test covers 1024 combinations with meteors both
disabled and enabled (2048 configurations). New test target is included in
the Studio CI aggregate/manifest, preserving the prior CI target-list repair.

Native GPU proof adds before/moving/faded/off/daylight captures, natural events
without preview, and all effects together. These and the actual native Studio
controls still require inspection before claiming a verified candidate.
No push, PR, merge or release gate closure.

## Verified candidate - 2026-10-20

The full Release build completed for Studio, Runtime and the three sky test
executables, with consistent embedded DX12 and SPIR-V shaders. The compiler
process crash above was recovered by the documented serial target retry.
60/60 selected SourceContract, NightSky, Moon and ShootingStars tests passed
(BUILD/meteors-tests.log). Five sky/reflection variants also compiled directly
for both shader backends.

Actual native GPU captures were inspected on DX12 and Vulkan. Preview frames
show a moving blue-white streak, then complete disappearance. On each backend,
meteor-faded.png equals meteor-off.png exactly, and meteor-daylight.png equals
meteor-daylight-off.png exactly. Natural scheduling, without preview, produced
visible events in 7 of 30 sampled frames over 15 simulated seconds at 120/min.
The combined capture preserves Moon, stars, Milky Way and aurora.
Evidence: BUILD/meteors-dx12/ and BUILD/meteors-vulkan/ (PNG frames and CSV).

DX12 serialized 960x540 proof timings: disabled 1.38171 ms, enabled 1.16037 ms.
This noisy short CPU+GPU measurement detects no obvious regression; it is not a
controlled GPU benchmark or a performance guarantee at production resolution.

The actual rebuilt Studio launched and opened the separate Renegade Sky Preview
project. Environment > Stars visibly contains the enable checkbox and, when
enabled, sightings/minute, brightness, trail length and Preview Shooting Star.
No additional section or workspace was added. Native evidence is in
BUILD/meteors-environment.png and BUILD/meteors-ui-observed.png. The owner was
actively adjusting this disposable preview; automation aborted on inspector
changes and preserved those edits. Native manual Undo/Redo, save/reopen and
collapse/expand acceptance remain open; their automated state/layout/disk tests
passed. Native Vulkan Studio interaction has not been manually verified.

Candidate: BUILD/moon/Studio/Release/RenegadeStudioSkyPreview.exe
SHA256: 83ADB83C326A922FB0590DF18D6E34DC569AB2E6E615A4AA0BE10C410A447487
Launcher: Tools/night_sky/LaunchSkyPreview.cmd
Project: BUILD/meteor-preview-projects/Renegade Sky Preview/Renegade Sky Preview.renegade
The separate Alpha process and main/UI Designer worktrees were not modified.
No push, PR, merge, owner shooting-star acceptance or release gate closure.
