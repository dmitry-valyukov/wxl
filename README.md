# wxl

Экспериментальная библиотека для настольных приложений Windows на C++23:
интерфейс WinUI 3 описывается прямо в коде, декларативно и без XAML, поверх
собственной тонкой проекции WinRT.

Название — сокращение от WinUI XAML-Less.

## Зачем

Официальный путь к WinUI 3 из C++ — C++/WinRT и XAML. Интерфейс там пишется
на втором языке: разметку компилирует свой компилятор, типы, к которым она
привязана, описываются в IDL, обработчики и привязки связываются с кодом по
именам. Заголовки проекции cppwinrt велики, и каждая единица трансляции, где
назван хоть один контрол, разбирает их заново. Каждый объект при этом — COM,
с запросами интерфейсов и атомарными счётчиками ссылок, хотя весь интерфейс
живёт в одном потоке.

wxl устроен иначе:

- **Своя проекция.** Генератор `winui-srcgen` (`wxl.gen`) читает метаданные
  `.winmd` из пакетов Windows App SDK и по JSON-профилю пишет обёртки только
  для нужных типов и членов. Проекцию cppwinrt разбирает лишь сборка самой
  wxl: публичные заголовки её не включают.
- **Тонкие обёртки.** Обёртка — один указатель на скрытую цепочку `Impl`,
  классы наследуют друг друга так же, как в WinUI.
- **Разметка — выражение C++.** Теги свойств, пресеты, шаблоны, привязки к
  полям `observable` и обработчики событий пишутся в фигурных скобках
  конструктора; события можно ждать через `co_await`.
- **Один поток как модель.** Счётчики ссылок без атомарных операций, память
  и кадры корутин — из пула STA-потока; всё, что блокирует, уходит в рабочий
  поток и возвращается возобновлением корутины.
- **Точка входа — у библиотеки.** Ни `wWinMain`, ни загрузчика Windows App
  Runtime, ни наследника `Application`: приложение реализует одну функцию
  `wxl_launched()`.

