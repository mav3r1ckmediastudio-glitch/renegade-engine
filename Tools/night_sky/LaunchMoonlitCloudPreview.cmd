@echo off
setlocal
for %%I in ("%~dp0..\..") do set "ROOT=%%~fI"
set "PREVIEW=%ROOT%\BUILD\moonlit\Studio\Release"
set "PROJECT=%ROOT%\BUILD\moonlit-preview-projects\Renegade Moonlit Clouds Preview\Renegade Moon Preview.renegade"
if not exist "%PREVIEW%\RenegadeStudio.exe" (
 echo Moonlit Clouds preview executable is missing. Build the isolated Release candidate first.
 pause
 exit /b 1
)
if not exist "%PROJECT%" (
 echo Isolated Moonlit Clouds preview project is missing.
 pause
 exit /b 1
)
echo Renegade Moonlit Clouds V1 - isolated test build.
echo Existing moon, night-sky and inventory projects will not be opened automatically.
echo Within Studio, Open Project:
echo %PROJECT%
pushd "%PREVIEW%"
start "Renegade Moonlit Clouds Preview" "%PREVIEW%\RenegadeStudio.exe" %*
popd
endlocal
