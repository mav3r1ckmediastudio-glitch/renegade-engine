# Environment / Night Sky recovery checkpoint

Code checkpoint: `642339498b094d92ffc3fee776ebe5789cec7bc9`
(parent recovery fix `45473834a7734d6894bb60688680e230e86a9bc6`).

Scope: `feature/night-sky-aurora-v1`, isolated worktree only. Base:
`a65afe12ff8f21c85286273c000860cc29857019`. No merge or push.

## Corrected authoring surface

The separate Night Sky navigation action, workspace flag and pane are removed.
Environment contains nine independently collapsible groups: Time of Day / Sun,
Realistic Sky, Clouds, Stars, Fog, Ocean, Weather / Precipitation, Night Atmosphere
and Aurora. Every existing Environment control is retained. The native Stars
slider keeps its existing weather command callbacks. Night sliders use their
existing preview/command path from Environment or a selected Weather entity.

Sun starts expanded; other groups start collapsed. Disclosure survives workspace
changes during the editor session and never modifies scene content. Hidden
children consume no rows; history/save actions follow the last section. Native
`wiGUI::Window::Update` excludes invisible children from scroll extents.
Skybox mode disables procedural night controls; tooltips explain the restriction.
A concurrent native colour-picker addition replaces the six RGB channel rows
with Zenith and Horizon colour buttons, transient previews, explicit Apply/Cancel
and one command per applied edit. It was inspected after arrival; switching to a
skybox now cancels an open picker. Its actual native usability remains unverified.

The old fixed Environment positions and unused section labels are removed.
Opening Environment also preserves the authored sun direction when repairing a
weather-only scene with a directional Sun. The Sun transform is derived from that
direction so the next native update does not restore identity/midday. This repair
and its initial test arrived concurrently during this session; it was preserved
and audited. The stronger transform-axis assertion failed: the existing pitch
formula mirrored Z and ApplySun seeded the opposite light direction. The fix uses
Wicked RunLightUpdateSystem's actual local +Y axis and checks six day/night times.
The older Sun test had asserted the incorrect seed sign and was corrected.

Other Inspector surfaces retain their existing layout. No renderer, shader,
Weather ABI, submodule pointer or upstream source change is part of this fix.

## Audit and evidence

- The old Night Sky source test required the rejected separate workspace. It now
  rejects that navigation/state and requires all nine Environment headings.
- The earlier general Inspector source contract pinned the old fixed Environment
  label position. It now checks the Inspector's Environment layout integration.
- Fresh DXC reflection of `skyPS_dynamic.hlsl` places Weather's `night_zenith`
  at relative byte 80 and `sky_exposure` at 136, matching native assertions.
  Reflection's enclosing Weather base is 640 (`720 - 80`, `776 - 136`).
- Reapplying `cmake/RenegadeNightSkyOverlay.cmake` leaves SHA-256 hashes unchanged
  for `ShaderInterop_Weather.h`, `wiScene.cpp`, `wiScene_Components.h`, `skyHF.hlsli`.
- Both shader verification scripts compiled all five real affected pixel shader
  variants successfully: DX12 and Vulkan SPIR-V. These are compilation checks.
- All 56 source contracts passed after migrating the obsolete layout assertion.

Final native regression suite: **58/58 pass**, covering all 56 source contracts,
NightSkyStateTests (512 disclosure combinations, metadata, real disk reopen,
Undo/Redo, legacy/default behavior, preservation of stars/exposure and weather-only
night repair), and SunTests (native transform agreement at six solar times).
This establishes CPU/authoring state behavior, not framebuffer rendering.

The full offline shader compiler was run with
`hlsl6 spirv shaderdump strip_reflection quiet rebuild` from
`BUILD/night-sky-check/WickedEngine/WickedEngine`, exit 0. The regenerated dump
contains 400 DX12 and 400 SPIR-V entries. Its SHA-256 is
`281ec5125a31a689e192fe1a0b90ada41da8fa5aab39f4ace8add16a67ee59dd`.

Release Studio compiled/linked after a stable-source recompile. The final Sun
repair relink was blocked by LNK1104 because a different session subsequently
launched the isolated Studio. That process was left running. The same final
objects and refreshed bridge/shader libraries were linked successfully as
`BUILD/night-sky-check/Studio/Release/RenegadeStudioRecovery.exe`:

