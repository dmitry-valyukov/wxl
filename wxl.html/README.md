# wxl.html

Разбор подмножества HTML для показа чужих сообщений (форумы, чаты) через
`RichTextBlock` без WebView: неизвестный тег исчезает, его текст остаётся.
Соседние разборщики той же модели — `wxl.bb` и `wxl.rsdn`, показ —
`HtmlBlock`, `BbBlock`, `RsdnBlock` в `wxl.ui`. Модуль для подключения:
`import wxl.html;`, цель сборки: `wxl::html`. Устройство —
[docs/html.md](../docs/html.md).
