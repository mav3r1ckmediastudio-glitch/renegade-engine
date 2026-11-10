# Functional scene-prefab acceptance

Local Windows x64 DX12 Release candidate built from the recorded source parent
plus the committed functional changes. Source/binary fingerprints are in
candidate-manifest.json. No owner castle content is used. The private package is
a disposable acceptance fixture and was neither promoted nor distributed.

## Commands and results

- cmake --build BUILD/selection --target RenegadeStudio RenegadeRuntime RenegadeSceneAssemblyWorkflowProof -j 2: PASS.
- cmake --build BUILD/selection --target RenegadeScenePrefabScriptTests RenegadeScenePrefabFunctionalProof RenegadeLightPickerTests RenegadeParticleEffectTests RenegadeEditorPointLookupFocusedTests RenegadeAssetCatalogueTests -j 2: PASS.
- Focused CTest: 9/9 PASS plus SceneAssemblyCore and EditorPointLookupSourceContract 2/2 PASS.
- ctest --test-dir BUILD/selection -R SourceContract --output-on-failure: 60/60 PASS.
- Earlier six CI failures rechecked locally: 6/6 PASS, including the complete offline assistant suite.
- RenegadeSceneAssemblyWorkflowProof Proof/FinalGeometryRegression: PASS.
- RenegadeScenePrefabFunctionalProof Proof/FunctionalBrazier5 <source-parent> <wicked-pin>: PASS.
- Actual Studio: select full brazier; Save Prefab modal; save UI Verified Brazier;
  drag its Asset Browser card into the scene; Unpack into five objects; Save and
  Reopen: PASS. Screenshots and final diagnostic snapshot included.
- Launch the staged BrazierProof.exe after native ApplyWindowsGameExecutableIdentity:
  PASS, package-relative bootstrap and package integrity PASS. Both effects render.
  Runtime diagnostic event script.scene.started confirms four governed instances.
  Closed normally after observation.

The native functional proof exercises duplicate, edit/Undo, scripted prefab
save/load, mesh-free light/effect prefab save/load/placement, two independent
instances, property edits/Undo/Redo, 120 frames of actual governed Lua per phase,
scene save/cold reopen, Unpack/save/reopen/Undo, native package closure and
instance-preserving packaged refresh. It checks materials/shader selection,
texture identities, fields, lifetime curves, shapes, owner IDs and script references.

The six old CI failures comprised stale menu/selection source contracts and stale
assistant whole-file fingerprints. Contracts now assert current ownership and
selection semantics. Existing importer/player/equipment/Surface Type instructions
were reviewed before renewing the affected fingerprints; prior QA limits remain.

No final release-gate claim: CI and independent exact-commit/owner acceptance are
pending. Main has not been merged and no build has been distributed.
