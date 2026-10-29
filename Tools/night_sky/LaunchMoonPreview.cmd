@echo off
setlocal
for %%I in ("%~dp0..\..") do set "ROOT=%%~fI"
set "PREVIEW=%ROOT%\BUILD\moon\Studio\Release"
if not exist "%PREVIEW%\RenegadeStudioMoonFinal.exe" (
 echo Moon preview executable is missing. Build the Release candidate first.
 pause
 exit /b 1
)
echo Textured Moon preview. Save your scene in any earlier preview before switching.
echo Open Project: %ROOT%\BUILD\moon-preview-projects\Renegade Moon Preview\Renegade Moon Preview.renegade
pushd "%PREVIEW%"
start "Renegade Textured Moon Preview" "%PREVIEW%\RenegadeStudioMoonFinal.exe" %*
popd
endlocal
