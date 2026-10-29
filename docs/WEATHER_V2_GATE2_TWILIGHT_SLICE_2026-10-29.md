# Weather V2 Gate 2 — Twilight & Moon-Cloud Lighting First Slice
**29 October 2026 — developer GPU candidate, NOT owner visually signed off.**

## Gate status and isolation
- Gate 1 clouds and Movement Speed (75 FPS at 8 in isolated DX12 Studio) were explicitly visually signed off by owner and **remain untouched** in `C:\Users\paulw\source\repos\renegade-cloud-formations-g1-20261028`.
- Gate 2 isolated path `C:\Users\paulw\source\repos\renegade-weather-g2-20261029`; branch `codex/weather-v2-g2-atmospheric-20261029`, root HEAD `150eca4b761d3f62f122df3ab5e8536ec084f926`, accepted river-enabled Wicked pin `ceb816b844b4ab2fa56a159ac60a4f65c931bdfc`.
- 114 accepted G1 root+Wicked changed/untracked source files copied byte-for-byte with SHA-256 checks. See `docs/WEATHER_G2_G1_SNAPSHOT_MANIFEST_2026-10-29.json`. This G2 worktree is *dirty by design*, reflecting the accepted G1 source plus G2 changes. No commit, push, PR or merge; Gates 3–5 and Snow V2 excluded.

## Direct problem reproduced
The accepted Gate 1 `Tools/night_sky/renegadeMoonCloudLighting.hlsli` switched its sole direct cloud light from Sun to Moon at solar Y=-0.09. Fixed-camera, frozen-weather, 80% Cumulus full-Moon GPU study exposed sudden lunar cloud illumination: as Sun fell beneath horizon, cloud/sky average brightness in sampled non-central regions leapt:
- sunY=-.103 (18.3): **0.25**
- sunY=-.136 (18.4): **5.41**
- sunY=-.170 (18.5): **12.69**
- sunY=-.203 (18.6): **14.31**
These are test-scene pixel values (0..255), not physical lux. Baseline capture at `BUILD/g2-twilight-baseline-dx12/` with `g2-twilight-baseline.csv`, 40 Moon-on/off cases, selected 960x540 camera; contact sheet `g2-baseline-twilight-contact-sheet.jpg`.
**Do not interpret sunrise 6AM or sunset 18h as a claim about true location/season**; Sun Y is directly recorded.

## Implemented G2 first slice
Authored source `Tools/night_sky/renegadeMoonCloudLighting.hlsli`, SHA-256 `8EA9BC0E14ED498651721C1FDE4D8A9AADB4FD97C3F924705367218D0B9109CC`:
- Sunlight unchanged with solar Y >= -0.04.
- Outgoing direct Sun intensity fades smoothly over Y=-0.04..-0.10; keep true solar direction while it remains nonzero.
- Select Moon only once solar direct contribution is gone. Independent Moon light eases from zero at Y=-0.10 to full Gate 1 authored moon intensity at Y=-0.30.
- Deep-night full Moon irradiance, phase response, above-horizon control, source direction, cloud self-shadow and render path unchanged; direct Moon/Sun directions are **never** incorrectly interpolated. No second raymarch or shadow pass; no new GPU ABI/scene controls.
- For Moon disabled/new/below horizon the direct sky source is zero after solar fade; native atmospheric ambient remains independently rendered.
- The initial slower lunar candidate `BUILD/g2-twilight-candidate1-saved.hlsli` made clouds too dark for too long; **REJECTED** after actual GPU comparison. Final candidate above is a separately measured correction, NOT assumed good solely from math.
- Original accepted Moonlighting shader preserved at `BUILD/g2-accepted-g1-moonlighting-baseline.hlsli` (SHA `94260CA519A2A1B7AE8135B7CEE95304E6F6A399EC1BE584751CA7C0B2BA374D`).
- `Tests/MoonlitCloudSourceContract.cmake` updated to assert the new source gates. `Tests/NightSkyGraphicsProof.h` added deterministic `g2-cloud-twilight-study` test-only GPU stage with a matching fixed camera, cloud settings and recorded Sun Y. No owner window or project touched.

## GPU observations
Current candidate DX12/Vulkan `BUILD/g2-twilight-candidate2-{dx12,vulkan}/` both returned exit 0 and exported 40 images each. DX12/Vulkan parity `BUILD/g2-twilight-dx12-vulkan-parity.json`: max average image difference **0.281/255**, maximum fraction of differences >5 only **0.00086**.
With the new candidate, Moon-enabled sampled cloud/sky luma changes smoothly:
- 18.3: 0.24, 18.4: **0.38**, 18.5: **2.36**, 18.6: **5.94**, 18.8: **13.04**, 19.0: **14.19**.
- Midday full/off, 18h sunset, 18.1 and 18.2, and deep night at 0h match Gate 1 baseline *pixel-exactly* in the test; at 20h minor interframe/compute differences were under 0.1 average.
Compare `BUILD/g2-twilight-before-after2.jpg`, `BUILD/g2-twilight-comparison2.json`.
Mathematical transition study `BUILD/g2-twilight-transfer-study.py` passes with maximum incremental intensity change `0.0099994` over solar Y=0.0004, no extra shadow work. This CPU study is supportive, not visual proof.

