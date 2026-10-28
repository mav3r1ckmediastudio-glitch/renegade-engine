# River inspector selected-entity repair — 2026-10-21

## Owner-observed failure

Owner selected River Water 7 in SELECT and saw the generic Transform / Character /
Materials inspector. Switching to Terrain immediately deselected the river,
leaving flow and water-colour controls disabled; clicking controls did nothing.
Owner observation overrides prior bridge command/unit-test success.

## Root cause and code correction

- `StudioRenderPath::SetTerrainWorkspaceActive(true)` automatically selected the
  first terrain entity on every entry. This invalidated the currently selected
  WATER spline for every river-only editor command.
- Preserve a selected native WATER spline or its point descendant when changing
  to the Terrain workspace. Terrain creation/sculpting still resolve the first
  terrain carrier independently; other entity-to-Terrain selection behaviour is
  unchanged.
- `LayoutTerrainSections` also exposes a dedicated RIVER // SELECTED PROPERTIES
  layout in the SELECT inspector, with independently collapsible Riverbed & Banks,
  Flow & Direction, and Water Colour & Clarity sections, initially open in SELECT.
  The persisted Terrain toolbox disclosures remain independent and unchanged.
- Generic inspector Transform/Materials/S1B rows do not obscure selected river
  properties. Existing river material/terrain-layer widgets are reused, not
  shadow copies.
- QueueSelectedRiverAppearance/Flow/Layer accept both SELECT and Terrain,
  checking the actual selected WATER spline and safe scene revision before
  applying command-backed Undo/Redo. No non-water entities gain editing rights.
- Successful new-river paint selection refreshes inspector immediately to
  activate the river material controls.
- Controls target Wicked native WATER RGB, depth-haze clarity, and animated
  normal-map UV direction/speed. No foam shader, physical current or surface
  opacity shader is claimed by this fix.

## Acceptance gate

A native build and source/CTest checks are necessary but not sufficient.
In an isolated project: create river, click flow/colour/clarity in SELECT,
save/reopen, test Undo/Redo, switch to Terrain and back while keeping river
selected, edit a point descendant, inspect visual results during Test Game.
Do not mark owner-accepted until these exact interactions work visibly.

Do not touch the accepted alpha, main, owner scene or running Studio; build an
independently named Studio candidate for review. No merge or push authorised.
