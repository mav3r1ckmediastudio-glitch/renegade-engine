@echo off
setlocal
for %%I in ("%~dp0..\..") do set "ROOT=%%~fI"
set "STUDIO=%ROOT%\BUILD\gate1\Studio\Release"
if not exist "%STUDIO%\RenegadeStudioCloudBoundaryG1.exe" exit /b 1
pushd "%STUDIO%"
start "Cloud Boundary Gate 1" "RenegadeStudioCloudBoundaryG1.exe" cloud-formation-boundary-preview
popd
endlocal
