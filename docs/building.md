# Сборка и проверки

## Требования

Всё дерево — один проект CMake, и почти всё в нём — модули C++20 с
`import std;`; отсюда требования к инструментам:

- Windows x64.
- Visual Studio 2026 (канал Insiders) с C++, тулсетом MSVC 14.53 и Windows
  SDK — ради `import std;` в модулях C++20.
- CMake 4.3 или 4.4: снизу — `cmake_minimum_required(VERSION 4.3)`, сверху —
  UUID ключа `import std;`, известный `cmake/wxl_import_std_gate.cmake` для
  4.1–4.4.
- Ninja: генераторы Visual Studio не строят BMI (скомпилированные интерфейсы
  модулей) для импортированных целей.
- PowerShell 7 (`pwsh`).
- vcpkg с портом `cppwinrt:x64-windows-static-md`, для тестов — ещё
  `gtest:x64-windows-static-md`.
- Один раз `vcpkg integrate install`: сборка находит vcpkg по `vcpkg.path.txt`
  или по `-DVCPKG_ROOT=<путь>`, а не по переменной окружения — её подменяет
  `vcvars64.bat`.
- Сеть при первой настройке: пакеты NuGet, `winmd` и `fmt` через
  `FetchContent`, Doxygen 1.18 и тема для сайта документации.

## От клона до окна

Проекция cppwinrt (`generated/`) и обёртки wxl (`wxl.ui/src/wxl/`)
генерируются и в репозиторий не входят. Из корня дерева, в `pwsh`:

```powershell
pwsh -NoProfile -File tools\restore-packages.ps1    # пакеты NuGet, версии — tools\packages.json
pwsh -NoProfile -File tools\projection.ps1          # проекция cppwinrt в generated\
pwsh -NoProfile -File tools\install-runtime.ps1     # экспериментальная среда Windows App Runtime
tools\build.ps1 -Target winui-srcgen -Define WXL_BUILD_DOCS=OFF
build\x64\wxl.gen\Debug\winui-srcgen.exe --profile wxl.gen\profiles\full.json
tools\build.ps1 -Configure -Target sample.quadratic
build\x64\samples\Quadratic\Debug\sample.quadratic.exe
```

- Обёртки сделаны по метаданным экспериментального канала WinUI (в стабильном
  нет `WrapPanel` и `LayoutPanel`): без среды этого канала, которая ставится
  для текущего пользователя рядом со стабильной, приложение не запустится.
- Пока обёрток нет, настройка пропускает `wxl.ui` и всё, что от неё зависит,
  и предупреждает об этом. Сборка генератор не запускает: после правки
  профиля генерация и `-Configure` повторяются.
- Все примеры сразу — `run-samples.bat`: собирает их в Debug и предлагает
  запустить любой из списка.
- `WXL_BUILD_DOCS=OFF` остаётся в кэше; сайт документации и его тест
  возвращает `tools/build.ps1 -Define WXL_BUILD_DOCS=ON`.
- Пример лежит в `build/x64/samples/<Папка>/<Конфигурация>/<цель>.exe`
  с ресурсами и DLL; цели — в `samples/README.md`.

## Сборка

`tools/build.ps1` сам поднимает окружение Visual Studio с закреплённым тулсетом
(другой — `-Toolset`) и настраивает `build/x64`, если тот не настроен:

```powershell
tools\build.ps1                                   # всё, Debug
tools\build.ps1 -Target sample.calc               # одна цель
tools\build.ps1 -Configure                        # настроить заново
tools\build.ps1 -Define WXL_BUILD_DOCS=OFF,WXL_BUILD_BENCHMARKS=ON
```

В Developer PowerShell тулсет берётся из уже поднятого окружения, `-Toolset`
там игнорируется, и годятся пресеты: `cmake --preset x64`, затем
`cmake --build --preset x64-debug` или `x64-release`. Ключи `-Define` остаются
в `CMakeCache.txt`; список через запятую разбирает PowerShell, а из `cmd`
(`pwsh -File`) он приходит одной строкой — оттуда по одному ключу за запуск.

