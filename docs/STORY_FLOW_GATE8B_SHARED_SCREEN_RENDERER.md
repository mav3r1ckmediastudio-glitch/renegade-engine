# Story Flow Gate 8B — Shared Screen Renderer

## Outcome

Create one Renegade-owned Screen renderer which Runtime uses immediately and
the Gate 8C Screen Editor preview must reuse unchanged. The renderer must draw
the complete Gate 8A contract without stock Wicked visual policy leaking into
the result.

This gate is rendering infrastructure. It does not add the Screen Editor
canvas, hierarchy, Inspector, mutations, history or Save/Return workflow.

## Locked implementation boundary

- `ScreenService` remains the serialized, UI-independent Screen authority.
- `BuildScreenRenderItems` resolves deterministic frame state, order, geometry
  and scaled metrics without requiring a GPU.
- `Renegade::ScreenRenderer` is the sole Wicked-backed presenter.
- `RuntimeScreenPresenter` retains Runtime input/action orchestration and
  delegates all visible Screen output to `Renegade::ScreenRenderer`.
- Gate 8C preview must instantiate `Renegade::ScreenRenderer`; it must not copy
  its state selection, resource, border, typography or canvas rules.
- Wicked remains pinned and unmodified.

## Visual contract

State precedence is:

1. disabled button;
2. pointer pressed;
3. pointer hover;
4. keyboard/gamepad focused;
5. normal.

Image and Text records that are non-interactive render normally. They are not
dimmed merely because they do not accept input.

The renderer draws an outer rounded border and an inset rounded content surface.
Border width, corner radius, font size, character/line spacing and shadow offset
all use the same canvas metric scale. A state image replaces the coloured
surface and receives the authored state tint and widget opacity. Text receives
the same state's foreground colour. No extra disabled fade, default font,
colour, alignment or corner treatment may be applied by Wicked.

## Automated acceptance

- authored back-to-front order survives frame projection;
- 1280x720 at 1920x1080 produces exactly 1.5x geometry and metrics;
- focused, pressed and disabled state selection is deterministic;
- disabled overrides requested pointer state for a Button;
- a non-interactive Image remains normal;
- Runtime controller focus, mouse/keyboard/gamepad action dispatch and
  hidden/disabled fail-closed behaviour remain green;
- Windows Debug and Release build/test workflows pass at the exact head.

## Owner Release acceptance

From the exact CI Release artifact:

1. Extract `RenegadeRuntime-Release.zip`.
2. Run `Run-LP03-Screen-Proof.cmd`.
3. Confirm the title/background/buttons are visible and correctly layered.
4. Confirm Play begins focused, pointer hover changes its authored state,
   mouse press shows the pressed state and keyboard/gamepad focus uses the
   focused state without an unexplained dimming overlay.
5. Confirm Play still enters the level and Quit still exits normally.
6. Report PASS or the exact visual/behavioural mismatch.

A visual or behavioural mismatch overrides green CI. Gate 8B is not complete
until the exact Release head passes this inspection.
