@echo off
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0mindduet-init-core.ps1" %*
exit /b %ERRORLEVEL%
