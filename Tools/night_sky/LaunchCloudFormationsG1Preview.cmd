@echo off
setlocal
for %%I in ("%~dp0..\..") do set "ROOT=%%~fI"
set "STUDIO=%ROOT%\BUILD\gate1\Studio\Release"
if not exist "%STUDIO%\RenegadeStudioCloudFormationsG1.exe" exit /b 1
pushd "%STUDIO%"
start "Cloud Formation Gate 1" "RenegadeStudioCloudFormationsG1.exe" cloud-formation-preview
popd
endlocal