| Ключ | По умолчанию | Что делает |
| --- | --- | --- |
| `BUILD_TESTING` | `OFF` | тесты GTest; пресеты его не трогают |
| `WXL_BUILD_DOCS` | `ON` | сайт документации Doxygen, цель `docs`, результат — `build/x64/docs/wxl/html/index.html` |
| `WXL_BUILD_BENCHMARKS` | `OFF` | бенчмарки `wxl.core`, `wxl.async`, `wxl.fmt`, `wxl.ui` |
| `WXL_STRICT_CORO` | `OFF` | строгий режим проверок корутин (см. [«Асинхронность»](async.md)) |
| `WXL_CPPWINRT_PROJECTION_DIR` | `generated` в корне дерева | каталог проекции cppwinrt |
| `WXL_WINDOWSAPPSDK_VERSION_TAG` | `experimentalF` | канал Windows App Runtime, который просит приложение; пустое значение — стабильный |
| `WXL_ENABLE_PCH` | `ON` | предкомпилированный заголовок `src/pch.h` у `wxl.core` и `wxl.async`; модульные единицы из него исключены (MSVC не принимает его перед объявлением модуля), так что ключ влияет только на время сборки обычных единиц трансляции |
| `WXL_INSTALL` | `ON` у `wxl.core`, настроенного самого по себе; в общем дереве `OFF` — чтобы `cmake --install` потребителя не тащил файлы wxl в его префикс | правила установки `wxl.core` (см. ниже, «`wxl.core` отдельно») |
| `WXL_UPDATE_INTELLISENSE` | `ON` у `wxl.core`, настроенного самого по себе; в общем дереве `OFF` | после сборки из среды разработки обновлять конфигурацию IntelliSense VS Code (см. [«Работа в редакторе»](editor.md)) |
| `WXL_BACKEND` | `WinUI3` | объявлен, но сборка его не читает |

Сквозной оптимизации (`/GL`, LTCG) нет, и это решение по замеру, записанному
в корневом `CMakeLists.txt` (сентябрь 2026): без неё пул памяти быстрее —
28.4 нс против 31.7 на глубоком сценарии и 38.8 против 48.9 на мелком, —
проверка UTF-8 быстрее на 18 %, разбор XML не меняется, исполняемые файлы в
пределах процента. А объектные файлы с `/GL` несут промежуточное
представление вместо кода: статические библиотеки дерева вырастают с 42 МБ
до 1.7 ГБ, и каждая компоновка Release перекомпилирует программу целиком.

## Тесты и бенчмарки

```powershell
tools\build.ps1 -Define BUILD_TESTING=ON
ctest --preset x64-debug                 # из корня: пресет ищется в текущем каталоге
ctest --preset x64-debug -R docs         # сайт документации как тест, при WXL_BUILD_DOCS=ON
```

- **Тесты GTest** по модулям (`<модуль>/tests` или `/test`), общая обвязка —
  `test-support/`: фикстура, стенд нагрузочных тестов, свой каталог у каждого
  теста и процесса, чтобы тесты не мешали друг другу под `ctest -j`.
- **Поверхность синтаксиса.** `wxl.ui.surface-test` компилирует, ничего не
  запуская, весь словарь синтаксиса и собирается без пути к проекции
  cppwinrt — имя `winrt::`, просочившееся в публичный заголовок, ломает
  сборку. В него входят сгенерированный `schema_surface.cpp` — строка на
  каждый элемент `schema.h` — и рукописный `wxl.ui/test/dsl_surface.cpp` для
  того, что написано руками. Правило: всё новое в `schema.h` приходит со
  своей строкой проверки.
- **Бенчмарки** — отдельные исполняемые файлы, каждый сам печатает свою
  таблицу.

Проверки корутин — на странице [об асинхронности](async.md).

## Сайт документации

Сайт строит Doxygen из `docs/` и исходников. В дереве wxl это цель `docs`
при `WXL_BUILD_DOCS=ON`: в сборку по умолчанию она не входит и строится,
когда её просят, — `tools/build.ps1 -Target docs` или
`cmake --build --preset x64-debug --target docs`.

Каталог `docs` — ещё и самостоятельный проект CMake без компилятора:

```
cmake -S docs -B build/docs
cmake --build build/docs --target docs
ctest --test-dir build/docs
```

Doxygen читает исходники, не собирая их, поэтому так сайт строится там, где
код не собирается: без MSVC, без настройки дерева wxl, без сгенерированных
проекции и обёрток (страниц обёрток WinUI тогда на сайте нет). Результат —
`build/docs/docs/wxl/html/index.html`.

Нужен Doxygen 1.18 или новее: это первый выпуск, который делает из
`export module` страницу модуля. Если в системе такого нет, настройка сама
скачивает закреплённую сборку 1.18.0 — для Windows и для Linux; на других
системах его ставят руками. Тема — `doxygen-awesome-css`, закреплённая по
коммиту, тоже приезжает при настройке (`cmake/wxl_doxygen.cmake`).

## Свой проект

Приложение на wxl объявляется функциями из `cmake/`. Так выглядит
`samples/Quadratic/CMakeLists.txt`:

