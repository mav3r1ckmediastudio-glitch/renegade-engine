# G5 cross-volume offline shader build correction

2026-10-31. Owner authorized fixing the E: build after free C: space measured16.1GiB. G4 remains signed off; no visual source changes.

## Confirmed cause

The new external-drive build failed all offline shader inputs, including DX12 and SPIR-V. The initial report mentioning only SPIR-V was incomplete. DXC loaded successfully, version1.9; compiler DLL hashes matched the source and working build. The tool backlog instead reported missing shader files directly under its build working directory.

ShaderInterop.h derives SHADER_INTEROP_PATH using GetPathRelative(current directory, __FILE__). wiHelper::MakePathRelative falls back to the filename when relative conversion fails across drive letters. This loses the shader directory, so subsequent compilation searches the E: build root while sources live on C:. Same-drive working builds do not encounter this fallback. This is a build-path defect, not evidence of shader logic regression.

## Correction

CMakeLists.txt includes cmake/RenegadeOfflineShaderCompiler.cmake immediately after Wicked configuration. That Renegade adapter renames only the upstream offline compiler entry when compiling it and adds Tools/OfflineShaderCompilerEntry.cpp. The wrapper sets the renderer shader-source path from CMake's explicit source directory before invoking the original compiler main. This affects only the offline tool, with no upstream source edits, shader changes, runtime overrides or renderer lookup changes. No Wicked core patch or pointer change is required.

Original root CMake is protected in the external G5 audit as CMakeLists.before-path-fix.txt. Rollback removes the adapter include and the two new build-tool files; rebuild the offline tool. Accepted Studio/Runtime output is not overwritten by this E: validation.

## Actual evidence and remaining work

Before: Debug-build-before-path-fix.log and shader-verbose.log report failures; offline tool log records missing source files and successful DXC loading.

After: new offline compiler builds, the same MSBuild shader-generation command completes for both DX12 and SPIR-V, produces a46,677,098-byte wiShaderDump.h and advances to compiling the embedded renderer. Shader files were not changed. The full Debug/Release application and test results are still pending; this is shader-stage verification only.

BUILD/g5-validation.py continues the original pass. BUILD/g5-ci-followup.py waits for it to exit before starting the canonical RenegadeStudioCI aggregate plus explicit weather test executables for both configurations, followed by unfiltered CTest with the official180-second per-case timeout. This avoids treating missing test executables from a partial target build as product failures. External G5 validation-status.json and Debug/Release-ci-build.log/-ci-tests.log are authoritative for the follow-up. Do not start concurrent build graphs or claim full CI/G5 completion from shader generation.

Next: resolve real configuration/test failures; package and verify all thunder originals/mono variants, snow resource and embedded clouds; real standalone package review. No commit, push, merge, remote CI or final G5 sign-off authorized/performed here.
