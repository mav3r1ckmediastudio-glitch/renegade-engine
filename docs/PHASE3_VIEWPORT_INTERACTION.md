# Phase 3 Viewport Interaction

## Outcome

Code commit `dc32684` turns the central scene view into an interactive editor
viewport:

- left-clicking rendered scene geometry selects its entity;
- clicking empty viewport space clears selection;
- viewport and hierarchy selection share `SelectionService`;
- the hierarchy row, inspector, translation gizmo, and cyan object silhouette
  follow the same selected entity;
- right mouse activates freelook only when initiated inside the viewport;
- W/A/S/D move relative to the camera while freelook is active;
- Q/E descend and ascend;
- Shift accelerates movement; and
- the mouse wheel changes movement speed while it is over the viewport.

The editor panels remain outside the viewport hit region and retain their
existing mouse and keyboard behaviour.

## Selection boundary

Studio uses Wicked's scene ray intersection for rendered object geometry, then
passes only the picked entity into Renegade's toolkit-independent
`SelectionService`. Panels do not keep separate selected-entity state.

The selected-object silhouette uses a reserved user-stencil value and a
Renegade-owned post-process mask. Studio restores the object's previous stencil
before Save As or scene reload and reapplies the editor marker afterward.
Selection therefore does not change the asset material or serialized WISCENE.

## Controls

| Input | Result |
|---|---|
| Left click | Select object under the pointer |
| Left click empty space | Clear selection |
| Hold right mouse + move | Freelook |
| W / A / S / D | Move forward / left / back / right |
| Q / E | Move down / up |
| Shift | Move four times faster |
| Mouse wheel | Decrease / increase camera speed |

## Windows acceptance

1. Launch the exact Release artifact through the DX12 launcher.
2. Open or create a project and enter the workspace.
3. Click three visibly separate scene objects.
4. For each object confirm the cyan silhouette, hierarchy, inspector, and gizmo
   identify the same entity.
5. Select a fourth object from the hierarchy and confirm the viewport outline
   follows it.
6. Click empty viewport space and confirm selection clears.
7. Exercise every camera control in the table.
8. Confirm clicking, scrolling, or typing over each editor panel does not move
   the camera or select geometry behind the panel.
9. Save As and Reopen with an object selected. Confirm the scene reopens without
   any permanent cyan material or outline.
10. Repeat steps 3-9 through the Vulkan launcher.

Required report:

```text
DX12 VIEWPORT SELECT PASS / OUTLINE PASS / CAMERA PASS /
SAVE ISOLATION PASS / VULKAN VIEWPORT PASS
```

A missing, displaced, or panel-obscuring outline is a visual failure even when
the build and automated tests pass.
