# Moon authoring v1 — development candidate

Scope agreed 2026-10-20: a visible lunar disc, eight named phases plus continuous
phase adjustment, angular size, brightness, automatic Time of Day motion, and
fixed positioning. Directional moonlight/shadows and automatic phase progression
are deferred. This is an artistic orbit, not a real astronomical calendar.

## Creator controls

Environment > Moon is the tenth collapsible Environment section. Enable the moon,
select a phase or use Phase (0/1=new, .25=first quarter, .5=full, .75=last quarter).
Size is the angular diameter in degrees (.1–12); brightness affects the disc only.
Follow Time of Day places the moon opposite the sun, with a heading offset. Its
selected phase does not change. Fixed Position exposes heading and elevation;
the latter is disabled in automatic mode. Below-horizon positions are hidden.
Controls are unavailable with texture skyboxes.

The final candidate uses NASA SVS CGI Moon Kit lunar albedo, shared by every
phase and embedded into Studio/Runtime. Source credit and SHA-256 are recorded
in EngineBridge/assets/moon/README.md. The initial procedural surface was
rejected during internal visual inspection as too artificial.
The disc covers background stars even on its unlit limb at night. Existing
atmospheric gradient and aurora composite in front, as do native clouds and fog.
Daytime moon contrast is reduced; native daytime sky is unchanged with moon off.

## State and renderer boundary

MoonService.h owns versioned renegade.moon.* Environment metadata, sanitation,
capture/store/hydration and SetMoonCommand. Slider previews affect transient
renderer state; one committed drag is one Undo step. Moon metadata is independent
of renegade.night_sky.* and defaults disabled for legacy/unknown schemas.
The existing Studio/Runtime scene hydration entry point restores both services.

Core patch decision: extend the existing documented pinned-Wicked overlay, not
the submodule revision or Wicked scene archive format. Append two float4 registers
to ShaderWeather: moon_parameters (phase, diameter, enabled brightness, texture descriptor)
and moon_orbit (heading, fixed elevation, automatic flag, reserved).
The fourth moon_parameters component holds the transient bindless albedo index;
MoonSurface.cpp loads a single embedded resource only when GPU rendering is
available and the moon is enabled. Headless metadata tests never create it. CPU Weather
fields are transient. Previous field offsets, including sky_exposure=136, remain
unchanged. Moon fields are at 1008 and 1024; ShaderWeather becomes 1040 bytes.
All C++ consumers and the complete HLSL6/SPIR-V package must be rebuilt together.

The modular renegadeMoon.hlsli runs in the existing realistic sky path before
height fog and native cloud compositing, including environment captures. A
uniform brightness-zero branch avoids surface work when disabled. No per-frame
CPU scene mutation or new light entity is needed to move the disc.

## Validation status

Implementation/build validation in progress; no owner-ready or release claim.
The first full package compile caught unsupported shader derivatives in compute
sky callers. Derivative-free edge smoothing replaced them. Five real pixel shader
variants now compile on both DX12 and SPIR-V, and DXIL reflection confirms moon
register offsets relative to the weather block (1008/1024; exposure still 136).
Full shader package, Release Studio/Runtime, native controls, phase/motion GPU
frames, save/reopen, Undo/Redo and legacy visuals still require final checks.

MoonStateTests exercises independent metadata, actual disk WISCENE roundtrip,
hydration, command Undo/Redo, legacy/unknown schemas, invalid values, automatic
rise/set and fixed position. NightSkyStateTests now checks all 1024 disclosure
combinations. NightSkyGraphicsProof additionally captures all eight phases,
small size, daylight, below-horizon and tracked orbit positions on actual GPU.
RenegadeMoonTests is included in the Studio CI aggregate and build manifest.

No push/PR/merge is authorized by this moon implementation task. Active shared
UI/assistant and spline-water worktrees are not modified.

## Verified textured candidate checkpoint

Release Studio and Runtime built successfully with the full HLSL6 + SPIR-V
shader package. Final focused CTest: 59/59 pass (BUILD/moon-final-tests.log).
GPU proofs completed on DX12 and Vulkan (BUILD/moon-final-dx12 and
BUILD/moon-final-vulkan). All eight phases, small size, below-horizon, daytime
and tracked automatic orbit frames were captured. Full/quarter/crescent phase
luminance ordering passed on both backends. Moon-off daylight native and
extensions frames were pixel-identical (maximum RGB delta 0).
Visually inspected the final full moon, eight-phase sheet, Vulkan last quarter
and daylight moon. Updated source-credit packaging configure also passes.

Studio: BUILD/moon/Studio/Release/RenegadeStudioMoonFinal.exe
SHA-256: F3E62F58C3EB0F140314FF8F399F288C1CBA71E4A771897BE6242F581B7DD35A
Runtime: BUILD/moon/Runtime/Release/RenegadeRuntime.exe
SHA-256: E75DA58821F06D257B2B40C4325166DE9B868D7046B535475644EC3B1A7910A8
Launcher: Tools/night_sky/LaunchMoonPreview.cmd
Project: BUILD/moon-preview-projects/Renegade Moon Preview/Renegade Moon Preview.renegade
Evidence: BUILD/moon-final-phase-sheet.png and backend directories above.
These binaries are a local development candidate, not a pushed/released build.

Actual Studio Environment interface was inspected with all ten headings and
Moon controls present. The owner interacted with the earlier procedural binary
and reported the feature was really good, with texture shadows as the only
reported issue. That running binary was BAC08E9244E5BDD57564C72E4F64AE0705CE7ED92C26F237BD45A61544138349.
It still holds unsaved owner edits: never force-close or overwrite it. The
textured executable is separate. A backup of the saved preview scene exists
under BUILD/moon-owner-settings-backup; this is not a capture of unsaved memory.
Input guards aborted when the inspector changed during concurrent interaction.
No arbitrary clicks were sent after those guard failures.

Remaining: final textured native editor Save/Reopen + Undo/Redo manual pass,
owner visual acceptance of the new surface, standalone Runtime visual check,
broader cloud/fog/reflection and controlled performance validation, independent
exact-head verification. No release gate closure, push or merge.
