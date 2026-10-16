@echo off
setlocal
for %%I in ("%~dp0..\..") do set "ROOT=%%~fI"
set "PREVIEW=%ROOT%\BUILD\night-sky-check\Studio\Release"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0CheckNightSkyPreview.ps1"
if errorlevel 1 (
  echo.
  echo The Night Sky preview is not validated. No Studio was launched.
  pause
  exit /b 1
)
echo.
echo Starting the isolated Night Sky Studio, NOT your normal Renegade build.
echo Open Project: Downloads\Renegade Night Sky Preview\Renegade Night Sky Visual Test
echo.
pushd "%PREVIEW%"
start "Renegade Night Sky Preview" "%PREVIEW%\RenegadeStudio.exe" %*
popd
endlocal
