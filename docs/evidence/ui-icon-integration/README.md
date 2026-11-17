# Native icon integration receipt — 16 November 2026

All review used an isolated copied DialogueReview project, never an owner
production project. The original Studio session remained open and untouched.

## Captured binary provenance

The story-tooltip and physics-review images belong to the first icon binary:
70C61BBE157A5B27A9D71D4EA275289819FFAD2808833BD17C2068781E9AB95D.
Story Flow renders the approved icons and delayed Search title; the Physics
icon opens the lab, without cyan world markers over its UI.

The final-* images listed below belong to the next reviewed binary:
4058E547CE498D832E4A694742CE7C1318F57CD889B228B115849D53E2CE9D1F.

- final-main: compact Studio toolbar, StoryFlow return icon and complete
  Environment numeric fields.
- final-disabled-tooltip: the disabled Save control still exposes its title,
  description and existing Ctrl+S binding.
- final-render-held: Render is held/depressed while Environment stays active.
- final-render-cancel: release outside leaves Environment active.
- final-render-release: matching release opens Render with complete numeric boxes.
- final-narrow-1280 / final-narrow-1024: native window widths keep toolbar groups
  separated. The narrower layout uses 28 px transform controls and folds the
  optional Save/Undo/Redo buttons back into their existing text menus.
- final-file-menu: File covers Search Scene at narrow width.
- final-saved: normal File > Save writes the disposable scene before close/reopen.

These window-width checks do not certify Windows OS DPI at 125% or 150%.
The current physical display was used; additional OS DPI checks remain pending.

The keyboard check found a toolbar focus reset during GUI-priority suspension.
The follow-up retains keyboard focus while cancelling held input and tips; an
outside click relinquishes focus. Native tooltip ownership now also observes
the native exclusive GUI flag. Final binary/reopen receipts are recorded below.

## Keyboard and reopen checkpoint

CBFCCD5704810917E511E047A62CDFE6B4C4BFF5917647568BD0399A0B22F982
passed the final keyboard/reopen check:
- receipt-open-level / receipt-render: the saved seven-entity scene reopens;
  two lights remain and the complete Render numeric fields are visible.
- receipt-tab-focus: Tab moves focus from Render to Physics; the inset white
  focus outline is distinct from the orange active Render state.
- receipt-keyboard-result: Enter opens Physics Lab with no world markers over UI.

The handoff binary additionally enforces the manifest title when an existing
native tooltip description is replaced after SetIcon:
FD6C80E951ABAEAA49CABFFF32059028805C8A0BEE5BCB1F29B9D4C5E6958DB6.
It is built as RenegadeStudio-Icons3.exe. The exact final live receipts follow.

## Latest owner-correction binary

E6596CA2670F860C0F05B02AB7AA353BA1EF21034E4374D3AE32C7F491A8E2DC
(RenegadeStudio-Icons4.exe) is the latest live review build.

- owner-story-gap: the Fit button ends 9 logical px above the lower icon row.
  The shared header/canvas boundary moved together; all square icons retain size.
- owner-audio-with-perspective, owner-audio-with-lit, owner-audio-with-show:
  Audio remains visible and active with each dropdown open. Menus draw above
  the toolbar and the Audio Inspector stays visible.
- owner-ready-review: latest review window left open after dismissing the menu.

The existing owner-operated review window was not closed for these corrections.
The new instance has a separate copied project/preferences. All 67 selected
checks pass, including rebuilt Journey fixed/responsive layout tests. Native
evidence is scoped to its stated binary; OS DPI 125%/150% and owner signoff remain
pending. No hosted CI, PR, push, merge or release was performed.

## Audio selection handoff checkpoint

RenegadeStudio-Icons6.exe SHA-256:
83FC83470A2798ACA3E5398C7A17DE1C4DA4CD477435583C0509C9BA165EF6ED.
This binary was built before the implementation commit; the configure-time Git
stamp is not an exact release identity receipt.

- audio-selection-player-before: viewport pick shows Player Start inspector.
- audio-selection-mix: Audio opens with Player retained; global mix is visible
  and scene prefab controls are hidden.
- audio-selection-light: one viewport click on Point Light immediately replaces
  Audio with LIGHT // Point Light. No inspector-panel click occurred.
- audio-selection-model-category: Audio was reopened, then Models expanded;
  category expansion keeps Audio open until an asset is selected.

The process closed before the hierarchy model-row, repeated same-asset and
Physics target checks completed; no native pass is claimed for those cases.
The code routes both hierarchy paths and completed box selection through the
same handoff. All 67 selected regressions pass. No owner project was modified.
The existing Icons5 window was preserved and remains the previous correction.
