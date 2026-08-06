@echo off
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0mindduet-init.ps1" %*
exit /b %ERRORLEVEL%
