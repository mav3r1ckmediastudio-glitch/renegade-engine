# ADR 0002: Defer the Production UI Toolkit

**Status:** Proposed; decision required in Phase 2
**Date:** 2026-07-29

## Context

Wicked's native `wiGUI` offers proven integration, while Dear ImGui Docking
offers a stronger modern-editor starting point. The supplied ImGui sample does
not prove the required HDR editor path.

## Decision gate

Keep `EngineBridge` services UI-independent. Prototype both viable routes as
needed and select the production toolkit only after verifying:

- HDR and SDR composition.
- DPI scaling.
- Keyboard/mouse capture.
- Multi-monitor behaviour.
- Docking and layout persistence.
- Viewport render-state safety.
- File dialogs and packaging.

## Consequences

Broad production panel work must not begin before this ADR is accepted with
evidence. ImGui Docking is preferred if it passes; `wiGUI` remains the fallback.
