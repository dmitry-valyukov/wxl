# StreamRefUriProbe

Что `RandomAccessStreamReference` делает с адресом `file:///` и что с такой
ссылкой делает буфер обмена. Только проекция cppwinrt, никаких обёрток wxl:
вопрос в том, что отвечает сама Windows. Те же шаги через обёртки wxl —
`sandbox/StreamRefUriWxlProbe`.

## Вопрос

`DataPackage::SetBitmap(RandomAccessStreamReference::CreateFromUri(Uri{"file:///…"}))`
и `Clipboard::SetContent` проходят без ошибки, формат `Bitmap` в буфере виден,
`DataPackageView::GetBitmapAsync` отдаёт ссылку, а `RandomAccessStreamReference::OpenReadAsync`
над ней бросает исключение. Объяснений три: схему `file` ссылка не поддерживает;
буфер обмена не может переслать ссылку, сделанную из адреса; ошибка в обёртке.

## Ответ

Windows 11 (10.0.26200), приложение без идентификатора пакета, Debug.

**Схему `file` не поддерживает сама ссылка.** `RandomAccessStreamReference::OpenReadAsync`
над ссылкой, сделанной `RandomAccessStreamReference::CreateFromUri` из любого
написания адреса с `file` (`file:///M:/…`, `file://localhost/M:/…`, с обратными
слэшами, с кириллицей и пробелом в имени — открытой и закодированной `%XX`,
с `#`, `+`, `&` в имени), завершается ошибкой `0x80004001` (`E_NOTIMPL`) с текстом
«Этот API поддерживает только схемы URI http, https, ms-appx и ms-appdata».
То же сказано в документации (`RandomAccessStreamReference.CreateFromUri`: допустимые
схемы — http, https, ms-appx, ms-appdata; `Windows.Foundation.Uri`: схема `file`
для приложений Windows Runtime заблокирована, ошибка приходит не при создании `Uri`,
а при подстановке его в вызов). Сам вызов `OpenReadAsync` не бросает: ошибка приходит
в возвращённой операции. Создание ссылки и `Uri` схему не проверяют.

**Буфер обмена ни при чём.** `Clipboard::SetContent` ссылку не открывает, поэтому и
проходит; ссылку открывает вставка (или `Clipboard::Flush`), и она получает тот же
`0x80004001`, что и прямое чтение. Так же в другом процессе и у стороннего читателя:
`System.Windows.Forms.Clipboard.GetImage` возвращает пустоту. После `Clipboard::Flush`
по такой ссылке буфер остаётся без картинки: `Flush` ошибки не говорит, а потом либо
формата `Bitmap` нет вовсе, либо `DataPackageView::GetBitmapAsync` падает с `0x800401D3`
(`CLIPBRD_E_BAD_DATA`) — в разных прогонах по-разному.

| способ | без буфера обмена | через буфер (`SetContent`, вставка в своём и в другом процессе) | с `Flush` |
|---|---|---|---|
| `CreateFromUri` `file:///…` (7 написаний) | `0x80004001` | положить можно, вставка `0x80004001` | положить можно, картинки в буфере нет |
| `CreateFromUri` `ms-appx:///…`, `ms-appdata:///…` | `0x80070057` | вставка `0x80070057` | — |
| `CreateFromUri` `http://127.0.0.1:порт/…` | читается | читается | в буфере BMP |
| `CreateFromStream(FileRandomAccessStream)` | читается, повторно тоже | читается | в буфере BMP |
| то же, но поток закрыт после создания ссылки | `0x80000013` (объект закрыт) | вставка `0x80000013` | картинки в буфере нет |
| `CreateFromFile(StorageFile)` | читается, повторно тоже | читается | в буфере BMP |
| `CreateFromStream(InMemoryRandomAccessStream)` | читается | читается | в буфере BMP |

Через буфер из семи написаний `file` проверены три (ASCII, `localhost`, кириллица с `%XX`),
из `ms-appx`/`ms-appdata` — `ms-appx`.

`ms-appx` и `ms-appdata` у приложения без идентификатора пакета — `E_INVALIDARG`
(`0x80070057`); с пакетом они, по документации, работают (не проверялось).

**Пути.** `StorageFile::GetFileFromPathAsync` и `FileRandomAccessStream::OpenAsync`
принимают только путь с обратными слэшами: с прямыми (`M:/…`) или смешанными (`M:\a/b.jpg`)
они падают с `0x800700A1` (`ERROR_BAD_PATHNAME`), текст сообщения при этом искажён
(`Длина указанного пути (UNABLE_TO_MASK_PATH) превышает максимально допустимое значение …`
с мусорным числом). `std::filesystem::path::generic_wstring()` даёт прямые слэши, а
`std::filesystem::path::operator/` не заменяет прямые слэши в добавляемой части — родной
путь получается через `make_preferred()`.

Файл ссылка, сделанная `CreateFromStream` или `CreateFromFile`, от записи не защищает: пока
она лежит в буфере, файл открывается на запись (проверено при `FileShare.Read`, `ReadWrite`
и `ReadWrite, Delete`); удаление не проверялось.

## Запуск

```
sandbox.stream-ref-uri-probe.exe                                  вся матрица
sandbox.stream-ref-uri-probe.exe --put stream [--flush] [--hold-ms 15000]
sandbox.stream-ref-uri-probe.exe --paste
```

Матрица занимает около минуты: проба открывает простое окно, берёт передний план
(`Clipboard` в классическом приложении работает только у окна переднего плана) и
**перезаписывает содержимое буфера обмена** синтетическими картинками. `--put` вид
(`uri`, `localhost`, `http`, `stream`, `file`, `memory`) кладёт ссылку и держит буфер,
`--paste` в другом процессе вставляет — так проверяется вставка не в том приложении,
которое копировало. Журнал — `stream-ref-uri-probe.log` рядом с исполняемым файлом,
файлы проверок — в `%TEMP%\stream-ref-uri-probe`.

Не проверялось: приложение с идентификатором пакета, `https` по сети (нужен сервер с
сертификатом, которому доверяет система), версии Windows до 11, `Clipboard` с историей и
роумингом (на машине проб они выключены, поэтому картинки в облако не уходят).