Подробный рассказ о замысле и генераторе — [первая статья цикла на
Хабре](https://habr.com/ru/articles/1080690/); остальные статьи цикла и разбор
устройства — в [README2.md](README2.md).

## Как выглядит

Квадратное уравнение из [samples/Quadratic/main.cpp](samples/Quadratic/main.cpp),
с сокращениями: модель — поля `observable`, интерфейс — одно выражение,
обработчиков ввода нет.

```cpp
class Equation : public noncopyable
{
    observable<Answer> answer;

public:
    observable<double> a{blank}, b{blank}, c{blank};
    observable<u16_text> D, x1, x2;

    Equation() {
        answer                      // три числа → ответ
            .follow(a, b, c, solve)
            .on_change([this](Answer const& answer) noexcept {
                D.set(answer.D);
                x1.set(answer.x1);  // ответ → три текста
                x2.set(answer.x2);
            });
    }
};

const Preset common {
    fontSize = 18,
    hAlign.center,
    vAlign.center,
};

const Preset input {
    common,
    width = 200,
};

// … пресеты txt и output

wxl::Teardown wxl_launched() {
    // Время жизни модели требуется продлить
    auto const model = std::make_shared<Equation>();
    Equation & eq = *model;

    auto window = CompositionWindow {
        title = u"WXL Quadratic",
        minSize = {910, 390},
        background = BackgroundImage {u"Assets/GlassBackdrop.png", BackgroundFill::UniformToFill},
        Card {
            GlassEffect {blurRadius = 24.0f},
            background = rgba(255, 255, 255, 0.35),
            hAlign.center,
            vAlign.center,
            StackPanel {
                spacing = 32,
                // … заголовок
                StackPanel {
                    orientation.horizontal,
                    spacing = 10,
                    NumberBox {
                        input,
                        placeholderText = u"a",
                        intermediateValue = BindInput {eq.a},
                        onLoaded = [](NumberBox const& control) {
                            control.focus(FocusState::Programmatic);
                        },
                    },
                    TextBlock {txt, u"· x² +"},
                    // … поля b и c
                },
                StackPanel {
                    orientation.horizontal,
                    spacing = 10,
                    TextBlock {txt, u"D ="},
                    TextBox {
                        output,
                        text = BindOutput {eq.D},
                    },
                },
                // … корни x₁ и x₂
            },
        }
    };

    window.appWindow().resize({1024, 480});
    window.activate();

    // Продлеваем время жизни модели
    return [model](TeardownReason) {};
}
```

Из [samples/Calculator/main.cpp](samples/Calculator/main.cpp): пресет табло —
с кистью-шаблоном и свечением `HaloEffect` — и клавиатура: двадцать кнопок
строятся обходом строкового литерала при компиляции, у каждой свой обработчик.

```cpp
// Общий стиль для шрифтового оформления LCD-экрана калькулятора
auto const lcdDisplayPreset = Preset {
    fontFamily = FontFamily{u"Assets/digitalism.ttf#Digitalism"},
    foreground = Template<SolidColorBrush>{rgb(44, 58, 28)},
    hAlign.stretch,
    textAlignment.right,
    FontWeight {600},
    HaloEffect {
        color = rgb(92, 138, 32),
        blurRadius = 14.0f
    },
};

// …

[&](iterate<u"C÷×√"
            u"789-"
            u"456+"
            u"123%"
            u"±0.="> key) {
    return Button {
        keyLayoutPreset,
        selectButtonEffect(key.value),
        hAlign.stretch, vAlign.stretch,
        row = key.index / 4 + 1,
        column = key.index % 4,
        onClick = [calc, symbol = key.value] { calc->press(symbol); },

        content = TextBlock {
            key.text(),
            fontSize = 28,
            FontWeight {600},
            hAlign.center,
            vAlign.center,
            // …
        },
    };
},
```

Как это устроено — теги, безымянные аргументы, пресеты и шаблоны, привязки,
эффекты — разобрано в [README2.md](README2.md#декларативный-синтаксис).

## Быстрый старт

### Требования

- **Windows, x64.**
- **Visual Studio 2026 с компонентом C++ и тулсетом MSVC 14.53.** Всё дерево собрано из
  модулей C++20 с `import std;`, а это умеют только свежие тулсеты; сейчас
  они приходят с каналом Insiders, и `tools\build.ps1` ищет установку с
  `vswhere -prerelease`. Тулсет закреплён в `tools\build.ps1` (ключ
  `-Toolset`) и в `CMakePresets.json`.
- **CMake 4.3 или 4.4 и Ninja.** Ключ CMake, открывающий `import std;`,
  экспериментальный, и его UUID меняется с каждым выпуском;
  [cmake/wxl_import_std_gate.cmake](cmake/wxl_import_std_gate.cmake) знает
  его только для этих версий. Генераторы Visual Studio не годятся: они не
  строят BMI импортированных целей. Подойдут `cmake` и `ninja` из состава
  Visual Studio, если версия CMake попадает в диапазон.
- **PowerShell 7** (`pwsh`): на нём написаны скрипты в `tools\`, через него
  работают и `.cmd`/`.bat` в корне.
- **vcpkg** с портом `cppwinrt` для триплета `x64-windows-static-md`, а для
  тестов ещё и `gtest`:

  ```powershell
  vcpkg install cppwinrt:x64-windows-static-md
  vcpkg install gtest:x64-windows-static-md    # только для тестов
  ```

  Сборка находит vcpkg по файлу
  `%USERPROFILE%\Local Settings\vcpkg\vcpkg.path.txt`, который vcpkg пишет
  сам, или по `-DVCPKG_ROOT=<путь>` при настройке. Переменная окружения
  `VCPKG_ROOT` на Windows не читается: `vcvars64.bat` подменяет её на vcpkg
  из состава Visual Studio, в котором нет порта `cppwinrt`
  ([cmake/vcpkg_bootstrap.cmake](cmake/vcpkg_bootstrap.cmake)).
- **Сеть на первую настройку:** NuGet-пакеты (шаг 1 ниже), исходники `winmd`
  и `fmt` через `FetchContent`, а для сайта документации — Doxygen 1.18, если
  в системе нет подходящего, и его тема (сайт отключается ключом
  `WXL_BUILD_DOCS`, см. «Сборка»).

### Первая настройка клона

Две вещи генерируются и в репозиторий не входят: проекция cppwinrt для
WinUI 3 в `generated\` и обёртки wxl в `wxl.ui\src\wxl\`. Поэтому чистый клон
настраивается в шесть шагов, из корня дерева:

1. **Пакеты NuGet** — Windows App SDK, Win2D, WebView2 в папку пакетов NuGet.
   Версии закреплены: выпуск Windows App SDK — ключ `windowsAppSdk` в
   [wxl.gen/profiles/base.json](wxl.gen/profiles/base.json), остальные — в
   [tools/packages.json](tools/packages.json). Уже скачанное повторно не
   качается.

   ```powershell
   pwsh -NoProfile -File tools\restore-packages.ps1
   ```

2. **Экспериментальная среда Windows App Runtime** — ставится для текущего
   пользователя рядом со стабильной. Обёртки сгенерированы по метаданным
   экспериментального канала WinUI (стабильный пакет не объявляет
   `WrapPanel` и `LayoutPanel`), и приложение просит среду этого канала
   (ключ `WXL_WINDOWSAPPSDK_VERSION_TAG`); без неё оно не запустится.

   ```powershell
   pwsh -NoProfile -File tools\install-runtime.ps1
   ```

3. **Проекция cppwinrt** в `generated\`. Отдельного скрипта для этого шага в
   дереве нет; это вызов `cppwinrt.exe` из порта vcpkg над метаданными
   пакетов из шага 1:

   ```powershell
   & "<vcpkg>\installed\x64-windows-static-md\tools\cppwinrt\cppwinrt.exe" `
       -input "<nuget>\microsoft.windowsappsdk.winui\<версия>\metadata\Microsoft.UI.Xaml.winmd" `
       -input "<nuget>\microsoft.windowsappsdk.winui\<версия>\metadata\Microsoft.UI.Text.winmd" `
       -input "<nuget>\microsoft.windowsappsdk.interactiveexperiences\<версия>\metadata\<новейшая папка>" `
       -input "<nuget>\microsoft.windowsappsdk.foundation\<версия>\metadata" `
       -input "<nuget>\microsoft.web.webview2\<версия>\lib\Microsoft.Web.WebView2.Core.winmd" `
       -input "<nuget>\microsoft.graphics.win2d\<версия>\lib\uap10.0\Microsoft.Graphics.Canvas.winmd" `
       -reference sdk -output generated
   ```

   `<nuget>` — папка пакетов NuGet: `%NUGET_PACKAGES%`, иначе
   `globalPackagesFolder` из `NuGet.Config`, иначе
   `%USERPROFILE%\.nuget\packages`. Версии — те же закреплённые: WinUI, Win2D
   и WebView2 — из `tools/packages.json`, Foundation и InteractiveExperiences —
   те, что объявляет выпуск Windows App SDK из `base.json`. Проекцию,
   сделанную в другом месте, называет ключ `WXL_CPPWINRT_PROJECTION_DIR`.

4. **Место для обёрток.** `wxl.ui/CMakeLists.txt` подключает `src/wxl`
   безусловно, поэтому до первой генерации там нужен хотя бы пустой
   `CMakeLists.txt`:

   ```powershell
   New-Item -ItemType Directory -Force wxl.ui\src\wxl | Out-Null
   if (-not (Test-Path wxl.ui\src\wxl\CMakeLists.txt)) {
       New-Item -ItemType File wxl.ui\src\wxl\CMakeLists.txt | Out-Null
   }
   ```

5. **Генератор и обёртки.** Генератор запускается из корня дерева: каталог
   вывода по умолчанию, `wxl.ui/src/wxl`, отсчитывается от текущего. Дерево
   собирается из профиля `full`; профили поменьше не содержат типов, на
   которых стоят примеры.

   ```powershell
   tools\build.ps1 -Configure -Target winui-srcgen
   build\x64\wxl.gen\Debug\winui-srcgen.exe --profile wxl.gen\profiles\full.json
   ```

   Сборка генератор не запускает: после правки профиля или самого генератора
   этот шаг повторяется руками.

6. **Всё остальное:**

   ```powershell
   tools\build.ps1 -Configure
   ```

### Сборка

`tools\build.ps1` сам поднимает окружение Visual Studio — находит установку
через `vswhere` и переносит в процесс переменные `vcvars64.bat` для
закреплённого тулсета, — поэтому хватает обычной оболочки. Каталог сборки —
`build\x64`; если он ещё не настроен, скрипт его настроит.

```powershell
tools\build.ps1                          # Debug
tools\build.ps1 -Config Release
tools\build.ps1 -Target sample.calc      # одну цель
tools\build.ps1 -Configure               # заново настроить перед сборкой
tools\build.ps1 -Define BUILD_TESTING=ON,WXL_BUILD_BENCHMARKS=ON
tools\build.ps1 -Toolset 14.52           # другой установленный тулсет MSVC
```

`-Define` передаёт записи кэша на шаг настройки (и потому вызывает его);
значения остаются в `CMakeCache.txt`, так что следующие запуски без них
сохраняют поставленное. Несколько записей — через запятую в одном `-Define`.

В Developer PowerShell, где окружение уже поднято, то же самое — пресетами,
из корня дерева:

```powershell
cmake --preset x64
cmake --build --preset x64-debug         # или x64-release
```

Пресет настройки один — `x64`: Ninja Multi-Config с конфигурациями Debug и
Release, триплет `x64-windows-static-md`, каталог `build\x64`. Пресеты сборки
и тестов — `x64-debug` и `x64-release`.

| Ключ | По умолчанию | Что делает |
| --- | --- | --- |
| `BUILD_TESTING` | `OFF` | тесты GTest |
| `WXL_BUILD_BENCHMARKS` | `OFF` | бенчмарки `wxl.core`, `wxl.async`, `wxl.fmt`, `wxl.ui` |
| `WXL_BUILD_DOCS` | `ON` | сайт документации Doxygen, цель `docs` |
| `WXL_STRICT_CORO` | `OFF` | проверки использования корутин во всех сборках, а не только в Debug |
| `WXL_CPPWINRT_PROJECTION_DIR` | `generated` | каталог проекции cppwinrt |
| `WXL_WINDOWSAPPSDK_VERSION_TAG` | `experimentalF` | канал Windows App Runtime; пустое значение — стабильный |

**Release и дефект MSVC.** Оптимизирующий компилятор при `/EHsc` не
разрушает локальные переменные корутины, у которой `final_suspend` —
`suspend_never`, и не освобождает её кадр; так устроена
`async::detached_task`. Описание и пример — [bugs/coro-report.md](bugs/coro-report.md)
(проверено на 14.51; в октябре 2026 дефект есть и в 14.53). Пока он не
исправлен, рабочая конфигурация — Debug.

### Запуск примеров

Примеры собираются вместе с деревом, каждый — своей целью. Исполняемый файл
лежит в `build\x64\samples\<Папка>\<Конфигурация>\<цель>.exe`, а рядом с ним —
ресурсы примера и библиотеки, нужные wxl во время работы (см.
`wxl_target_runtime` ниже), так что запускать можно прямо оттуда:

```powershell
tools\build.ps1 -Target sample.calc
build\x64\samples\Calculator\Debug\sample.calc.exe
```

| Папка | Цель | Запуск (Debug) |
| --- | --- | --- |
| HelloHere | `sample.hello` | `build\x64\samples\HelloHere\Debug\sample.hello.exe` |
| Quadratic | `sample.quadratic` | `build\x64\samples\Quadratic\Debug\sample.quadratic.exe` |
| Calculator | `sample.calc` | `build\x64\samples\Calculator\Debug\sample.calc.exe` |
| CustomTitleBar | `sample.custom-titlebar` | `build\x64\samples\CustomTitleBar\Debug\sample.custom-titlebar.exe` |
| HtmlView | `sample.htmlview` | `build\x64\samples\HtmlView\Debug\sample.htmlview.exe` |
| Gallery | `sample.gallery` | `build\x64\samples\Gallery\Debug\sample.gallery.exe` |
| Trayed | `sample.trayed` | `build\x64\samples\Trayed\Debug\sample.trayed.exe cmd /k dir` |

Trayed запускает программу, названную в его командной строке. Что
показывает каждый пример — в [samples/README.md](samples/README.md).

Все примеры разом собирает `tools\build.ps1 -Target samples/all`.
`run-samples.bat` в корне собирает их в Release и показывает меню: номер — и
пример запущен; о Release см. выше. `run-gen-ui.cmd` собирает в Debug и
запускает редактор профилей генератора `wxl.gen.ui`.

### Тесты

`BUILD_TESTING` по умолчанию выключена, и пресеты её не трогают, поэтому
тесты просят один раз на каталог сборки — значение остаётся в
`CMakeCache.txt`:

```powershell
tools\build.ps1 -Define BUILD_TESTING=ON    # или cmake --preset x64 -DBUILD_TESTING=ON
ctest --preset x64-debug
ctest --preset x64-debug -R docs           # сайт документации как тест
```

`ctest` ищет пресет в `CMakePresets.json` текущего каталога, поэтому
запускается из корня дерева. Ответ «No tests were found» значит, что каталог
сборки настроен без тестов.

## Состав

| Каталог | Что внутри |
| --- | --- |
| `wxl.core` | Ядро, модуль `wxl.core`: пул памяти STA-потока, счётчики ссылок, `observable` и события, текст UTF-8/UTF-16 с графемами, пути и файлы, `nullable` |
| `wxl.async` | Корутины `task` и `detached_task`, цикл `sta_loop` с асинхронными файлами, отмена, lock-free очереди и каналы, `future`, потоки |
| `wxl.fmt` | `text_builder` и `format` поверх библиотеки fmt |
| `wxl.logging` | Журнал |
| `wxl.xml`, `wxl.json` | Читатели XML и JSON |
| `wxl.html`, `wxl.bb`, `wxl.rsdn` | Разбор HTML, BB- и RSDN-разметки для `HtmlBlock`, `BbBlock`, `RsdnBlock` |
| `wxl.highlight` | Подсветка синтаксиса кода |
| `wxl.gen.common`, `wxl.gen` | Генератор обёрток `winui-srcgen`: профили, обход метаданных, вывод |
| `wxl.gen.ui` | Визуальный редактор профилей генератора — приложение на wxl |
| `wxl.ui` | WinUI 3 без XAML: обёртки, декларативный синтаксис, окна, привязки, эффекты, точка входа |
| `samples` | Примеры приложений |
| `sandbox` | Пробы механизмов WinUI, каждая — отдельное приложение |
| `test-support` | Общая обвязка тестов |
| `cmake`, `tools` | Скрипты CMake и сборки |
| `docs` | Главная страница сайта документации |

## Как подключить к своему проекту

Приложение на wxl объявляется в CMake функцией `wxl_add_executable` из
[cmake/wxl_add_executable.cmake](cmake/wxl_add_executable.cmake). Так выглядит
[samples/Quadratic/CMakeLists.txt](samples/Quadratic/CMakeLists.txt) целиком:

```cmake
wxl_add_executable(sample.quadratic
    main.cpp
)

target_link_libraries(sample.quadratic PRIVATE wxl::fmt)

wxl_target_icon(sample.quadratic app.ico)
wxl_target_assets(sample.quadratic Assets)
```

- `wxl_add_executable(<цель> <исходники>…)` — оконный исполняемый файл
  (`WIN32`, точка входа — `wWinMain` из `wxl::ui`), слинкованный с `wxl::ui`.
  Манифест он генерирует из [cmake/wxl_app.manifest.in](cmake/wxl_app.manifest.in);
  свой `.manifest` среди исходников его заменяет.
- `wxl_target_runtime(<цель>)` — кладёт рядом с исполняемым файлом то, что
  нужно wxl во время работы: DLL загрузчика Windows App Runtime, Win2D и
  `WebView2Loader.dll`. `wxl_add_executable` вызывает его сама; руками — только
  для цели, объявленной иначе.
- `wxl_target_icon(<цель> <файл.ico>)` — значок ресурсом в исполняемом файле.
  Его показывает Проводник, его же wxl ставит окнам без своего значка и
  значку в трее.
- `wxl_target_assets(<цель> <папка>)` — копирует папку рядом с исполняемым
  файлом, по правилу на файл: изменённый файл копируется без перелинковки.
  Адрес ресурса без схемы (`u"Assets/logo.png"`) wxl ищет рядом с
  исполняемым файлом.

В самом приложении — заголовки `wxl.ui` (`ui.h` и нужные рядом с ним, как в
[samples/HelloHere/pch.h](samples/HelloHere/pch.h)) и одна функция
`wxl::Teardown wxl_launched()`
([wxl.ui/src/launch.h](wxl.ui/src/launch.h)).

**Внутри дерева wxl** проще всего: новая папка в `samples/` со своим
`CMakeLists.txt` и строка `add_subdirectory` в `samples/CMakeLists.txt`.

**Своим проектом рядом с деревом wxl** — подключая подкаталоги wxl. Порядок
важен: vcpkg и ворота `import std;` открываются до `project()`, стандарт и
модули — до первого подкаталога. Дерево wxl при этом должно пройти первую
настройку (проекция и обёртки уже сгенерированы), а пресет проекта — давать
Ninja и тот же триплет vcpkg (проще всего взять `CMakePresets.json` wxl):

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

## Статус и ограничения

wxl — экспериментальная библиотека одного автора. Интерфейс меняется вместе
с задачами, совместимость между коммитами не обещается, выпусков и CI нет:
сборка и тесты идут на машине разработчика.

В дереве есть всё, что перечислено в «Составе», и семь примеров; перенос
WinUI 3 Gallery идёт по группам контролов. Ограничения:

- Набор инструментов узкий — см. [Требования](#требования).
- Проекция cppwinrt и обёртки не хранятся в репозитории, а для проекции в
  дереве нет скрипта — см. [Первую настройку](#первая-настройка-клона).
- Release-сборку портит дефект MSVC в корутинах — см. [Сборку](#сборка).
- Тип или член WinUI, которого нет в профиле `full`, недоступен, пока его не
  добавят в профиль и не перегенерируют обёртки.
- Своего наследника класса WinUI с переопределёнными виртуальными методами
  приложение пока не пишет, а компонент со своим потоком берётся через
  cppwinrt напрямую — см. [README2.md](README2.md#обёртки-и-кэш-интерфейсов).

## Документация и статьи

- [README2.md](README2.md) — как устроен wxl и почему так: проекция,
  обёртки, синтаксис, асинхронность, окно, со ссылками на исходники.
- Статьи цикла на Хабре, вышедшие и готовящиеся, — [список в
  README2](README2.md#статьи); о замысле — первая, ссылка в начале.
- Сайт документации собирается из исходников и статей в дереве:
  `tools\build.ps1 -Target docs`, результат —
  `build\x64\docs\wxl\html\index.html`; главная страница —
  [docs/index.md](docs/index.md).
- [samples/README.md](samples/README.md) — что показывает каждый пример.

## Лицензия

GNU GPL версии 3, текст — в [LICENSE](LICENSE).

По вопросам альтернативного лицензирования обращаться к автору: `vdimas@mal.ru`.
