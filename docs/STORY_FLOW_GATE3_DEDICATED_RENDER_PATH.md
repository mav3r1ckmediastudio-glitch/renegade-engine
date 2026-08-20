# Story Flow Gate 3B — Dedicated Render Path

**Parent slice:** Gate 3 authoring foundation (`83e8450ac44d4cfb4383d8d78f703ce72a5d396a`)

## Purpose

This slice retires the Gate 1 hosting architecture. Story Flow no longer depends on the 3D Level Editor render path to update and draw its workspace. It owns a first-class `RenegadeStoryFlowRenderPath` derived from Wicked `RenderPath2D`.

Wicked `Application::ActivatePath()` stops the previous path, starts the next path, and only the active path receives the application's normal update/render lifecycle. When Story Flow is active, the 3D Level Editor path is therefore inactive rather than continuing to tick and render underneath the Story Flow surface.

## Lifecycle established

- Project Hub and project-loading UX remain owned by the existing 3D Studio path.
- Opening/continuing a project with a valid `startup_flow` activates the dedicated Story Flow path after the current frame.
- Projects without `startup_flow` remain on the legacy Level Editor path for compatibility.
- Story Flow uses the full application canvas rather than `CreatorAssetStudioChrome::ViewportBounds()`.
- Existing presentation-only layout persistence remains unchanged.
- `RequestLevelEditor()` and `RequestStoryFlow()` are explicit transition seams for Gate 4 Level open/return integration, so merely having a startup Flow will not prevent deliberate editor transitions.
- Shared project/session/document state remains owned outside either render path.

## Deliberate exclusions

This is the render-path promotion slice, not the complete Gate 3 Graph UI. The visible graph remains the accepted engineering canvas while the mutable `StoryFlowAuthoringSession` from the previous Gate 3 slice remains presentation-independent.

The next Gate 3 slice binds native Graph/Inspector controls to that authoring session, including Flow Save, dirty state, Undo/Redo, node/route mutation and diagnostics.

Journey View remains Gate 6.
