# wxl

wxl (WinUI XAML-Less) — экспериментальная библиотека для настольных
приложений Windows на C++23: интерфейс WinUI 3 описывается прямо в коде,
декларативно и без XAML. Под ней — собственный генератор проекций
`winui-srcgen`: по метаданным `.winmd` и профилю он пишет тонкие обёртки
только для нужных типов. Примеры — обычные исполняемые файлы без упаковки,
запускаются прямо из каталога сборки.

## Зачем

Официальный путь к WinUI 3 из C++ — C++/WinRT и XAML: интерфейс пишется на
втором языке со своим компилятором, IDL и связью с кодом по именам; огромные
заголовки проекции cppwinrt разбирает каждая единица трансляции, где назван
хоть один контрол; каждый объект — COM с запросами интерфейсов и атомарными
счётчиками ссылок, хотя весь интерфейс живёт в одном потоке.

wxl устроен иначе:

- **Своя проекция.** Проекцию cppwinrt разбирает только сборка самой wxl:
  публичные заголовки её не включают, приложение их не компилирует.
- **Тонкие обёртки.** Обёртка — один указатель на скрытую цепочку `Impl`,
  классы наследуют друг друга так же, как в WinUI.
- **Разметка — выражение C++.** Теги свойств, пресеты, шаблоны, привязки к
  полям `observable` и обработчики пишутся в скобках конструктора; события
  можно ждать через `co_await`.
- **Один поток как модель.** Интерфейс WinUI живёт в одном STA-потоке
  (single-threaded apartment: объекты COM вызываются только из потока, где
  созданы). Объекты, которые пишет сама wxl, поэтому считают ссылки без
  атомарных операций и берут память из пула этого потока; объекты WinUI
  остаются COM-объектами. Всё, что блокирует, уходит в рабочий поток.
- **Точка входа — у библиотеки.** Ни `wWinMain`, ни загрузчика Windows App
  Runtime, ни наследника `Application`: приложение пишет одну функцию,
  `wxl_launched()`.

