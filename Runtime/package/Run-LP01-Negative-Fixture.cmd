@echo off
cd /d "%~dp0"
echo LP01 negative fixture launcher
echo.
echo 1 - Invalid project version
echo 2 - Escaping startup path
echo 3 - Missing startup scene
echo 4 - Corrupt WISCENE
echo 5 - Missing manifest
set /p choice=Choose a case:

if "%choice%"=="1" goto invalid_version
if "%choice%"=="2" goto escaping_path
if "%choice%"=="3" goto missing_scene
if "%choice%"=="4" goto corrupt_scene
if "%choice%"=="5" goto missing_manifest

echo Unknown choice.
exit /b 2

:invalid_version
start "" /wait "RenegadeRuntime.exe" dx12 --project "Content\LP01\Invalid Version\InvalidVersion.renegade"
goto result

:escaping_path
start "" /wait "RenegadeRuntime.exe" dx12 --project "Content\LP01\Escaping Path\EscapingPath.renegade"
goto result

:missing_scene
start "" /wait "RenegadeRuntime.exe" dx12 --project "Content\LP01\Missing Scene\MissingScene.renegade"
goto result

:corrupt_scene
start "" /wait "RenegadeRuntime.exe" dx12 --project "Content\LP01\Corrupt Scene\CorruptScene.renegade"
goto result

:missing_manifest
start "" /wait "RenegadeRuntime.exe" dx12 --project "Content\LP01\Missing Manifest\Missing.renegade"

:result
set "runtime_exit=%ERRORLEVEL%"
echo.
echo Exit code: %runtime_exit%
echo Evidence log: Logs\RuntimeBootstrap.log
pause
exit /b %runtime_exit%
