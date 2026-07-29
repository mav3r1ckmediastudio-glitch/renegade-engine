# Toolchain Record

## Declared baseline

| Item | Required/expected | Evidence status |
|---|---|---|
| Operating system | Windows 11 x64 | Pending first Windows run |
| Visual Studio | Current release supporting the pinned solution | Pending first Windows run |
| MSVC toolset | `v145`, C++17 | Declared by pinned `.vcxproj` files; observation pending |
| Windows SDK | Windows 10 or 11 SDK | Pending first Windows run |
| CMake | 3.19 or newer | Pending first Windows run; not used by the solution baseline |
| Git | Submodule-capable current release | Pending first Windows run |
| GPU | DX12-capable | Pending local graphics smoke |
| GPU driver | Current stable driver | Pending local graphics smoke |
| Vulkan runtime | Required for development cross-check | Pending local graphics smoke |

`Tools/Collect-Windows-Baseline.ps1` writes observed values to
`artifacts/phase1/.../toolchain.json` and `toolchain.md`. CI uploads those files
with every Debug and Release run. Local GPU and driver evidence must come from
the same machine used for the visual smoke checks.

## Canonical build path

The Phase 1 baseline uses `WickedEngine/WickedEngine.sln` and the upstream
target sequence:

1. `OfflineShaderCompiler`
2. `hlsl6 spirv shaderdump strip_reflection`
3. `Editor_Windows`
4. `Tests`

Both Debug and Release use platform `x64`. Exact commands, output paths, hashes,
warnings, timings, and failures are captured by `Tools/Build-Windows.ps1`.
See `docs/BUILD_WINDOWS.md`.

No machine result is claimed in this document until the Windows workflow has
actually run.
