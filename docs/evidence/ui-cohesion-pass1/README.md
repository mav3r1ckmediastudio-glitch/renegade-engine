# UI first-pass native receipt — 16 November 2026

Review checkout: E:/RenegadeUI-20261116, isolated branch
fix/ui-cohesion-pass1-20261116. No owner production project was modified.

The eight initial screenshots in this folder were captured from native DX12
Studio binary SHA-256 8AC7907B8A63F38ACDC328BFD51DEE96D97B4A15548D8586BF8B55CA96AF0775.
They demonstrate:

- final-main: 280 px Environment inspector with complete numeric boxes beside
  the scrollbar, including negative values and fractional particle size.
- final-render: the top Render tab's right-hand inspector; full numeric boxes.
- final-player-selected: one viewport click selects Player Start and immediately
  displays the inspector without a separate click on the right-hand panel.
- final-button-held / final-button-released: a sustained Timeline header hold
  leaves it expanded and visually depressed; release collapses it exactly once.
- final-audio: Scene Mix has a separate heading row above Master.
- final-new-light-inspector: Add > Point Light, disable Keep placing, Place Light,
  then one viewport click creates/selects the new light and immediately shows
  its inspector. No right-hand-panel click was used.
- final-file-save-menu: File popup covers Search Scene; the light inspector
  remains visible without acquiring panel focus. The test scene saved normally.

That run also opened the Light Picker from the unified twelve-row Add menu.
The scene was saved with seven entities before normal close/reopen. Studio ran
at roughly the monitor's 75 FPS throughout these functional checks.

The text/grid follow-up binary is
429072F38556E50363F74723EB647A7198A2BEFB8BC1D050ACA97ADE7A4D16C7.
Its additional changes are bounded header padding, 12 px Timeline status/help
rows with adequate height, and four-column layer bits in narrow inspectors
(eight columns when sufficiently wide). Native reopening passed, as recorded
below; the eight earlier screenshots belong to the preceding binary. All four
directly relevant policy/source tests pass.

Automated validation: six CTest suites (UI policy and five dialogue suites)
pass. All 60 source contracts pass after retaining protected importer types and
updating old Add-menu/typography expectations. Latest modified Timeline/S1B/
Scene UI contracts and the policy executable pass again after text/grid changes.
This is local Release/native evidence, not hosted Debug/Release CI or universal
completion of the 65-family UI audit. Icons and remaining specialist UI
migrations are subsequent work.

Follow-up 429072F38556E50363F74723EB647A7198A2BEFB8BC1D050ACA97ADE7A4D16C7
native receipts:
- handoff-reopened-level: saved scene reopens with seven entities and two lights;
  Environment numeric fields remain fully visible.
- handoff-scene-grid: layer mask wraps cleanly into four columns at narrow width.

That run exposed a separate ownership case: the viewport marker changed the
selection while Environment retained its Inspector. SelectSceneObject now exits
Environment/Terrain/Render as the hierarchy callback already does. This final
routing correction built successfully as
9E7E371B96B0EDBF769535D210DB09F2E9E86780E7862499197072A0F1F221AD;
marker/frame-loop/Scene UI source contracts pass. Its native receipts are:

- routing-environment-before / routing-light-after: Environment is active, then
  one viewport click on the light activates Scene and displays LIGHT // Point
  Light immediately, without a panel click.
- routing-render-before / routing-render-to-light: Render is active with complete
  numeric boxes, then one viewport click displays the light Inspector immediately.

The final binary remains open for owner review. The disposable saved scene has
seven entities and two lights. These are scoped functional receipts, not a claim
that every specialist surface has completed migration.
