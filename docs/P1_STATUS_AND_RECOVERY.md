# P1 status and recovery checkpoint

Date: 2026-10-03. Branch: `feature/p1-first-person-arms-rig`.
Implementation baseline: `51513b95e98e6f962c0052da3f92f1b8c279b280`.
Local recovery reference: `recovery/p1-baseline-20261003`.
Wicked remains pinned at `3a800b7134aafe58461093c8abb2e274d4e64033`.

**P1 is implemented in part. Placeholder framing, idle and directional movement
are owner-verified after camera synchronization repair 4dd9953. P1 is not complete.**
The source exists in four commits; crashed chats have not erased it.
This document separates source capabilities from demonstrated behaviour.

## Commit inventory

| Commit | Changes |
| --- | --- |
| f33c1e5 | View Rig foundation; transient Player-parented presentation root; primary, off-hand and two-hand support sockets; proxy geometry; foreground policy; lifecycle integration; initial tests. |
| 9b8e7ad | Stable arms asset ID in Player settings; Studio selector using command history; governed reusable-asset loading; view-model sanitization; packaging dependency and Test Level snapshot handling. |
| 0ef91cb | Synthetic governed-asset serialization/load tests and cleanup/ownership assertions. |
| 51513b9 | Native animation binding; semantic Idle/Walk/Sprint selection; guarded crossfades; movement presentation and diagnostics; animation tests. |

## What is implemented and what the evidence establishes

| Area | Source implementation | Evidence / acceptance limit |
| --- | --- | --- |
| One authoritative Player | Existing controller owns movement and collision; View Rig is its child. | Synthetic transform and ownership assertions. Native movement regression passes in Release and Debug after 4dd9953; owner confirms directional proxy stutter is gone. |
| Camera-relative rig | Pose derives yaw/pitch and eye height; Player hierarchy propagates movement. | Synthetic translation and orientation checks. These are not live rendered skin checks. |
| Both hands | Independent primary, off-hand and support sockets retain fixed-offset fallback; explicitly tagged native bones/grip transforms can bind each role. | Serialized and packaged-load regressions plus 300-frame DX12 animation proof check same-frame world matrices, camera agreement and pause. Native Hand Grips controls and command-backed asset save/reopen are implemented; production skinning/grip/IK and owner acceptance of this checkpoint remain open. See P1_HAND_SOCKET_BINDINGS.md and P1_HAND_GRIP_EDITOR.md. |
| Foreground rendering | Native Wicked foreground flag; no shadows or reflection visibility. | Component flag assertions. No rendered-pixel acceptance in the P1 tests. |
| Arms assignment | Player settings store a stable governed asset ID; selector offers imported products plus NONE and MISSING. | Source integration and settings tests. Exact-build owner UI save/reopen and Undo/Redo still need direct verification. |
| Imported view model | Instantiates reusable product, attaches its root, removes Character/physics components and replaces proxy geometry. | Synthetic packaged-product round trip, sanitization and cleanup. A correctly posed real skinned asset is not yet accepted. |
| Animation ownership | Authored semantic metadata takes priority; name inference is a fallback when authored assignments are absent. Root motion is disabled for accepted movement clips. | Synthetic request/state checks. Idle/Walk/Sprint motion and fades on actual rendered arms still need inspection. |
| Crossfades | Native clips with matching target/path coverage blend; incompatible coverage has a guarded switch path. | State/weight assertions. Not a blanket guarantee of smooth transitions across arbitrary arm assets. |
| Test Level snapshot | Copies governed arms inputs through the existing snapshot workflow. | Source implementation and related regression tests. A real arms project launched from Studio is still unverified here. |
| Build Game | Arms dependencies and packaged reusable-asset resolution are implemented. | Synthetic manifest/product loading. No independent exported real-arms game acceptance. |
| Lifecycle | Spawn/despawn/reset integration is present; movement presentation receives zero dt when paused. | Synthetic lifecycle and source tests. Direct pause/reset/relaunch visual testing remains outstanding. |

## What P1 does not yet provide

- Accepted first-person pose, orientation, scale, materials and camera framing.
- A dedicated creator workflow for view-model alignment/offset/FOV tuning.
- Accepted production grip/IK. Creator-facing hand-bone/grip assignment now has
  a native Studio editor and automated save/reopen/Undo/Redo/rollback evidence;
  this is not a production arms or combat acceptance claim.
- Production arm animations or accepted subjective animation quality.
- Fire, reload, equip, directional melee, bow, crossbow or spell actions.
  These belong to the later P2-P10 combat programme; a proof Attack clip does
  not implement a Player attack.
- Owner-verified Test Level and independent Build Game parity with real arms.

## Why the previous Runtime experiments are not acceptance evidence

The earlier test scene included a world-space arms Character in addition to
the first-person asset, and a finite floor. A live position capture placed the
camera below the floor's vertical extent and outside its Z extent. This establishes
a bad view of that fixture at capture time, not a proven root cause in rendering.

The first no-arms comparison changed startup_scene but left Story Flow enabled;
Story Flow still selected the arms scene. That comparison was invalid.

Several Blender exports changed orientation, scale, hierarchy/rest transforms,
and animation-source relationships. None received owner acceptance. A 180-degree
rotation about Blender Y also flips its vertical Z direction, so it cannot be
treated as a simple upright yaw correction without examining the coordinate basis.

