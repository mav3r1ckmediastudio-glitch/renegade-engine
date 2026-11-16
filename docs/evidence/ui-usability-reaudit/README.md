# UI usability re-audit evidence

Read with [the findings](../../UI_USABILITY_REAUDIT_20261116.md) and [65-family coverage](../../UI_USABILITY_COVERAGE_20261116.csv).

All captures use a disposable copy of the seven-entity DialogueReview level and isolated preferences. They establish only the observations recorded in the coverage CSV. No owner project or authored dialogue/Screen was modified.

Source inventory is pinned to `d04da64e42880521bed707dc0750de757b20b04e`, rather than the evolving working tree. See [normalized source fingerprints](source-inventory.json).

## Binary identities

| Capture build | SHA-256 |
|---|---|
| Icons6 | 83FC83470A2798ACA3E5398C7A17DE1C4DA4CD477435583C0509C9BA165EF6ED |
| Icons8 | 3B5E413C4A61956D37C4DE3957D45B9E2136EC260A1463F42DDA72C4515725E2 |
| Icons9 | 43D1C97E707BEF733EDCED29EB14FC3A1A2EA45D9B4BBB08C1A50C91B5101C01 |

Icons6 predates the modal correction. Icons8 includes that correction. Icons9 adds the Dialogue return correction. No claim about the concurrent wireframe work is made.

## Captures

| Evidence name / file | Exact capture build | Source capture |
|---|---|---|
| [light-small](light-small.png) | Icons8 | final-light-small |
| [texture-small](texture-small.png) | Icons6 | audit-texture-browser-1280 |
| [particle-small](particle-small.png) | Icons8 | final8-particle-small |
| [dialogue-small](dialogue-small.png) | Icons8 | final8-dialogue-small |
| [physics-scroll](physics-scroll.png) | Icons6 | audit-physics-scroll-4 |
| [hub-held](hub-held.png) | Icons8 | audit-hub-held |
| [new-project-small](new-project-small.png) | Icons8 | audit-new-project-small |
| [hub](hub.png) | Icons8 | audit-project-hub |
| [graph](graph.png) | Icons9 | audit-story-graph |
| [screen-editor](screen-editor.png) | Icons8 | audit-screen-small |
| [screen-animation](screen-animation.png) | Icons8 | audit-screen-animation |
| [numeric-tooltip](numeric-tooltip.png) | Icons8 | audit-number-commit |
| [numeric-undo](numeric-undo.png) | Icons8 | audit-number-undo-button |
| [panel-resize](panel-resize.png) | Icons8 | audit-splitter-widened |
| [menus](menus.png) | Icons9 | menus-contact-sheet |
| [audio-to-model](audio-to-model.png) | Icons6 | audit-audio-to-model |
| [audio-same-asset](audio-same-asset.png) | Icons6 | audit-audio-same-asset |
| [physics-target](physics-target.png) | Icons6 | audit-physics-target |
| [light-inspector](light-inspector.png) | Icons8 | final-light-select |
| [model-inspector](model-inspector.png) | Icons6 | audit-audio-to-model |
| [asset-browser](asset-browser.png) | Icons8 | audit-asset-browser |
| [drawers](drawers.png) | Icons8 | drawers-contact-sheet |
| [prefab-modal](prefab-modal.png) | Icons8 | audit-prefab-modal |
| [terrain-empty](terrain-empty.png) | Icons8 | audit-terrain-empty |
| [terrain-rivers](terrain-rivers.png) | Icons8 | audit-terrain-rivers |
| [render-main](render-main.png) | Icons8 | audit-render-main |
| [navigation](navigation.png) | Icons9 | audit-navigation |
| [nav-undo-held](nav-undo-held.png) | Icons9 | audit-nav-undo-held |
| [nav-undo-released](nav-undo-released.png) | Icons9 | audit-nav-undo-released |
| [player-inspector](player-inspector.png) | Icons8 | final-player |
| [journey](journey.png) | Icons9 | audit-journey-restored |
| [physics-world](physics-world.png) | Icons6 | audit-physics-world |
| [physics-rigid](physics-rigid.png) | Icons6 | audit-physics-rigid |
| [physics-constraint](physics-constraint.png) | Icons6 | audit-physics-constraint |
| [audit-identity](audit-identity.png) | Icons8 | final-identity |

The menus image crops the six menu popups into one comparison. The drawers image crops the Console, Output and Diagnostics bodies into one comparison. Other captures retain the native window. Physics world/rigid/constraint images exclude the unrelated notification seen in some later page captures.

The Terrain/Rivers captures show the no-terrain surface only. Particle effect presets were empty in the isolated fixture; the included Texture Library contained 50 textures. This does not establish that owner presets are missing.

Separate regression receipts retain the before/after cases: [modal resize](../ui-modal-resize/README.md) and [Dialogue return](../ui-dialogue-resume/README.md).

## Coverage reference rules

The CSV evidence_names values resolve to PNG basenames in this directory. Entries starting ../ui-dialogue-resume resolve to that sibling receipt directory. No native evidence is claimed for blank entries.

No hosted CI, push, PR, merge or release. The complete native/asset-state/DPI matrix remains explicitly open in the CSV.
