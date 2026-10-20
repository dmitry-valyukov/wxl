# wxl

wxl (WinUI XAML-Less) — экспериментальная библиотека для настольных
приложений Windows на C++23: интерфейс WinUI описывается прямо в коде,
декларативно и без XAML. Под ней — собственный генератор проекций
`winui-srcgen`: по метаданным `.winmd` и профилю он пишет тонкие обёртки
только для нужных типов. Примеры — обычные исполняемые файлы без упаковки,
запускаются прямо из каталога сборки.

Документация — на сайте и в дереве, в `docs/`:

- устройство wxl по слоям и причины решений —
  [сайт](https://dmitry-valyukov.github.io/wxl/index.html),
  [docs/index.md](docs/index.md);
- сборка и подключение к своему проекту —
  [сайт](https://dmitry-valyukov.github.io/wxl/md_docs_2building.html),
  [docs/building.md](docs/building.md);
- цикл статей на Хабре —
  [сайт](https://dmitry-valyukov.github.io/wxl/md_docs_2articles.html),
  [docs/articles.md](docs/articles.md).

## Зачем

Официальный путь к WinUI из C++ — C++/WinRT и XAML: интерфейс пишется на
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

Остальные примеры, от калькулятора до переноса WinUI Gallery, — в
[samples/README.md](samples/README.md); устройство синтаксиса — в
[docs/dsl.md](docs/dsl.md).

## Быстрый старт

Нужны Windows x64, Visual Studio 2026 (канал Insiders) с тулсетом MSVC 14.53,
CMake 4.3 или 4.4, Ninja, PowerShell 7 (`pwsh`) и vcpkg, у которого один раз
выполнен `vcpkg integrate install`: сборка находит vcpkg по `vcpkg.path.txt`
или по `-DVCPKG_ROOT=<путь>`.

Из корня дерева, в `pwsh`, от клона до окна:

```powershell
pwsh -NoProfile -File tools\restore-packages.ps1    # пакеты NuGet, версии — tools\packages.json
pwsh -NoProfile -File tools\projection.ps1          # проекция cppwinrt в generated\
pwsh -NoProfile -File tools\install-runtime.ps1     # экспериментальная среда Windows App Runtime
tools\build.ps1 -Target winui-srcgen -Define WXL_BUILD_DOCS=OFF
build\x64\wxl.gen\Debug\winui-srcgen.exe --profile wxl.gen\profiles\full.json
tools\build.ps1 -Configure -Target sample.quadratic
build\x64\samples\Quadratic\Debug\sample.quadratic.exe
```

Оговорки к этим шагам, ключи CMake, тесты и подключение к своему проекту — на
странице сборки, [docs/building.md](docs/building.md).

## Состав

| Каталог | Что внутри |
| --- | --- |
| `wxl.core` | Ядро, модуль `wxl.core`: пул памяти STA-потока, счётчики ссылок, `observable` и события, текст UTF-8/UTF-16 с графемами, пути и файлы, `nullable` |
| `wxl.async` | Корутины `task` и `detached_task`, цикл `sta_loop` с асинхронными файлами, отмена, lock-free очереди и каналы, `future`, потоки |
| `wxl.fmt`, `wxl.logging` | `text_builder` и `format` поверх библиотеки fmt; журнал |
| `wxl.xml`, `wxl.json` | Читатели XML и JSON |
| `wxl.html`, `wxl.bb`, `wxl.rsdn`, `wxl.highlight` | Разбор HTML, BB- и RSDN-разметки для `HtmlBlock`, `BbBlock`, `RsdnBlock`; подсветка кода |
| `wxl.gen.common`, `wxl.gen`, `wxl.gen.ui` | Генератор `winui-srcgen` и визуальный редактор его профилей (собрать и запустить — `run-gen-ui.cmd`) |
| `wxl.ui` | WinUI без XAML: обёртки, декларативный синтаксис, окна, привязки, эффекты, точка входа |
| `samples`, `sandbox`, `test-support`, `cmake`, `tools`, `docs` | Примеры; пробы механизмов WinUI; обвязка тестов; скрипты CMake и сборки; страницы сайта документации |

## Статус и ограничения

Библиотека одного автора: интерфейс меняется вместе с задачами,
совместимость между коммитами не обещается, выпусков и CI нет, перенос
WinUI Gallery идёт по группам контролов.

**Release.** MSVC для x64 с оптимизацией (`/O1`, `/O2`) и синхронной моделью
исключений (`/EHsc`) не разрушает локальные переменные корутины, у которой
`final_suspend` возвращает `suspend_never`, и не освобождает её кадр, если
решит, что в теле корутины ничего не бросает; с `/Od` или `/EHa` всё верно
(найдено на 14.51, в октябре 2026 так же ведёт себя 14.53). Так устроена
`async::detached_task`, а выход приложения ждёт, пока живых кадров не
останется, — поэтому пример, собранный с `-Config Release`, может не
закрыться; пока дефект не исправлен, рабочая конфигурация — Debug.
Подробности — [bugs/coro-report.md](bugs/coro-report.md).

- Типа или члена WinUI, которого нет в профиле `full`, нет и в wxl, пока его
  не добавят в профиль и не перегенерируют обёртки.
- Своего наследника класса WinUI с переопределёнными методами
  (`I<Класс>Overrides`) приложение пока не пишет (прототип агрегации —
  `sandbox/Overrides`, не собирается); свою раскладку дают `CustomLayout` и
  `CustomVirtualizingLayout`.

## Лицензия

GNU GPL версии 3, текст — в [LICENSE](LICENSE).

По вопросам альтернативного лицензирования обращаться к автору: `vdimas@mail.ru`.
