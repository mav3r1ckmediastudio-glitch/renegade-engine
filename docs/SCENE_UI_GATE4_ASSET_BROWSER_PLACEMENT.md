# Scene UI Gate 4 — Asset Browser / Placement / Shared-Control Recovery

## Baseline

Gate 4 starts from post-Gate-3 `main`:

`98d68fb15c225dfb1e1e4db936ca054567791c4f`

Gate 3 is already owner-accepted and merged. Gate 4 does not reopen its
Inspector, Hierarchy or Scene-shell acceptance.

## Gate boundary

This is a Scene Editor remediation gate, not an asset-governance redesign.

Gate 4 repairs the creator-facing Asset Browser presentation and locks the
already-implemented end-to-end placement path. It deliberately preserves:

- LC01 stable identity and registry semantics;
- LP07 / LP08 asset catalogue and creator workflow policy;
- `.rasset` reusable-model products;
- preview-first model import;
- governed texture products and `ASSIGN BASE` semantics;
- the existing command stack, scene format and Wicked runtime integration.

The audit found the backend substantially healthier than the historical UI bug
reports. The remaining bounded regression is primarily presentation/readability,
so Gate 4 adds a read-only Asset Browser presentation seam instead of replacing
working lifecycle code.

## Readability recovery

Asset Browser creator controls now opt into their own typography policy without
changing the shared defaults used by Story Flow, Screen Editor, Project Hub or
the creator-import workspace:

- search: 12 px;
- tags: 12 px;
- state / format / rig filters: 11 px;
- compact action buttons: 10 px;
- folder names: 11 px;
- asset card names: 11 px;
- asset card metadata: 10 px;
- missing-thumbnail / folder fallback: 10 px.

The card footprint remains the accepted `148 x 112` pixels. Gate 4 does not
inflate the cards or drawer merely to fit larger text; the Gate 2 minimum drawer
still exposes a complete first row.

`RenegadeStudioAssetBrowserReadability.cpp` is presentation-only. It reads the
base browser's actual folder visibility, scroll rows, asset list and selected
path, then repaints only the small text-bearing regions. The base chrome remains
the sole owner of hit testing, selection, scrolling, folder collapse and drag
state.

Missing previews are represented honestly as `NO PREVIEW`; folder cards use
`FOLDER`. A missing thumbnail is not presented as a valid image.

## Audited creator workflow

### Import and reveal

The existing Asset Browser shortcut still routes model import through
`Action::ImportModel`, so models enter the accepted guided preview/material/
lighting/animation/thumbnail/final-import workflow rather than being silently
placed into the Scene.

After commit, `RevealCreatorAsset`:

1. validates StableId and project-relative path;
2. clears filters that could hide the newly imported asset;
3. refreshes the catalogue;
4. verifies the registered/current/placeable model product;
5. verifies the destination browser card is visible;
6. verifies the captured thumbnail can be loaded;
7. restores browser selection to that exact stable product; and
8. starts drag preparation for placement.

Gate 4 preserves this implementation rather than substituting a filesystem-only
browser path.

### Search, filters and actions

The existing query path remains authoritative for:

- name/tag/state/StableId text search;
- catalogue state;
- source format;
- skinned / animated / static model filtering.

`PLACE`, `ASSIGN BASE`, `REIMPORT` and `SAVE TAGS` continue to derive their
enabled state from the actual selected governed product and Scene selection.
Disabled controls are therefore meaningful rather than fake actions.

### Drag and placement

The existing `CreatorAssetDragPreview.cpp` path remains unchanged by Gate 4.
The audit confirms that it already provides:

- background reusable-product preparation and a bounded preparation cache;
- warm-up for visible placeable model cards;
- a live textured reusable-instance preview;
- object and terrain surface picking;
- Y=0 working-plane fallback when no object/terrain surface is hit;
- grounded placement through `ImportService::ResolveGroundedPlacementY`;
- queued release when background preparation has not yet finished;
- viewport-bound drop validation;
- Escape and right-click cancellation;
- cleanup of preview entities without leaving ghost Scene content; and
- final placement through `PlaceReusableModelCommand` executed by the Studio
  command stack, retaining Undo/Redo discipline.

Gate 4 does not duplicate or rewrite this placement implementation.

## Source regression contract

`RenegadeSceneUiGate4SourceContract` locks both sides of the recovery:

- readable Asset Browser typography and the read-only overlay must remain;
- accepted card geometry must remain bounded;
- import/reveal/thumbnail/filter/enable-state wiring must remain;
- drag preparation, queued drop, surface picking, grounding, cancellation and
  command-owned placement must remain; and
- the new readability source must stay part of the Renegade Studio target.

The contract intentionally uses semantic source anchors rather than fragile
indentation-sensitive blocks.

## Owner Release acceptance

Use one Release package after exact-head CI is green. One running build is
sufficient; resize the same window rather than requesting separate packages.

1. Open the Asset Browser at approximately `1280x720`, `1680x945` and
   `1920x1080`.
2. Folder names, card names, card metadata, search/tags and filter text are
   comfortably readable and do not overlap.
3. Existing thumbnails are visible. A missing thumbnail says `NO PREVIEW` and
   a folder fallback says `FOLDER`.
4. Search plus state / format / rig filters visibly change the shown assets.
5. Selecting a current placeable model enables `PLACE`; inappropriate products
   do not expose a fake placement action. A governed texture uses `ASSIGN BASE`
   only when there is a valid editable material target.
6. Import one model through the guided importer. After final commit, its card is
   revealed/selected in the Asset Browser with the captured thumbnail.
7. Drag a placeable model card into the Scene. The live preview follows the
   resolved surface, release places it, and Undo/Redo removes/restores it.
8. A cold drag/release while preparation is still running queues rather than
   silently losing the drop.
9. Escape, right click, or dropping outside the viewport cancels cleanly with no
   ghost preview entities.
10. Resize Hierarchy, Inspector and bottom drawer. The browser remains bounded,
    controls stay reachable, and the minimum drawer still shows one complete
    card row.
11. Reimport and creator-tag save retain their existing behaviour.

If these checks pass, Gate 4 is owner-accepted. Do not expand Gate 4 into Terrain,
Environment, grid/snapping or new asset-governance features.