## Validation / remaining Gate 2
- New isolated G2 configure and exact G1-source baseline NightSkyTests Release build both exit0.
- G2 HLSL6/SPIR-V embedded shader rebuild and RenegadeNightSkyTests Release link exit0; `BUILD/g2-twilight-candidate2-build.log`.
- Manual `cmake -P Tests/MoonlitCloudSourceContract.cmake` passed, G2 `git diff --check` 0 and both G1/G2 Wickeds on accepted river pin. G1 shader SHA unchanged.
- G2 Studio Release, focused native moon/cloud regressions, scene save/reopen, 1440p and 75 FPS gate still require validation. The signed-off G1 75 FPS performance baseline remains a **constraint**, not a claim this G2 candidate achieves it.
- Moon actual editor horizon/camera visibility, motion, all Moon phases, dynamic sunlight, cloud shadow and fog/ocean/aurora regressions and real gameplay remain **Gate 2 open**. The visual owner must accept G2 before G2 sign-off.

## Studio review and final first-slice regression results (29 October 2026)
- **Release Studio built and linked successfully:** `BUILD/g2-twilight-studio-release-build.log`, exact executable copy `BUILD/g2/Studio/Release/RenegadeStudioWeatherG2TwilightOct29.exe`, SHA256 `4F3311FC28B39E340A1400EB0E3DB9D324A7E4506420B832531550F201FF45F8`.
- New isolated Studio review launched and was responding, PID 6716 at launch, window caption **Renegade Studio - WEATHER GATE 2 TWILIGHT REVIEW [DX12]**. It does not replace any Gate 1 preview. The screen focus was left with the user (Edge) rather than stealing it. On entering the app Project Hub, select the separately copied `Cloud Body Motion Preview` scene and open Main Level if not already loaded.
- Relaunch via `BUILD/Run-Weather-G2-Twilight-Review.cmd`; this uses `--cloud-motion-preview --dx12` and detects an already running review.
- Separate source/project copy: `BUILD/cloud-body-motion-preview/CloudBodyMotionPreview.renegade`; Main.wiscene SHA256 `A3E48B0887435F5DDE764A361E6BED90B067FBDA6A6E90D2F10BDD54CB99A960` matching G1 original.
- **Release Runtime built and linked successfully:** `BUILD/g2-twilight-runtime-release-build.log`, `BUILD/g2/Runtime/Release/RenegadeRuntime.exe`. A full user-driven Test Game/packaged verification is still pending.
- **Focused Release CTest 7/7 green**: `BUILD/g2-first-lighting-focused-ctest.log` (NightSky, Moon, ShootingStars, MoonlitCloud source, CloudFormation, CloudsV2 source).
- Additional targeted 40-scene DX12/Vulkan twilight captures and 2 API Moon visibility/phase studies have all passed. Details earlier in this document.
- Preservation: `BUILD/g2-source-preservation-audit.json` audited all 114 original G1 transferred files; five changed as intended (G2 moonlighting HLSL, 2 tests, HANDOFF, generated G2 shader), zero unexpected differences. Accepted Gate 1 binary SHA unchanged. Both G1/G2 Wicked fork checkouts pinned to accepted `ceb816b844b4ab2fa56a159ac60a4f65c931bdfc`. No commits, pushes, PRs, merges, or edits to owner windows/projects.

## Review steps
Open the uniquely named Gate 2 Studio app and its local Cloud Body Motion Preview project. Choose `Environment → Clouds` with Cumulus and ~80% coverage. Set Time of Day to **Custom**, move slowly between **18:00 and 19:00**, and enable the Moon under `Environment → Moon` (Full Moon phase, Fixed Position, above horizon ~32°, brightness 0.9). Watch the cloud edges receive faint directional moonlight gradually after sunset. Use 12:00 and 00:00 to confirm accepted daylight and deep-night behaviour. Moon direction must be in the camera view to see the disc; cloud lighting still responds when the Moon disc is offscreen.
This first slice specifically addresses the Sun/Moon transition, not a full Gate 2 signoff. **Do not call Gate 2 complete** until owner visual acceptance and real Studio/Runtime 1080p/1440p FPS, cloud shadows, moving viewpoint, fog/river/ocean/aurora regression checks. In particular, the twilight sky can be very dark around 18.2–18.3; owner judgement of atmospheric illumination is still necessary. Gate 1's signed-off 75 FPS preview must remain intact.
