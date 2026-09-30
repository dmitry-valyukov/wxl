# Gallery — WinUI 3 Gallery без XAML

Перенос [WinUI 3 Gallery](https://github.com/microsoft/WinUI-Gallery) на wxl:
те же типы WinUI 3, разметки нет. Оболочка — `Shell.cpp` (окно с `TitleBar` и
`NavigationView`, история переходов), каталог контролов —
`Assets/Data/ControlInfoData.json`, страница контрола — по файлу
`<Контрол>Page.cpp`, а её код — `Snippets/<Контрол>/*.h`: тот же текст и
выполняется, и показывается под примером.

Введения к примерам — `Snippets/<Контрол>/*.html`, показываются `HtmlBlock`;
оригинал даёт их статичным `TextBlock`.

Какие страницы перенесены, видно в `ItemPages.cpp`: контрол, которого там нет,
в навигации и на плитках недоступен, как в оригинале контрол, не попавший в
сборку.

## Происхождение данных

Каталог `ControlInfoData.json`, картинки в `Assets/ControlImages`,
`Assets/HomeHeaderTiles`, `Assets/GalleryHeaderImage.png`, значок
`Assets/Tiles/GalleryIcon.ico` и `Assets/Slices.png` взяты из репозитория
WinUI 3 Gallery, © Microsoft Corporation, лицензия MIT.
