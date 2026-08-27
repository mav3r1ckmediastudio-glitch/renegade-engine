# Scene UI Gate 6 — Build Windows Game recovery

**Status:** implementation candidate; Windows CI and packaged owner acceptance
are required before merge.

**Recovery base:** `1e0470a9e530dd20c42ddf16662c3771aaede825`
(merged Scene UI Gate 5 / PR #105).

## Owner-reported blockers

Gate 6 acceptance exposed two independent Build Windows Game failures:

1. a fresh project could show `Runtime Ready` in Story Flow while the build
   read an older on-disk Flow and reported no route from Game Start; and
2. a Terrain scene referenced Renegade's default grass files beside Studio, so
   the dependency graph correctly rejected them as outside the creator project.

The first failure was a document-lifecycle bug. The Scene Editor BUILD menu
called the packaging controller directly and therefore bypassed dirty Scene and
Story Flow persistence. The second was a packaging-policy gap introduced by
the accepted Terrain material path: the standalone package did not stage the
same Renegade-owned resources that Runtime resolves from `Content/...`.

## Recovered contract

Every Studio Build Windows Game entry point now follows one path:

`request -> save dirty Scene -> save dirty Story Flow -> validate saved route -> build`

Cancellation or failure at either save boundary blocks the build and surfaces
the failure in Studio. The packaging controller never reads unsaved authoring
state.

Story Flow's green `Runtime Ready` state and the saved standalone build use the
same deterministic route resolver. Green therefore requires a bounded default-
state route from permanent Game Start to Complete Game; Screen outcome parity
remains an additional requirement.

Terrain and precipitation resources remain outside creator project ownership.
The controller declares an exact Renegade-owned file allowlist and uses the
same declarations to hash and stage:

- `Content/terrain/default_grass/default_grass_basecolor.tga`;
- `Content/terrain/default_grass/default_grass_normal.tga`;
- `Content/terrain/default_grass/default_grass_surface.tga`;
- `Content/weather/snowflake.dds`.

An outside-project diagnostic is removed only when it originates from a Scene
and its canonical filesystem identity exactly matches one declared bundled
source. An unrelated or arbitrary external dependency remains fatal. Runtime
support destinations are package-root paths rather than creator `GameData` and
remain covered by the existing hash, manifest, integrity and staging checks.

## Automated acceptance

`RenegadeWindowsGameBuildProjectTests` must prove:

- the live shared route authority accepts a valid Level journey;
- it rejects an unrouted Game Start;
- an ungoverned external Terrain texture remains `outside_project`;
- declaring an unrelated bundled resource does not admit that texture;
- the exact governed Terrain texture is admitted and planned as hashed Runtime
  support at its package-root destination.

The Gate 10 project-home source contract additionally rejects a return to the
direct Scene chrome build call and locks the two-document save handoff, shared
route validation and four bundled resource declarations.

## Required owner acceptance

After exact-head Debug/Release CI is green, use the packaged Release Studio:

1. create a fresh project and leave the seeded
   `Game Start -> Level -> Complete Game` journey intact;
2. make both the Level and Story Flow dirty, choose BUILD > BUILD WINDOWS
   GAME..., and confirm both documents save before packaging starts;
3. confirm the build completes and launch the promoted executable directly;
4. confirm the Level loads and Terrain uses the default grass material;
5. repeat with an existing Terrain project that previously reported the
   outside-project dependency;
6. deliberately reference an arbitrary file outside the project and confirm
   the build still fails closed.

Compilation and CI alone do not complete this recovery. The owner must confirm
the packaged Release behavior before Gate 6 or PR #106 can merge.