```cmake
wxl_add_executable(sample.quadratic
    main.cpp
)

target_link_libraries(sample.quadratic PRIVATE wxl::fmt)

wxl_target_icon(sample.quadratic app.ico)
wxl_target_assets(sample.quadratic Assets)
```

- `wxl_add_executable(<цель> <исходники>…)` — оконный exe с точкой входа из
  `wxl::ui` и манифестом из `cmake/wxl_app.manifest.in`; зовёт
  `wxl_target_runtime`.
- `wxl_target_runtime(<цель>)` — DLL загрузчика Windows App Runtime, Win2D и
  `WebView2Loader.dll` рядом с exe; руками — только для цели, объявленной иначе.
- `wxl_target_icon(<цель> <файл.ico>)` — значок в ресурсах exe; его же wxl
  ставит окнам без своего значка и значку в трее.
- `wxl_target_assets(<цель> <папка>)` — папка рядом с exe, по правилу на файл;
  адрес ресурса без схемы (`u"Assets/logo.png"`) ищется там же.

В исходниках — заголовки `wxl.ui` (`ui.h` и соседи, как в
`samples/HelloHere/pch.h`) и функция `wxl::Teardown wxl_launched()` из
`launch.h`. Проще всего — папка в `samples/` и строка в
`samples/CMakeLists.txt`.

### Рядом с деревом wxl

Свой проект рядом с деревом wxl подключает его подкаталоги. Порядок
важен: vcpkg и ворота `import std;` открываются до `project()` — UUID ворот
привязан к выпуску CMake, и открыть их может только верхний уровень, — а
стандарт и модули задаются до первого подкаталога. Дерево wxl при этом уже
настроено (проекция и обёртки сгенерированы), а пресет проекта даёт Ninja и
тот же триплет vcpkg — проще всего взять `CMakePresets.json` wxl:

```cmake
cmake_minimum_required(VERSION 4.3)

set(WXL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../wxl")
include("${WXL_ROOT}/cmake/vcpkg_bootstrap.cmake")
include("${WXL_ROOT}/cmake/wxl_import_std_gate.cmake")

project(MyApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_MODULE_STD 1)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
add_compile_options(/permissive- /utf-8)

include("${WXL_ROOT}/cmake/wxl_helpers.cmake")

foreach(module wxl.core wxl.xml wxl.fmt wxl.async wxl.html wxl.bb wxl.highlight wxl.rsdn)
    add_subdirectory("${WXL_ROOT}/${module}/src" "${module}" EXCLUDE_FROM_ALL)
endforeach()
add_subdirectory("${WXL_ROOT}/wxl.ui" wxl.ui EXCLUDE_FROM_ALL)

wxl_add_executable(myapp main.cpp)
wxl_target_assets(myapp Assets)
```

### FetchContent

Исходники собираются вместе с проектом потребителя, тем же компилятором и с
теми же флагами, поэтому каждый `.ixx` компилируется ровно один раз на всё
дерево сборки:

```cmake
cmake_minimum_required(VERSION 4.3)

# ДО project(): гейт CMake для `import std;`. UUID привязан к версии CMake:
# этот — для 4.1–4.3, у 4.4 свой; актуальные лежат в
# cmake/wxl_import_std_gate.cmake внутри wxl.
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")

project(my_app LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(wxl GIT_REPOSITORY <url> GIT_TAG <tag>)
FetchContent_MakeAvailable(wxl)

add_executable(app main.cpp)
target_link_libraries(app PRIVATE wxl::core)
```

### `wxl.core` отдельно

`wxl.core` настраивается и сам по себе, со своими пресетами
(`wxl.core/CMakePresets.json`, каталог сборки `wxl.core/build/x64`); тогда у
него включён `WXL_INSTALL` и пишет правила установки. Приложению на `wxl.ui`
нужен путь «рядом с деревом».

## Release и дефект MSVC

MSVC для x64 с оптимизацией (`/O1`, `/O2`) и синхронной моделью исключений
(`/EHsc`) не разрушает локальные переменные корутины, у которой
`final_suspend` возвращает `suspend_never`, и не освобождает её кадр, если
решит, что в теле корутины ничего не бросает; с `/Od` или `/EHa` всё верно
(`bugs/coro-report.md`: найдено на 14.51, в октябре 2026 так же ведёт себя
14.53). Так устроена `wxl::async::detached_task`, а выход приложения ждёт,
пока живых кадров не останется, — поэтому пример, собранный с
`-Config Release`, может не закрыться. Пока дефект не исправлен, рабочая
конфигурация — Debug.
