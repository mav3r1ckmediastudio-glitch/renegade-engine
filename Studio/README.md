# Studio

Renegade's custom Windows-first editor application.

## Current Phase 3 increment

`RenegadeStudio` is a Windows-first shell that:

- owns its window, branding, message loop, and per-monitor DPI setup;
- renders through Wicked's `RenderPath3D`;
- obtains scene state through `Renegade::EngineBridge`;
- starts in a Renegade-owned holographic Project Hub;
- creates, validates, opens, and remembers `.renegade` projects;
- renders a generated live Proving Ground instead of presenting only a cube;
- exposes permanent hierarchy, inspector, content, toolbar, and viewport
  regions;
- selects scene objects by clicking them in the viewport and synchronises that
  selection with the hierarchy, inspector, and transform gizmo;
- draws an editor-only cyan silhouette around the selected object;
- supports right-mouse freelook and keyboard fly navigation within the
  viewport;
- edits position, rotation, and scale through the Inspector;
- switches Move, Rotate, and Scale gizmos with W/E/R;
- focuses, duplicates, and deletes selected entities;
- provides undoable full-transform, duplicate, and delete commands;
- supports Ctrl+S, Ctrl+Shift+S, Ctrl+D, Delete, F, Ctrl+Z, and Ctrl+Y;
- hides generated grid internals from the creator hierarchy; and
- preserves Save As, Reopen, DX12, Vulkan, input, and DPI paths.

ADR 0002 accepts `wiGUI` as the native foundation. The visual language,
components, layout, and workflows belong to Renegade.

See `docs/PHASE3_PROJECT_HUB.md` and
`docs/PHASE3_VIEWPORT_INTERACTION.md`, then
`docs/PHASE3_EDITOR_USABILITY.md`, for the current acceptance criteria.
