@echo off
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" -Uninstall %*
if errorlevel 1 echo Uninstallation failed. Read the message above.
pause
