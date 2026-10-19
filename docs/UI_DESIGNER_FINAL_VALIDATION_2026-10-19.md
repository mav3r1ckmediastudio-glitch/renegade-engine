# Current status: owner-test readiness remains open

Native artwork import crash repaired in source and rebuilt; native recheck is blocked by repeated title-bar close input during QA startup. Full Basic UX acceptance remains unfinished. See `UI_DESIGNER_BASIC_UX_AUDIT_2026-10-19.md` latest continuation. Historical candidate evidence follows.

# UI Designer final owner-test validation

UI Designer owner-test candidate, 19 October 2026

The 29 approved requirements have implementation coverage after the source audit and six final authoring fixes. Final Studio/Runtime Release builds pass; full CTest is 231/231 PASS (72s). Native creator tests cover aggregate geometry with one-step history, mixed-control focus order, shared-Screen explicit routing contexts, stable destination identities, independent pane collapse, project/Screen grid settings and native starter restoration. Native validation found and repaired modal picker click-through before this candidate was packaged.

The final packaged Studio built the included standalone game through Build Game, including smoke validation, package isolation and final-path promotion. A separate copy ran without project arguments while its originating project and personal library were unavailable. Real Level/HUD, pause/settings/resume retaining Level session, governed Quit, restart UI scale/budget persistence, three exact client sizes and non-system module closure passed. The repository remained available: this is not a pristine clean-machine certification.

All work remains uncommitted on feature/ui-designer-complete-20261017, baseline 60634cec26feb7ffc2d0f941d3381865fa3b41c7. No commit, push or merge occurred. Official release acceptance remains open: AGENTS.md requires a different AI or human to verify the exact commit with docs/VERIFICATION_CHECKLIST.md. This candidate is identified by executable and source-content hashes instead.

Evidence limits: the actual monitor is 96 DPI and no physical controller is connected. GPU time, resident UI memory and end-to-end input latency are explicitly unmeasured; advisory budgets cover measured binding CPU plus authored widget/track counts. Save/audio/graphics pages without a configured gameplay backend explain that capability; eight optional gameplay modules remain disabled editable templates. No arbitrary modal-stack or optional backend expansion is implied by the approved scope.

Studio SHA256: `ac3c172bf046217411dea2ec620592ccc7eb3fe8a630364fa915f4c0e043162d`

Standalone game SHA256: `b5ff5af45ecd2f1f05cdf200858efc8cb3683f7aa2e4f0032b11da5c1788f79e`

Evidence: BUILD/ui-final-full-results-2.xml; BUILD/ui-final-full-tests-2.log; BUILD/ui-scaffold-native/native-final-authoring-proof.json; BUILD/ui-scaffold-native/native-final-export-runtime-proof.json; BUILD/ui-scaffold-native/native-final-export-package-proof.json. Earlier native binding, attributes, library, transitions and maximum-scale proofs are indexed in the requirement matrix and historical HANDOFF entries. The initial full run was 229/231: one missing DiagnosticService test executable and one stale blood-atlas expectation (existing implementation is 8x8/64, test expected 4x4/16). Both were corrected without changing gameplay presentation; final full suite has no failures/skips.


### Final verification for registry import crash repair
- Complete Release build: `cmake --build BUILD/ui-complete --config Release -- /m:1`, exit 0; `BUILD/ux-acceptance-20261019/all-build.log` and exit file. Studio and Runtime both rebuilt.
- Full regression: `ctest --test-dir BUILD/ui-complete -C Release --output-on-failure`, **230/231 PASS**, exit 8, 212.26 seconds. The only failure is `RenegadeProjectileWorldTests`, which hung during CRT exit joining a worker thread and was deliberately terminated after stack capture so remaining tests could run. This is NOT a nominal assertion pass or a fully green suite. Native debugger sample: `BUILD/ux-acceptance-20261019/stalled-test.log`.
- All other rebuilt regressions, including registry/resource/Screen/StoryFlow suites, passed. New registry regression covers the missing historical provenance crash. The post-fix native import remains UNVERIFIED because QA windows receive title-bar close input during startup; uninterrupted desktop access is the immediate validation blocker.
- No new owner-test release packaged. Outstanding full Basic Inspector audit, artwork clipping/runtime/loading lifecycle, StoryFlow recovery/rename placement acceptance and clean native recheck remain open. Do not deliver the private QA package as a completed candidate.