Подробный рассказ о замысле и генераторе — [первая статья цикла на
Хабре](https://habr.com/ru/articles/1080690/).

## Как выглядит

Квадратное уравнение из [samples/Quadratic/main.cpp](samples/Quadratic/main.cpp)
(с сокращениями): модель — поля `observable`, в окне — привязанные к ним поля.

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

const Preset input {
    common,
    width = 200,
};

// … в окне:
NumberBox {
    input,
    placeholderText = u"a",
    intermediateValue = BindInput {eq.a},
},
// …
TextBox {
    output,
    text = BindOutput {eq.D},
},
```

Остальные примеры, от калькулятора до переноса WinUI 3 Gallery, — в
[samples/README.md](samples/README.md); устройство синтаксиса — в
[README2.md](README2.md#декларативный-синтаксис).

## Быстрый старт

### Требования

- Windows x64.
- Visual Studio 2026 (канал Insiders) с C++, тулсетом MSVC 14.53 и Windows
  SDK — ради `import std;` в модулях C++20.
- CMake 4.3 или 4.4: снизу — `cmake_minimum_required(VERSION 4.3)`, сверху —
  UUID ключа `import std;`, известный
  [cmake/wxl_import_std_gate.cmake](cmake/wxl_import_std_gate.cmake) для 4.1–4.4.
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

### От клона до окна

Проекция cppwinrt (`generated\`) и обёртки wxl (`wxl.ui\src\wxl\`)
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
- `WXL_BUILD_DOCS=OFF` остаётся в кэше; сайт документации и его тест
  возвращает `tools\build.ps1 -Define WXL_BUILD_DOCS=ON`.
- Пример лежит в `build\x64\samples\<Папка>\<Конфигурация>\<цель>.exe`
  с ресурсами и DLL; цели — в [samples/README.md](samples/README.md), все
  разом — `-Target samples/all`.

### Сборка

`tools\build.ps1` сам поднимает окружение Visual Studio с закреплённым тулсетом
(другой — `-Toolset`) и настраивает `build\x64`, если тот не настроен:

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
| `WXL_BUILD_DOCS` | `ON` | сайт документации Doxygen, цель `docs`, результат — `build\x64\docs\wxl\html\index.html` |
| `WXL_BUILD_BENCHMARKS` | `OFF` | бенчмарки `wxl.core`, `wxl.async`, `wxl.fmt`, `wxl.ui` |

### Тесты

```powershell
tools\build.ps1 -Define BUILD_TESTING=ON
ctest --preset x64-debug                 # из корня: пресет ищется в текущем каталоге
ctest --preset x64-debug -R docs         # сайт документации как тест, при WXL_BUILD_DOCS=ON
```

## Состав

| Каталог | Что внутри |
| --- | --- |
| `wxl.core` | Ядро, модуль `wxl.core`: пул памяти STA-потока, счётчики ссылок, `observable` и события, текст UTF-8/UTF-16 с графемами, пути и файлы, `nullable` |
| `wxl.async` | Корутины `task` и `detached_task`, цикл `sta_loop` с асинхронными файлами, отмена, lock-free очереди и каналы, `future`, потоки |
| `wxl.fmt`, `wxl.logging` | `text_builder` и `format` поверх библиотеки fmt; журнал |
| `wxl.xml`, `wxl.json` | Читатели XML и JSON |
| `wxl.html`, `wxl.bb`, `wxl.rsdn`, `wxl.highlight` | Разбор HTML, BB- и RSDN-разметки для `HtmlBlock`, `BbBlock`, `RsdnBlock`; подсветка кода |
| `wxl.gen.common`, `wxl.gen`, `wxl.gen.ui` | Генератор `winui-srcgen` и визуальный редактор его профилей (собрать и запустить — `run-gen-ui.cmd`) |
| `wxl.ui` | WinUI 3 без XAML: обёртки, декларативный синтаксис, окна, привязки, эффекты, точка входа |
| `samples`, `sandbox`, `test-support`, `cmake`, `tools`, `docs` | Примеры; пробы механизмов WinUI; обвязка тестов; скрипты CMake и сборки; главная страница сайта документации |

## Как подключить к своему проекту

Приложение на wxl объявляется функциями из [cmake/](cmake/). Так выглядит
[samples/Quadratic/CMakeLists.txt](samples/Quadratic/CMakeLists.txt):

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
[samples/HelloHere/pch.h](samples/HelloHere/pch.h)) и функция
`wxl::Teardown wxl_launched()` из [wxl.ui/src/launch.h](wxl.ui/src/launch.h).
Проще всего — папка в `samples/` и строка в `samples/CMakeLists.txt`; проект
рядом с деревом wxl и остальные ключи сборки — в [README2.md](README2.md#сборка).

## Статус и ограничения

Библиотека одного автора: интерфейс меняется вместе с задачами,
совместимость между коммитами не обещается, выпусков и CI нет, перенос
WinUI 3 Gallery идёт по группам контролов.

**Release.** MSVC для x64 с оптимизацией (`/O1`, `/O2`) и синхронной
моделью исключений (`/EHsc`) не разрушает локальные переменные корутины, у
которой `final_suspend` возвращает `suspend_never`, и не освобождает её кадр,
если решит, что в теле корутины ничего не бросает; с `/Od` или `/EHa` всё
верно ([bugs/coro-report.md](bugs/coro-report.md): найдено на 14.51, в
октябре 2026 так же ведёт себя 14.53). Так устроена `async::detached_task`, а
выход приложения ждёт, пока живых кадров не останется, — поэтому пример,
собранный с `-Config Release`, может не закрыться. Пока дефект не исправлен,
рабочая конфигурация — Debug.

- Типа или члена WinUI, которого нет в профиле `full`, нет и в wxl, пока его
  не добавят в профиль и не перегенерируют обёртки.
- Своего наследника класса WinUI с переопределёнными методами
  (`I<Класс>Overrides`) приложение пока не пишет (прототип агрегации —
  `sandbox/Overrides`, не собирается); свою раскладку дают `CustomLayout` и
  `CustomVirtualizingLayout`.

## Документация

[README2.md](README2.md) — как устроен wxl и почему так, по слоям, и список
статей цикла; главная страница сайта документации —
[docs/index.md](docs/index.md).

## Лицензия

GNU GPL версии 3, текст — в [LICENSE](LICENSE).

По вопросам альтернативного лицензирования обращаться к автору: `vdimas@mal.ru`.
