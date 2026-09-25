@echo off
rem Собирает редактор профилей wxl.gen.ui в Debug и запускает его. Цель
rem wxl.gen.ui — редактор и то, на чём он держится, без тестов и документации.
setlocal
set "ROOT=%~dp0"
set "EDITOR=%ROOT%build\x64\wxl.gen.ui\Debug"

pwsh -NoLogo -NoProfile -File "%ROOT%tools\build.ps1" -Target wxl.gen.ui
if errorlevel 1 (
    rem LNK1168 — редактор ещё открыт и держит свой exe.
    pause
    exit /b 1
)

start "" /D "%EDITOR%" "%EDITOR%\wxl.gen.ui.exe"
