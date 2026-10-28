# html

Разбор подмножества HTML для показа чужих сообщений (форумы, чаты) через
`RichTextBlock` без WebView: неизвестный тег исчезает, его текст остаётся.
Соседние разборщики той же модели — `wxl.bb` и `wxl.rsdn`, показ —
`HtmlBlock`, `BbBlock`, `RsdnBlock` в `wxl.ui`. Область модуля `wxl.core`:
`import wxl.core;`, цель сборки: `wxl::core`. Устройство —
[docs/html.md](../../../docs/html.md).
