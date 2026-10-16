# PR #183 Studio CI correction

The Studio Debug job for PR head `3ce02fe` built successfully, then CTest
failed with `RenegadeNightSkyTests (Not Run)`: its executable did not exist.
The build script uses explicit targets and had not included the newly
registered night-sky test target. Both Windows baseline configurations passed.

`Tools/Build-Studio-Windows.ps1` now includes `RenegadeNightSkyTests` in the
configuration-independent build command and in its artifact target manifest.
CTest registration and assertions remain enabled. No rendering code changes.

Validation:

- PowerShell AST parser: no syntax errors.
- Build command and manifest checked to include the new executable.
- `cmake --build BUILD/night-sky-check --config Release --target RenegadeNightSkyTests --parallel 4`
- `ctest --test-dir BUILD/night-sky-check -C Release -R '(SourceContract|^RenegadeNightSkyTests$|^RenegadeSunTests$)' --output-on-failure`

Local Release target build succeeded and CTest passed 58/58 (1.84 seconds).
Logs: BUILD/pr183-target-build.log and BUILD/pr183-fix-ctest.log.
Replacement CI must still complete before claiming the PR green. Existing owner acceptance and outstanding manual,
performance, Runtime visual and independent verification gates are unchanged.

## Current-main integration

The initial fix exposed a build-script conflict with main's existing aggregate
`RenegadeStudioCI` optimization. Merged origin/main (380dcc0) into the isolated
feature branch, retained its aggregate build and test timeout, and added
`RenegadeNightSkyTests` to the aggregate CMake dependencies. The artifact target
manifest also retains the night-sky entry. Main's working copy is untouched.
The final integrated PR head requires fresh CI; the local 58-test pass above
predates this integration.

Final integration validation: CMake configure/generate succeeded, and generated
RenegadeStudioCI.vcxproj contains a ProjectReference to RenegadeNightSkyTests.
PowerShell syntax validation passed. Focused CTest rerun uses existing Release
binaries; a full integrated rebuild remains the responsibility of fresh PR CI.