The subsequent clean experimental fixture also produced implausible live
positions and incorrectly framed arms. Its cause remains unresolved.
Neither that fixture nor the new experiment named p1-correct-arms is accepted.
The earlier explanation that orientation alone caused all observed failures
was too strong; the evidence does not isolate a single cause.

Prior import tests and successful asset_loaded diagnostics establish loading.
They do not establish visible, correctly oriented first-person arms.

## Recovery performed

- Preserved the temporary Runtime diagnostic source and Git patch under ignored
  `BUILD/renegade/p1-recovery-20261003-2217/`.
- Restored RuntimeApplication.cpp to the implementation baseline. Foreground
  rendering is enabled as in that baseline; no diagnostic toggle remains.
- Preserved all experimental BUILD assets/evidence. They are not release inputs.
- Left the original owner arms source unchanged.
- Rebuilt baseline Runtime and P1 tests; results are recorded below.
- No new Runtime window was launched during this recovery.
- No P1 gate, visual quality claim or packaged parity claim is marked accepted.

## Baseline validation

| Check | Result |
| --- | --- |
| Runtime Release build | PASS, exit 0 |
| Runtime Debug build | PASS, exit 0 |
| P1 test executable Release build | PASS, exit 0 |
| P1 test executable Debug build | PASS, exit 0 |
| P1 Release tests | 2/2 PASS; 0.17 seconds |
| P1 Debug tests | 2/2 PASS; 0.17 seconds |
| Related Release regression | 9/9 PASS; 0.39 seconds; includes the two P1 tests |
| Runtime/EngineBridge/Studio/Tests source vs baseline | No diff |
| Wicked source / pin | Clean and unchanged |
| Visual first-person gameplay | NOT ACCEPTED; recovery does not claim a fix |

Existing MSB8029 intermediate/output-directory warnings remain.

Baseline Runtime SHA-256:
- Release: `2DCBBB2F8F618A5D322E93163C73F8F5770A6A3EEB7E6EAA22A7330876554053`
- Debug: `FC37270F9C004C543EEEDDAF0739B40DCC449F55387F89723780C76574764E75`

These are local incremental builds, not clean-checkout CI. The P1 test executable
uses synthetic Scene/animation/product fixtures; it does not run a rendered
first-person gameplay session or prove real-asset animation quality.

## Exact commands

From repository root, using the installed VS18 BuildTools MSBuild and CTest.
For Configuration=Release and then Configuration=Debug:

```text
MSBuild BUILD/renegade/Runtime/RenegadeRuntime.vcxproj /m:2 /nologo /verbosity:quiet /p:Configuration=<configuration> /p:Platform=x64 /p:BuildProjectReferences=false
MSBuild BUILD/renegade/Tests/RenegadePlayerViewRigTests.vcxproj /m:2 /nologo /verbosity:quiet /p:Configuration=<configuration> /p:Platform=x64 /p:BuildProjectReferences=false
ctest --test-dir BUILD/renegade -C <configuration> -R RenegadePlayerViewRig --output-on-failure
ctest --test-dir BUILD/renegade -C Release -R "Player|ReusableAsset|TestLevelSnapshot" --output-on-failure
```

Logs: `BUILD/renegade/p1-recovery-20261003-2217/`.
Executable identity must include configuration and SHA-256, not only the
historical embedded Git revision. Baseline binary hashes are recorded there.

## Placeholder progress after recovery

The owner requested that supplied arms stop being used. A separate scene with
no imported arms now renders stable floor/landmarks and the built-in coloured
proxies extending from the lower view. Two inspected captures span 62 seconds.
See [P1_PLACEHOLDER_PROOF](P1_PLACEHOLDER_PROOF.md) for the reusable fixture
generator, evidence and remaining manual movement/look checks. This does not
establish a skinned-arms fix or complete P1.

## Controlled next task

The next implementation session must first establish one stable fixed fixture
and a proven view-model coordinate contract. Do not continue cycling FBX variants.

1. Use a disposable scene with a valid solid floor, lighting, visible landmarks
   and one safely placed Player Start. Record file hashes and exact Runtime hash.
2. Run it with NO imported arms. Verify actual Player position remains grounded,
   the floor/landmarks are visible, and normal movement/pitch/yaw work.
   Stop if this fails; asset orientation cannot explain a no-asset failure.
3. Verify simple asymmetric View Rig geometry against that same camera. This
   isolates coordinate basis and camera/rig agreement from skinning/import.
4. The owner has withdrawn the supplied-arms experiment. Do not use or re-export
   the original owner arms or GGMAX proof arms. Complete snapshot and independent
   package checks with the accepted proxies. A future skinned view model must have
   a documented rest/animation basis and agreed scope before replacing this control.
5. Bind that one derived asset to the unchanged fixture. Change one variable per
   recorded comparison; do not replace fixture and asset together.
6. Inspect rendered Idle/Walk/Sprint, transitions, pitch/yaw, wall proximity,
   pause/resume and reset. Then verify assignment Undo/Redo and save/reopen in
   the matching Studio and launch Test Level.
7. Build an independent game and verify the same real-arms result.
8. Obtain owner or independent exact-commit verification before accepting P1.

Acceptance: arms extend forward from below the camera; hands are ahead of
elbows, the view stays clear, both arms retain correct skinning/materials, and
camera-relative framing stays stable during movement and look changes.
The bounded next outcome is a stable visible rig, not more combat features.
