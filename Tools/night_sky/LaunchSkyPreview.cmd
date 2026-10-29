@echo off
setlocal
for %%I in ("%~dp0..\..") do set "ROOT=%%~fI"
set "PREVIEW=%ROOT%\BUILD\moon\Studio\Release"
if not exist "%PREVIEW%\RenegadeStudioSkyPreview.exe" (
 echo Sky preview executable is missing. Build the Release candidate first.
 pause
 exit /b 1
)
echo Moon and shooting stars preview. Save any earlier unsaved scene before switching.
echo Open Project: %ROOT%\BUILD\meteor-preview-projects\Renegade Sky Preview\Renegade Sky Preview.renegade
pushd "%PREVIEW%"
start "Renegade Sky Preview" "%PREVIEW%\RenegadeStudioSkyPreview.exe" %*
popd
endlocal
