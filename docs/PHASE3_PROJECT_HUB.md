# Phase 3 Project Hub and Workspace Increment

## Outcome

Code commit `30c5d3c` begins the production-direction Studio foundation.

It adds:

- a versioned `.renegade` project descriptor;
- `ProjectService` create, open, validate, and recent-project operations;
- persistent recent-project ordering;
- a holographic Project Hub with create, open, select, launch, and return
  workflows;
- a Renegade-owned smoked-black/cyan `wiGUI` theme;
- permanent toolbar, hierarchy, inspector, content-browser, and viewport
  regions;
- direct viewport object selection synchronised through `SelectionService`;
- an editor-only cyan selected-object silhouette;
- right-mouse freelook and WASD/QE fly-camera navigation within the viewport;
- a generated live Proving Ground starter scene with PBR primitives, emissive
  grid, lights, shadow, fog, and environment colour; and
- automated project lifecycle and recent-project persistence coverage.

This is the first implementation of the approved visual direction. It is not
the finished UI and does not yet include the Identity Handshake animation,
project thumbnails, docking, scene tabs, asset browsing, or terrain editing.

## Project lifecycle

Create Project:

1. Accept a validated Windows project name.
2. Select an existing parent folder.
3. Create a project subfolder without overwriting a non-empty folder.
4. Write the v1 descriptor.
5. Copy the generated Proving Ground to `Content/Scenes/Main.wiscene`.
6. Register the project in the recent-project list.
7. Load the project scene into the existing `StudioSession`.

Open Project:

1. Validate the descriptor format and version.
2. Reject absolute or escaping startup-scene paths.
3. Require the startup WISCENE to exist.
4. Register the project as most recent.
5. Load the startup scene and enter the workspace.

## Visual intent

The approved direction remains an industrial holographic workstation:

- smoked near-black panels;
- ice-blue/cyan projected edges and interaction states;
- amber reserved for warnings;
- readable, restrained glow;
- colour-accurate 3D viewport; and
- Renegade-owned layout and terminology.

The live UI is an implementation baseline, not an attempt to reproduce the
concept mockup pixel-for-pixel in one increment.

## Windows acceptance

Use `Studio/package/README-FIRST.txt` in the Release artifact.

Required result:

```text
DX12 HUB PASS / CREATE PASS / RECENTS PASS / REOPEN PASS / VULKAN PASS
```

Visual or behavioural failure overrides a green compile.

The expanded viewport interaction acceptance test is recorded in
`docs/PHASE3_VIEWPORT_INTERACTION.md`.
