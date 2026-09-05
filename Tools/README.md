# Tools

Renegade-owned import, shader, packaging, migration, feature-inventory, and
validation tools live here.

## Phase 1 Windows baseline

- `Collect-Windows-Baseline.ps1` records the machine and toolchain.
- `Build-Windows.ps1` verifies the Wicked pin, compiles x64 Debug/Release
  reference targets, packages the editor, and records evidence.
- `Run-Windows-Smoke.ps1` runs the required human-observed DX12 and Vulkan
  checks.
- `Windows-Build.Common.ps1` contains shared pin, MSBuild, logging, and hashing
  helpers.

See `docs/BUILD_WINDOWS.md` for the exact workflow.

## Phase 2 Studio shell

- `Build-Studio-Windows.ps1` configures the Renegade-owned CMake graph, builds
  `RenegadeStudio` for x64 Debug and/or Release, packages the executable and
  fixture content, and records hashes and logs.

See `docs/PHASE2_STUDIO_SHELL.md` for scope and acceptance evidence.
