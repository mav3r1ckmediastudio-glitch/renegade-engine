@echo off
setlocal
for %%I in ("%~dp0..\..") do set "ROOT=%%~fI"
set "STUDIO=%ROOT%\BUILD\moonlit\Studio\Release"
set "EXE=%STUDIO%\RenegadeStudioCloudsV2Macro.exe"
set "PROJECT=%ROOT%\BUILD\moonlit-preview-projects\Renegade Moonlit Clouds Preview\Renegade Moon Preview.renegade"
if not exist "%EXE%" (
 echo Clouds V2 Macro Formation preview exe not built.
 pause
 exit /b 1
)
echo Clouds V2 Gate 1: broader volumetric formations, isolated preview.
echo Save and close other Renegade Studio preview before reopening the project.
echo Open Project in the hub:
echo %PROJECT%
pushd "%STUDIO%"
start "Renegade Clouds V2 Macro Formation Preview" "%EXE%"
popd
endlocal
