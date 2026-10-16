@echo off
setlocal
for %%I in ("%~dp0..\..") do set "ROOT=%%~fI"
set "PREVIEW=%ROOT%\BUILD\night-sky-check\Studio\Release"
if not exist "%PREVIEW%\RenegadeStudio.exe" (
  echo Night Sky preview not built. Expected "%PREVIEW%\RenegadeStudio.exe"
  exit /b 1
)
if not exist "%PREVIEW%\Content" (
  echo Night Sky preview resources are missing. Finish the standalone Studio build first.
  exit /b 2
)
echo Starting the isolated Renegade Night Sky preview.
echo Active Renegade Studio will not be replaced, but use a COPY of any project for testing.
pushd "%PREVIEW%"
start "Renegade Night Sky Preview" "%PREVIEW%\RenegadeStudio.exe" %*
popd
endlocal
