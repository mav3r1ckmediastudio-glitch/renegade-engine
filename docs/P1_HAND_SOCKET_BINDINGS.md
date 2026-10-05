# P1 native hand socket binding checkpoint

Date: 2026-10-03. Branch: feature/p1-first-person-arms-rig.
Status: local implementation candidate; full P1 remains open.

## Result and contract

Primary, off-hand and two-hand support sockets can follow authored native Wicked
bones or grip transforms below bones. The Player controller, interpolated
camera/rig coordinate contract and movement presentation remain unchanged.
Both governed project loading and packaged loading call the same binding operation
before removing fallback proof geometry.

An anchor declares its role with a true native MetadataComponent boolean:

| Role | Metadata key |
| --- | --- |
| Primary hand | renegade.player.view_socket.primary |
| Off hand | renegade.player.view_socket.off_hand |
| Two-hand support | renegade.player.view_socket.two_hand_support |

The marked entity must have a native transform and belong to the loaded view-model
skeleton: it is a bone listed in that hierarchy's ArmatureComponent or a grip
transform descending from such a bone. Names are irrelevant. One anchor may
declare several roles; each role may have only one anchor. Unrelated world
metadata is ignored.

The socket adopts the anchor's full world pose through native Component_Attach.
Its local transform is identity. A grip offset therefore belongs on the authored
anchor transform, not on an additional runtime skeleton evaluator. Missing roles
retain their original independent fixed offsets below the presentation root.
All bindings are validated before any socket changes. Duplicates, non-skeletal
anchors and cyclic ownership return errors; asset commit preserves proxies when
binding validation fails. No automatic bone-name guess is introduced.

Native metadata survives WISCENE serialization and entity-ID remapping.
Runtime parentage, socketTargets and bindings are transient. Native Scene update
evaluates animation before hierarchy propagation, so no late-frame socket copying
or second controller is necessary.

## Verification

- Runtime, PlayerViewRigTests and manual GPU fixture build in Release and Debug:
  PASS, exit 0. Existing MSB8029 warnings remain.
- Related Player/ViewRig/TestLevelSnapshot CTest checks: 5/5 per configuration.
- Generated two-bone asset survives WISCENE save/reload and native instantiate.
- Packaged governed rasset loader binds all three remapped anchors and strips
  gameplay/physics contamination; despawn retains only the authoritative Player.
- Headless regressions reject duplicate/non-skeletal/cyclic anchors atomically,
  exclude unrelated world tags and preserve per-role fallback offsets.
- Manual DX12 proof drives native translation AND rotation clips through Idle,
  Walk and Sprint with forward/backward/sideways movement plus pitch/yaw.
  All three socket world matrices match their anchors in the same frame.
- Release and Debug 300-frame proofs: socket matrix error 0; maximum camera/rig
  error 2.38419e-07; actual animated local Z range 0.0592 metres; paused matrix
  and timer errors 0; three render captures each; exit 0.
- Idle/Walk/Sprint rendered captures inspected: orange/blue generated forearms
  extend from below the view; yellow primary, green off-hand and purple support
  markers stay on the corresponding authored grips.
- New Release standalone Runtime launches the unchanged accepted proxy project,
  reports DX12 STARTED and exits 0 after closing only its newly launched window.
  This is baseline startup regression evidence, not new owner controls acceptance.

Release Runtime SHA-256:
3B3E0BBDDCDD7CAB9251BCA92B7E64EE2C63B7406AAC5107D10D24513F0968AE

Debug Runtime SHA-256:
DE0D86E3F5AD45D6507BBC3934743DF7AA63789F059126CE06B20AEA53925F3D

Ignored evidence: BUILD/renegade/p1-socket-proof, with release-gpu.log,
debug-gpu.log, final CTest logs, release-standalone-bootstrap.log and generated
socket-rig WISCENE/idle/walk/sprint PNGs. Build logs use p1-socket prefixes under
BUILD/renegade.

## Exact commands

Use installed VS18 MSBuild and CMake CTest from repository root, for each
configuration Release and Debug; CL=/MP4:

~~~text
MSBuild BUILD/renegade/Runtime/RenegadeRuntime.vcxproj /m:2 /nologo /verbosity:quiet /p:Configuration=<configuration> /p:Platform=x64 /p:BuildProjectReferences=false
MSBuild BUILD/renegade/Tests/RenegadePlayerViewRigTests.vcxproj /m:2 /nologo /verbosity:quiet /p:Configuration=<configuration> /p:Platform=x64 /p:BuildProjectReferences=false
MSBuild BUILD/renegade/Tests/RenegadePlayerViewRigFixture.vcxproj /m:2 /nologo /verbosity:quiet /p:Configuration=<configuration> /p:Platform=x64 /p:BuildProjectReferences=false
ctest --test-dir BUILD/renegade -C <configuration> -R "Phase6Gate1Player|PlayerViewRig|TestLevelSnapshot" --output-on-failure
~~~

From BUILD/renegade/Runtime/<configuration>, for Release (Debug uses debug output):

~~~text
../../Tests/Release/RenegadePlayerViewRigFixture.exe --socket-proof ../../p1-socket-proof/socket-rig.wiscene
~~~

Standalone launch: newly built Release RenegadeRuntime.exe, argument
--project <absolute path to BUILD/renegade/p1-placeholder-proof/RuntimeProof.renegade>.
After ten seconds, copy executable-local Logs/RuntimeBootstrap.log to the proof
folder; CloseMainWindow on the returned process only; wait for exit 0.

## Limits and next outcome

This socket checkpoint is a native C++/metadata boundary. The subsequent Studio
Hand Grips authoring implementation is documented in P1_HAND_GRIP_EDITOR.md.
Generated rigid geometry and native animated bones prove attachment mechanics;
they do not prove skinned deformation, production hand anatomy/materials, grip IK,
weapon actions or subjective real-arm animation quality. Wicked owns animation
and parent transforms. The reference Editor uses native Component_Attach for
hierarchy authoring; no equivalent Renegade semantic hand-binding UI parity is
claimed and no new stock Editor acceptance session was performed.

The owner-approved proxy export and Studio Runtime bundle remain preserved.
Original owner/GGMAX arms are still withdrawn. Do not resume FBX experiments.

The next bounded authoring task is implemented in P1_HAND_GRIP_EDITOR.md.
Agree on one separately generated/authored skinned view model for rendered
owner acceptance after exact-build verification of the grip editor. Do not replace the accepted
control scene, skip creator exposure, mark P1 complete or push/merge this work.
