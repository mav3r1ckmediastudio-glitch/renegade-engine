# Scene UI Gate 3 — Inspector, Hierarchy and Typography Recovery

## Baseline and dependency

Gate 2 passed owner verification and was squash-merged to `main` as:

`caaa56230bf1cede49e6dd23041963d4e21ce143`

Gate 3 is reconciled onto that exact merged baseline before it is exposed as a pull request or CI candidate.

## Gate objective

Recover creator-facing Scene Editor readability without reopening accepted Story Flow, Screen Editor, Asset Browser or rendering architecture.

This gate addresses:

- unreadably small Scene Inspector controls;
- hierarchy readability and visual hierarchy;
- small Scene-owned shell labels directly adjacent to Scene authoring;
- long status/error text colliding with the FPS/ownership group;
- preserving existing Inspector scrolling/reachability and command wiring while typography changes.

## Explicit non-scope

Gate 3 does **not**:

- redesign Story Flow or Screen Editor UI;
- globally increase the shared Renegade control defaults;
- redesign Asset Browser cards, metadata, filters or placement — Gate 4 owns that work;
- rewrite Environment or Terrain authoring behavior — Gate 5 owns deep functional verification;
- alter Scene format, `.rasset` governance, asset identity, Wicked rendering, terrain generation or construction-grid architecture.

## Typography policy

The accepted shared Renegade controls keep their existing defaults so already-accepted workspaces do not change implicitly.

Scene Inspector opts into dedicated readable wrappers:

| Surface | Text size |
| --- | ---: |
| Inspector title | 16 px |
| Inspector section headings | 13 px |
| Scene Inspector text inputs | 12 px |
| Scene Inspector combo boxes | 12 px |
| Scene Inspector slider labels | 11 px |
| Scene Inspector slider numeric values | 11 px |
| Scene Inspector check boxes | 11 px |
| Scene Inspector buttons | 11 px |
| Hierarchy title | 12 px |
| Hierarchy entity names | 12 px |
| Hierarchy categories/search | 11 px |
| Scene chrome secondary authoring text | minimum 10 px |

The previous shared defaults remain 10 px for text/button/combo/check box and 9/10 px for slider label/value. Asset Browser card typography remains unchanged for Gate 4.

## Inspector ownership

The existing right-docked `inspectorPanel_` remains the functional control host. Gate 3 changes the Scene Inspector control types, not their callbacks, services, transactions or workspace ownership.

Existing layout and scroll content remains authoritative:

- Transform controls stay in the Scene Inspector;
- Light controls stay in the selected-light Inspector;
- Environment controls continue through the existing long scrollable Environment layout;
- Terrain controls continue through the existing Terrain layout;
- Scene/Environment/Terrain mutual exclusion from Gate 2 remains unchanged.

This prevents a typography recovery from becoming a functional rewrite.

## Status and error reporting

The Scene footer remains a compact single-line summary, but it may no longer render underneath the right-side FPS/ownership group.

Gate 3 derives a width-aware character budget from the fixed right-side boundary and ellipsizes only the footer presentation. The complete `statusText_` remains intact.

When Output is open, the raw status string is wrapped to the available drawer width and rendered across multiple lines without ellipsizing. This gives long build/status errors a genuinely readable presentation rather than merely preserving the string in memory.

## Hierarchy recovery

The hierarchy keeps its existing 28 px rows and scrolling behavior to avoid wasting viewport space. Readability improves through typography rather than inflating every row:

- title: 12 px;
- category/search: 11 px;
- entity names: 12 px;
- secondary arrows/icons/counts: minimum 10 px.

Selection reveal, category collapse, filtering and scrolling remain the existing implementation.

## Source contract

`RenegadeSceneUiGate3SourceContract` locks:

- backwards-compatible shared control defaults;
- Scene-only readable Inspector wrappers;
- representative Transform, Light, Environment and Terrain controls using those wrappers;
- Project Hub and creator-import controls retaining independent typography;
- existing 16/13 px Inspector heading hierarchy;
- existing Inspector layout ownership;
- 10/11/12 px Scene chrome hierarchy;
- width-bounded status footer rendering;
- wrapped, unellipsized raw status presentation in Output.

The contract is intended to catch regression of the architectural boundary, not substitute for owner visual acceptance.

## Required resolutions

The eventual consolidated owner Release check must include:

- 1280 × 720;
- 1680 × 945;
- 1920 × 1080.

## Single owner Release acceptance

After Gate 3 is reconciled to accepted Gate 2, source-audited, squashed to one candidate and passes one authoritative four-check CI set, one Release build is sufficient for owner verification.

Check:

1. Select ordinary entities and confirm Transform labels, X/Y/Z fields and action buttons are comfortably readable.
2. Select a light and confirm type, sliders, check boxes and values are readable with no overlap.
3. Open Environment, scroll through the full Environment/Sun/Ocean content and confirm labels, values and controls remain readable and reachable.
4. Open Terrain, scroll through Terrain/Material/Sculpt controls and confirm readability and reachability.
5. Switch Scene → Environment → Terrain → Environment → Scene repeatedly and confirm no specialist controls merge or remain stale.
6. Exercise a populated/deep Hierarchy: category collapse/expand, search/filter, scrolling, selection and viewport-driven selection reveal.
7. Trigger or observe a long status/error message. Confirm the footer stays inside its allotted area and Output presents the complete message as readable wrapped lines.
8. Resize Hierarchy and Inspector at all three required resolutions and confirm typography does not create clipping/overlap.
9. Return to Story Flow and confirm accepted Story Flow typography/interaction has not changed unexpectedly.
10. Open Asset Browser only as a regression smoke check; card/detail redesign is intentionally deferred to Gate 4.

## CI policy

Gate 3 follows the remedial build policy:

1. complete the bounded implementation;
2. source-audit all promised behavior;
3. run focused source contracts without using CI as an interactive debugger;
4. reconcile to final Gate 2 / `main`;
5. squash the development trail to one candidate commit;
6. open one draft PR;
7. use one four-check authoritative CI pass as candidate proof;
8. provide one consolidated owner Release candidate.

No build/patch/build loop is planned.
