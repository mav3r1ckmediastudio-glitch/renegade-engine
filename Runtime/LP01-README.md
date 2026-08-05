# LP01 — Project-Aware Runtime Bootstrap

**Baseline:** `619e169eaa2d7322b5afb4097bdcd8b88b4b5898`
**Pinned Wicked:** `3a800b7134aafe58461093c8abb2e274d4e64033`
**Status:** bounded proof-of-concept patch; no merge authority

## What this patch proves

- `RenegadeRuntime.exe` requires an explicit `--project` descriptor.
- Windows argument tokenization preserves quoted paths containing spaces.
- The existing `.renegade` descriptor is validated through a read-only
  `ProjectService::InspectProject()` seam.
- Startup scene paths are checked against the project root after
  canonicalization.
- Runtime loads the project-selected WISCENE through `SceneService`.
- Missing/invalid descriptors and corrupt WISCENEs fail closed with stable exit
  codes and `Logs/RuntimeBootstrap.log`.
- Runtime no longer silently falls back to `Content/cube.wiscene`.
- No Studio target, Studio service or running Studio process is involved.

## What this patch does not prove

- Story Flow, Game Start, named outcome transitions or Test All.
- Asset identity, dependency closure, cooking or creator-game packaging.
- Clean consumer-machine deployment.
- A final Runtime manifest or Target Architecture.
- Vulkan parity until the packaged Vulkan launcher is owner-tested.

## Build

From a Visual Studio developer command prompt:

```bat
powershell -ExecutionPolicy Bypass -File Tools\Build-Studio-Windows.ps1 -Configuration Release -Clean
```

The existing build chain compiles `RenegadeRuntimeBootstrapTests` through the
`RenegadeBridgeTests` dependency and runs it through CTest.

## Packaged positive test

Run:

```text
Run-RenegadeRuntime-DX12.cmd
```

Expected:

- Runtime title contains `LP01 Bootstrap Fixture`.
- `Logs\RuntimeBootstrap.log` contains `status=PASS` and `code=SUCCESS`.
- `startup_scene` points to
  `Content/LP01/Valid Project/Content/Scenes/BootstrapTest.wiscene`.

The fixture deliberately copies Wicked's cube WISCENE to a project-owned path.
The proof is the explicit project path, absence of fallback behaviour and
structured evidence—not a claim that the cube content itself is a game.

## Negative tests

Run:

```text
Run-LP01-Negative-Fixture.cmd
```

Each case must show a clear startup failure, return a non-zero code and replace
`Logs\RuntimeBootstrap.log` with the corresponding structured failure.

| Exit code | Meaning |
|---:|---|
| 20 | Missing `--project` |
| 21 | Invalid/duplicate launch arguments |
| 22 | Descriptor/version/path/file validation failed |
| 23 | Canonical startup scene escaped project root |
| 24 | WISCENE failed to load |

## Stop/reconsider rule

Do not proceed to stable identity or Story Flow if the packaged Runtime still
depends on Studio state, silently loads the default cube, reads from repository
paths, or cannot fail deterministically for the supplied negative fixtures.
