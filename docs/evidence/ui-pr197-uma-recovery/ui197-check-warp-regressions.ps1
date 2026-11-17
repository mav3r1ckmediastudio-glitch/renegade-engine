$ErrorActionPreference='Stop'
$env:GIT_CONFIG_COUNT='3'
$env:GIT_CONFIG_KEY_0='safe.directory';$env:GIT_CONFIG_VALUE_0='E:/RenegadeUIPreflight-20261117'
$env:GIT_CONFIG_KEY_1='safe.directory';$env:GIT_CONFIG_VALUE_1='E:/RenegadeUIPreflight-20261117/WickedEngine'
$env:GIT_CONFIG_KEY_2='safe.directory';$env:GIT_CONFIG_VALUE_2='E:/RenegadeUIPreflight-20261117/ThirdParty/imnodes'
$env:MSBUILD_EXE_PATH='C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe'
$env:PATH=(Split-Path $env:MSBUILD_EXE_PATH)+';'+$env:PATH
$env:CL='/MP2'
$env:RENEGADE_HOSTED_HEADLESS_GRAPHICS='1'
Set-Location E:/RenegadeUIPreflight-20261117
$logs='E:/RenegadeUI-20261116/BUILD/ui197-uma-recovery'
$filter='^Renegade(GpuUploadBufferTests|RendererOverlayComposition|RiverDefaultTerrainRecovery|MaximumTerrainRiver_.*|MaximumTerrainGPU_.*|TerrainInitializationRecovery|TerrainPreviewRender[012]Recovery)$'
if((Get-Content BUILD/renegade/CMakeCache.txt | Out-String) -notmatch 'RENEGADE_TEST_FORCE_WARP:BOOL=ON'){throw 'Real WARP test configuration is not enabled'}
& ctest --test-dir BUILD/renegade -C Release -R $filter --timeout 180 --output-on-failure *> "$logs/tests-fixed-warp-release.log"
if($LASTEXITCODE -ne 0){Get-Content "$logs/tests-fixed-warp-release.log" -Tail 90;throw 'Corrected Release WARP regression checks failed'}
Get-Content "$logs/tests-fixed-warp-release.log" -Tail 12
& cmake --build BUILD/renegade --config Debug --target RenegadeGpuUploadBufferTests RenegadeRiverTerrainFollowProof --parallel 1 *> "$logs/build-fixed-warp-debug.log"
if($LASTEXITCODE -ne 0){Get-Content "$logs/build-fixed-warp-debug.log" -Tail 45;throw 'Corrected Debug WARP build failed'}
& ctest --test-dir BUILD/renegade -C Debug -R $filter --timeout 180 --output-on-failure *> "$logs/tests-fixed-warp-debug.log"
if($LASTEXITCODE -ne 0){Get-Content "$logs/tests-fixed-warp-debug.log" -Tail 90;throw 'Corrected Debug WARP regression checks failed'}
Get-Content "$logs/tests-fixed-warp-debug.log" -Tail 12
Write-Output 'FIXED_NATIVE_WARP_REGRESSION_CHECKS_PASS_BOTH_CONFIGURATIONS'