```text
MSBuild BUILD/night-sky-check/Studio/RenegadeStudio.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /p:TargetName=RenegadeStudioRecovery /m:4
cmake --build BUILD/night-sky-check --config Release --target RenegadeNightSkyTests --parallel 4
ctest --test-dir BUILD/night-sky-check -C Release -R '(SourceContract|^RenegadeNightSkyTests$|^RenegadeSunTests$)' --output-on-failure
RenegadeStudioRecovery.exe startup-smoke
```

The existing guarded launcher still targets the earlier locked
`RenegadeStudio.exe`. For this checkpoint launch **RenegadeStudioRecovery.exe**
directly; do not treat the old launcher as exact-commit validation.

Final recovery executable SHA-256:
`d1b72f6cd1795f19a4d33e8a2a504d3a2cb29faec23521e66445098a8e13a965`.

A fresh `--fixture BUILD/night-sky-recovery-evidence/Midnight.wiscene` followed
by `--verify-fixture` passed disk reopen, midnight settings and runtime hydration.
The fixture now serializes a directional Sun at midnight, matching the normal
Environment lifecycle. This helper update also arrived concurrently and was
reviewed before inclusion. It does not constitute a rendered sky check.

The allocation-only smoke exits 0 with the **bare** `startup-smoke` argument.
It returns before creating a window; it proves neither application startup nor
rendering. An initial invocation with `--startup-smoke` was wrong: Wicked's
parser does not remove dashes. It entered normal initialization and was stopped
after 60 seconds by its known owned PID/path. It is not recorded as a passing
probe. No unrelated editor process was stopped.

Local logs/artifacts (ignored build output):
`BUILD/night-sky-recovery-build.log`, `BUILD/night-sky-recovery-source-tests.log`,
`BUILD/night-sky-recovery-dx12.log`, `BUILD/night-sky-recovery-vulkan.log`,
`BUILD/night-sky-recovery-sky-reflection.txt`,
`BUILD/night-sky-recovery-shaderdump.log`, `BUILD/night-sky-recovery-final-build.log`,
`BUILD/night-sky-recovery-sun-build.log`, `BUILD/night-sky-recovery-alternate-link.log`,
`BUILD/night-sky-recovery-final-test-build.log`, `BUILD/night-sky-recovery-tests.log`.

Commands use the installed VS 2026 CMake/CTest binaries:

```text
cmake --build BUILD/night-sky-check --config Release --target RenegadeStudio RenegadeNightSkyTests --parallel 4
ctest --test-dir BUILD/night-sky-check -C Release -R SourceContract --output-on-failure
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/night_sky/verify_wicked_sky_dx12.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/night_sky/verify_wicked_sky_vulkan.ps1
cmake -DCMAKE_SOURCE_DIR=. -P cmake/RenegadeNightSkyOverlay.cmake
```

The first build attempt was blocked by sandbox access to installed Windows SDK
metadata; retry with SDK access proceeded. Script execution policy required a
process-local invocation flag; no persistent execution policy was changed.

## Explicit acceptance blockers

Windows computer-use initialization succeeded, but `sky.list_windows()` failed:
`Computer Use native pipe is unavailable: failed to connect native pipe: The
system cannot find the file specified. (os error 2)`. Retry and kernel reset /
reinitialization produced the same failure. No framebuffer was inspected and no
new screenshot exists. The user's already-running Studio in another worktree
was identified by process path and left untouched.

Therefore daytime nonblack rendering, midnight star visibility/twinkling,
aurora animation and dawn fading, actual native disclosure/click/scroll behavior,
onscreen Undo/Redo/save/reopen, GPU performance and standalone Runtime rendering
remain unverified. No visual acceptance, release gate or completion is claimed.
Do not tune the aurora until baseline day sky and native stars pass onscreen.

Next: restore a working native desktop inspection connection, launch only the
isolated build against a disposable preview project, and execute
`Tools/night_sky/PreviewChecklist.md`. Independently verify the exact code commit
using `docs/VERIFICATION_CHECKLIST.md` before any gate closure.
