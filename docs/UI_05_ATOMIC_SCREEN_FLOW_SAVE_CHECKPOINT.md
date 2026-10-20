# UI-05 — Screen/StoryFlow coordinated Save checkpoint

Date: 16 October 2026. Branch `feature/p3-original-impact-vfx`, existing HEAD `60634ce`, dirty shared tree. All work local, uncommitted, no merge/push or game project mutation.

## Implementation

- `EngineBridge/include/renegade/bridge/ScreenFlowAtomicSaveService.h` (new): a bounded, UI-independent Save service for a loaded `ScreenAuthoringSession` plus a loaded `StoryFlowAuthoringSession` with matching project IDs. The service serializes current and previously-saved snapshots through their existing authoritative document writers in a temporary directory, compares the originally loaded disk bytes to the previous snapshots, validates both new staged documents, and commits both through the repository's existing journaled `ProjectDocumentTransaction`. It rechecks on-disk baselines at transaction Prepare. It refuses divergent external edits, missing saved history baselines or mismatched projects, and never rewrites route/action authority.
- Existing authoring sessions give this named service limited friend access to their saved history indices. Only after transaction success and byte-verified commit does the service advance both saved indices. It does not reload either session or destroy Undo/Redo history.
- `Studio/src/RenegadeScreenEditorWorkspace.h/.cpp` and `RenegadeScreenEditorRenderPath.h`: the native Screen Editor Save control delegates to an injected coordinated callback, with standalone legacy Screen Save retained only when no coordinator is registered.
- `Studio/src/StoryFlowStudioIntegration.h`: injects the paired Save callback from the active StoryFlow coordinator. This saves both current Screen and authoritative Flow together, reports success/failure in the existing status surfaces, and uses `activeProjectRoot_` as the journal containment boundary.

## Evidence

- Rebuilt Release `RenegadeStoryFlowGate8EOutcomeParityTests` target **PASS**; log `BUILD/ui05-pair-save-test-build.log`.
- Rebuilt CTest `RenegadeStoryFlowGate8EOutcomeParityTests` **PASS 1/1** (1.43 s). Extended test verifies injected failure on the second file replacement rolls both documents back; actual Save preserves Screen + Flow Undo history and file round-trip; idempotent save; and fail-closed external disk edit detection.
- Scoped `git diff --check` **PASS** after UI callback wiring.
- Linked Studio Release build `BUILD/ui05-joint-save-studio-release.log`: **PASS**, exit 0 (356.67 seconds). Updated executable `BUILD/renegade/Studio/Release/RenegadeStudio.exe`.
- Final six targeted Screen/StoryFlow Release CTest suites: **PASS 6/6** (2.50 seconds). The rebuilt outcome-parity suite includes actual failure-injection and two-file save/reopen tests.
- Scoped `git diff --check`: **PASS**.
- Native GUI Save click and actual post-save Runtime loading are **not yet live-tested**, so this is a compiled, regression-tested implementation checkpoint, not owner visual acceptance.

## Remaining work

- Native keyboard/mouse Save, Ctrl+Z interleaving, Screen switching and Flow persistence need an isolated live test; unit tests do not prove physical editor interaction.
- No atomic *cross-document Undo for changes to both documents in one gesture* is claimed: current destination pick mutates only Flow; the combined Studio timeline delegates to existing history authorities. Atomic Save coordinates disk commit of both documents.
- Complete UI Designer UI-001–029 beyond Basic widgets, project Screen list, snapping/grid, Layers and symbolic destinations; unimplemented toolbox entries remain disabled. StoryFlow outcome audits, legacy game Runtime integration, templates, accessibility and custom widget families need their own implementation and tests.
- Do not replace the Studio executable while it is open. Do not touch concurrent assistant, projectile and game asset work.
