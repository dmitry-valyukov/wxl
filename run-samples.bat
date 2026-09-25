@echo off
rem Собирает примеры wxl в Release и запускает выбранный из списка.
rem Цель samples/all — только примеры и то, на чём они держатся: ни тесты,
rem ни документация, ни песочница не собираются.
setlocal EnableExtensions EnableDelayedExpansion

rem Текст меню в этом файле в UTF-8, поэтому кодовая страница консоли на
rem время работы переключается и в конце возвращается прежняя.
for /f "tokens=2 delims=:" %%c in ('chcp') do set "OLDCP=%%c"
set "OLDCP=%OLDCP: =%"
chcp 65001 >nul

set "ROOT=%~dp0"
set "SAMPLES=%ROOT%build\x64\samples"
set "blank=0"

rem Чем занят каждый пример — по имени его папки. Папка без строки здесь
rem попадёт в список с именем исполняемого файла вместо пояснения.
set "about.HelloHere=весь интерфейс одной функцией, переключение темы"
set "about.Calculator=калькулятор на пресетах и градиентных кистях"
set "about.Quadratic=квадратное уравнение на observable-полях"
set "about.Trayed=консоль чужой программы в своём окне, из трея"
set "about.HtmlView=показ разметки HTML, BB и RSDN без тяжеловесного WebView2"
set "about.CustomTitleBar=свой заголовок окна и масштаб всего острова"

echo Сборка примеров, Release...
call :build
if errorlevel 1 (
    rem Ninja спотыкается на dyndep, когда в каталоге сборки остался выхлоп
    rem от другого набора целей; лечится очисткой конфигурации.
    echo.
    echo Сборка не задалась; чищу Release и повторяю.
    pwsh -NoLogo -NoProfile -File "%ROOT%tools\build.ps1" -Config Release -Target clean
    call :build
)
if errorlevel 1 (
    echo.
    echo Собрать примеры не удалось.
    pause
    goto :quit
)

rem Список — по папкам примеров в дереве, а не по каталогу сборки: выхлоп
rem примера, уехавшего в песочницу, остаётся там до очистки и в меню не нужен.
:menu
set "count=0"
for /d %%d in ("%ROOT%samples\*") do (
    for %%f in ("%SAMPLES%\%%~nxd\Release\*.exe") do (
        set /a count+=1
        set "exe[!count!]=%%~ff"
        set "what=!about.%%~nxd!"
        if not defined what set "what=%%~nf"
        set "col=%%~nxd                 "
        set "name[!count!]=!col:~0,17! — !what!"
        set "what="
    )
)
if %count%==0 (
    echo.
    echo Собранных примеров нет в "%SAMPLES%".
    pause
    goto :quit
)

echo.
echo   Примеры wxl, Release
echo.
for /l %%i in (1,1,%count%) do echo     %%i. !name[%%i]!
echo     0. Выход
echo.
set "choice="
set /p "choice=Номер примера: "
if not defined choice (
    rem Три пустых ответа подряд значат, что отвечать некому: так бывает,
    rem когда ввод скрипту перенаправили и он кончился.
    set /a blank+=1
    if !blank! geq 3 goto :quit
    goto menu
)
set "blank=0"
if "!choice!"=="0" goto :quit

set "pick="
for /l %%i in (1,1,%count%) do if "!choice!"=="%%i" set "pick=%%i"
if not defined pick (
    echo Нет такого пункта.
    goto menu
)

set "target=!exe[%pick%]!"
for %%f in ("!target!") do set "workdir=%%~dpf"
set "workdir=!workdir:~0,-1!"
echo.
echo Запускаю !name[%pick%]!; список вернётся, когда закроете окно примера.
start "" /wait /d "!workdir!" "!target!"
goto menu

:build
pwsh -NoLogo -NoProfile -File "%ROOT%tools\build.ps1" -Config Release -Target samples/all
exit /b %errorlevel%

:quit
chcp %OLDCP% >nul
endlocal
exit /b 0
