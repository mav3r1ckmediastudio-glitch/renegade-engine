# Phase 3 Studio Shell Rebuild

## Why this exists

`HANDOFF.md` recorded a blocker: *"The approved concept imagery is still not
stored in this repository. Ask the project owner for the reference images
before making subjective redesign decisions."* The project owner has since
supplied that reference directly:

- `Renegade_Studio_UI_Design_Tokens_v1.0.json` - colour/typography/spacing tokens.
- `Renegade_Brand_Guidelines_v1.0.pdf` - brand system, including a page 18
  Studio UI mockup captioned *"This mockup is the visual design anchor for
  Studio. The live editor is functional scaffolding and must be rebuilt
  toward this direction rather than treated as the finished interface,"*
  plus explicit Studio UI principles (viewport-first, hidden-until-needed
  drawers, dockable-by-design panels) and a component-styling spec (exact
  geometry, button roles, tabs).
- `Renegade_Studio_Workspace_Prototype_v1.0_Standalone.html` - a working
  HTML/CSS/JS reference implementation of the target layout and behaviour:
  exact panel dimensions, draggable splitters, a tabbed/collapsible bottom
  dock, scene tabs with a dirty indicator, a floatable Inspector, and an
  unsaved-changes modal.

An earlier pass (see git history immediately before this commit) only
reapplied brand colours to the existing flat panels. That was colour, not
the structural rebuild the mockup calls for. This milestone is that
structural rebuild, built directly against the three reference documents
above and against Wicked's real `wi::gui` widget capabilities (`Window`
already supports native drag-resize/move/collapse; `TreeList` already
supports parent-child items) rather than inventing a UI framework.

## What changed

All of it lives in `Studio/src/StudioApplication.cpp` / `.h`, plus one new
small, real (non-mocked) EngineBridge service.

**Resizable, collapsible, persisted panels.** `hierarchyPanel_` and
`inspectorPanel_` now carry `wi::gui::Window::WindowControls::RESIZE_RIGHT` /
`RESIZE_LEFT`; the bottom dock (`contentPanel_`) carries `RESIZE_TOP`. Each
has an `OnResize` hook that updates `leftPanelWidth_` / `rightPanelWidth_` /
`bottomDockHeight_` and re-runs `ResizeLayout()`. Geometry, and
Hierarchy/Inspector visibility, persist through the same
`ProjectService` preference store the grid-visibility toggle already uses
(extended with `float` `Set`/`GetEditorPreference` overloads, following the
existing `bool` pattern exactly) - this is the "Camera speed and editor
layout... should follow this route" follow-up `HANDOFF.md` asked for.
`Window > Reset Workspace` restores the shipped defaults (320/360/300,
dock closed, both panels visible).

**Scene tab strip.** A single tab (Renegade has one open scene, not many)
showing the scene file name and a real dirty indicator: dirty is
`session_->Commands().UndoCount() != lastSavedUndoCount_`, not a cosmetic
flag, updated in `RefreshStatus()` and cleared by `MarkSavedForDirtyTracking()`
on every successful save/reopen/project load. The tab's close (x) button
routes through the new `RequestCloseScene()`.

**Bottom dock with real tab-switching.** Four tabs - Asset Browser, Console,
Output, Diagnostics - matching `workspace.bottom_drawers` in the design
tokens. Clicking a tab opens the drawer to that content; clicking the active
tab again collapses it, exactly like the prototype. Collapsed height is a
fixed 32px; open height is user-resizable and persisted.

**Asset Browser backed by a real filesystem scan, not placeholder text.**
New `EngineBridge::ContentBrowserService` (`ContentBrowserService.h/.cpp`)
recursively scans the open project's `Content/` directory and classifies
files by extension (Scene/Mesh/Texture/Shader/Other). Studio's folder list
and thumbnail-card grid are driven entirely by this real scan plus a real
substring search field - there are no invented file names. A freshly
created project's Content directory only has `Scenes/Main.wiscene`, so a new
project's browser will look sparse; that is the honest state of the
project's disk, not a bug. Displayed pool is capped at 10 folders / 16 cards
(`MaximumVisibleAssetFolders` / `MaximumVisibleAssetCards`) for now.

**Console and Output panels render `wi::backlog`** - Wicked's and
Renegade's real engine log - rather than fabricated log lines. Wicked does
not separate an "Output" stream from the log, so Output intentionally shows
the same real feed as Console rather than inventing a second, different one.
Diagnostics shows live adapter name, resolution, entity count, and
undo/redo counters.

**Menu bar: File / Edit / View / Window.** Every item calls an already-real
capability (Save, Save As, Close Scene, Undo, Redo, toggle Hierarchy/
Inspector/Assets/Console/Output/Diagnostics, Reset Workspace). **Build is
intentionally absent** - there is no build/package pipeline yet, and a menu
item that does nothing would misrepresent what Studio can do.

