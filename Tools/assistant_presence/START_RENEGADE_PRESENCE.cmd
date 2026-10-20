@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0START_RENEGADE_PRESENCE.ps1"
if errorlevel 1 (
 echo.
 echo Renegade Living Core did not launch. See the message above.
 pause
)
exit /b %errorlevel%
