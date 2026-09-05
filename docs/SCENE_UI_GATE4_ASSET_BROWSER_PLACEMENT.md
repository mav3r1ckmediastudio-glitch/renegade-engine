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
reports. Gate 4 therefore keeps the accepted asset-governance and placement
services and repairs the creator-facing presentation/lifecycle seams around them.

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
path, then repaints the small text-bearing regions and the thumbnail well after
base chrome. The base chrome remains the sole owner of hit testing, selection,
scrolling, folder collapse and drag state.

### Thumbnail aspect preservation

The audit confirmed that the base Asset Browser previously drew every captured
thumbnail directly into the fixed `134 x 68` preview rectangle. Because it did
not inspect the source texture dimensions, any thumbnail whose native aspect
ratio differed from that rectangle was stretched or squashed.

Gate 4 corrects that presentation defect without changing capture or asset
storage. The readability overlay now:

1. clears the fixed preview well back to its neutral dark surface;
2. reads the captured thumbnail texture's native width and height;
3. calculates the source and target aspect ratios;
4. scales the thumbnail uniformly to fit completely inside the `134 x 68`
   preview area;
5. centres the result horizontally and vertically; and
6. leaves unused space as neutral letterbox/pillarbox background.

No thumbnail is cropped and no axis is scaled independently. The Asset Browser
therefore presents the same proportions that were captured by the importer.
Existing stored thumbnails benefit immediately because the correction is in the
browser renderer rather than the capture transaction.

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

### Save / leave / reopen texture persistence regression

Owner runtime review found a blocker after the initial Gate 4 candidate: a
textured reusable model was correct immediately after placement, but after
saving the Level, leaving it and reopening it, the model returned without its
textures.

The persisted data was not being lost. Renegade's governed texture contract
intentionally stores stable texture asset IDs in each material's serializable
WISCENE metadata while leaving Wicked `TextureMap::name` empty. The live Wicked
`Resource` handle is not serializable. Consequently every loaded Level must run
`RestoreMaterialTextureBindings()` against the active project to resolve those
stable IDs back to governed `.rasset` payloads and recreate the live texture
resources.

LP08 Gate 3 originally wired that idempotent restore into
`CreatorAssetStudioChrome::Update()`. The hook was later lost while the creator
chrome was reworked; placement still restored textures before merging the
prepared reusable model, which hid the omission until the next Save/Open cycle.
Gate 4 restores the established lifecycle hook. Current restoration already
deduplicates repeated StableIds and skips already-live resources, so the normal
per-frame Studio call does not reload a healthy texture on every frame.

This is deliberately a lifecycle wiring repair, not a new material persistence
format. Existing saved Levels that still contain their governed stable-ID
metadata can rehydrate again when opened; they do not require asset reimport or
texture reassignment.

### Final owner-found importer and hierarchy regressions

The final owner Release pass found five remaining presentation/state regressions.
The Gate 4 candidate now also includes all five corrections:

1. the importer human scale-reference artwork is restored to the last known
   visible transparent asset;
2. the final Import page reflows the `THUMBNAIL & IMPORT`, thumbnail preview,
   capture, status, confirm and cancel block below the Asset Name and
   `Content/Models` fields so those controls no longer overlap;
3. closing/cancelling the importer re-submits the current Scene workspace action
   after preview-weather restoration so Environment and Terrain specialist
   controls cannot remain simultaneously exposed;
4. hierarchy parent rows now have disclosure state, with `Reusable Asset Instance`
   roots collapsed on first sight and expandable/collapsible by the user; and
5. the Scene Hierarchy scrollbar retains wheel support and now owns an LMB
   press-drag-release gesture with a wider invisible hit target around the
   narrow visual thumb.

These corrections are intentionally bounded to importer presentation/lifecycle
and Scene Hierarchy interaction. They do not alter reusable-model placement,
asset identity, scene serialization or command ownership.

## Source regression contract

`RenegadeSceneUiGate4SourceContract` locks both sides of the recovery:

- readable Asset Browser typography and the read-only overlay must remain;
- accepted card geometry must remain bounded;
- thumbnail rendering must use native texture dimensions, uniform aspect-fit
  scaling and centered letterbox/pillarbox space;
- creator chrome update must rehydrate governed material textures from the
  active Scene/project stable-ID metadata after load;
- import/reveal/thumbnail/filter/enable-state wiring must remain;
- drag preparation, queued drop, surface picking, grounding, cancellation and
  command-owned placement must remain; and
- the new readability source must stay part of the Renegade Studio target.

The contract intentionally uses semantic source anchors rather than fragile
indentation-sensitive blocks. Existing LP08 material-texture tests remain the
service-level proof that WISCENE preserves stable IDs and that an explicit
restore reconstructs the live resource; Gate 4 now also protects the Studio
lifecycle call that had regressed.

## Owner Release acceptance

Use one Release package after exact-head CI is green. One running build is
sufficient; resize the same window rather than requesting separate packages.

1. Open the Asset Browser at approximately `1280x720`, `1680x945` and
   `1920x1080`.
2. Folder names, card names, card metadata, search/tags and filter text are
   comfortably readable and do not overlap.
3. Existing thumbnails are visible and retain the same proportions as the
   thumbnails captured in the importer; tall, square and wide subjects must not
   appear stretched or squashed. Unused preview space may letterbox/pillarbox.
4. A missing thumbnail says `NO PREVIEW` and a folder fallback says `FOLDER`.
5. Search plus state / format / rig filters visibly change the shown assets.
6. Selecting a current placeable model enables `PLACE`; inappropriate products
   do not expose a fake placement action. A governed texture uses `ASSIGN BASE`
   only when there is a valid editable material target.
7. Import one model through the guided importer. After final commit, its card is
   revealed/selected in the Asset Browser with the captured thumbnail at the
   correct aspect ratio.
8. Drag a textured placeable model card into the Scene. The live preview follows
   the resolved surface, release places it with its textures, and Undo/Redo
   removes/restores it.
9. Save the Level, leave it for Story Flow, then reopen the same Level. The
   placed model must retain every governed material texture without reimport,
   reassignment or touching the Asset Browser.
10. Use the normal Scene Reopen/Open lifecycle where practical and confirm the
    same textured model rehydrates there as well.
11. A cold drag/release while preparation is still running queues rather than
    silently losing the drop.
12. Escape, right click, or dropping outside the viewport cancels cleanly with
    no ghost preview entities.
13. Resize Hierarchy, Inspector and bottom drawer. The browser remains bounded,
    controls stay reachable, and the minimum drawer still shows one complete
    card row.
14. Reimport and creator-tag save retain their existing behaviour.
15. Open the model importer and confirm the 1.82 m human reference is visible;
    on the final Import page, `Content/Models` and the thumbnail/capture block do
    not overlap.
16. Cancel the importer while Environment or Terrain is active. The Inspector
    must return to exactly one active specialist panel without requiring an
    extra heading click.
17. Reusable Asset Instance roots begin collapsed, can be expanded/collapsed,
    and long descendants remain bounded inside the Scene Hierarchy.
18. With enough hierarchy rows to show the scrollbar, drag its thumb with LMB
    and confirm the visible row range tracks the drag while mouse-wheel scrolling
    still works.

If these checks pass, Gate 4 is owner-accepted. Do not expand Gate 4 into Terrain,
Environment, grid/snapping or new asset-governance features.
