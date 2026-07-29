# Project Charter

## Mission

Build a distinctive, dependable game engine and authoring environment using
Wicked Engine as the rendering and low-level foundation while exposing Wicked's
relevant capabilities through Renegade-owned editor, scripting, runtime, and
programmer surfaces.

Renegade is a working title.

## Primary user

A small game-development team or technically capable creator who needs a
Windows-first editor, repeatable asset workflow, Lua/C++ extensibility, and
standalone game packaging without rebuilding a modern renderer from scratch.

## V1 goals

- Windows x64 editor and runtime using DirectX 12.
- Vulkan validation on Windows during development.
- Custom editor shell, workflows, visual language, and project system.
- Stable service layer between UI and Wicked internals.
- WISCENE scene persistence.
- Lua-first gameplay scripting with stable C++ extension access.
- Standalone Windows player and repeatable packaging.
- Classified exposure of every capability in the pinned Wicked baseline.
- Reproducible builds, documented tests, and independent verification.

## Exposure tiers

| Tier | Meaning | Required surface |
|---|---|---|
| T1 | Routine creator-facing capability | Full UI, undo/redo, persistence, docs, and functional test |
| T2 | Specialist or advanced capability | Advanced UI/tool, persistence where relevant, docs, and test/demo |
| T3 | Low-level or programmer capability | Stable C++ access, Lua where supported, API docs, and sample/test |

## Non-goals for V1

- Rewriting Wicked's renderer, ECS, physics, or native scene format for branding.
- Visual scripting before dependable Lua and C++ workflows.
- Marketplace, multiplayer service, cloud collaboration, or console export.
- Shipping every Wicked-supported operating system at launch.
- Exposing every low-level GPU function as an editor control.

## Ownership boundary

Renegade owns its name, UI, editor shell, services, project and asset model,
runtime/player, templates, validation, documentation, packaging, and original
extensions. Wicked retains its original identity, licence, engine subsystems,
shaders, importers, and reference editor within the pinned dependency.

## Phase 0 exit gate

The repository has:

- A reproducible identity and clean default branch.
- An exact Wicked upstream pin.
- An approved charter and roadmap.
- A feature-exposure ledger.
- Licensing and third-party notice policy.
- Repository-first handoff and verification rules.
