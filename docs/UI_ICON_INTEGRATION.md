# Approved Studio icon integration — 16 November 2026

Based on UI cohesion checkpoint `9eb8410` on `fix/ui-cohesion-pass1-20261116`.
The owner approved the complete icon suite in Design Renegade Icon System and
authorized integration. This changes Studio presentation and toolbar input only.

## Artwork and rendering

All 65 original SVG masters, the supplied manifest and design specification are
in `Studio/assets/icons`. One padded atlas contains their supplied 48 px PNG
versions. Every tile was compared byte-for-byte as RGBA pixels with the approved
ZIP; all 65 match. The source ZIP SHA-256 is
`7ad7ac523cf367ffa486d71f61431a8aedd55e23e580d5cbd7cc6e4e7890388c`.

StudioIcons loads this texture once and draws its source rectangles through
Wicked's existing DPI-aware image renderer. Artwork is never replaced with
Unicode or an icon font. All assets are installed under Content/ui/icons.
No project adopts these editor-only resources and the existing real logo stays.

## Controls and ownership

Studio transform tools, Undo/Redo/Save, Test Game and the Scene/Environment/
Terrain/Render/Physics workspace controls now use square 32 logical px buttons.
Below 1120 logical px, transform buttons use the approved 28 px compact size;
Undo/Redo/Save remain available through File/Edit and their existing shortcuts.
The main menus retain their text labels.

Story Flow uses the same artwork and palette for its command bar, utilities,
navigation rail, view switches, existing terminal actions and Fit/Start
navigation. The Level Editor return button uses the StoryFlow icon. The complete
65-icon catalogue is available; icons for commands with no existing UI/action
are not exposed as invented capabilities. Graph semantics, role/route colours
and authored runtime Screens remain unchanged.

Custom chrome uses one control descriptor for drawing and hit bounds. A matching
release dispatches once; release outside, Escape, lost input and disabled states
cancel. Native controls retain their existing release callbacks and handlers.
Hover and held states differ from persistent orange selection. Keyboard focus
uses a separate inset outline. Pointer-only workspace resets cancel a held
gesture without unnecessarily clearing toolbar keyboard focus.

Every icon has a delayed title tooltip. Existing descriptions and real bindings
are retained. Tooltips have opaque dark backgrounds, clamp to canvas edges and
do not own pointer input. Popup/modal ownership suspends them. Disabled native
icon controls retain their titles while an exclusive GUI owner suppresses tips.

## Validation scope

Release Studio built successfully. The selected regression command is:

```powershell
ctest --test-dir BUILD/native -C Release -R 'SourceContract|StudioUiPolicy|RenegadeDialogue|^RenegadeStoryFlowJourneyUiLayoutTests$' --output-on-failure
```

All 67 selected tests pass: 60 source contracts plus UI policy, five dialogue
suites and the existing Journey UI layout suite. Four existing contracts were updated from obsolete permanent
text-label/return-button geometry expectations to the icon renderer, disabled
utilities, existing Render action and compact return geometry. The Audio/menu contract now retains input blocking and foreground composition
without requiring the Audio button to disappear. Protected-importer guards remain.

Native evidence and exact binary hashes are recorded in
`evidence/ui-icon-integration/README.md`. Review uses a disposable copied project
with seven entities and two lights. Real Windows DPI settings at 125% and 150%
are separate from the native narrow-window checks; do not describe window-size
checks as OS DPI certification. Independent owner acceptance and hosted CI remain
pending. No push, PR, merge, release or package is performed by this task.

## Build receipt

Configure: cmake -S . -B BUILD/native -DRENEGADE_EMBED_SHADERS=ON.
Native build: MSBuild BUILD/native/Studio/RenegadeStudio.vcxproj /t:Build
/p:BuildProjectReferences=false /p:Configuration=Release /p:Platform=x64
/p:TargetName=RenegadeStudio-Icons3 /v:minimal, with CL=/MP6.
Exit 0. Existing nodiscard warnings remain; no new compile error remains.

Atlas source and installed resource SHA-256 both match:
F358668F7E798B9F57A90E95CD9B6F05AAFD9C042DC7DEE37C6A2D36673B9F51.
The preceding title-tooltip executable SHA-256 is
FD6C80E951ABAEAA49CABFFF32059028805C8A0BEE5BCB1F29B9D4C5E6958DB6.


## Owner spacing and Audio corrections

