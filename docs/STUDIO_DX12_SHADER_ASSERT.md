# Studio DX12 shader startup assertion

## Diagnosis

The Debug Studio launch failed before entering the hand-grip editor. The failure
at wiGraphicsDevice_DX12.cpp line 3939 is CreateCommandSignature, not graphics
pipeline creation. CDB with the native DX12 debug layer captured:

```text
ID3D12Device::CreateCommandSignature:
Root parameter slot (0) was not declared to hold constants.
STATE_CREATION ERROR #743: CREATECOMMANDSIGNATURE_INVALID
```

The call stack reaches StudioRenderPath::LoadGridResources through
wi::renderer::LoadShader and GraphicsDevice_DX12::CreateShader.

The pinned backend creates counted DRAW and DRAW_INDEXED command signatures for
every vertex shader, including shaders that only use direct draws. In that
creation path its root-parameter index is 0. RenegadeGridVS originally placed
CBV(b0) in slot 0. ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT alone does not meet the
root-constant requirement; the earlier shader comment suggesting it did was
incorrect. The RenegadeImGui pair had the same incompatible layout. Debug asserts
on the rejected HRESULT; Release's disabled assertion previously concealed it.

## Correction

Both stages of each Renegade-owned pair now embed matching root signatures with
RootConstants(num32BitConstants=1, b999) as their first parameter. This follows
Wicked's native b999 convention and reserves the one DWORD written by its counted
indirect signature. CBV(b0) remains the projection/grid constant buffer binding;
its root slot moves to 1 and Wicked's pipeline optimizer maps the register.

No shader calculation, arms transform, controller, project, export, backend
assertion, upstream source or submodule pointer is changed.

## Reproduction and regression

RenegadeStudioShaderDx12Proof is a manual Windows GPU target, excluded from
headless CTest. It enables debugdevice, compiles all four shipped HLSL files,
calls Wicked's real CreateShader path, and creates each shader-pair pipeline
description. This proves command-signature admission; actual viewport rendering
is checked separately in Studio.

Commands from repository root (MSBuild/cmake/ctest on PATH):

```text
cmake -S . -B BUILD/renegade
MSBuild BUILD/renegade/Tests/RenegadeStudioShaderDx12Proof.vcxproj /m:2 /nologo /verbosity:quiet /p:Configuration=<configuration> /p:Platform=x64 /p:BuildProjectReferences=false
BUILD/renegade/Tests/<configuration>/RenegadeStudioShaderDx12Proof.exe Studio/shaders
MSBuild BUILD/renegade/Studio/RenegadeStudio.vcxproj /m:2 /nologo /verbosity:quiet /p:Configuration=<configuration> /p:Platform=x64 /p:BuildProjectReferences=false
ctest --test-dir BUILD/renegade -C <configuration> -R "Phase6Gate1Player|PlayerViewRig|TestLevelSnapshot|ReusableAssetTests" --output-on-failure
```

Use a distinct Release TargetName while the owner's Studio is running. HLSL files
are runtime assets deployed under Content/shaders by the Studio post-build step;
an executable hash alone does not identify this fix.

The original grid shader reproduced #743 in the new native proof under CDB.
With the grid pair corrected and the original ImGui pair retained, the grid
succeeded and the ImGui vertex stage failed with the same error. Both corrected
pairs pass in Debug and Release, exit 0. Related CTest passes 6/6 per configuration.

Evidence logs under BUILD/renegade: dx12-shader-proof-Debug.log,
dx12-shader-proof-Release.log, dx12-shader-proof-negative.txt,
dx12-imgui-negative.txt, dx12-imgui-negative-direct.log, dx12-ctest-Debug.log
and dx12-ctest-Release.log. The original Studio stack is recorded under
p1-hand-grips-proof/dx12-assert-before.txt.

## Remaining acceptance

Native Studio startup/render evidence is recorded in HANDOFF.md after testing.
This repair does not close P1, establish production skinned-arms acceptance, or
replace independent owner verification. No push or merge is authorized.

## Native application evidence

Both Studio builds succeed, exit 0. Release uses TargetName=RenegadeStudioDx12Fix.
Debug Studio was run under CDB with debugdevice, entered the Project Hub, loaded
the isolated HandGripsProof project, rendered its floor/landmarks and analytic
grid, then opened the native Hand Grips editor and read its persisted primary
Z=0.583 setting. This is agent-operated evidence, not owner acceptance.

Screenshots: BUILD/renegade/p1-hand-grips-proof/dx12-debug-grid.png and
dx12-debug-hand-grips.png. CDB application logs: dx12-studio-Debug.txt and
dx12-studio-Release.txt in that directory. Release startup reaches the rendered
welcome screen; complete viewport/hand-editor inspection was done in Debug.

The synthetic project copies a different project's descriptor/Flow identity;
its unrelated Story Flow stable-identity error repeats in the application log.
It does not prevent the inspected viewport/editor, and is not a DX12 validation
failure. This disposable fixture is not a production project acceptance claim.
Both agent-owned debugger sessions were stopped after capture. The owner's
existing Studio session and accepted standalone export were not replaced.

Native Debug capture contains zero DX12 errors or validation breaks. Two nonfatal
#680 depth-stencil-view pipeline warnings appeared; these are separate from the
repaired #743 command-signature failure and remain recorded in the log. Release
startup capture contains no DX12 errors or warnings.

All four deployed shader files match their source hashes in both configurations.
The binaries were rebuilt on parent f737777 with this patch before committing;
their displayed source revision remains that parent.
