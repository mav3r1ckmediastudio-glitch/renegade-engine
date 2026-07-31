# Phase 3 — Workspace stabilization correction

## Why this correction exists

Packaged Windows review of the first stabilization pass found two visual and
interaction failures that CI could not detect:

- the right Inspector exposed the viewport through its transparent stock
  container and therefore did not belong to the accepted dark workspace; and
- replacing the stock windows also removed their resize behaviour without
  supplying Renegade-owned splitters.

These are Patch 1 defects. They are corrected before the scene-workflow phase.

## Corrected workspace contract

- The Inspector base is fully opaque `Surface0`; the viewport never colours or
  textures the panel behind its controls.
- Hierarchy and Inspector vertical dividers are draggable with an east/west
  resize cursor and restrained orange hover/drag state.
- The open bottom drawer has a draggable top divider with a north/south cursor.
- Hierarchy width, Inspector width and drawer height are clamped so the central
  viewport always retains a useful editing area.
- All three dimensions persist through the existing editor-preference service
  and remain outside WISCENE and `.renegade` project data.

## Environment slider control

`RenegadeSlider` owns every visible pixel while retaining Wicked's mature
pointer and numeric-entry mechanics. It presents a bold label, squared track,
restrained orange fill/knob and an adjacent precise numeric value.

The current Environment values use the control:

- sky exposure and ambient intensity;
- fog start and density;
- height-fog base and top;
- cloud coverage, base altitude and thickness.

Typing remains available and can extend a slider's practical range. Transform
XYZ remains numeric because an arbitrary bounded slider would be misleading;
the viewport gizmo is its continuous visual control.

## Undo/Redo discipline

At drag start Studio captures the selected weather state. During dragging it
applies a direct temporary preview so the viewport responds immediately. On
release it restores the captured state and executes exactly one
`SetWeatherCommand` from before to after. One drag therefore produces one Undo
step. Numeric entry follows the same command path.

## Packaged acceptance

1. Inspector is dark and opaque in Transform and Environment modes.
2. Left and right splitters resize their panels without scene selection or
   gizmo input leaking through.
3. The drawer top edge resizes only while the drawer is open.
4. Minimum/maximum clamps preserve a usable viewport.
5. All three dimensions survive a complete Studio restart.
6. Every Environment slider previews continuously and its numeric box accepts
   precise values.
7. Each drag adds exactly one Undo step; Undo and Redo restore the correct
   before/after weather state.
8. Presets, toggles, save/reopen, clouds, fog, height fog and cloud shadows
   retain their previously accepted behaviour under DX12 and Vulkan.

Product-owner packaged review passed all correction checks before PR #7 was
merged to `main` at `beefe97`.
