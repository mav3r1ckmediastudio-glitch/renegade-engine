# UI Designer alpha local preflight â€” 28 November 2026

Implementation candidate: `3fa45577b1e9da5d9b88bb7be394f3f1820709d9` on `feature/ui-loading-media`, reconciled with main `4e26d75cc78b75575c2a458aeb938e1dd8c3870c`. Final documentation has a later commit identity. Hosted CI must validate that final PR head. Existing vendor worktree dirt and local logs are excluded; no vendor source or gitlink change is part of this PR.

## Result

An isolated checkout with pinned submodules compiled the full `RenegadeStudioCI` aggregate in x64 Release and Debug. Unfiltered Release CTest passed **356/356**; Debug passed **340/340**. Both Studio startup smoke checks passed. Configuration-specific registration counts were audited; all 255 registered native executable targets are reachable from the CI aggregate in each configuration. All 61 source contracts passed. The loading-frame guard accepts the real two-path implementation and rejects four mutations: duplicate gameplay update, duplicate loading update, advancing loading time, and loading fallthrough.

Canonical packaging from `Tools/Build-Studio-Windows.ps1` assembled Studio and Runtime archives for both configurations. Its two-process LP05 dependency proof and four-process LC01 identity/update/move/reopen proof passed with immutable fixture checks. Packaged Studio startup from an unrelated working directory passed. The two stock loading videos and all 25 files across four portable UI prefab manifests were present with matching hashes. All four archives also passed complete staged-file inventory, entry size, duplicate-entry and CRC checks. Exact executable/archive records and durations are in [results.json](results.json).

## Issues repaired before push

- `5c2e75d`: registered ScreenClickSoundTests now builds through the CI aggregate.
- `5d55905`: publishing multiple prefab buttons with the same outcome emits one shared action declaration. A real publish/inspect regression covers this case.
- `58b5499`: the loading source contract checks exclusive gameplay/loading paths and collision admission, replacing an obsolete whole-file update count. Mutation checks retain its failure sensitivity.
- `3fa4557`: the production Build Game dependency reader includes optional ButtonSounds.rui-audio settings and required default/per-button audio. Native production-collector regression covers closure, missing clips and malformed settings.

The first Debug attempt exhausted local disk space while writing symbols. Its log is retained with the local artifacts. Only disposable preflight intermediates were reset; filesystem compression and discarded incremental linker state recovered capacity. The full Debug build and unfiltered tests then ran successfully with unchanged compiler/linker settings and no exclusions. User projects were untouched.

## Observed native preview instability

The first complete Debug run passed 339/340 and reported an access violation in RenegadeMaximumTerrainGPU_preview while preparing its second private Player Start preview. The full failure log and original fixture were retained. Without product changes, a debugger replay, five fresh native processes using the same fixture and the original-order four-case terrain chain all passed. The subsequent complete 340-test Debug run with native crash capture also passed. Exact replay timings and input hashes are recorded in results.json.

The cause remains unconfirmed and is an open terrain-preview stability observation. The preview service and its proof are unchanged versus main; the passing replays do not establish the origin of the failure. These checks do not close independent release acceptance.

## Reproduction and scope

Configure with `cmake -S <checkout> -B <build> -A x64 -DRENEGADE_EMBED_SHADERS=ON`, using the workflow-pinned LP07 fixtures and pinned submodules. For each configuration, run `cmake --build <build> --config <configuration> --target RenegadeStudioCI --parallel 8 -- /p:CL_MPCount=4`, `ctest --test-dir <build> -C <configuration> --timeout 180 --output-on-failure`, then Studio `startup-smoke`. The local toolchain supplied the pinned DirectXMath include and bounded `/MP4`. Packaging reused the canonical script's post-build packaging block unchanged; this avoided its initial clean-vendor guard after configure-time overlays had already been applied.

Native feature-specific authoring, history, reopen and Runtime screenshots remain in the dated loading-media, carousel, navigation, location-text-effects, ammo-counter, minimap and player-hurt evidence folders. They identify their original checkpoints and are not relabelled as new final-binary visual proof. Generic canonical packaging does not establish every custom HUD's exported-game visual behaviour.

Wizard/common tooltips are the next separate PR. Existing scripting coverage and limitations remain in FEATURE_MATRIX.csv, including no dedicated carousel override API. Large-scene minimap performance, physical-controller/alternate-DPI coverage, clean-machine deployment and independent exact-commit release acceptance remain separate. Wait for all four hosted Debug/Release Studio/baseline checks before merging.
