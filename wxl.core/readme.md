# wxl.core

Библиотека ядра wxl, собранная как C++20-модули (`wxl.core` + партиции по одной на файл).

## IntelliSense в VS Code для модулей

**Зачем.** Из коробки в редакторе все `import :partition;` подсвечиваются как ошибки. Причина —
несовершенство сегодняшнего инструментария в сценарии сборки модулей: расширение C/C++ читает
интерфейсы модулей (`.ifc`) только если ему явно передать `/reference <модуль>=<путь>`, а CMake
кладёт эти ключи в файлы ответов `*.obj.modmap`, на которые в `compile_commands.json` остаётся лишь
ссылка `@…modmap` — разворачивать её cpptools не умеет. Провайдер конфигурации CMake Tools тоже
их не отдаёт.

**Как оживить:**

1. Собрать проект (нужны готовые `.ifc`; конфигурация по умолчанию — `Debug`).
2. Запустить генератор конфигурации:
   `pwsh tools/gen-intellisense-config.ps1` (для другой конфигурации — `-Config Release`).
   Он соберёт все `*.obj.modmap`, развернёт пути и выпишет `/reference` в `.vscode/c_cpp_properties.json`.
3. Если подсветка не обновилась — «C/C++: Restart IntelliSense for Active File» или перезагрузить окно.

Шаг 2 руками нужен только в первый раз: дальше генератор запускается сам после сборки `wxl.core`
(post-build шаг). Он срабатывает **только если сборка запущена из среды разработки** — признаком
служат переменные окружения, которые IDE выставляет своим дочерним процессам (`VSCODE_PID` и
родственные, `VSAPPIDNAME`/`VSAPPIDDIR` у Visual Studio). Окружение Developer Command Prompt
признаком не считается (`VisualStudioVersion` и `VSINSTALLDIR` ставит и `vcvars64.bat`), поэтому
консольные сборки и CI конфиг редактора не трогают. Принятое решение пишется в
`build/x64/intellisense-config.log`. Отключается опцией `WXL_UPDATE_INTELLISENSE=OFF`; в чужом
дереве (wxl как сабпроект) шаг выключен по умолчанию.

Конфиг обновляется под ту конфигурацию, которую только что собрали: собрал Release — в редакторе
`.ifc` из Release.

**Чего не делать:** не соглашаться на предложение VS Code использовать CMake Tools как
`configurationProvider`. Тогда cpptools берёт аргументы у провайдера, игнорирует `compilerArgs`
из `c_cpp_properties.json`, и `/reference` до IntelliSense не доходит.

Требуется расширение C/C++ версии, умеющей читать текущий формат `.ifc` (для MSVC 14.51 это IFC 0.44 —
поддержан начиная с pre-release 1.33.x).

**Побочный эффект.** Прочитанные `.ifc` cpptools держит открытыми, и сборка может упасть на
`error C3474: could not open output file '….ifc'`. Лечится перезапуском IntelliSense
(«C/C++: Restart IntelliSense for Active File», перезагрузка окна) — файлы освобождаются, сборка идёт.

## Подключение из другого проекта

Требования к потребителю: **генератор Ninja или Ninja Multi-Config** и открытый экспериментальный гейт
`import std;` — он должен быть выставлен **до `project()`**, поэтому wxl не может сделать это за
потребителя и вместо этого падает с понятной ошибкой.

Требование Ninja — это ограничение **генераторов Visual Studio в CMake**: они не поддерживают
`import std;` и не умеют собирать BMI импортированных таргетов. Сам MSBuild модули поддерживает
штатно (`.ixx` компилируется как `CompileAsCppModule`, граф модулей строится через
`ScanSourceForModuleDependencies`, `import std;` включается метаданными `BuildStlModules`), так что
для экосистемы VS путь лежит через NuGet-пакет с `build/native/*.props|targets`, а не через
CMake-генератор VS.

**Вариант 1 — FetchContent (основной).** Исходники собираются вместе с проектом потребителя, тем же
компилятором и с теми же флагами, поэтому каждый `.ixx` компилируется ровно один раз на всё дерево
сборки:

