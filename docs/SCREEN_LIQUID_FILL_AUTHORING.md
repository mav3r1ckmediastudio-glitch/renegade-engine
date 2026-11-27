# Create a liquid fill or another masked gauge

Use the Player HUD Screen for an indicator that appears during gameplay. A Title Screen is a menu canvas; it does not supply a possessed player's live health.

## Inspect the supplied vial

Choose Elements, search for Animated vial and insert it. In Layers, expand its root:
- The root positions and sizes the composition.
- Liquid is an ordinary Progress layer with a masked effect and an optional gameplay binding.
- Glass and metal is a separate Image layer. The supplied cork, glass and metal are combined in one PNG; they are not separate editable artwork layers.

Select a part in Layers to edit it. Selecting Liquid exposes Fill settings. Selecting Glass and metal exposes the ordinary image controls. Moving/resizing the root scales the composition; editing a child affects that child. For a group with several liquid layers, select the intended Progress layer explicitly.

Choose Health in Fill settings to bind to the possessed player's health.current / health.maximum. Save. Add Health text - current Player from Elements for a numeric readout. In the editor, live health may be unavailable until a Player is possessed in gameplay; use Preview fill to inspect appearance without changing saved health.

## Build a different gauge

1. Choose Elements > Basic > Liquid Fill. This creates one editable Progress layer with no gameplay binding.
2. Set its position and dimensions in the ordinary Inspector. Set Fill value from 0 to 1 for a manually authored value.
3. Open Fill settings and choose your mask. White defines the fill shape; black or transparent pixels remain empty. PNG preserves transparent edges. A grayscale mask works best; coverage uses its red channel multiplied by alpha.
4. The importer fits bounds automatically. Fit area to mask recalculates them; the four percentage fields permit deliberate adjustment. Mask images must be readable PNG, BMP or JPEG, at most 4096 pixels per side, and contain a visible fill region.
5. Choose a direction: left to right, right to left, bottom to top or top to bottom.
6. Choose a colour. Use wave height 0 and bubbles 0 for a clean battery/shield gauge. Toggle animation for moving liquid.
7. Leave Current blank for manual fill, or enter an existing gameplay attribute. With Maximum supplied, the fill is Current / Maximum; with Maximum blank, Current should already be 0..1. Health/Mana/Strength presets select those possessed-player attributes; they do not create new gameplay systems.
8. Add separate ordinary Image layers for a frame, glass, scratches or other artwork, and Text layers for labels. Order layers normally. Put related parts in a group before saving a reusable composition.

The installed Content/UI/Examples/BatteryMask.png is a small technical example: a white battery silhouette with a transparent exterior. Try it at 100% to see the entire shape, then 65% in each direction. At partial fill, an optional separate frame Image can keep the empty part of a gauge visible.

## Preview, save and reuse

Preview fill temporarily changes this layer's rendering only. It neither changes the saved Progress value nor damages/heals the player. Cancel restores the authored rendering. Importing a mask or sound may leave a project-owned imported asset even when Cancel discards the settings.

Save applies settings as one Undo operation. The effect can be switched off to return to an ordinary Progress element; Undo restores it. Screen Save/reopen retains mask, bounds, direction, colour, binding and effect settings.

Select the whole group/root, then Elements > Reusable > Save selected as UI prefab. Saved UI prefabs can insert it again or import a package manifest. New instances receive fresh widget identities and destination-owned resources. Cross-project bindings require explicit assignment.

This authoring path supports masked fills and independent artwork layers. Condition thresholds, progressive armour damage overlays and the creation wizard remain separate planned work.
