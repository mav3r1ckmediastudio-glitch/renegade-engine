# Windows Baseline Build

This is the canonical Phase 1 build path for the pinned Wicked Engine reference
editor. It verifies the foundation; it does not build the future Renegade Studio
UI.

## Requirements

- Windows 11 x64.
- Git with submodule support.
- Current Visual Studio or Build Tools with:
  - Desktop development with C++.
  - MSVC `v145` x64/x86 build tools.
  - A Windows 10 or Windows 11 SDK.
- PowerShell 5.1 or newer.
- A DirectX 12-capable GPU for the primary visual check.
- A Vulkan-capable driver/runtime for the development cross-check.

The solution itself declares C++17 and the `v145` toolset. CMake 3.19 or newer
is recorded for later Renegade targets but is not required by this Visual
Studio-solution baseline.

## Clean clone

```powershell
git clone --recurse-submodules https://github.com/mav3r1ckmediastudio-glitch/renegade-engine.git
Set-Location renegade-engine
```

For an existing clone:

```powershell
git pull --ff-only
git submodule update --init --recursive
```

Every build script refuses to continue unless `/WickedEngine` is exactly at
`3a800b7134aafe58461093c8abb2e274d4e64033` with no tracked source changes.

## Build Debug and Release

Open PowerShell in the repository root:

```powershell
.\Tools\Collect-Windows-Baseline.ps1
.\Tools\Build-Windows.ps1 -Clean
```

The build script:

1. verifies the pinned Wicked commit;
2. locates MSBuild;
3. builds `OfflineShaderCompiler`;
4. generates embedded DX12 and Vulkan shader dumps;
5. builds `Editor_Windows` and `Tests` for x64 Debug and Release;
6. checks expected executables, hashes them, and packages the reference editor;
7. writes commands, logs, timings, hashes, and a machine-readable result under
   `/artifacts/phase1`.

Build one configuration:

```powershell
.\Tools\Build-Windows.ps1 -Configuration Debug -Clean
.\Tools\Build-Windows.ps1 -Configuration Release -Clean
```

`/artifacts` and Wicked's `/BUILD` directory are intentionally ignored by Git.

## Visual smoke check

After a successful Release build:

```powershell
.\Tools\Run-Windows-Smoke.ps1
```

The script launches the pinned Wicked Editor once with `dx12` and once with
`vulkan`. Close each editor after checking that it opens, renders a viewport,
accepts input, and exits cleanly. Record PASS, FAIL, or SKIP when prompted.

Screenshots remain required manual evidence. Put them in the smoke evidence
directory printed by the script and reference their filenames in the notes.
Automated build success must not be reported as a visual pass.

## CI

`.github/workflows/windows-baseline.yml` runs the same scripts on a GitHub-hosted
Windows runner for both Debug and Release. Each job uploads its toolchain record,
complete logs, result manifest, executable hashes, and editor package.

CI proves recursive checkout and compilation. A GitHub runner has no suitable
interactive GPU/display path for the editor, so DX12 and Vulkan visual checks
must be performed on a Windows development machine.

## Result interpretation

- `PASS`: build outputs were produced and hashed, or a human observed the
  requested graphics smoke check.
- `PASS_WITH_LIMITATIONS`: at least one visual backend was skipped.
- `FAIL`: compilation, expected-output checks, process exit, or visual
  observation failed.

Phase 1 remains incomplete until Debug and Release build evidence, DX12 visual
evidence, the Windows Vulkan result, and independent verification all exist for
the same commit.
