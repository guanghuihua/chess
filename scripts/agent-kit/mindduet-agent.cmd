@echo off
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0mindduet-agent-core.ps1" %*
exit /b %ERRORLEVEL%
