# Renegade Engine

[![Ask DeepWiki](https://img.shields.io/badge/Ask-DeepWiki-blue)](https://deepwiki.com/mav3r1ckmediastudio-glitch/renegade-engine)

Renegade is a Windows-first game engine and creator environment built on
[Wicked Engine](https://github.com/turanszkij/WickedEngine). Wicked supplies the
renderer, native scene/ECS and low-level physics facilities. Renegade owns its
Studio editor, project/asset workflows, Runtime, gameplay framework, creator UX,
diagnostics and build lifecycle.

**Current documentation baseline: main `618b231`, 10 November 2026.**
PR #193 is merged, combining functional prefabs with the earlier lighting and
particle work. The full feature candidate passed four pre-merge Windows checks.
This remains active pre-alpha development; it is not a distribution-ready v1.

## What is integrated

- **Studio:** Project Hub, Story Flow/Journey, Screen authoring, hierarchy,
  Inspector, Asset Browser, native gizmos, Undo/Redo and owned document lifecycle.
- **Selection and assemblies:** Shift-click toggling in viewport/hierarchy,
  directional box selection, shared-pivot move/rotate/scale, batch duplicate/delete,
  Group/Edit Pieces/Unpack and reusable scene prefabs.
- **Functional prefabs:** native components, lights, complete particle effects,
  material/shader settings, project resources and governed script attachments;
  independent instance identity/reference remapping, reopen and standalone proof.
- **World:** finite terrain creation/sculpt/painting/expansion; grass; spline
  rivers, riverbed linking, tributaries, rapids/foam and placeable waterfalls.
  Terrain recovery covers 33×33 chunks without removing transaction budgets.
- **Environment:** realistic sky, sun/time, night sky, aurora/Milky Way, volumetric
  cloud formations, weather presets/transitions, lightning/thunder, rain/snow
  and grass wind.
- **Lighting and particles:** native light picker, saved colours/presets and six
  scripted behaviours; layered particle picker, governed texture browser,
  stationary animated sprites, fields, variation, lifetime curves, shapes/timing
  and instance-safe Lua controls.
- **Assets:** native static OBJ/GLB/FBX and Character import/reimport, preview,
  materials/maps, animation assignment/crossfades, stable identities/provenance
  and governed project asset moves.
- **Gameplay:** Player Start and first-person arms, equipment/actions, physical
  projectiles/Hitscan and surface impacts; Character navigation/behaviour;
  optional inventory, item/container and Screen presentation systems.
- **Scripting:** governed Lua lifecycle, typed ACTION/SCRIPT/GLOBAL SCRIPT
  properties, references, project-owned modules, stock Actions and Library
  adoption/update handling. This is not yet a general third-party plugin SDK.
- **Runtime/builds:** Test Game, project/Story Flow startup, dependency-aware
  Windows build staging, package identity/integrity and standalone launch.
- **Responsiveness:** direct native point lookups, stable editor overlays,
  saved VSync and an animated Test Game preparation indicator with background
  snapshot writing/validation after the initial native capture.

Integrated does not mean every historical programme gate is complete. See
[CURRENT_STATUS](docs/CURRENT_STATUS.md) for merge/evidence provenance and limits.

## Start here

| Need | Document |
|---|---|
| Use current creator workflows | [Creator guide](docs/CREATOR_GUIDE.md) |
| Understand exactly what is merged/verified | [Current status](docs/CURRENT_STATUS.md) |
| Find guides, specifications and dated evidence | [Documentation index](docs/README.md) |
| Build Studio/Runtime or the Wicked baseline | [Windows build guide](docs/BUILD_WINDOWS.md) |
| Understand ownership and persistence | [Architecture](docs/ARCHITECTURE.md) |
| Check remaining work | [Roadmap](docs/ROADMAP.md) |
| Continue engineering work | [Handoff](HANDOFF.md), [AGENTS](AGENTS.md), [AI workflow](docs/AI_WORKFLOW.md) |
| Check feature-specific evidence/limits | [Feature matrix](docs/FEATURE_MATRIX.csv) |
| Verify a release candidate | [Verification checklist](docs/VERIFICATION_CHECKLIST.md) |

For current feature instructions: [prefabs](docs/SCENE_ASSEMBLY_WORKFLOW.md),
[lights](docs/LIGHT_PICKER_AND_BEHAVIOURS.md),
[particles](docs/PARTICLE_PICKER_AND_LAYERED_EFFECTS.md),
[particle Lua](docs/PARTICLE_LUA_API.md),
[importer](docs/MODEL_IMPORTER_REBUILD.md),
[Character workflow](docs/CHARACTER_WORKFLOW_PROGRESS.md) and
[live diagnostics](docs/LIVE_DIAGNOSTIC_ACCESS.md).

## Build baseline

- Wicked fork: https://github.com/mav3r1ckmediastudio-glitch/WickedEngine.git
- Branch: `renegade-rocky-riverbed-v1`
- Git submodule pin: `82557d439a41d104f559b0d3134d160b7bf6154e`
- Primary platform: Windows x64 / DirectX 12.
- Documented configure-time sky/weather, allocator and particle core patches
  are separate from the gitlink; see [UPSTREAM_SYNC](docs/UPSTREAM_SYNC.md).

~~~powershell
git clone --recurse-submodules https://github.com/mav3r1ckmediastudio-glitch/renegade-engine.git
Set-Location renegade-engine
.\Tools\Build-Studio-Windows.ps1 -Configuration Release
~~~

Use the build guide for prerequisites and clean-submodule requirements. Preserve
uncommitted work before updating a checkout. The baseline build script builds the
Wicked reference; it is not a substitute for building Renegade Studio/Runtime.

## Project layers

| Path | Responsibility |
|---|---|
| WickedEngine | Pinned renderer, scene/ECS, native systems and reference editor |
| EngineBridge | UI-independent services, documents, commands and asset transactions |
| Studio | Renegade authoring UI and project workflows |
| Runtime | Standalone player, gameplay and governed script execution |
| Tools | Build, import, shader, packaging and assistant tools |
| Tests | Headless, native/GPU, source, lifecycle and packaged checks |
| Templates | Starter/template integration area |
| docs | Current guidance, specifications and scoped historical evidence |
| assets | Renegade-owned installed/editor resources |

## Acceptance and distribution

Compilation cannot establish creator usability, visual quality or runtime parity.
Persisted state needs save/reopen checks; gameplay changes need actual Runtime
checks. A visual or behavioural failure overrides an automated pass.

Wicked remains MIT licensed with its original notices. Renegade's project-wide
licensing/distribution policy is still governed by [LICENSING](docs/LICENSING.md).
Merging code does not authorize distributing another alpha or third-party assets.
