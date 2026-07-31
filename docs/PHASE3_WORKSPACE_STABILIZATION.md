# Phase 3 — Workspace stabilization

## Outcome

This patch turns the accepted Renegade-owned chrome proof into a stable editor
workspace without changing the EngineBridge or Wicked submodule boundaries.
It is the first workspace-stabilization pass, not a claim that every future
editor system already exists.

## Authority boundary

- The accepted Renegade Studio visual proof remains the authority for Studio
  layout, colour, density, controls and interaction.
- `Renegade_Brand_Guidelines_v1.0` is authoritative only for the official
  Renegade mark, wordmark lockup and its logo type treatment.
- No palette, page layout, UI typography or other interface direction from
  that brand document is approved for Studio by implication.
- Studio packages and renders the supplied official wordmark lockup. It does
  not redraw, respell or substitute the mark.

## Stabilized workspace

- Hierarchy names, headings, drawer text and Inspector controls use bold,
  high-contrast off-white text. Muted text is reserved for genuinely disabled
  or unavailable states.
- The Inspector host, section labels, fields, buttons, checkboxes, combo boxes
  and scrollbar are square Renegade-owned presentation. The global Project Hub
  theme is reasserted only outside this workspace boundary.
- Chrome hit regions consume their pointer press. Menu, tool, drawer and
  viewport-control clicks cannot leak through to scene selection or gizmos.
- File exposes Project Hub, Save, Save As and Reopen through the existing
  editor workflows.
- Edit exposes Undo, Redo, Duplicate and Delete.
- View exposes Focus Selection, editor-grid visibility and drawer destinations.
- Perspective, Lit and Show controls expose the states the current backend can
  actually provide. Orthographic and Unlit are visibly marked not exposed;
  they are not fake controls.
- Grid visibility and the open/closed drawer plus last active drawer tab use
  the existing editor-preference service and survive Studio restart.

## Drawer interaction contract

The Asset Browser/Console/Output/Diagnostics drawer:

1. opens from a tab;
2. closes when the active tab is clicked again;
3. closes from the visible chevron in either the tab strip or drawer header;
4. closes on Escape;
5. closes when the creator clicks outside it; and
6. remains open for clicks inside its content region.

The drawer content honestly reflects current backend coverage. Asset import,
console collection, build output and runtime diagnostics data models remain
future capabilities and are not simulated.

## Packaged acceptance

Required on Windows DX12 Release, followed by the Vulkan cross-check:

1. Official wordmark is crisp, correctly proportioned and not replaced by the
   previous placeholder mark.
2. Hierarchy, drawer and Inspector copy is readable at 1920×1080 and the
   compact-width breakpoint.
3. Transform and Environment inspectors contain no cyan rounded stock chrome.
4. All five drawer-close paths above behave as documented.
5. Chrome clicks never select a scene entity behind the control.
6. File/Edit/View actions execute their existing workflows and keyboard
   shortcuts continue to work.
7. Drawer and grid preference survive a complete Studio restart.
8. Transform, gizmo, weather, clouds, fog, cloud shadows, Undo/Redo and scene
   save/reopen regressions still pass.
