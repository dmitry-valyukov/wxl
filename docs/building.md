# Сборка и проверки

## Требования

Всё дерево представляет собой единый проект CMake, и почти весь код в нём
реализован в виде модулей C++20 с использованием `import std;`. Это
накладывает следующие требования к инструментарию:

- **Операционная система:** Windows x64.
- **Компилятор:** Visual Studio 2026 (канал Insiders) с поддержкой C++,
  набором инструментов MSVC версии 14.53 и Windows SDK; они необходимы для
  работы `import std;` в модулях C++20.
- **Система сборки:** CMake версии 4.3 или 4.4. Нижняя граница задана в
  конфигурации как `cmake_minimum_required(VERSION 4.3)`. Верхнюю границу
  определяет UUID-ключ механизма `import std;`: сценарию
  `cmake/wxl_import_std_gate.cmake` эти ключи известны для версий 4.1–4.4.
- **Генератор:** Ninja. Генераторы Visual Studio не строят BMI
  (скомпилированные интерфейсы модулей) для импортированных целей.
- **Командная оболочка:** PowerShell 7 (`pwsh`).
- **Управление пакетами:** vcpkg с портом `cppwinrt:x64-windows-static-md`.
  Для тестов дополнительно требуется порт `gtest:x64-windows-static-md`.
- **Интеграция vcpkg:** требуется однократно выполнить
  `vcpkg integrate install`. Сборка находит vcpkg по файлу `vcpkg.path.txt`
  или по ключу `-DVCPKG_ROOT=<путь>`, а не по переменной окружения, поскольку
  её подменяет `vcvars64.bat`.
- **Сетевое подключение:** необходимо при первой настройке для загрузки
  пакетов NuGet, зависимостей `winmd` и `fmt` через механизм `FetchContent`,
  а также Doxygen 1.18 и темы оформления сайта документации.

## От клона до окна

Сгенерированная проекция `cppwinrt` (каталог `generated/`) и декларативные
обёртки `wxl` (каталог `wxl.ui/src/wxl/`) намеренно исключены из
репозитория. Для их генерации и запуска первого приложения выполните в среде
`pwsh` из корня дерева следующие команды:

```powershell
pwsh -NoProfile -File tools\restore-packages.ps1    # пакеты NuGet, версии — tools\packages.json
pwsh -NoProfile -File tools\projection.ps1          # проекция cppwinrt в generated\
pwsh -NoProfile -File tools\install-runtime.ps1     # экспериментальная среда Windows App Runtime
tools\build.ps1 -Target winui-srcgen -Define WXL_BUILD_DOCS=OFF
build\x64\wxl.gen\Debug\winui-srcgen.exe --profile wxl.gen\profiles\full.json
tools\build.ps1 -Configure -Target sample.quadratic
build\x64\samples\Quadratic\Debug\sample.quadratic.exe
```

- **Экспериментальный канал:** обёртки построены по метаданным
  экспериментального канала WinUI, поскольку в стабильном канале нет
  `WrapPanel` и `LayoutPanel`. Среда этого канала устанавливается для
  текущего пользователя рядом со стабильной; без неё приложение не
  запустится.
- **Отсутствие обёрток:** пока обёртки не сгенерированы, настройка
  пропускает `wxl.ui` и всё, что от неё зависит, и выводит об этом
  предупреждение. Сборка не запускает генератор, поэтому после правки
  профиля необходимо повторить генерацию и `-Configure`.
- **Все примеры сразу:** сценарий `run-samples.bat` собирает их в
  конфигурации Debug и предлагает запустить любой из списка.
- **Сайт документации:** значение `WXL_BUILD_DOCS=OFF` сохраняется в кэше.
  Чтобы собрать сайт документации и его тест, выполните
  `tools/build.ps1 -Define WXL_BUILD_DOCS=ON`.
- **Расположение примеров:** собранный пример находится в
  `build/x64/samples/<Папка>/<Конфигурация>/<цель>.exe` вместе с ресурсами и
  DLL. Список целей приведён в `samples/README.md`.

## Сборка

Сценарий `tools/build.ps1` сам поднимает окружение Visual Studio с
закреплённым набором инструментов (другой задаётся ключом `-Toolset`) и
настраивает каталог `build/x64`, если тот ещё не настроен:

```powershell
tools\build.ps1                                   # всё, Debug
tools\build.ps1 -Target sample.calc               # одна цель
tools\build.ps1 -Configure                        # настроить заново
tools\build.ps1 -Define WXL_BUILD_DOCS=OFF,WXL_BUILD_BENCHMARKS=ON
```

