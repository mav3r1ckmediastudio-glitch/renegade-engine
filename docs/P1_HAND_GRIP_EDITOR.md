# P1 Studio Hand Grips editor

Date: 2026-10-04. Branch: feature/p1-first-person-arms-rig.
Status: local implementation candidate; full P1 remains open.

The Player inspector now exposes EDIT HAND GRIPS after choosing an available
governed first-person arms asset. The separate native Studio panel edits primary,
off-hand and two-hand support roles. Each role selects a native skeleton bone and
a position in metres plus rotation in degrees relative to that bone. NONE retains
the Runtime's default camera socket offset.

Edits use an isolated asset working copy and command history. Undo and Redo operate
on this copy. SAVE SHARED ASSET persists the asset; CLOSE / DISCARD UNSAVED discards
pending edits. Saving affects every Player using that asset. Restart Test Level
to load the new asset. The panel does not provide a live arms preview or grip IK.

## Native ownership and persistence

Studio handles widgets, role/bone choices and scalar values. EngineBridge owns
asset loading, native skeleton discovery, validation, commands and persistence.
Runtime continues using Wicked hierarchy/animation and the existing socket tags;
it gains no second skeleton evaluator or Player controller.

Bindings store canonical JSON arrays of hierarchy names instead of transient
entity IDs. Ambiguous or unnamed bone paths, missing bones, malformed settings
and non-finite/out-of-range offsets fail validation. The creator transform wrapper
is excluded from identity so reimport retains the same bone paths.

Applying settings creates isolated native grip transforms below the chosen bones,
with semantic socket metadata and identity scale. Reapplying removes only verified
isolated editor-owned grip transforms. A marker on a bone or transform with children
fails before mutation.

Save verifies the native serialized payload by reopening it in memory. Position
and quaternion orientation must survive. DirectX roll/pitch/yaw decomposition is
used consistently, including equivalent Euler angles at the gimbal limit.
The product, managed projection and registry are committed through the shared
ProjectDocumentTransaction journal. Failures roll back all three files. An editor
refuses to overwrite a product or projection changed since it opened.

The asset's hand_grips import option and provenance recipe preserve these choices
when applying a subsequent native import recipe. Reimport fails explicitly if a
selected bone is absent. It does not silently choose a similarly named bone.

## Verification and limits

The Player View Rig regression now exercises native grip offsets, validation
before mutation, editor Undo/Redo, injected failure after the second file
replacement, rollback, successful retry, asset reopen, import-recipe reapplication
and stale-save rejection. The manual DX12 fixture applies nonzero three-axis
rotation and offsets before native save/reload and Runtime binding, then drives
300 frames of native Idle/Walk/Sprint animation and Player motion.

Use VS18 MSBuild and CMake CTest from the repository root, with CL=/MP4.
Build EngineBridge first, then the dependent targets. After CMake regeneration,
start a fresh MSBuild invocation so it reads the regenerated project.

~~~text
MSBuild BUILD/renegade/EngineBridge/RenegadeEngineBridge.vcxproj /m:2 /nologo /verbosity:quiet /p:Configuration=<configuration> /p:Platform=x64 /p:BuildProjectReferences=false
MSBuild BUILD/renegade/Tests/RenegadePlayerViewRigTests.vcxproj /m:2 /nologo /verbosity:quiet /p:Configuration=<configuration> /p:Platform=x64 /p:BuildProjectReferences=false
MSBuild BUILD/renegade/Tests/RenegadePlayerViewRigFixture.vcxproj /m:2 /nologo /verbosity:quiet /p:Configuration=<configuration> /p:Platform=x64 /p:BuildProjectReferences=false
MSBuild BUILD/renegade/Runtime/RenegadeRuntime.vcxproj /m:2 /nologo /verbosity:quiet /p:Configuration=<configuration> /p:Platform=x64 /p:BuildProjectReferences=false
MSBuild BUILD/renegade/Studio/RenegadeStudio.vcxproj /m:2 /nologo /verbosity:quiet /p:Configuration=<configuration> /p:Platform=x64 /p:BuildProjectReferences=false
ctest --test-dir BUILD/renegade -C <configuration> -R "Phase6Gate1Player|PlayerViewRig|TestLevelSnapshot|ReusableAssetTests" --output-on-failure
BUILD/renegade/Tests/<configuration>/RenegadePlayerViewRigFixture.exe --socket-proof BUILD/renegade/p1-hand-grips-proof/socket-proof-<configuration>.wiscene
~~~

Release uses TargetName=RenegadeStudioHandGrips to avoid replacing the owner's
running RenegadeStudio.exe. The separate proof project and synthetic two-bone
asset are disposable fixtures, not accepted production arms or a genuine FBX
import. They do not prove skinned deformation, production grip anatomy or a real
weapon action. Original owner/GGMAX arms remain withdrawn.

The owner-approved proxy project, standalone export and Studio Runtime bundle
are preserved. Real skinned-arms acceptance and owner verification of this exact
checkpoint remain open. No P1 completion, push or merge is claimed.

## Checkpoint evidence

Release and Debug EngineBridge, Runtime, Studio and both proof targets build,
exit 0. The related CTest selection passes 6/6 in each configuration. Both native
300-frame GPU proofs with authored offsets report socket matrix error 0,
camera/rig error 2.38419e-07 metres, animated bone Z range 0.0592 metres, pause
matrix/timer errors 0, three captures and exit 0.

The rebuilt Release native Studio was opened on an isolated fixture project.
A slider changed primary Z from 0.24 to 0.583 metres; SAVE SHARED ASSET persisted
0.5829999446868896 in the governed recipe. Closing and reopening the native editor
restored 0.583 with a clean working copy. The corrected Player section layout and
absence of the scene gizmo over the panel were visually inspected. UI evidence is
BUILD/renegade/p1-hand-grips-proof/studio-reopened.png. This is agent-operated
native evidence, not owner acceptance.

At this original checkpoint Debug Studio's standalone launch hit a DX12
CreateCommandSignature assertion (wiGraphicsDevice_DX12.cpp line 3939) before
reaching the editor. Subsequent diagnosis identified incompatible root signatures
in the Studio grid and ImGui shaders. See [STUDIO_DX12_SHADER_ASSERT](STUDIO_DX12_SHADER_ASSERT.md)
and the later HANDOFF entry for the correction and native startup evidence.
The original checkpoint's compilation/headless/GPU proofs remain historical;
they did not establish Debug Studio startup acceptance.

Release Studio candidate SHA-256:
7EA283E885D82A0FE6A1188660E57F138624E6BA987F5461C02BFB7CB2A078DF

Release Runtime SHA-256:
47EFF1BDA3795FB70CFB1FDF4803AFDA11932C8AD7FEB34F6831D19326286539

Debug Runtime SHA-256:
0E1FC32C4A81C911CC74E34F01FC83583BEA0F8770870FAECFC2CA7EC182650F

These binaries were compiled from the checkpoint changes on parent 2f8662d
before the source commit; their displayed source revision remains that parent.
No exact-commit owner verification is inferred from their hashes.
