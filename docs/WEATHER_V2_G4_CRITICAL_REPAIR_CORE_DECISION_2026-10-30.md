# G4 critical regression repair — core-patch decision
Owner authorized all four repairs and a visual-test build on 30 October 2026.
Keep paired local weather branches; no push/merge or submodule pointer change.
Prior source, dirty patches and scene bytes preserved in external baseline.
Renderer change is necessary: the regression is in core cloud sampling/reprojection.
Remove the event-dependent structural reconstruction override. Integrate the
unchanged approved sheet radiance separately along the same density/transmittance
samples, resolve only transient RGB with actual phase positions, and composite
after baseline history. No new march/shadow samples, no cloud opacity/history
changes, no event-specific structural blur or clock reset. Capture/probe baseline
excludes short-lived lightning, preventing probe-history ghosts.
Keep native audio backend unchanged; thunder completion uses sample progress
rather than the callback flag initially true before OnBufferStart.
Required validation: DX12 shader compilation, exact Runtime/Studio build,
actual waveform output, multi-frame dark-gap captures and measured Storm costs.
This record is an implementation decision, not owner visual signoff.

Hardware proof found decoded output ends at 214560 samples while WAV metadata can be longer. Add a Windows-only native GetQueuedBufferCount wrapper over XAudio2 GetState; use BuffersQueued rather than callback-owned IsEnded or exact sample-count equality for retirement. This does not alter generic voice lifecycle or callbacks.

Measured baseline Storm all-sky cache cost ~4.9ms GPU versus ~4.4ms visible camera cloud raymarch. Budget the separate hemispherical cache across four row slices, preserve full initial fill, advance checkerboard phase only after all four slices, and compensate EMA for elapsed update count. No visible-camera sample count, density field, rain/snow quality or cloud animation-clock changes. Validate measured cost and fallback/reflection appearance after this independent patch.

Cache slicing A/B showed no material timing benefit and was REVERTED before delivery. No capture cadence/resolution/EMA change is retained. Retained performance correction only skips mathematically disabled native layers and native density-validity work whose coverage cannot pass its existing threshold; independent Renegade formation functions, density values and all sample budgets are unchanged. Diagnostic frame/timestamp CSVs now buffer in RAM and write only at shutdown, avoiding capture-induced disk stalls. The camera At-direction correction supersedes the initial report's unverified orientation assumption.

Actual Studio loopback remained silent despite advancing source samples and native gains 1. CoreAudio inspection confirmed active Studio session gain 0 on the correct endpoint; restoring only that session to gain 1 produced live thunder (loopback peak 0.120985). StartupMediaFoundationPlayer used MR_POLICY_VOLUME_SERVICE/IMFSimpleAudioVolume and faded that shared process session to zero. Replace with MR_STREAM_VOLUME_SERVICE/IMFAudioStreamVolume channel gain so startup fading cannot silence Wicked/XAudio2. Preserve video/fade visuals and respect the Windows application-session gain. This supersedes the original report's ranking: session-wide startup mute is confirmed, voice startup race is an independent confirmed defect.
