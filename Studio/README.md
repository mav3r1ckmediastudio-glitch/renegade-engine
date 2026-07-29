# Studio

Renegade's custom editor application. Production implementation begins after the
Phase 2 UI, HDR, DPI, input, and viewport proof.

## Current proof

`RenegadeStudio` is a Windows-first shell that:

- owns its window, branding, message loop, and per-monitor DPI setup;
- renders through Wicked's `RenderPath3D`;
- obtains scene state through `Renegade::EngineBridge`;
- loads the packaged `Content/cube.wiscene` fixture; and
- displays a temporary diagnostic status label.

The label uses `wiGUI` only as a low-cost diagnostic surface. It is not a
production UI-toolkit decision. ADR 0002 remains open.

The second Phase 2 increment adds a diagnostic hierarchy, selection-bound
translation inspector, and Undo/Redo buttons. These controls exercise
UI-independent EngineBridge services; they are not the final editor design.

See `docs/PHASE2_EDITOR_INTERACTION.md` for its acceptance criteria.
