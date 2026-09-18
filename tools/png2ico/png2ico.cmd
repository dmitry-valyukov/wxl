@echo off
rem Проверяем, передан ли файл батнику
if "%~1"=="" (
    echo Ошибка: Перетащите PNG файл на этот батник или укажите имя файла в консоли.
    echo Пример: convert.bat logo.png
    pause
    exit /b 1
)

rem Запускаем скрипт через uv, передавая ему полный путь к файлу
uv run "%~dp0png2ico.py" "%~1"

if %errorlevel% neq 0 (
    echo Произошла ошибка во время сборки иконки.
    pause
    exit /b %errorlevel%
)
