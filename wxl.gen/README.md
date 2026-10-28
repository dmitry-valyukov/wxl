# wxl.gen

Генератор проекции `winui-srcgen`: читает метаданные `.winmd` по профилю из
`profiles/` (`<имя>.winxl-profile`) в версиях, которые выбрало восстановление
NuGet сборки (ключ `--packages`), и пишет обёртки, теги, схему и её тест в
каталог `--out`: заголовки — в него, тела методов — в `parts/wrapper-NN.cpp`,
число которых задаёт `--parts`. В дереве генератор запускает сама сборка
(`cmake/wxl_ui_profile.cmake`) и пишет в `<сборка>/wxl.ui/full/wxl`.
Библиотека `wxl.gen` и исполняемый файл `winui-srcgen`; профили и обход
метаданных, общие с редактором профилей, — в `wxl.gen.common`. С флагом
`--docs` (и `--doc-language`) генератор пишет в обёртки документацию
метаданных комментариями `///`. Устройство —
[docs/projection.md](../docs/projection.md), запуск —
[docs/building.md](../docs/building.md).
