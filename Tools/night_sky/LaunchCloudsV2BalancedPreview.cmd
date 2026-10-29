@echo off
setlocal
for %%I in ("%~dp0..\..") do set "ROOT=%%~fI"
set "STUDIO=%ROOT%\BUILD\moonlit\Studio\Release"
set "EXE=%STUDIO%\RenegadeStudioCloudsV2Balanced.exe"
set "PROJECT=%ROOT%\BUILD\moonlit-preview-projects\Renegade Moonlit Clouds Preview\Renegade Moon Preview.renegade"
if not exist "%EXE%" (
 echo Clouds V2 Balanced build is missing.
 pause
 exit /b 1
)
echo Clouds V2 Gate 1 / Balanced Comparison — isolated Release preview.
echo Save current scene and close the earlier preview BEFORE opening the same project here.
echo In the hub, Open Project:
echo %PROJECT%
pushd "%STUDIO%"
start "Renegade Clouds V2 Balanced Preview" "%EXE%"
popd
endlocal
