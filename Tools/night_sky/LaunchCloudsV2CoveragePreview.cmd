@echo off
setlocal
for %%I in ("%~dp0..\..") do set "ROOT=%%~fI"
set "STUDIO=%ROOT%\BUILD\moonlit\Studio\Release"
set "EXE=%STUDIO%\RenegadeStudioCloudsV2Coverage.exe"
set "PROJECT=%ROOT%\BUILD\moonlit-preview-projects\Renegade Moonlit Clouds Preview\Renegade Moon Preview.renegade"
if not exist "%EXE%" (
 echo Clouds V2 Coverage preview executable not built.
 pause
 exit /b 1
)
echo Clouds V2 Gate 1 coverage fix (independent editor).
echo Save your current scene and close the older preview before opening the same project here.
echo Then open this test project from the Project Hub:
echo %PROJECT%
pushd "%STUDIO%"
start "Renegade Clouds V2 Coverage Fix" "%EXE%"
popd
endlocal