**Unsaved-changes modal and save toast**, replacing the previous native
`wi::helper::messageBoxCustom` confirmation in `ReturnToProjectHub()` with
one in-brand modal (Cancel / Discard / Save) reached through the new
`RequestCloseScene()`, which is now the single gatekeeper - `ReturnToProjectHub()`
itself no longer prompts, so the modal's Discard/Save buttons do not
double-prompt. A toast confirms a successful save and fades after ~1.6s.

**Colour-role corrections carried over from the previous (colour-only) pass**
are also in this diff: panel background is Carbon `#171716` (was
mistakenly Obsidian), default borders are neutral Gunmetal `#474442` (was
mistakenly Forge), and the theme's ambient glow highlight is now off by
default - the brand guidance is "glow is reserved for focus, launch and
live energy," not a constant effect on every panel. A `DangerColor`
(`#D95555`) and `SuccessColor` (`#4CC38A`) were added for the Discard button
and future use, matching the brand's functional palette.

## Explicitly out of scope

- **Tabbed Inspector (Details/Rendering/Physics).** The brand component-
  styling page shows Inspector tabs. Reorganizing the Inspector's existing,
  already-accepted Transform and Environment Authoring fields under a new
  tab system was deliberately deferred: Environment Authoring passed full
  Windows acceptance (`DX12 ENVIRONMENT PASS / ... / VULKAN ENVIRONMENT
  PASS`) and this change cannot be visually verified in the environment it
  was authored in. Restructuring already-accepted, working UI without the
  ability to test it is a real regression risk for a mostly-cosmetic gain
  (there is no Material/Light authoring yet to justify a Rendering tab, and
  no Physics authoring to justify a Physics tab - see `HANDOFF.md`'s "Next
  bounded milestone"). Recommended as its own follow-up once Light and
  Material Authoring lands and there is real content for those tabs.
- **Typography.** Still Wicked's built-in font; no Roboto/Roboto Condensed
  files exist anywhere in this repository or its submodule. Unchanged from
  the previous milestone's note.
- **Docking panels into each other as shared tabs** (e.g. dragging Inspector
  onto Hierarchy to combine them) is not implemented. Panels resize and
  collapse (bottom dock) and the Inspector "float" menu item toggles between
  docked and undocked position, but true drag-to-redock is a larger follow-up.
- **Menu click-away-to-close.** Menus close on Escape, on selecting an item,
  or on toggling the same menu button again - not yet on clicking elsewhere
  on the canvas. A minor interaction gap, not a missing capability.
- Asset import, thumbnails/previews for meshes and materials, and terrain
  authoring remain not started, as before.

## Required verification (not yet performed)

This was authored and reviewed with no DirectX12/Vulkan toolchain and no
Windows machine available, so - per `AGENTS.md` - it is **unverified**:
not built, not run, not visually inspected. Every wi::gui API used here
(`Window::WindowControls::RESIZE_*`, `OnResize`, `TextInputField::OnInputAccepted`
`sValue`, `wi::backlog::getText()`, `wi::graphics::GraphicsDevice::GetAdapterName()`,
`wi::config::Section::Set/GetFloat`) was checked against the pinned Wicked
source (`3a800b71`) rather than assumed, and the file was checked for
balanced braces/parens after every edit, but neither of those is a
substitute for a real compile and a Windows test pass.

Before this is accepted:

1. `git submodule update --init --recursive`, build Studio DX12 and Vulkan
   per `docs/BUILD_WINDOWS.md`.
2. Confirm it compiles - this is the first real risk. ~1,500 new/changed
   lines across `EngineBridge` and `Studio` with no compiler in the loop is
   the single biggest risk in this change; expect to fix build errors.
3. Run the full Phase 3 Editor Visual Polish / Environment Authoring
   regression list (grid, gizmo, selection, undo/redo, save/reopen) to
   confirm the shell rebuild did not disturb existing behaviour.
4. Exercise every new interaction: drag-resize Hierarchy/Inspector/bottom
   dock and confirm persistence across restart; open/collapse each of the
   four dock tabs; create or open a project and confirm the Asset Browser
   shows its real `Content/` contents; trigger Console/Output/Diagnostics
   and confirm live log/stat content; open each menu and confirm every item
   fires; edit the scene, confirm the tab's dirty indicator lights, then
   close the scene and confirm the modal appears with working
   Cancel/Discard/Save; save and confirm the toast appears and fades.
5. Record the result in `docs/VERIFICATION_CHECKLIST.md` and update
   `HANDOFF.md` per the usual process.
