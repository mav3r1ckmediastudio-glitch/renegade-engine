$ErrorActionPreference='Stop'
$env:GIT_CONFIG_COUNT='3'
$env:GIT_CONFIG_KEY_0='safe.directory';$env:GIT_CONFIG_VALUE_0='E:/RenegadeUIPreflight-20261117'
$env:GIT_CONFIG_KEY_1='safe.directory';$env:GIT_CONFIG_VALUE_1='E:/RenegadeUIPreflight-20261117/WickedEngine'
$env:GIT_CONFIG_KEY_2='safe.directory';$env:GIT_CONFIG_VALUE_2='E:/RenegadeUIPreflight-20261117/ThirdParty/imnodes'
$env:MSBUILD_EXE_PATH='C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe'
$env:PATH=(Split-Path $env:MSBUILD_EXE_PATH)+';'+$env:PATH
$env:CL='/MP2'
Remove-Item Env:RENEGADE_HOSTED_HEADLESS_GRAPHICS -ErrorAction SilentlyContinue
Set-Location E:/RenegadeUIPreflight-20261117
$logs='E:/RenegadeUI-20261116/BUILD/ui197-uma-recovery'
& cmake -S . -B BUILD/renegade -A x64 -DRENEGADE_EMBED_SHADERS=ON -DCMAKE_SUPPRESS_REGENERATION=ON -DRENEGADE_TEST_FORCE_WARP=OFF *> "$logs/config-fixed-hardware.log"
if($LASTEXITCODE -ne 0){Get-Content "$logs/config-fixed-hardware.log" -Tail 35;throw 'Hardware configuration failed'}
& cmake --build BUILD/renegade --config Release --target RenegadeGpuUploadBufferTests RenegadeStudio --parallel 1 *> "$logs/build-fixed-hardware-release.log"
if($LASTEXITCODE -ne 0){Get-Content "$logs/build-fixed-hardware-release.log" -Tail 40;throw 'Hardware Studio build failed'}
if((Get-Content BUILD/renegade/CMakeCache.txt | Out-String) -notmatch 'RENEGADE_TEST_FORCE_WARP:BOOL=OFF'){throw 'Test-only software configuration was not disabled'}
Write-Output 'FIXED_HARDWARE_STUDIO_RELEASE_BUILD_READY'
