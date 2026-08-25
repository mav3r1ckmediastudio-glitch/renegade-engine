# Story Flow Gate 9F — UX Hardening and Gate 9 Acceptance

**Status:** implementation candidate; exact-head CI and owner packaged visual
acceptance required.

## Scope boundary

Gate 9F hardens the accepted Journey/Graph product. It does not redesign the
Journey, add presentation-only chapters/groups, implement auto-splice, or alter
Runtime Flow semantics. The owner-approved concept remains the hierarchy and
density reference, subject to the documented interim pre-release fidelity
revisit.

## Control audit

| Surface | Gate 9F behaviour |
|---|---|
| Select | Returns to the Journey selection surface. |
| Arrange | Fits the active Journey or Graph presentation. |
| Filter | Toggles detached Journey content; visibly unavailable in Graph. |
| Search | Opens the real name search and focuses the active view. |
| Preview / Test Play | Saves dirty Flow first and launches the governed project Runtime; save failure blocks launch. |
| Validate | Exposes the current authoritative diagnostics in the Inspector. |
| Saved / Undo / Redo | Reflect and operate on shared Flow history. |
| Project selector / Hub | Opens the governed Project Hub. |
| Levels / Screens | Open the Journey-native governed destination composer. |
| Assets | Opens the existing governed Asset Browser workspace. |
| Variables | Visibly `NOT YET`; selecting it explains that the project-variable workspace is unavailable. |
| Settings / Main Menu | Visibly `N/A`; selecting either reports its staged unavailability. |
| Zoom / Fit / Start | Operate on Journey only and remain fixed in screen space. |
| Journey / Graph | Switch synchronized views over the same Flow document. |
| Add Action / destinations | Create and rewire real stable-ID routes in Journey. |
| Open Editor | Uses the governed Level or Screen lifecycle handoff. |

The former Inspector pseudo-tabs and decorative close glyph are absent. No
surfaced control may silently consume input without performing its documented
operation or explaining its unavailable state.

When Add Action is unavailable, its own label explains why: `TERMINAL`,
`ENTRY SET`, `NO ACTIONS`, `LIMIT REACHED` or `UNAVAILABLE`. The creator is not
left with an inert button carrying an apparently valid command label.

## Diagnostic and readability contract

- toolbar, rail, overview and Inspector text uses an 8–14px hierarchy;
- secondary Inspector text remains high contrast;
- internal diagnostic codes are not creator-facing copy;
- unreachable content names the affected destination and Game Start;
- Status and visible Validation messages preserve their complete text and wrap
  at word boundaries; long identifiers split safely instead of ellipsizing;
- Graph places Status directly below Validation rather than at the remote
  bottom edge of the Inspector; Journey reserves a non-overlapping wrapped
  message stack above its fixed lower controls;
- compact height shows the available messages plus an honest remaining count;
- status copy is separated from its heading and uses readable text;
- 1280x720 and 1920x1080 keep cards, controls and overview out of the Inspector.

## Automated acceptance

The Journey recovery source contract pins the authoritative logo hash and
rejects the old wordmark, additive logo blending, raw diagnostic codes,
Status/Validation ellipsizing,
decorative Inspector controls, legacy lifecycle panels, type-coloured card
frames, tiny zoom, zoom-scaled overview chrome and Journey-to-Graph exit
routing. The layout test verifies both target resolutions, the hard
Journey/Inspector boundary and non-overlapping fixed navigation/overview hosts.

Preview must save dirty Flow, remain blocked if the document is still dirty,
and then call `RequestProjectPlayFromStoryFlow`; no LP04 Test Level snapshot is
created for project Preview.

## Packaged owner acceptance

Use the exact-head Release artifact at 1920x1080 and 1280x720:

1. Confirm the supplied fractured-crest logo is complete, undistorted and
   correctly transparent.
2. Confirm toolbar, rail, card, Inspector, Validation, status and overview text
   is comfortably readable.
3. Exercise Select, Arrange, Filter, Search, Validate, Undo/Redo, zoom, Fit and
   Start; confirm visible state follows each operation.
4. Confirm Variables, Settings and Main Menu visibly report that they are not
   available rather than behaving as dead controls.
5. Make a Flow edit and choose Preview. Confirm Flow saves first and the
   governed project Runtime launches.
6. Confirm an unreachable card is named in plain language under Validation.
7. Zoom and pan at both resolutions. Confirm cards never render beneath the
   Inspector and the navigation/overview frames never scale.
8. Switch to Graph and back. Confirm topology, selection and routes remain
   synchronized and Filter is unavailable in Graph.
9. Save, close and reopen. Confirm the same Journey/Graph topology and layout
   return.

Green CI is necessary but is not Gate 9 acceptance. A failed owner screenshot
or interaction overrides all automated results. PR #100 remains draft until the
owner explicitly accepts the packaged candidate and authorizes the normal merge
decision.
