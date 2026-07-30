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
  regions; and
- preserves the verified selection, gizmo, Undo/Redo, Save As, Reopen, DX12,
  Vulkan, input, and DPI paths from Phase 2.

ADR 0002 accepts `wiGUI` as the native foundation. The visual language,
components, layout, and workflows belong to Renegade.

See `docs/PHASE3_PROJECT_HUB.md` for the current acceptance criteria.
