@echo off
rem Double-click launcher for Update-DiceVox.ps1 (lets it run without changing PowerShell settings).
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Update-DiceVox.ps1" %*
