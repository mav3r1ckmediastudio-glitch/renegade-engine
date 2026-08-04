# Asset Import Review V1 — Resume Notes

_Last session end. Read this first when resuming._

## Where things stand

**Committed (stable rollback point):** `15c93e6` "Phase4: Asset Import Review V1 (Stages 2-3)"
on branch `phase4/asset-thumbnails-v1` (parent `619e169`). This is the **box** reference
build and is the safe base.

**Uncommitted working tree (this session's extra work):**
- Silhouette **billboard** reference (cyan, camera-facing quad) replacing the box.
  Texture `Studio/assets/reference/human_silhouette.png` (staged to `Content/reference/`).
  Loaded once at startup in `Load()`; billboard yaw updated per-frame in
  `UpdateImportReviewReferenceBillboard()`. Mirror bug FIXED (yaw reversed).
- The billboard **compiles and renders correctly**, BUT the device-lost bug (below) is
  still present, so this is intentionally **not committed** yet.

To return to the stable box build: `git checkout -- Studio/src Studio/CMakeLists.txt`
(and delete `Studio/assets/reference/human_silhouette.png`), or just run the committed
build. Do NOT `git add .`.

## The open bug: DX12 "Device Lost on Present" (0x887a0001) on Finish

Confirmed from `BUILD/Studio/Release/log.txt`:
```
DX12 error: commandlist ... Close() failed with 0x80070057 (parameter is incorrect)
D3D12: device removed, cause: DXGI_ERROR_INVALID_CALL
```
It fires around FINISH (save + place). It is **NOT** caused by the reference figure
(happens with both the 3D glTF human AND the trivial billboard).

Immediately before the failure the log shows:
```
Scene::Serialize took 0.00 seconds
[Warning] File not found: .../Content/Models/textures/Material_51_baseColor.png
[Warning] File not found: .../Content/Models/textures/Material_51_normal.png
```
The missing-texture warnings turned out to be a red herring, not the cause (see below).

### Root cause (fix applied, needs your build to confirm)

`CreateImportReviewLighting()`, the two review lights, and the review weather entity are
**not new relative to the committed box build** — all three landed together with the box
reference in `15c93e6`. So "box vs billboard vs 3D human" was never the actual variable;
the common thread across every repro is `FinishImportReview()` / `CancelImportReview()`
tearing down the review-only helper entities (lights, weather, reference) via
`RemoveImportReviewHelpers()` **synchronously inside the Finish/Cancel click handler**,
immediately followed by `Scene::Serialize` and the scene merge — all before the next
frame boundary. Checked and ruled out along the way: the review lights never call
`SetCastShadow(true)` (no shadow-atlas allocation), and the review weather never sets
`REALISTIC_SKY` (no atmosphere LUT generation) — so it wasn't raw GPU cost from the
lighting itself, it was entity teardown racing an in-flight command list that still
referenced those entities' GPU-visible resources.

**Fix applied (uncommitted, in this session's working tree):** `Studio/src/StudioApplication.cpp`,
both `FinishImportReview()` and `CancelImportReview()` now call
`wi::graphics::GetDevice()->WaitForGPU()` immediately before `RemoveImportReviewHelpers()`,
so the GPU fully drains the outstanding frame before the review lights/reference/weather
are freed. This is the standard fix for "destroyed a GPU-visible resource the current
frame's command list still points at."

## Next steps (in order)

1. **Build and test FINISH** with the fix in place (`RemoveImportReviewHelpers` now
   preceded by `WaitForGPU()` in both `FinishImportReview` and `CancelImportReview`).
   If the device-lost error is gone, this was the bug — commit the billboard + this fix
   together and resume Stage 4/5.
2. If it still reproduces, **enable the D3D12 debug layer** to get the exact invalid
   call: run `RenegadeStudio.exe dx12 debugdevice` (flag already wired up in
   `wiApplication.cpp:731`, no code change needed) or `gpuvalidation` for deeper
   resource-lifetime checking. That will name the exact freed resource if this fix
   didn't fully cover it.
3. Only after Finish is confirmed crash-free, separately revisit the source-texture
   preservation gap (Stage 4: `Material_51_*` not copied alongside the `.wiscene`) —
   that's a real asset-completeness bug but is no longer believed to be the crash cause.

## Remaining roadmap
- Stage 4: destination-folder field + path sanitisation, duplicate-name suffixing,
  source-file preservation (ties into the texture bug above), preserve reviewed floor
  offset on placement.
- Stage 5: thumbnail capture (clean frame) + embed in `.wiscene` via
  `wi::Archive::SetThumbnailAndResetPos` / read back with `PeekThumbnail`.
- Nice-to-haves you asked for: numeric "set height to N m" field; more review-panel
  options; the 3D glTF human via a job-thread/safe-point load
  (`ImportService::LoadReferenceModel` is staged but disabled).

## Build/run reminders
- Build: VS 2022 dev shell, then `cmake --build BUILD --config Release --target RenegadeStudio`.
- Run: `BUILD\Studio\Release\RenegadeStudio.exe dx12` (or `vulkan`).
- Local compile loop works on this machine (Desktop Commander), no GitHub round-trip needed.