```cmake
cmake_minimum_required(VERSION 4.2)

# ДО project(): гейт CMake для `import std;`. UUID привязан к версии CMake,
# актуальный лежит в cmake/wxl_import_std_gate.cmake внутри wxl.
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "f35a9ac6-8463-4d38-8eec-5d6008153e7d")

project(my_app LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(wxl GIT_REPOSITORY <url> GIT_TAG <tag>)
FetchContent_MakeAvailable(wxl)

add_executable(app main.cpp)
target_link_libraries(app PRIVATE wxl::core)
```

**Вариант 2 — установленный пакет.** `cmake --install <build> --config Debug` и то же для `Release`
(устанавливать можно только собранные конфигурации), затем у потребителя:

```cmake
find_package(wxl 0.1.0 CONFIG REQUIRED COMPONENTS core)
target_link_libraries(app PRIVATE wxl::core)
```

Пакет разбит на компоненты — по одному экспорт-набору на компонент: `core` нужен, например,
генератору кода, а приложению достаточно попросить `winui`, который притянет `core` транзитивно.
`find_package(wxl CONFIG REQUIRED)` без `COMPONENTS` подключает всё, что есть в установке
(компоненты определяются по файлам `wxl-<компонент>-targets.cmake`, руками список нигде не ведётся).
Запрос отсутствующего компонента даёт внятную ошибку вида
`wxl component 'winui' is not part of this installation`. Ставить по отдельности тоже можно:
`cmake --install <build> --config Release --component core`.

Пакет кладёт `lib/wxl.core.lib` и `lib/wxl.cored.lib`, исходники интерфейсов модулей и `abi.h`/
`platform.h` в `include/wxl/`, а также CMake-glue в `lib/cmake/wxl/`. Собранные `.ifc` не
поставляются принципиально — они привязаны к конкретной сборке компилятора и флагам, поэтому
интерфейсы модулей компилируются один раз в дереве потребителя.

`find_package` работает и против несобранного в install дерева:
`find_package(wxl CONFIG REQUIRED COMPONENTS core PATHS <wxl-build-dir>)`.

Вместе с пакетом приезжают хелперы (`wxl_add_module_library`, `wxl_set_target_folders`,
`wxl_add_gtest`) — `wxl-config.cmake` подключает их сразу, так что потребитель может описывать свои
модульные таргеты в том же стиле.

**Вариант 3 — NuGet-пакет для MSBuild/Visual Studio.** Собирается из готового дерева CMake:
`pwsh packaging/nuget/pack.ps1` (кладёт `.nupkg` в `build/nuget`). Потребителю достаточно ссылки на
пакет — остальное настраивает `build/native/wxl.core.props|targets` из самого пакета:

```xml
<ItemGroup>
  <PackageReference Include="wxl.core" Version="0.1.0" />
</ItemGroup>
```

Пакет добавляет `.ixx` в компиляцию проекта (граф модулей MSBuild строит сам), включает
`/std:c++latest`, `BuildStlModules` (это `import std;` на языке MSBuild), `/utf-8`, прописывает
include-каталог и нужную конфигурации `.lib`. Версия MSVC, которой собрана библиотека, записывается
в props при упаковке и сверяется при сборке — при несовпадении сборка падает с внятным сообщением, а
не с грудой ошибок компилятора (обойти можно `WxlCoreAllowToolsetMismatch=true`).

Требование Ninja сюда не относится: ограничение было у генераторов Visual Studio в CMake, а MSBuild
собирает модули сам. Цена — интерфейсы компилируются **в каждом проекте**, который их импортирует,
тогда как в CMake они собираются один раз на всё дерево.

Внутри `.nupkg` лежит тот же install-префикс, поэтому пакет работает и как CMake-пакет: достаточно
указать `CMAKE_PREFIX_PATH` на распакованный/восстановленный каталог пакета (например
`~/.nuget/packages/wxl.core/0.1.0`) и вызвать привычный `find_package(wxl CONFIG REQUIRED COMPONENTS core)`.

Правила установки включаются опцией `WXL_INSTALL` (по умолчанию — только когда wxl собирается как
самостоятельный проект, чтобы `cmake --install` потребителя не тащил наши файлы в его префикс).