JourneyWorkspaceHeaderHeight is shared by the shell, workspace and render path.
Its 192 logical px header gives the 32 px Fit button a 9 logical px gap above the
lower icon row, while keeping the header/canvas hit and scissor bounds aligned.
Existing fixed/responsive Journey layout tests were rebuilt and pass unchanged.

Audio remains drawn while Perspective/Lit/Show menus own input. The existing
foreground popup pass draws the menu above the persistent button; exclusive input
ownership still prevents clicks leaking through a dropdown.

The owner-correction executable is RenegadeStudio-Icons4.exe:
E6596CA2670F860C0F05B02AB7AA353BA1EF21034E4374D3AE32C7F491A8E2DC.
It uses the same Release Build command with TargetName=RenegadeStudio-Icons4.
The latest combined 67-test command passes (9.73 s, exit 0).

## Specialist inspector context correction

Opening Audio retains a Scene workspace internally. The selected-object toolbar
had checked only Scene mode, so Group/Edit Pieces/Save Prefab/Unpack appeared
whenever an asset was selected. DrawSelectionGesture now excludes Audio,
particles, navigation and Physics from that toolbar, its scene-editing hints
and the assembly-editing footer. HandleAssemblyToolbar excludes the same
specialist contexts, so hidden controls cannot respond to pointer input.
Viewport box-selection feedback remains available.

Release build RenegadeStudio-Icons5.exe passed, SHA-256:
ACFD66371311AC6096A27BBB8DBD75AE406840E248A44946BCE4ED6CD5562EF3.
The same 67 selected checks pass (2.75 s, exit 0). The owner then confirmed
the reported issue was fixed. No additional exact-binary screenshot claim is
made for this correction; the earlier receipts keep their original provenance.
The existing owner-operated window with unsaved edits was preserved. The new
review instance uses a separate copied project and preferences.

## Selected-asset inspector handoff

The previous Audio correction suppressed scene prefab controls, but did not
release Audio's inspector ownership on a new asset selection. Explicit viewport
picks, both hierarchy paths and completed box selections now notify the chrome
after updating the authoritative selection. The chrome releases Audio, legacy
particle and navigation ownership, then chooses the matching specialist for a
sound, legacy particle or navigation entity. Ordinary assets use the existing
Scene inspector immediately. Render ownership is also cleared by the modern
hierarchy path, consistent with viewport selection.

Opening Audio with an existing ordinary selection still shows the global mix;
it is the explicit selection event that hands ownership back. Empty-background
clicks keep that mix open. Physics Lab retains hierarchy targets for its
authoring workflow. No scene inventory scan or second selection model is added.

Release candidate: BUILD/native/Studio/Release/RenegadeStudio-Icons6.exe,
SHA-256 83FC83470A2798ACA3E5398C7A17DE1C4DA4CD477435583C0509C9BA165EF6ED.
The same 67 selected checks pass (12.06 s). Native proof: select Player Start,
open Audio (mix stays open with the retained selection and prefab controls
hidden), click Point Light in the viewport: LIGHT // Point Light appears
immediately, without clicking the panel. Reopening Audio also remains possible.
Expanding Models leaves Audio active, as intended for a category-only click.

The review process closed before the model-row, same-asset and Physics native
checks could complete. Those are not claimed as native passes. No Application
Error/Windows Error Reporting event was found during the check; the exit cause
was not established. The other open Icons5 instance remains the previous
toolbar-only correction and does not contain this routing fix. Native proof
used a separate copied project/preferences. Owner acceptance remains pending.

## Modal resize failure found during the usability audit

A native 1280x720 shrink/restore with the Light Picker open reproduced main
viewport stripes and stale inspector geometry. Light/Particle/Dialogue now share
the safe opaque 2D composition boundary; no suspended main 3D image is sampled.
Light creation refreshes hierarchy/inspector when it clears selection, and native
resize queues one complete inspector reconciliation. Exact-binary before/after
evidence, build and 67-check receipt: evidence/ui-modal-resize/README.md.
The separately observed fixed-height modal clipping remains an audit finding.

Returning from Edit display exposed a second lifecycle issue: native Window show
re-enabled conditional Dialogue controls after they had been hidden. Resume now
shows first and reapplies current field/target visibility afterwards. Basic,
Advanced and resized-display round trips pass in Icons9; see the exact-binary
receipt in evidence/ui-dialogue-resume.
