# Смайлы RSDN

Картинки смайлов форума, на которые ссылается токенизатор `wxl.rsdn`:
код `:)` в тексте становится `<img src="smiles/smile.gif" width="15"
height="15" alt=":)">`, и показывающий разметку кладёт эту папку рядом с
ней — `MarkupBlock::baseDirectory()`, как для любой относительной
картинки. Образец `samples/HtmlView` копирует её в `Assets/smiles`.

Файлы взяты из RsdnFormatter (`Format/Resources/Binary`, лицензия MIT,
© Russian Software Developer Network) без изменений; `Facepalm.gif`
переименован в строчные, как называет его сам форматтер. Таблица кодов и
размеров — в `src/rsdn.cpp`, размеры те, что рисует форум.
