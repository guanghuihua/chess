@echo off
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0mindduet-agent.ps1" %*
exit /b %ERRORLEVEL%
