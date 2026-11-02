# Coherent-UMA upload-buffer recovery core decision — 2026-11-02

## Trigger and independent reproduction

Studio Debug CI on recovery parent `7896ee62a81a2aaf92a24d91710bc29253ee6d30`
failed the default-361 river proof during terrain render warm-up. Its adapter was
Microsoft Basic Render Driver; the assertion was DX12 CreatePlacedResource in
the GPU buffer path. The local hardware Release suite had passed 304/304, so that
success did not establish hosted software-adapter correctness.

An independent native DX12 proof on DESIGN queried WARP: UMA=1,
CacheCoherentUMA=1. A placed buffer in an UPLOAD heap with
ALLOW_UNORDERED_ACCESS returned E_INVALIDARG (0x80070057). The same buffer without
that flag was created and CPU-mapped successfully. This uses direct HRESULT
checks, not Wicked's assertion wrapper.

Pinned wiRenderer.cpp SuballocateGPUBuffer selects UPLOAD for coherent UMA, but
unconditionally adds UNORDERED_ACCESS to its backing buffer. Native mesh
general-buffer aliases use vertex/index/shader reads and do not request a UAV.
DX12 upload resources must stay in GENERIC_READ:
https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_heap_type

## Bounded core patch

An EngineBridge adapter cannot change the private global renderer allocator's
descriptor. Do not disable the adapter capability, shrink the default terrain,
skip the proof, suppress assertions or reduce rendering quality.

`cmake/WickedUmaUploadFix.cmake` generates a Windows build copy of the pinned
`WickedEngine/WickedEngine/wiRenderer.cpp`. It adds UNORDERED_ACCESS only when
the allocator selects DEFAULT usage. UPLOAD keeps vertex/index/shader access,
the existing UMA memory strategy, CPU mapping, alias offsets and block sizes.
DEFAULT keeps its existing UAV support.

Both WickedEngine_ext_shaders and WickedEngine_emb_shaders use the generated
file with their existing PCH/include configuration. The submodule source and
gitlink remain unchanged. Configure fails unless the exact source anchor occurs
once. Unchanged generated text is not rewritten.

## Protecting checks and reproduction

- RenegadeGpuUploadBufferTests independently checks the rejected old DX12
  descriptor, corrected descriptor, production suballocator, preserved DEFAULT
  UAV support and native mesh-style aliased buffer creation. On coherent UMA it
  also verifies uploaded bytes through the mapped child.
- RenegadeRiverDefaultTerrainRecovery retains every native 361-chunk generation,
  linking, locality, history, mask, external-edit refusal, readback and save/reopen
  assertion. Its safety limits and bounded timeout are unchanged.
- Rebuild RenegadeStudioCI and run complete CTest in Release, plus all four
  required GitHub jobs on the final source. Actual packaged Studio/export/runtime
  and extracted-ZIP checks remain required.

Initial direct DX12 negative/positive proof passed on DESIGN. Production-target
preflight, successor exact-head CI and packaged acceptance are recorded in the
PR/VALIDATION receipts; they are not implied by this decision.

## Future upstream integration

Conflict area: SuballocateGPUBuffer descriptor construction and the renderer's
split embedded/external shader build. Re-evaluate this overlay when the pinned
allocator changes. Propose the invalid DX12 upload/UAV combination upstream,
with the native reproduction and adapter architecture values. No upstream sync,
release-gate closure or main merge is authorised here.
