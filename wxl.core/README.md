# wxl.core

Ядро **wxl** — модуль C++20 `wxl.core` с партициями по одной на файл: пул памяти
STA-потока, счётчики ссылок, `wxl::core::observable` и события, текст UTF-8/UTF-16 с
графемами, пути и файлы, `wxl::core::nullable`. Сборка, ключи и подключение — [«Сборка и проверки»](../docs/building.md);
IntelliSense в VS Code для модулей — [«Работа в редакторе»](../docs/editor.md);
текст и таблица графемных кластеров — [«Текст и разметка»](../docs/text.md).

Области модуля, каждая в своём каталоге `src/`:

- [fmt](src/fmt/README.md) — буфер и построитель текста поверх `fmt`, `format`.
- [async](src/async/README.md) — корутины, `sta_loop`, отмена, `future`, потоки,
  lock-free очереди и каналы.
- [logging](src/logging/README.md) — журнал, где строка — один вызов.
- [xml](src/xml/README.md) — читатель XML: дерево видов в буфер, узлы в арене.
- [json](src/json/README.md) — читатель JSON той же конструкции.
- [html](src/html/README.md) — терпимый разбор подмножества HTML для `HtmlBlock`.