В Developer PowerShell набор инструментов берётся из уже поднятого
окружения, и ключ `-Toolset` там игнорируется. В этой среде можно
использовать пресеты: `cmake --preset x64`, затем
`cmake --build --preset x64-debug` или `x64-release`. Ключи `-Define`
сохраняются в `CMakeCache.txt`. Список ключей через запятую разбирает
PowerShell; при запуске из `cmd` (`pwsh -File`) он передаётся одной строкой,
поэтому оттуда задаётся по одному ключу за запуск.

| Ключ | По умолчанию | Что делает |
| --- | --- | --- |
| `BUILD_TESTING` | `OFF` | Включает тесты GTest; пресеты этот ключ не задают. |
| `WXL_BUILD_DOCS` | `ON` | Включает сайт документации Doxygen: цель `docs`, результат — `build/x64/docs/wxl/html/index.html`. |
| `WXL_BUILD_BENCHMARKS` | `OFF` | Включает бенчмарки `wxl.core`, `wxl.async`, `wxl.fmt`, `wxl.ui`. |
| `WXL_STRICT_CORO` | `OFF` | Включает строгий режим проверок корутин (см. [«Асинхронность»](async.md)). |
| `WXL_CPPWINRT_PROJECTION_DIR` | `generated` в корне дерева | Задаёт каталог проекции cppwinrt. |
| `WXL_WINDOWSAPPSDK_VERSION_TAG` | `experimentalF` | Задаёт канал Windows App Runtime, который запрашивает приложение; пустое значение означает стабильный канал. |
| `WXL_ENABLE_PCH` | `ON` | Включает предкомпилированный заголовок `src/pch.h` у `wxl.core` и `wxl.async`. Модульные единицы из него исключены (MSVC не принимает его перед объявлением модуля), поэтому ключ влияет только на время сборки обычных единиц трансляции. |
| `WXL_INSTALL` | `ON` у `wxl.core`, настроенного самостоятельно; в общем дереве — `OFF`, чтобы `cmake --install` потребителя не переносил файлы wxl в его префикс | Включает правила установки `wxl.core` (см. ниже, «`wxl.core` отдельно»). |
| `WXL_UPDATE_INTELLISENSE` | `ON` у `wxl.core`, настроенного самостоятельно; в общем дереве — `OFF` | После сборки из среды разработки обновляет конфигурацию IntelliSense VS Code (см. [«Работа в редакторе»](editor.md)). |
| `WXL_BACKEND` | `WinUI3` | Объявлен, но сборка его не читает. |

Сквозная оптимизация (`/GL`, LTCG) не используется. Это решение принято по
замеру, записанному в корневом `CMakeLists.txt` (сентябрь 2026). Без неё пул
памяти работает быстрее: 28.4 нс против 31.7 на глубоком сценарии и 38.8
против 48.9 на мелком. Проверка UTF-8 ускоряется на 18 %, разбор XML не
меняется, а исполняемые файлы отличаются в пределах процента. Кроме того,
объектные файлы с `/GL` содержат промежуточное представление вместо кода:
статические библиотеки дерева вырастают с 42 МБ до 1.7 ГБ, а каждая
компоновка Release перекомпилирует программу целиком.

## Тесты и бенчмарки

```powershell
tools\build.ps1 -Define BUILD_TESTING=ON
ctest --preset x64-debug                 # из корня: пресет ищется в текущем каталоге
ctest --preset x64-debug -R docs         # сайт документации как тест, при WXL_BUILD_DOCS=ON
```

- **Тесты GTest:** расположены по модулям (`<модуль>/tests` или `/test`);
  общая обвязка находится в `test-support/`. Она включает фикстуру, стенд
  нагрузочных тестов и отдельный каталог для каждого теста и процесса, чтобы
  тесты не мешали друг другу при запуске `ctest -j`.
- **Статическая верификация синтаксиса:** цель `wxl.ui.surface-test` выполняет
  компиляцию всего декларативного словаря без запуска кода и собирается без
  путей к проекции cppwinrt. Благодаря этому любое имя `winrt::`, случайно
  просочившееся в публичный заголовок, гарантированно ломает сборку. В эту
  цель входят сгенерированный файл `schema_surface.cpp`, содержащий по одной
  проверочной строке на каждый элемент из `schema.h`, и рукописный тест
  `wxl.ui/test/dsl_surface.cpp` для валидации компонентов, написанных
  вручную. Действует правило: любое добавление или изменение в `schema.h`
  должно сопровождаться сопутствующей строкой проверки.
- **Бенчмарки:** это отдельные исполняемые файлы, каждый из которых сам
  выводит свою таблицу.

Проверки корутин описаны на странице [об асинхронности](async.md).

## Сайт документации

Сайт строит Doxygen из каталога `docs/` и исходников. В дереве wxl это цель
`docs`, доступная при `WXL_BUILD_DOCS=ON`. В сборку по умолчанию она не
входит и строится только по запросу: `tools/build.ps1 -Target docs` или
`cmake --build --preset x64-debug --target docs`.

