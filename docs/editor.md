# Работа в редакторе

## IntelliSense в VS Code для модулей

**Зачем.** Из коробки в редакторе все `import :partition;` подсвечиваются как
ошибки. Причина — несовершенство сегодняшнего инструментария в сценарии
сборки модулей: расширение C/C++ читает интерфейсы модулей (`.ifc`) только
если ему явно передать `/reference <модуль>=<путь>`, а CMake кладёт эти ключи
в файлы ответов `*.obj.modmap`, на которые в `compile_commands.json` остаётся
лишь ссылка `@…modmap` — разворачивать её cpptools не умеет. Провайдер
конфигурации CMake Tools тоже их не отдаёт.

**Где работает.** Всё ниже — про `wxl.core`, открытый в VS Code отдельной
папкой и собранный своей настройкой: `cmake --preset x64` и
`cmake --build --preset x64-debug` в каталоге `wxl.core`, каталог сборки —
`wxl.core/build/x64`. Скрипт пишет `wxl.core/.vscode/c_cpp_properties.json`;
в общем дереве wxl, как и в любом другом, где `wxl.core` — подпроект, шаг
после сборки выключен по умолчанию.

**Как оживить:**

1. Собрать проект (нужны готовые `.ifc`; конфигурация по умолчанию — `Debug`).
2. Запустить генератор конфигурации:
   `pwsh wxl.core/tools/gen-intellisense-config.ps1` (для другой
   конфигурации — `-Config Release`, для другого каталога сборки —
   `-BuildDir`). Он соберёт все `*.obj.modmap`, развернёт пути и выпишет
   `/reference` в `.vscode/c_cpp_properties.json`.
3. Если подсветка не обновилась — «C/C++: Restart IntelliSense for Active
   File» или перезагрузить окно.

Шаг 2 руками нужен только в первый раз: дальше генератор запускается сам
после сборки `wxl.core` (post-build шаг). Он срабатывает **только если сборка
запущена из среды разработки** — признаком служат переменные окружения,
которые IDE выставляет своим дочерним процессам (`VSCODE_PID` и родственные,
`VSAPPIDNAME`/`VSAPPIDDIR` у Visual Studio). Окружение Developer Command
Prompt признаком не считается (`VisualStudioVersion` и `VSINSTALLDIR` ставит
и `vcvars64.bat`), поэтому консольные сборки и CI конфиг редактора не
трогают. Принятое решение пишется в
`wxl.core/build/x64/intellisense-config.log`. Отключается опцией
`WXL_UPDATE_INTELLISENSE=OFF`.

Конфиг обновляется под ту конфигурацию, которую только что собрали: собрал
Release — в редакторе `.ifc` из Release.

**Чего не делать:** не соглашаться на предложение VS Code использовать CMake
Tools как `configurationProvider`. Тогда cpptools берёт аргументы у
провайдера, игнорирует `compilerArgs` из `c_cpp_properties.json`, и
`/reference` до IntelliSense не доходит.

Требуется расширение C/C++ версии, умеющей читать текущий формат `.ifc` (для
MSVC 14.51 это IFC 0.44 — поддержан начиная с pre-release 1.33.x).

**Побочный эффект.** Прочитанные `.ifc` cpptools держит открытыми, и сборка
может упасть на `error C3474: could not open output file '….ifc'`. Лечится
перезапуском IntelliSense («C/C++: Restart IntelliSense for Active File»,
перезагрузка окна) — файлы освобождаются, сборка идёт.
