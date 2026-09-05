# Phase 3 — Robust Wicked-backed scene documents

## Decision

Renegade retains its own Project Hub, chrome, prompts, terminology and layout.
Open, Reopen, Save and Save As now cross a UI-free `SceneDocumentService`
boundary that uses the
pinned Wicked Engine's public `wi::Archive`, `Scene::Serialize`, scene merge,
job system and thread-safe event APIs.

This slice does not fork Wicked Engine, modify its submodule, instantiate
`EditorComponent`, or display any stock Wicked Editor window. If a later slice
proves that a required operation cannot be exposed through the pinned public
APIs, that requires a separate documented core-patch decision.

## Scope

- Add a move-only `PreparedSceneOpen` result.
- Prepare archive validation and deserialization without mutating the active
  document.
- Run Studio preparation through Wicked's low-priority job system.
- Commit scene, path, selection reset and history reset together at
  `EVENT_THREAD_SAFE_POINT`.
- Preserve the current document on missing, incompatible or rejected files.
- Adopt the first authored scene camera using the same fields Wicked Editor
  copies during `EditorComponent::Open`.
- Keep the current document visible but block viewport edits while loading.
- Make untitled Save-before-Open continue automatically after a successful
  Save As; cancel or failed save aborts replacement.
- Surface load progress and failure in both the workspace and Project Hub.
- Use the same Wicked archive preparation for Runtime startup loading.
- Serialize Save and Save As to a temporary WISCENE before touching an
  existing destination.
- Reopen the temporary and final archives as validation.
- Preserve the previous valid document as an openable `*.bak.wiscene`.
- Maintain the newest ten automatic WISCENE backups under
  `Saved/Backups/Scenes/<scene-name>`.
- Mark the document clean only after the validated replacement succeeds.
- Route Reopen through the same unsaved-change and asynchronous Open path.
- Ignore document shortcuts during unfinished gizmo, slider and terrain-stroke
  interactions so partial previews cannot be mistaken for completed edits.

Undo/Redo, selection and hierarchy remain Renegade's current implementations.
Dirty state is still command-history based, but successful save completion and
failed-save preservation now belong to the document boundary. A later staged
migration may replace those state models without changing Renegade's UI.

## Architecture

```text
Renegade File/Open and Project Hub
              |
              v
SceneDocumentService::PrepareOpen (Wicked job system)
              |
              v
wi::Archive + Scene::Serialize into candidate scene
              |
              v
EVENT_THREAD_SAFE_POINT
              |
              v
SceneDocumentService::CommitPreparedOpen
              |
              v
active Wicked scene + path + selection/history reset
```

Save uses the inverse protected transaction:

```text
Renegade Save / Save As
              |
              v
serialize temporary WISCENE -> reopen and validate
              |
              v
protect previous -> atomic replace -> validate final
              |
              v
previous .bak + rolling backups + clean saved revision
```

`SceneService::LoadScene` remains only as the Runtime startup entry point. It
uses the same `PrepareWickedSceneOpen` operation and does not own editor
selection, history or replacement prompts.

## Automated evidence

`RenegadeBridgeTests` now covers:

- missing-file preparation preserving the active document;
- invalid/incompatible archive-header rejection preserving scene, selection,
  history and dirty state;
- valid preparation leaving the active document unchanged;
- successful commit replacing scene and path together;
- selection/history reset only after successful commit; and
- authored-camera identity carried through the prepared commit;
- transform, weather, native terrain settings, sculpt height data and blend
  pixels surviving a Save/Open round trip;
- Save As preserving the source archive while writing current state;
- failed save preserving document path and dirty state;
- overwrite preserving the previous scene in an openable backup; and
- rolling automatic backup retention capped at ten files.

Linux syntax-only checks cover the changed bridge, Studio, Runtime and test
translation units. This environment has no CMake installation and cannot run
the Windows binaries or CTest.

## Packaged Windows acceptance

Do not mark this passed from compilation or headless tests.

### Verified so far

- Windows Debug and Release CI are green at cleanup base `0a1d0b6`.
- The project owner verified on packaged DX12 that a sculpted terrain reopens
  with all 169 chunks and remains unchanged through the first terrain update.
- The post-load wipe was caused by bundled-material rebinding leaving a clean
  material dirty; `c7eea43` preserves the incoming dirty state and
  `0a1d0b6` removes the temporary diagnostics.

This proves the reported terrain persistence failure is fixed. It does not yet
prove the complete backup, forced-failure, Runtime, or Vulkan checklist.

1. Build Debug and Release and run the full CTest suite.
2. Package Release and launch DX12.
3. From Project Hub, open two visibly different WISCENE files.
4. Repeat from the Studio File menu.
5. Confirm the hierarchy, scene-tab name, viewport, authored camera and terrain
   visibly change.
6. Confirm a `5b0f34c` terrain scene preserves sculpted heights, seams,
   materials and grass after opening.
7. With a dirty saved scene, test Save, Discard and Cancel.
8. With a dirty untitled scene, choose Save, complete Save As, and confirm the
   originally selected scene opens without invoking Open a second time.
9. Cancel that Save As and confirm the current scene remains active.
10. Test missing, incompatible and several genuinely truncated/corrupt files;
    the current scene must remain intact and an error must be visible in both
    workspace and Project Hub. Only invalid/incompatible headers are covered by
    the current headless test, so packaged corrupt-file testing remains a
    specific acceptance risk.
11. Open a scene containing a camera and confirm the viewport adopts it.
12. Save a visibly sculpted and painted terrain, close Studio, reopen it and
    compare heights, seams, materials, hierarchy, weather and transforms.
13. Overwrite that scene and confirm `<name>.bak.wiscene` opens as the state
    immediately before the overwrite.
14. Confirm `Saved/Backups/Scenes/<name>` contains openable timestamped scene
    versions and retains no more than ten after repeated saves.
15. Force a save failure (for example a read-only destination) and confirm the
    old file remains openable, the active path is unchanged and the scene stays
    dirty with a visible error.
16. Close, relaunch and repeat.
17. Repeat the functional pass with the `vulkan` argument.

Only the project owner's packaged pass can promote the feature-matrix result.
