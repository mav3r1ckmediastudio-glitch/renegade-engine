@echo off
setlocal
cd /d "%~dp0"

if not exist "RenegadeRuntime.exe" (
    echo ERROR: RenegadeRuntime.exe is missing beside this script.
    exit /b 2
)

RenegadeRuntime.exe dx12 ^
  --project "Content\LP02\Valid Flow\FlowProject.renegade" ^
  --flow-outcome level.complete ^
  --flow-outcome level.complete

set "RESULT=%ERRORLEVEL%"
echo.
echo Runtime exit code: %RESULT%
echo Evidence: Logs\RuntimeBootstrap.log
exit /b %RESULT%
