# Current Handoff

**Date:** 2026-07-30

**Project:** Renegade Engine (working title)

**Active phase:** 2 — final Windows display gate

**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`

**Published branch:** `main` at
`db6b6cac23ad37b7e01593da1a747ef82977fa5a`

**Prepared display-gate code:**
`1fc5b629a94ad3417fd8ad645ed3bc0cd49ce6b3`

**Prepared UI decision and evidence:**
`11e6944d9525e10999548b9a7c3ff65af287cbd1`

**Pinned Wicked commit:**
`3a800b7134aafe58461093c8abb2e274d4e64033`

## Current outcome

The Renegade-owned Phase 2 Studio and Runtime shells are functional on the
project owner's Windows GPU.

Human-observed Studio acceptance:

- fixture scene loads and renders;
- hierarchy selection and transform inspector work;
- the viewport translation gizmo works;
- ten edits undo and redo in order;
- UI clicks do not also manipulate the viewport gizmo;
- Save As and repeated Save As create valid WISCENE files;
- Reopen restores the saved transform; and
- Studio remains open after saving.

Human-observed Runtime acceptance:

- the fixture scene renders; and
- hierarchy, inspector, gizmo, Save, Undo, and Redo controls are absent.

The submitted `cube.wiscene` had a valid archive header and a complete
Zstandard payload that decompressed from 986 bytes to 4,935 bytes.

## Published CI evidence

At published `b4da74a`:

- Phase 2 workflow
  [30494616840](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/actions/runs/30494616840)
  passed Windows x64 Debug and Release.
- Windows baseline
  [30494616805](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/actions/runs/30494616805)
  passed Windows x64 Debug and Release.

The project owner then published `be5167b` and `db6b6ca`; the new SceneService
archive lifecycle passed Save As, repeated Save As, and Reopen on Windows.

## Accepted UI foundation

ADR 0002 accepts `wiGUI` as the production integration and rendering
foundation.

This does not select Wicked's stock Editor or stock styling. Renegade owns its
UI/UX, theme, docking, layouts, components, icons, project hub, panels, and
workflows. `EngineBridge` remains UI-toolkit independent.

The pinned ImGui Docking sample was rejected for Phase 3 because:

- it explicitly sets `allow_hdr = false` with the comment that ImGui does not
  support HDR;
- platform multi-viewports and keyboard navigation are disabled; and
- its source warns that DPI font scaling does not produce good results.

The decision can be revisited only if a future pinned ImGui backend passes all
mandatory platform gates.

## Prepared display-gate increment

Code commit `1fc5b62`:

- parses `dx12` or `vulkan` before graphics-device creation;
- fixes the Windows title-bar encoding defect;
- labels the actual graphics backend in Studio and Runtime titles;
- displays adapter, physical resolution, logical size, colour space, and FPS;
- applies the Windows-recommended rectangle on per-monitor DPI changes;
- compiles Renegade application sources as UTF-8 on MSVC; and
- packages double-click DX12/Vulkan launchers and checklists.

Documentation commit `11e6944`:

- accepts ADR 0002;
- records completed interaction, persistence, and Runtime acceptance;
- updates the feature-exposure matrix;
- defines the final display/input gate; and
- updates the roadmap and changelog.

Wicked remains exact and unmodified.

## Validation completed in this workspace

- `git diff --check` passed.
- Every `docs/FEATURE_MATRIX.csv` row has 16 fields.
- The submodule resolves exactly to the documented Wicked commit.
- Source comparison confirms the ImGui HDR and platform limitations.
- Source comparison confirms Wicked selects the backend during
  `Application::SetWindow()`, so argument parsing must happen first.
- The final Windows C++ compile cannot run in this Linux workspace because
  CMake and the Windows SDK are unavailable.

## Publication and Windows verification

1. Publish `1fc5b62`, `11e6944`, and this handoff commit to `main`.
2. Require both GitHub Actions workflows to pass Debug and Release.
3. Download the new Release artifact.
4. In the Studio package, follow `PHASE2-DISPLAY-GATE.txt`.
5. In the Runtime package, follow its shorter display-gate checklist.
6. Record:
   `DX12 PASS / VULKAN PASS / DPI PASS / INPUT PASS /
   HDR PASS OR NOT AVAILABLE`.
7. Give a different AI or human the exact published commit and
   `docs/VERIFICATION_CHECKLIST.md`.

## Next bounded work after the gate passes

1. Close Phase 2.
2. Run a Renegade UI/UX design increment before broad production panel code.
3. Define the first proving-ground scene so the viewport no longer presents
   only a cube.
4. Accept the project-metadata ADR.
5. Begin Phase 3 with the project hub and Renegade-owned dockable workspace.
