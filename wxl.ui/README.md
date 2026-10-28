# wxl.ui

Графический стек WinUI без XAML: сгенерированные обёртки, декларативный
синтаксис, окна, привязки данных, визуальные эффекты и точка входа
`wxl_launched()`. Подключается заголовками (`ui.h` и соседние), цель сборки:
`wxl::ui`; обёртки (`<wxl/X.h>`) в репозиторий не входят: их пишет генератор
`winui-srcgen` шагом сборки в `<сборка>/wxl.ui/full/wxl`
(`cmake/wxl_ui_profile.cmake`). Устройство — [docs/wrappers.md](../docs/wrappers.md),
[docs/dsl.md](../docs/dsl.md), [docs/events.md](../docs/events.md),
[docs/window.md](../docs/window.md); каталог эффектов —
[docs/effects-catalog.md](../docs/effects-catalog.md).
