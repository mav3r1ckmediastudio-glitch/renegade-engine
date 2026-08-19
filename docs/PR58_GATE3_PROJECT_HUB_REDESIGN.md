# PR #58 — Gate 3 Project Hub Redesign

Status: **IMPLEMENTED / CI + OWNER ACCEPTANCE PENDING**

Gate 3 replaces the old utility-style Project Hub presentation while deliberately preserving the existing project lifecycle callbacks underneath it.

## Scope

Gate 3 is presentation and interaction hierarchy only. It does **not** change project creation/open semantics, project switching transactions, scene adoption, dirty-state safety, recent-project persistence format, artwork metadata, or reopen performance.

Accepted Gate 2 startup behavior remains authoritative:

`logo reveal → saved/first-run identity → identity handshake → ENTER HUB → iris split → live Project Hub`

## Implemented Hub presentation

- full-window Renegade-owned Hub surface with no old inset floating-panel presentation;
- top-left `RENEGADE` branding and large `PROJECT HUB` title;
- startup-only `IDENTITY ACCEPTED` language removed from the Hub;
- dark graphite/near-black Hub palette with restrained cyan and orange accents;
- left project action rail containing:
  - project name input;
  - `CREATE NEW PROJECT`;
  - `OPEN PROJECT...`;
  - `BACK TO EDITOR // <current project>` only when an active project exists;
- central `RECENT PROJECTS` area supporting the existing maximum of eight recent projects;
- recent project entries are numbered technical rows/cards and expose the full descriptor path as a tooltip;
- first valid recent project is selected by default for presentation convenience only; selection does not open/adopt a project;
- selected-project details panel shows project name, descriptor path and Renegade project format;
- explicit primary `OPEN PROJECT` action for the selected recent project;
- bottom project-services/status strip;
- responsive layout: recent entries can use two columns where width permits and compact to one column on narrower windows;
- `OPEN SCENE...` is intentionally absent from the Project Hub. The existing editor-level Open Scene command remains available inside Renegade Studio.

## Preserved behavior

The existing callbacks remain the lifecycle authority:

- `CreateProject()`
- `OpenProject()`
- `OpenProjectDescriptor()`
- `OpenSelectedRecentProject()`
- `ReturnToProjectHub()`
- `SelectRecentProject()`
- `SetProjectHubVisible()`

Gate 3 does not claim to harden those operations. Reliability work belongs to Gate 5.

## Explicitly deferred

### Gate 4

- persistent 16:9 project artwork;
- custom artwork selection;
- automatic screenshot capture;
- fallback project artwork;
- richer recent-project card metadata.

### Gate 5

- transactional Create/Open/Continue/Switch reliability;
- project/scene adoption hardening;
- explicit Asset Browser refresh after project switch.

### Gate 6

- dirty-state `SAVE / DON'T SAVE / CANCEL` safety.

### Gate 7

- reopen/startup performance optimization;
- unique StableID governed texture preparation reuse;
- reveal initialization concurrency and safe reveal SKIP behavior.

Importer UX remains outside PR #58 scope.

## Source boundary

The net source-code delta from the accepted Gate 2D head is confined to:

- `Studio/src/StudioApplication.cpp`

This gate does not change the accepted Gate 2 startup components, EngineBridge project services, scene services, governed asset services, importer implementation, or project persistence data structures.

## Required CI proof

Gate 3 cannot pass until the exact implementation/documentation head passes:

- Renegade Studio Windows x64 Debug build + startup smoke;
- Renegade Studio Windows x64 Release build + startup smoke;
- Windows baseline x64 Debug;
- Windows baseline x64 Release.

## Required owner runtime acceptance

The exact-head Release package must demonstrate:

1. accepted Gate 2 startup sequence and iris transition still work;
2. iris transition reveals the redesigned Hub with no editor/old-Hub flash;
3. Hub owns the full client area cleanly;
4. `RENEGADE` / `PROJECT HUB` hierarchy is readable and visually coherent;
5. `OPEN SCENE...` is absent from the Hub;
6. Create Project and Open Project controls remain interactive;
7. recent projects populate correctly when available;
8. recent selection updates the details panel and `OPEN PROJECT` target;
9. Back to Editor appears only when a current project exists and returns to the editor;
10. resize/maximize behavior is reasonable with no major overlap or clipping;
11. direct EXE, DX12 launcher and Vulkan launcher remain viable.

Gate 3 remains **NOT PASSED** until exact-head CI and owner runtime acceptance are both complete.
