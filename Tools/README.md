# Renegade tools

Build, import, shader, packaging, validation and assistant tooling. Current
integrated baseline: main 618b231, 10 November 2026.

- Build-Studio-Windows.ps1 configures/builds Studio and Runtime with embedded
  shaders, builds the test graph, runs CTest and records application outputs.
- Build-Windows.ps1 builds the pinned native Wicked reference baseline;
  Collect-Windows-Baseline.ps1 records the toolchain.
- Windows-Build.Common.ps1 owns the exact pin guard, shared DirectXMath selection,
  native command failure propagation, logging and hashes.
- Package-RecoveryAlpha.py stages/archives approved recovery candidates using
  exact source/validation identity. It is not permission to distribute a build.
- assistant_knowledge contains curated, source-fingerprinted knowledge/manual
  support; assistant_presence owns the installed assistant transport/UI tooling.
- Shader/resource tools support the currently integrated build graph; they do not
  yet constitute a third-party shader-editor plugin SDK.

See [BUILD_WINDOWS](../docs/BUILD_WINDOWS.md), [current status](../docs/CURRENT_STATUS.md),
[assistant knowledge](assistant_knowledge/README.md) and
[documentation index](../docs/README.md).