Каталог `docs` также является самостоятельным проектом CMake без
компилятора:

```
cmake -S docs -B build/docs
cmake --build build/docs --target docs
ctest --test-dir build/docs
```

Doxygen читает исходники, не собирая их, поэтому таким способом сайт можно
построить там, где код не собирается: без MSVC, без настройки дерева wxl и
без сгенерированных проекции и обёрток (в этом случае страниц обёрток WinUI
на сайте нет). Результат находится в `build/docs/docs/wxl/html/index.html`.

Требуется Doxygen версии 1.18 или новее: это первый выпуск, который создаёт
из `export module` страницу модуля. Если в системе такой версии нет,
настройка сама скачивает закреплённую сборку 1.18.0 для Windows или Linux; на
других системах Doxygen необходимо установить вручную. Тема оформления
`doxygen-awesome-css`, закреплённая по коммиту, также загружается при
настройке (`cmake/wxl_doxygen.cmake`).

## Свой проект

Приложение на wxl объявляется функциями из каталога `cmake/`. Так выглядит
`samples/Quadratic/CMakeLists.txt`:

```cmake
wxl_add_executable(sample.quadratic
    main.cpp
)

target_link_libraries(sample.quadratic PRIVATE wxl::fmt)

wxl_target_icon(sample.quadratic app.ico)
wxl_target_assets(sample.quadratic Assets)
```

- **`wxl_add_executable(<цель> <исходники>…)`:** объявляет оконный exe с
  точкой входа из `wxl::ui` и манифестом из `cmake/wxl_app.manifest.in` и
  вызывает `wxl_target_runtime`.
- **`wxl_target_runtime(<цель>)`:** размещает рядом с exe DLL загрузчика
  Windows App Runtime, Win2D и `WebView2Loader.dll`. Вызывать её вручную
  требуется только для цели, объявленной иначе.
- **`wxl_target_icon(<цель> <файл.ico>)`:** добавляет значок в ресурсы exe.
  Этот же значок wxl назначает окнам без собственного значка и значку в
  трее.
- **`wxl_target_assets(<цель> <папка>)`:** размещает папку рядом с exe, по
  правилу на каждый файл. Адрес ресурса без схемы (`u"Assets/logo.png"`)
  ищется там же.

В исходниках подключаются заголовки `wxl.ui` (`ui.h` и соседние, как в
`samples/HelloHere/pch.h`) и определяется функция
`wxl::Teardown wxl_launched()`, объявленная в `launch.h`. Проще всего
создать папку в `samples/` и добавить строку в `samples/CMakeLists.txt`.

### Рядом с деревом wxl

Проект, расположенный рядом с деревом wxl, подключает его подкаталоги.
Порядок при этом важен. Менеджер vcpkg и ворота `import std;` открываются до
`project()`: UUID ворот привязан к выпуску CMake, и открыть их может только
верхний уровень. Стандарт и модули задаются до первого подкаталога. Дерево
wxl к этому моменту должно быть уже настроено (проекция и обёртки
сгенерированы), а пресет проекта должен задавать Ninja и тот же триплет
vcpkg; проще всего взять `CMakePresets.json` из wxl:

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

Исходники wxl собираются вместе с проектом потребителя, тем же компилятором
и с теми же флагами. Поэтому каждый `.ixx` на всё дерево сборки компилируется
лишь однажды — при сборке самой библиотеки:

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

Библиотеку `wxl.core` можно настроить и самостоятельно, с её собственными
пресетами (`wxl.core/CMakePresets.json`, каталог сборки
`wxl.core/build/x64`). В этом случае у неё включён `WXL_INSTALL`, и она
формирует правила установки. Приложению на `wxl.ui` необходим вариант
«рядом с деревом».

## Release и дефект MSVC

В конфигурации **Release** (`/O1` или `/O2` вместе с `/EHsc`) MSVC не
вызывает деструкторы локальных переменных корутины, у которой
`final_suspend` возвращает `std::suspend_never`, и не освобождает её кадр,
если считает, что тело корутины не бросает исключений. Дефект проявляется
в MSVC для x64; с `/Od` или `/EHa` код работает верно. Дефект найден на
тулсете 14.51 и воспроизводится на текущем 14.53 (октябрь 2026); подробный
отчёт — в `bugs/coro-report.md`. Поскольку завершение приложения ждёт, пока
не останется живых кадров корутин, демонстрационные примеры, собранные с
`-Config Release`, могут зависнуть при закрытии окна.

**Обход:** собирайте в конфигурации **Debug**, без оптимизации (`/Od`) или с
асинхронной моделью исключений (`/EHa`). Пока дефект не исправлен, рабочей
конфигурацией остаётся Debug.
