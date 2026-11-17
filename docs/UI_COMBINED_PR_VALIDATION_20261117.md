# Combined Studio UI and wireframe validation — 17 November 2026

Application implementation: `7fd95b5b3607974e7275673fa0bcc3e1b1c6af2a`.
CMake diagnostic/configure stamp: `c2eccc33fa1c2e13c06bc50b94f16ffa308d12a5`.
Base integration: `fceda089336f6f05047846bcc4053e2868b708d0` (merged Dialogue PR #196).
The implementation commit adds one final Screen Editor tooltip render pass and three reviewed test-harness corrections to that configure checkpoint. The complete Debug/Release replay used those exact four file hashes, recorded in validation.json. The later documentation/evidence commit changes no application inputs. Hosted CI generates the exact PR-tip diagnostic stamp normally.

This combines the [cohesion pass](UI_COHESION_PASS1.md), [approved icon integration](UI_ICON_INTEGRATION.md), [whole-scene wireframe modes](WIREFRAME_VIEW_MODE.md) and [seven usability corrections](UI_USABILITY_COMPLETION_20261116.md). The owner accepted the previous UIFinish7 native review with “It looks good”; that acceptance is scoped to that review.

## Preflight corrections

- Integrated the already-merged Dialogue corrections before validation. Retained the compact example-audio notice.
- Refreshed reviewed built-in-help whole-file fingerprints after UI changes. Existing workflow needles, steps and fail-closed checks remain intact.
- Fixed stale inspector titles for literal-ellipsis asset names. Full titles now have one setter, separate from fitted display text. The old executable reproduced the defect; the final candidate was checked with the same rename and Undo.
- Preserved existing detailed action help when assigning toolbar icons, including Screen Editor autosave guidance. Native review then exposed a separate missing render pass for the Screen Editor's six manually owned toolbar icons. Their hints now render after ordinary controls and the collapse handles, before child pickers. Save's disabled-state autosave guidance and the full Fit View hint were verified on the final native binary.
- The initial full Debug run exposed an old text-label command contract. It now checks distinct Test Game/Build Game icons, lifecycle tooltips, catalogue titles and native handoffs while preserving the save/build guards. Three deliberately broken variants are rejected.
- The desktop terrain proof stopped its generation worker after reserving the last chunk but inspected meshes before the next normal update published that worker's scene. A bounded publication drain retains the precondition of exactly 361 chunks and every original mesh, height, history and GPU check. Targeted receipts show one pending mesh becomes zero after one native update.
- The CPU foam proof constructed std::string error messages on every successful hot-loop cell check. A test-local literal-message helper preserves every condition and failure message without that allocation. The same 320x160 domain, two sources, simulation duration, tolerances and 180-second deadline now complete in 15.82 seconds. Targeted contract/default-terrain/initialization/foam checks pass 4/4.
- Recovered the previous UTF-16 build/test receipts as readable UTF-8 text; results are unchanged.

## Build and automated validation

| Configuration | Earlier complete CI aggregate | Final incremental build | Rerun unfiltered CTest | Package proofs | Startup smoke |
|---|---|---|---|---|---|
| Debug | PASS | Studio + five recovery targets PASS | 321/321 PASS | Dependency + registry/reopen PASS | Exit 0 |
| Release | PASS | Studio + five recovery targets PASS | 334/334 PASS | Dependency + registry/reopen PASS | Exit 0 |

Built and packaged icon atlases/manifests match the source; each Studio package contains all 65 approved SVGs. Studio and Runtime packages were staged for both configurations.

| Configuration | Studio SHA-256 | Runtime SHA-256 |
|---|---|---|
| Debug | `e046320a43037412a3740b10b283ecc24e99d74c76f7de55731522df5457f779` | `bd09ce40fe31db1baef6c528a25d2e2ba1ff53b982f1a182b7f499dbcbeb90b1` |
| Release | `b5aa67b58e9c850909b89d6a6eaec473770e9c0b6d3940c95f477c1233847f63` | `2821832fcd60940e85d8ce52bd14d8edd509c8b0433f93099c4846c04719cdd0` |


The validation checkout was isolated from the owner's configured application, project and Wicked overlays. Its configured revision remained at the diagnostic checkpoint above; the final four-file delta was guarded by exact hashes in both checkouts before the replay, then committed without changing the tested bytes. Wicked remains pinned to `82557d439a41d104f559b0d3134d160b7bf6154e`; no submodule pointer changed.

The local exFAT drive required explicit configuration with CMAKE_SUPPRESS_REGENERATION=ON and bounded workers. A complete RenegadeStudioCI aggregate build, unfiltered Debug 321/321 and Release 334/334 tests, dependency/registry proofs and packages passed at the configure checkpoint plus the three recorded test corrections. The final eight-line Screen Editor change belongs only to the Studio executable (Studio/CMakeLists.txt); it changes no runtime, bridge, test or build-system source.

The final incremental flow rebuilds Studio in both configurations, then reruns every unfiltered test and every package proof. Five test targets whose linker output was interrupted during the earlier drive-contention attempts are also rebuilt conservatively. All other test/runtime sources are identical to the completed aggregate. No assertion, case or deadline is removed or relaxed. Earlier complete aggregate logs, final incremental build logs and both rounds' test logs are retained separately.

A task-local copy of Tools/Build-Studio-Windows.ps1 retains startup-media checks, separate-process dependency and registry/reopen proofs, and Studio/Runtime packaging. It verifies the configured dependency's exact pin while resuming expected CMake overlays. The final build uses one worker; interrupted four-worker and serial aggregate attempts are retained and are not counted as successful builds. These local execution adaptations change no product or hosted-workflow files.

The earlier complete aggregate commands used RenegadeStudioCI in Debug and Release. The final incremental commands are:

~~~powershell
$finalTargets = @(
  'RenegadeStudio', 'RenegadeEquipmentAssetTests',
  'RenegadeHudReferencePersistenceTests', 'RenegadeRiverFlowFieldTests',
  'RenegadeRiverFoamTests', 'RenegadeStoryFlowGate5ScreenLifecycleTests'
)
cmake --build BUILD/renegade --config Debug --target $finalTargets --parallel 1
ctest --test-dir BUILD/renegade -C Debug --timeout 180 --output-on-failure
cmake --build BUILD/renegade --config Release --target $finalTargets --parallel 1
ctest --test-dir BUILD/renegade -C Release --timeout 180 --output-on-failure
~~~

An additional source/help/policy pass used:

```powershell
ctest --test-dir BUILD/native -C Release -R 'SourceContract|Assistant|StudioUiPolicy' --output-on-failure
```

It passed 66/66, including packaged help and the 84-case workflow suite. Startup-smoke constructs the application and exits before graphics; it is not a rendering proof. Rendering evidence is from the native checks below.

## Final native candidate

Two explicitly scoped native candidates were used:

- Broad candidate `D91BD0E6FA9567032F14010FF32371C989339089943D8BBD058619B7987DC7C0` (configure checkpoint plus the three test corrections): checked Story Flow toolbar spacing and tooltips; launched the actual Runtime from Test Game and returned via Stop; immediate selection/Audio handoff; literal-ellipsis rename and Undo; Environment, Render and lower Player numeric fields; primary-first model inspector and selection scroll reset; all twelve Add rows; Shaded/Wireframe/Overlay switching; opaque Light modal and private preview over overlay; reachable modal footer at 1024 width; Dialogue/Screen/Back/Escape navigation. The old UIFinish7 executable reproduced the stale-name heading before the corrected candidate. Scene rename changes were undone.
- Final candidate `B5AA67B58E9C850909B89D6A6EAEC473770E9C0B6D3940C95F477C1233847F63` adds only the eight-line Screen Editor toolbar tooltip render pass (file hash recorded in validation.json). Verified the complete Fit View hint above the collapse handle, disabled Save's autosave guidance, clean Back/Dialogue/Escape navigation, immediate light inspector and Audio-to-same-light handoff. Both disposable native applications closed cleanly before packaging; the owner's original application was left open.

The runtime capture shows the DialogueReview conversation and its three authored choices. Native 75 FPS readings are observations, not a performance benchmark. The 1024-width modal uses bounded horizontal/vertical scrolling; this does not claim that every body column fits simultaneously.

Checks used a disposable DialogueReview project and isolated preferences. Temporary rename edits were undone. No authored owner project, dialogue or Screen was changed. The owner's existing review application remained open.

## Limits and submission

This is scoped regression validation, not a guarantee of zero defects or an independent release signoff. Windows OS DPI 125/150%, every state of the 65-family inventory, all Physics pages at 1024 width, complex terrain/MSAA and path-traced transitions remain outside the recorded native coverage.

All documentation/evidence was prepared before the single branch push and PR creation, avoiding a second CI restart for a documentation follow-up. Hosted CI status is reported on the PR; local passes do not stand in for the four hosted results. No merge or release was performed.

[Evidence index](evidence/ui-pr-validation/README.md).

Local PowerShell 5 command redirection produced UTF-16 command output that the shared logger appended as UTF-8 with interleaved NULs. Evidence text repairs that ASCII log encoding and normalizes paths/whitespace; raw logs remain retained. The test totals are checked against the correctly decoded console transcript.
