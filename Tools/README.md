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
