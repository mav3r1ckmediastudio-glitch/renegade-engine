# PR #197: renderer composition regression and recovery

Status at preparation: corrective local validation; hosted checks are a separate gate. PR #197 remains draft until all four jobs pass on its current commit.

Implementation: `affe8d14ecde5e92302658368dc841b45c3f09e8`. Local configure stamp: `585407a260448de34773ffef5b51a1c8f58526e9`, with the exact corrective file hashes recorded in the evidence JSON.

## What went wrong

The wireframe generator read the pinned renderer and wrote `RenegadeWickedFix/wiRenderer.cpp`. The existing UMA generator had already written its memory correction to that same file. The wireframe step therefore removed the accepted rule that keeps upload heaps read-only while retaining unordered access on DEFAULT heaps.

DESIGN's NVIDIA preflight exercised the DEFAULT path. It did not validate the production coherent-UMA path used by CI's Microsoft Basic Render Driver. The earlier 321/321 and 334/334 local results were real hardware results, but they did not justify treating the hosted matrix as passed.

Original hosted results at `585407a`: both Windows baseline jobs passed; Studio Debug had ten failures/blocked cases, and Studio Release failed its allocator policy test. Both logs identify the same lost memory fix. No merge occurred.

## Correction and regression coverage

- Windows wireframe generation now consumes the UMA-corrected stage and writes a distinct `wiRendererWireframe.cpp`.
- Both original and intermediate sources are excluded from compilation; each embedded/external shader target compiles exactly one final composed renderer.
- Missing UMA input/policy fails configuration. The DEFAULT-heap UAV path and all six overlay PSOs are retained.
- A new cold/reconfigure integration test checks both patches, actual target source selection, stable bytes/timestamps and preservation of the pinned input. The original generator is deliberately rejected by that test.
- `RENEGADE_TEST_FORCE_WARP=ON` is an opt-in Windows test configuration. It generates a backend differing only in adapter selection, using `EnumWarpAdapter` explicitly. Allocation, mapping and rendering code remain unchanged. The default is OFF; it is not a distributable configuration.

## Recorded validation

| Check | Result |
|---|---|
| Actual WARP, original generator, unchanged allocator probe | Expected failure, exit 1; same CI policy failure |
| Actual WARP, corrected Debug affected matrix | 11/11 PASS, 912.82 s |
| Actual WARP, corrected Release affected matrix | 11/11 PASS, 666.77 s |
| Cold/reconfigure composition, both shader targets | PASS; original generator rejected |
| Source/help/policy suite | 66/66 PASS, 137.25 s |
| Normal NVIDIA allocator and mapped mesh alias | PASS, test-WARP option OFF |
| Normal Release Studio native viewport/modal review | PASS |


The before probe reports Microsoft Basic Render Driver with coherent UMA enabled and fails the same allocator-policy assertion as hosted Release. After correction, the unchanged allocator test passes on actual WARP in both configurations, including byte comparison of the mapped mesh-style alias.

The eleven-case WARP runs include the original affected allocator/terrain cases and the new composition check. Existing assertions, dependencies and deadlines are retained. The 66-case source/help/policy pass is separate. These scoped correction checks do not claim a new complete 322/335 local aggregate pass.

The normal Release Studio was rebuilt with the test adapter option OFF. NVIDIA GeForce RTX 4070 Ti is reported by the unchanged allocator probe. Studio SHA256: 8bbd82eb2512c486f4be3f787eda89d1ddd95fc33d56a213434c9eb66c96c3b6.

The reviewed captures show fresh Story Flow, Shaded, whole-scene Wireframe, shaded coplanar overlay, and the opaque Light Picker with its shaded private preview while the scene uses overlay mode. Returning to Shaded leaves Main.WISCENE clean and Undo/Redo at zero. The isolated candidate was closed cleanly; owner projects were not edited.


## Submission and limits

Only the bounded correction, regression tooling and evidence are added. The Wicked pin remains `82557d439a41d104f559b0d3134d160b7bf6154e`; existing configured submodule overlays and owner projects are preserved.

Windows diagnostic settings were restored after the initial external-driver probe: no registered diagnostic apps and force-WARP false. The successful software probes use the isolated test build, not a global driver override.

The previous [combined preflight receipt](UI_COMBINED_PR_VALIDATION_20261117.md) is historical hardware validation. This receipt records the gap and correction. Hosted checks must be verified against the current PR SHA before any ready-to-merge recommendation. No merge or release is authorized.

[Evidence](evidence/ui-pr197-uma-recovery/README.md).
