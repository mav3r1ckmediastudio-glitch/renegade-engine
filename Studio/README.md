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
