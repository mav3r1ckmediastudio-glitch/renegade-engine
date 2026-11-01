@echo off
setlocal
for %%I in ("%~dp0..\..") do set "ROOT=%%~fI"
set "STUDIO=%ROOT%\BUILD\moonlit\Studio\Release"
set "EXE=%STUDIO%\RenegadeStudioCloudsV2.exe"
set "PROJECT=%ROOT%\BUILD\moonlit-preview-projects\Renegade Moonlit Clouds Preview\Renegade Moon Preview.renegade"
if not exist "%EXE%" (
 echo Clouds V2 isolated Release build is missing.
 pause
 exit /b 1
)
if not exist "%PROJECT%" (
 echo The separately owned moonlit-cloud preview project is missing.
 pause
 exit /b 1
)
echo Renegade Clouds V2 / Weather Gate 1 - isolated visual preview.
echo Save any unsaved work in the older Studio first.
echo This launcher will NOT close or replace other Renegade sessions.
echo Open Project in the Clouds V2 Studio:
echo %PROJECT%
pushd "%STUDIO%"
start "Renegade Clouds V2 Preview" "%EXE%"
popd
endlocal
