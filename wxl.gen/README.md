# wxl.gen

Генератор проекции `winui-srcgen`: читает метаданные `.winmd` по профилю из
`profiles/` и пишет обёртки, теги, схему и её тест в `wxl.ui/src/wxl/`.
Библиотека `wxl.gen` и исполняемый файл `winui-srcgen`; профили и обход
метаданных, общие с редактором профилей, — в `wxl.gen.common`. С флагом
`--docs` (и `--doc-language`) генератор пишет в обёртки документацию
метаданных комментариями `///`. Устройство —
[docs/projection.md](../docs/projection.md), запуск —
[docs/building.md](../docs/building.md).
