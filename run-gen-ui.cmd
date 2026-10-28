@echo off
rem Собирает редактор профилей wxl.gen.ui в Debug и запускает его. Цель
rem wxl.gen.ui — редактор и то, на чём он держится, без тестов и документации.
rem Пакеты профиля редактор читает из списка восстановления этой сборки: его
rem пишет настройка build\x64, и редактор получает его ключом --packages.
setlocal
set "ROOT=%~dp0"
set "EDITOR=%ROOT%build\x64\wxl.gen.ui\Debug"
set "PACKAGES=%ROOT%build\x64\wxl.packages\restored-packages.json"

pwsh -NoLogo -NoProfile -File "%ROOT%tools\build.ps1" -Target wxl.gen.ui
if errorlevel 1 (
    rem LNK1168 — редактор ещё открыт и держит свой exe.
    pause
    exit /b 1
)

start "" /D "%EDITOR%" "%EDITOR%\wxl.gen.ui.exe" --packages "%PACKAGES%"
