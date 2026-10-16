# Night Sky / Environment visual checkpoint — 2026-10-16

Current candidate: soft green/pink curtains with sky-area Coverage.
The owner rejected the first front-facing effect, then rejected the pronounced
straight filaments introduced by the first coverage revision. Those intermediate
builds are superseded. The final shader uses height-varying, low-contrast detail
and curved folds; it retains the expanded sides/rear/upper-sky distribution.

## Build and authoring

- BUILD/night-sky-check/Studio/Release/RenegadeStudioAuroraPreview.exe
- SHA256: 61C70DC13CFDEFF35FC6E16057E90523BE15BC08D3AB8D5643389DF6DC7D8844
- Tools/night_sky/LaunchNightSkyPreview.cmd selects this executable.
- The pinned WickedEngine SHA and shared GPU ABI are unchanged.
- All nine sections live in Environment; no separate Night Sky tab.
- Zenith/Horizon colour buttons open the native Wicked colour wheel. Full sRGB
  gamut maps proportionally to the 0–0.1 linear night range without channel clipping.
- Coverage expands a local display into broken outer curtains around the viewer
  and another sheet higher overhead. Periodic coordinates avoid a rear seam.
  Clear gaps remain; maximum coverage is not a solid full-sky overlay.
- Existing metadata, commands and slider Undo/save semantics are unchanged.
  The guard now accepts authored saved settings instead of exact fixture values.

## Actual evidence

The separate Studio executable was launched and the standalone preview inspected.
During the session all nine disclosures were expanded/collapsed; native daylight,
dusk, dawn, midnight, stars, night gradient and aurora were inspected onscreen.
Native colour preview/Cancel, Apply, Undo/Redo, Save and Reopen were inspected.
UI evidence lives in BUILD/night-sky-check/qa-section-* and qa-final-*.
The coverage candidate was inspected in Studio (qa-coverage-environment.png).
The final softer shader is directly verified through real RenderPath3D GPU
captures on DX12 and Vulkan, not a generated mockup or CPU approximation:
BUILD/night-sky-soft-coverage-dx12 and BUILD/night-sky-soft-coverage-vulkan.
The final softer Studio executable was launched; further automated clicks were
stopped because concurrent desktop interaction made native panel targeting unsafe.

GPU captures cover day, dusk, dawn, midnight, native stars, gradient and animation;
Coverage 0/30/60/100 looking front/right/back/left and upwards. The final soft
front/rear/upper-sky frames were inspected directly. Day-native and day-extensions
DX12 frames are pixel-identical. Animation frames differ visibly after 120 frames.
Full HLSL6 and SPIR-V shaderdump rebuilt consistently with the executable.
Native ABI assertions retain sky_exposure at byte 136. Earlier DXC reflection
independently agrees with the native ABI.

Final source/native contract sweep: 58/58 pass. Final preview disk guard passes,
including all colour channels, speed, coverage and other hydrated renderer fields.
Timing CSVs record serialised CPU+GPU work in an empty 960x540 scene amid desktop
and compiler load; they are not representative complex-scene GPU benchmarks.

## Open acceptance and session limits

Owner artistic acceptance remains open. Complex cloud/fog/reflection scenes,
representative high-resolution performance, standalone packaged Runtime and
independent exact-head release review remain unverified. Final softer Studio
scene interaction still needs an uninterrupted native pass; the final shader's
DX12/Vulkan rendering itself is visually verified. No full release/gate closure.

A late automated native click hit New Clip after the active panel changed,
creating an unsaved empty animation clip in the disposable preview. Automation
was stopped and the owner was informed to Undo it. No main project was changed.
No main-repository edits, resets, merges or pushes were made. Only the isolated
feature tree and disposable preview were changed.
