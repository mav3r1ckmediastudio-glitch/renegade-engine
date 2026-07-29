# ADR 0002: Use wiGUI as the Production UI Foundation

**Status:** Accepted
**Date:** 2026-07-30

## Context

Wicked's native `wiGUI` offers proven integration, while Dear ImGui Docking
offers a stronger modern-editor starting point. The supplied ImGui sample does
not prove the required HDR editor path.

The production toolkit must coexist with the same render path used by the
standalone player, support Windows HDR composition, remain DPI aware, route
input correctly, and permit a Renegade-owned visual language and workflow.

## Evidence

- The pinned ImGui Docking sample sets `allow_hdr = false` with the source
  comment `Imgui doesn't support HDR`.
- The same sample leaves platform multi-viewports and keyboard navigation
  disabled and contains a warning that DPI font scaling does not produce good
  results.
- The Renegade `wiGUI` proof shares Wicked's normal `Application` composition
  path, whose swap chain retains `allow_hdr = true`.
- On a Windows GPU, the Release Studio proof passed hierarchy selection,
  transform fields, viewport gizmo input, ten Undo operations, ten Redo
  operations, Save As, repeated Save As, and Reopen.
- The Release Runtime proof rendered the same scene without editor controls.
- File-dialog and repeated save defects were corrected outside Wicked, leaving
  the pinned submodule unchanged.

## Decision

Use `wiGUI` as Renegade's production UI integration and rendering foundation.
Keep `EngineBridge` independent of the toolkit.

This is not a decision to ship Wicked's stock Editor or its stock visual style.
Renegade owns:

- information architecture and workflows;
- docking and layout persistence;
- theme, typography, spacing, icons, and interaction states;
- project hub, panels, toolbars, menus, inspectors, and content browser; and
- accessibility, shortcuts, and user preferences.

## Consequences

- Phase 3 can begin without an unproven second UI renderer in the frame graph.
- Renegade must implement or extend docking and modern editor behaviour above
  `wiGUI`.
- The Windows Vulkan, mixed-DPI/multi-monitor, and physical HDR checks remain
  Phase 2 platform validation tasks.
- ImGui can be reconsidered only if a future pinned backend demonstrates HDR,
  multi-viewport, DPI, and input parity without weakening the service boundary.
