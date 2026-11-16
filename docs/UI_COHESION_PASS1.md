# UI cohesion — first pass (16 November 2026)

Isolated branch: `fix/ui-cohesion-pass1-20261116`, based on dialogue checkpoint
`7d4839ab1b048a8c423dbed76654b3d86d0eeac5`.
The owner authorized fixing the audited issues before designing toolbar icons.
This is a scoped first pass, not completion of all 65 audited UI families.

## Resulting behaviour

- Scene markers and capsule edges compose before the native GUI. Their draw pass
  uses viewport scissoring and a shared workspace/modal/loading visibility policy.
- Physics Lab uses the same composition policy. Its existing normal scene Render
  path remains; scene editor overlays are excluded while the lab owns the UI.
- Loading covers and chrome popups compose in a final foreground pass. File/Edit
  menus therefore cover Search Scene and native inspector controls consistently.
- Loading cover ownership also blocks background editor input, including the
  visible Ready/Idle handoff period after background work has finished.
- All twelve Add rows use one model, hit layout, renderer and release gesture.
  Press arms one row; matching release invokes once. Dragging away, Escape and
  focus loss cancel. A dismissal click cannot select the scene beneath a popup.
- Shared button faces reuse the dialogue interaction direction: clear hover,
  darker held state, two-pixel depression, release activation and disabled state.
  Existing native callbacks remain authoritative. Text uses font metrics and
  bounded UTF-8 ellipsis. Disclosure chevrons use geometry rather than optional
  font glyphs.
- Core Studio buttons, player/prefab controls and light/particle/dialogue action
  buttons share the renderer and opaque charcoal/orange tokens.
- Inspector background clearing now targets its own base Widget, avoiding
  Window::SetColor propagation into child controls. Native font shadow and
  background-blur styling are removed from the main editor theme.
- Metadata preset and Pickup / Item share one flow-owned row. Audio Scene Mix
  has a reserved heading row above Master, and hidden source controls consume
  no layout rows.

## Ownership and limits

No project schema, gameplay state, save format, engine scene or Lua API changed.
No toolbar icons are added. Existing File/Edit labels remain. Specialist trees,
canvas tools, sliders and all remaining audit surfaces require later passes.
No new Wicked patch, upstream upgrade or submodule pointer change is intended.
The six pre-existing Wicked overlay files were copied unchanged into the
isolated dependency worktree solely to reproduce the accepted dialogue build.
The original dialogue checkout, owner Studio session and concurrent arms work
are preserved. Native review uses a disposable copied project and preferences.

## Validation receipt

- Pure policy/gesture executable: PASS with g++ C++17 (all 64 surface-state
  combinations; release once; drag-out/different-row cancellation; Escape/focus
  cancellation; last Add row; held and disabled button offsets).
- Existing JP01 Physics Lab, Marker Icons and S1B Inspector source contracts:
  PASS before native build. Physics contract updated to require shared Studio
  composition, while retaining its normal-scene Render requirement.
- Native Release Studio compilation and the Windows policy executable: PASS.
- Six CTest suites PASS: UI policy plus dialogue core/assets/save policy/display/
  scripting. The 60 source contracts pass after updating obsolete Add-menu and
  typography expectations; the protected importer contract is retained unchanged.
- Native review initially confirmed menu coverage, all twelve Add rows, clean
  Physics Lab and Story Flow. The owner then found inspector occlusion, title/
  Character text overlap and numeric clipping. These prompted the corrections
  below. Scoped native verification of those corrections passed as recorded below.

Native acceptance targets: existing-project loading and Story Flow handoff;
Search Scene behind File/Edit; all twelve Add rows; Physics Lab without cyan
markers; prefab controls; Character/Pickup row; Audio heading; button hover,
held/release/cancel; ordinary scene marker selection; light/particle/dialogue
modal presentation; save/reopen of the disposable project.

## Owner-review corrections

Menu input no longer calls Activate on the entire chrome widget: that promoted
its opaque inspector backdrop above the actual inspector. While a popup owns
input, Studio updates the scene with ordinary GUI input suspended and updates
only chrome input. The final popup draw pass remains independent of widget
sorting. Specialist manual controls also pause until the popup is dismissed.
Selecting an asset must therefore expose its inspector immediately.

Inspector content width now reserves the native scrollbar column before the
bar appears. Scene/Player, Environment, Terrain/River, Material, Render and
Waterfall share this rule; numeric value boxes remain inside the content area.
Inspector titles use a bounded single line at 14 px with a full-title tooltip.
Character body text uses the common 12 px weight and is measured at the actual
content width, so its reason/status rows cannot resize over subsequent controls.
The protected importer keeps its original native control types for this pass;
its full interaction/typography migration remains a separate audited surface.

## Native acceptance checkpoint

Binary 8AC7907B8A63F38ACDC328BFD51DEE96D97B4A15548D8586BF8B55CA96AF0775
passed the reported core checks: complete numeric fields in narrow Environment,
Render and Player inspectors; immediate Player selection; newly placed Point
Light inspector without a panel click; clear Audio heading; matching twelve-row
Add popup; File covering Search Scene while the light inspector stays visible;
held Timeline action waits until release and toggles once. The test scene saved
with seven entities. See evidence/ui-cohesion-pass1/README.md and screenshots.

The text/grid follow-up binary is
429072F38556E50363F74723EB647A7198A2BEFB8BC1D050ACA97ADE7A4D16C7.
It adds narrow layer-grid wrapping and adequate Timeline status/help typography.
Native reopening preserved seven entities and two lights, complete Environment
numeric fields and the four-column narrow layer grid.

Final binary 9E7E371B96B0EDBF769535D210DB09F2E9E86780E7862499197072A0F1F221AD
also routes deliberate viewport selection back from Environment/Terrain/Render
to the selected asset Inspector, matching hierarchy selection. Native checks
from Environment and Render each selected the light with one viewport click and
immediately displayed LIGHT // Point Light, without clicking the panel. The
marker/frame-loop/Scene UI contracts pass after this correction. This latest
binary is open for owner review. No production project or owner unsaved state
was discarded. No hosted CI, push, PR, merge or package occurred.
