# Renegade Engine — Current Handoff

**Date:** 2026-09-02

**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`

**Branch:** `phase6/gate4-lua-gameplay-lifecycle`

**Main/base:** `6a135aa2a2ae15a723235406afec8f7f8b12d2cd`
(`Phase 6 Gate 3: spatial audio and mixing (#125)`).

**Local branch state:** one committed Gate 3 Scene Mix label-spacing repair
(`b7cdc99`) plus the uncommitted Gate 4 implementation described below.

**Remote branch state:** still at the merged Gate 3 base. No Gate 4 push, PR or
CI run has been started.

**State:** LOCAL CANDIDATE AUDITED; FIRST PUSH PENDING. Do not represent Gate 4
as built or owner-accepted until the exact candidate passes all four Windows
jobs, then the Save/Reopen, Test Level and packaged owner checks.

## Accepted baseline

PR #125 is merged. Its merge commit is
`6a135aa2a2ae15a723235406afec8f7f8b12d2cd`. The owner confirmed working global
audio, transient Preview Play/Stop and positional 3D playback in Test Level.
Audio-specific zones were removed and remain deferred to one shared ZoneService.

The narrow Scene Mix UI correction requested after that acceptance is carried
locally: `RenegadeAudioWorkspace` reserves an explicit heading row above the
Master slider. It has not been pushed separately so it can share Gate 4's one
expensive CI cycle.

## Gate 4 contract

The canonical contract is
[`docs/PHASE6_GATE4_LUA_GAMEPLAY_LIFECYCLE.md`](docs/PHASE6_GATE4_LUA_GAMEPLAY_LIFECYCLE.md).

Gate 4 uses:

- native WISCENE `ScriptComponent` filename persistence;
- versioned Renegade gameplay-script metadata;
- project-relative `Content/Scripts/*.lua` authority;
- one Wicked-owned Lua VM;
- one Renegade-owned lifecycle dispatcher;
- deterministic persistent-entity-ID ordering; and
- value/ID-shaped gameplay APIs with no raw engine pointers.

## Local implementation

### EngineBridge

`GameplayScriptService` now provides:

- contained `.lua` import into `Content/Scripts` with collision-free naming;
- regular-file, non-symlink and 1 MiB source bounds;
- non-executing Lua syntax validation;
- project-relative metadata authority plus Wicked-safe native absolute/
  scene-relative filename round-trip;
- `CreateGameplayScriptCommand` Execute/Undo/Redo;
- `SetGameplayScriptEnabledCommand`;
- stable project-relative path resolution;
- immediate stable UUID assignment so unsaved Test Level attachments start;
- governed source staging into the isolated Test Level shadow project;
- pre-Scene-update clearing of native Wicked `PLAYING`/`PLAY_ONCE`; and
- `GameplayScriptRuntime` with Start, Update, Pause, Resume, Reset and Stop.

Each source returns one lifecycle table. Optional callbacks receive the same
table as `self` and a context containing the carrier's persistent UUID. Runtime
sorts instances by that UUID. A load/callback error disables only its own
instance and appends a structured diagnostic.

The Lua v1 surface adds:

- `renegade.entity`: stable-ID exists/find/position and an explicit temporary
  native-ID adapter for JP01 physics calls;
- `renegade.input`: value/pressed/down over Gate 2 gameplay actions;
- `renegade.player`: possession and feet position;
- `renegade.audio`: stable-ID source play/stop; and
- the already accepted `renegade.physics` namespace unchanged.

### Runtime

`RuntimeApplication` now:

- clears governed native script flags before every Wicked Scene update;
- starts scripts after Player Start possession for each Scene revision;
- updates scripts only on unpaused gameplay frames;
- dispatches Pause/Resume with audio pause ordering preserved;
- dispatches Reset/Stop before reloading authored startup state;
- stops Level scripts while a Screen owns Runtime; and
- reports new diagnostics once through Wicked backlog.

### Studio

`ADD > GAMEPLAY SCRIPT...` opens a `.lua` chooser, imports and syntax-checks the
source, creates an undoable transform-free WISCENE carrier, selects it and
refreshes Hierarchy, Inspector and Asset Browser. The Inspector shows the exact
project-relative source and an undoable Enabled checkbox at the top; transform
and generic object sections are suppressed because they do not apply.

### Tests

`RenegadePhase6Gate4GameplayScriptTests` covers import containment, path
traversal rejection, command/Enabled round-trip, valid/invalid syntax,
immediate stable identity, native flag exclusion, API access, deterministic
callback order, pause suppression, Reset/Stop and failing-script isolation.

The existing Test Level snapshot test now proves the referenced governed Lua
file is copied byte-for-byte into the isolated shadow project.

`RenegadePhase6Gate4SourceContract` pins the Studio, Runtime and ownership
boundaries. Both are registered through `Tests/Phase6Gate4.cmake`.

## Local verification completed

The exact Wicked submodule pin and imnodes pin were initialized locally for
header-level verification. The following pass:

```text
git diff --check
g++ -std=c++17 -fsyntax-only EngineBridge/src/GameplayScriptService.cpp
g++ -std=c++17 -fsyntax-only EngineBridge/src/TestLevelSnapshotService.cpp
g++ -std=c++17 -fsyntax-only Tests/Phase6Gate4GameplayScriptTests.cpp
g++ -std=c++17 -fsyntax-only Tests/TestLevelSnapshotTests.cpp
g++ -std=c++17 -fsyntax-only Runtime/src/RuntimeApplication.cpp
g++ -std=c++17 -fsyntax-only Studio/src/RenegadeStudioChrome.cpp
g++ -std=c++17 -fsyntax-only Studio/src/StudioApplication.cpp
manual exact-token equivalent of Phase6Gate4SourceContract.cmake
FEATURE_MATRIX.csv parsed as 38 rows with a consistent 16-column schema
```

The full Studio source passes the Linux header check with only a compile-command
shim for its pre-existing Win32 `GetModuleFileNameW/MAX_PATH` call. This
workspace has no CMake executable, so no local target link/build is claimed.
Windows CI remains authoritative.

Pinned Wicked source was checked directly: `ScriptComponent::_flags`,
`PLAYING`, `PLAY_ONCE` and `IsPlaying()` match the implementation, and Wicked's
Scene update runs playing script components before other Scene systems. This is
why Gate 4 clears native flags before `wi::Application::Update()` rather than
waiting until Renegade callback startup.

## Promotion next

1. Confirm no unrelated worktree changes and create one coherent local commit.
2. Push the complete commit tree to the existing remote Gate 4 branch, verify
   every remote blob/tree SHA and only then open the draft PR/start CI.

## Required exact-head owner check

After all four Windows jobs pass:

1. Add a valid Lua file and confirm its named carrier is selected.
2. Confirm the Inspector path and Enabled control are readable and functional.
3. Save/Reopen and confirm the script attachment persists.
4. Run Test Level and prove Start/Update, Pause/Resume and Reset.
5. Add one throwing script and confirm another script continues with one clear
   diagnostic.
6. Build Windows Game and repeat the script proof from the packaged executable.
7. Recheck Player Start, global/3D sound and Scene Mix label spacing.

Do not begin Gate 5 objective behavior or shared zones inside this Gate 4 PR.
