# Build Renegade on Windows

**Current source baseline:** main 618b231, 10 November 2026.
**Pinned Wicked gitlink:** `82557d439a41d104f559b0d3134d160b7bf6154e`.
This guide distinguishes the Renegade application build from the Wicked reference
baseline. [UPSTREAM_SYNC](UPSTREAM_SYNC.md) records pin/overlay ownership.

## Prerequisites and checkout

Use Windows x64, Git/submodules, Visual Studio or Build Tools with Desktop
development with C++, a Windows SDK, CMake 3.19+ and CTest. The checked-in Wicked
native solution requires its declared v145 toolset. CMake selects an installed
supported generator/toolset; local v143 Release receipts do not replace the hosted
native-solution baseline. Do not link objects/libraries built with incompatible
MSVC toolsets. Python is needed for the registered assistant/resource tests;
follow workflow and script prerequisites for the checkout being built.

~~~powershell
git clone --recurse-submodules https://github.com/mav3r1ckmediastudio-glitch/renegade-engine.git
Set-Location renegade-engine
git rev-parse HEAD
git submodule status
~~~

Before updating an existing checkout, preserve uncommitted work. Do not reset
owner files or generated overlays to make a build pass. Shared
Windows-Build.Common.ps1 verifies the exact pin and rejects tracked Wicked changes
before its build starts; CMake then applies documented overlays. Use a separate
clean checkout for a fresh scripted build when an existing checkout has patches
or owner work. Never change the gitlink to match a convenient local library.

## Studio and Runtime

From a suitable developer PowerShell with cmake/ctest on PATH:

~~~powershell
.\Tools\Build-Studio-Windows.ps1 -Configuration Release
~~~

The default builds Debug and Release:

~~~powershell
.\Tools\Build-Studio-Windows.ps1
~~~

The script configures BUILD/renegade with embedded shaders, builds Studio,
Runtime and the test/dependency targets, runs CTest, stages support resources and
writes logs, build-result metadata, hashes and application archives below
artifacts/studio. These are engineering outputs, not automatic release approval.
-Clean removes that script's build directory; use it only on a disposable
build checkout. An incremental build can instead use its existing configured
CMake directory; do not mix single-config Ninja and multi-config VS layouts.

CMake's configured graph applies the documented sky/weather, shared-allocator
and particle patches. A dirty generated Wicked worktree afterwards is not a
new approved upstream commit. Preserve it and its decision provenance; do not
commit it as an unrelated submodule update.

## Wicked reference baseline

~~~powershell
.\Tools\Collect-Windows-Baseline.ps1
.\Tools\Build-Windows.ps1 -Configuration Debug
.\Tools\Build-Windows.ps1 -Configuration Release
~~~

This builds the pinned Wicked native solution, offline shaders and reference
Editor/Tests, not the Renegade Studio UX. It writes toolchain/results/packages
below artifacts/phase1. The scripts consistently select the pinned DirectXMath
headers. A fresh clean submodule is required by the script's guard.

## Checks and interpretation

| Check | What it establishes |
|---|---|
| Windows baseline Debug/Release | Pinned native reference solution/toolchain build and expected outputs |
| Studio Debug/Release | Renegade application/test graph, registered checks and process-startup smoke |
| Local native/GPU proofs | Their named component/resource/render behaviour on the recorded device |
| Actual Studio + save/reopen | Creator interactions and persisted state for the tested workflow |
| Standalone/relocated package | Runtime closure, bootstrap, integrity and behaviour for that tested output |

Run tests with the configuration appropriate to the generated build:

~~~powershell
ctest --test-dir BUILD/renegade -C Release --output-on-failure
~~~

Do not imply a fixed test count: registration changes with the build options and
configuration. Headless tests cannot call renderer-dependent scene factories.
GPU/native and manual-only proofs have separate execution requirements.

## CI and documentation-only work

.github/workflows/windows-baseline.yml and studio.yml provide the normal four
jobs. The LP04 handshake probe is manual-only; hosted graphics limitations are
recorded in its existing evidence document. Do not dispatch it as a documentation
check.

At the owner's explicit request, a docs-only commit may carry `[skip ci]`.
GitHub skips push/pull_request workflows for that commit; required checks may
remain pending. Do not alter protection rules, claim skipped jobs passed or
manually dispatch a build. Retain the skip marker if later merging this docs-only
change is intended to remain CI-free.

The latest prefab candidate cb590f0 passed four pre-merge checks. That receipt is
not a substitute for future code-change validation, nor approval to distribute.
