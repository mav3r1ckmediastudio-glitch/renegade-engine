@echo off
setlocal
cd /d "%~dp0"
set "PROJECT=%CD%\Content\LP03\Valid Screen\ScreenProject.renegade"
if not exist "%PROJECT%" (
  echo LP03 fixture is missing: "%PROJECT%"
  exit /b 2
)
echo Launching LP03 Runtime screen proof on DX12...
echo Project: "%PROJECT%"
RenegadeRuntime.exe dx12 "--project=%PROJECT%"
set "RESULT=%ERRORLEVEL%"
echo Runtime exit code: %RESULT%
if exist "Logs\RuntimeBootstrap.log" type "Logs\RuntimeBootstrap.log"
exit /b %RESULT%
